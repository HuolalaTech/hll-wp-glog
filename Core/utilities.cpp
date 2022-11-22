//
// Created by issac on 2021/1/20.
//

#include "utilities.h"
#include "Glog.h"
#include "InternalLog.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
//#include <uuid/uuid.h>
#include <zlib.h>

namespace glog {

int64_t cycleClockNow() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

WallTime_t wallTimeNow() {
    // Now, cycle clock is retuning microseconds since the epoch.
    return cycleClockNow() * 0.000001;
}

void tmNow(::tm *outTm) {
    auto time = static_cast<time_t>(wallTimeNow());
    ::localtime_r(&time, outTm);
}

time_t midnightEpochSeconds() {
    ::tm now{};
    tmNow(&now);
    now.tm_hour = now.tm_min = now.tm_sec = 0;
    return ::timelocal(&now);
}

uint16_t reverseU16(uint16_t value) {
    return (((value & 0xFF) << 8) | ((value & 0xFF00) >> 8));
}

uint32_t reverseU32(uint32_t value) {
    return (((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) |
            ((value & 0xFF000000) >> 24));
}

uint16_t toLittleEndianU16(uint16_t value) {
    return SYS_ENDIAN == Endianness::BigEndian ? reverseU16(value) : value;
}

uint32_t toLittleEndianU32(uint32_t value) {
    return SYS_ENDIAN == Endianness::BigEndian ? reverseU32(value) : value;
}

uint16_t readU16Le(uint16_t value) {
    return SYS_ENDIAN == Endianness::BigEndian ? reverseU16(value) : value;
}

//void generateSync(uint8_t out[]) {
//    uuid_t u_t = {0};
//    char uuid[36];
//
//    uuid_generate(u_t);
//    uuid_unparse(u_t, uuid);
//
//    // microseconds since the epoch
//    int64_t timestamp = cycleClockNow();
//
//    char input[36 + 1 + 16 + 1]; // uuid + '@' + timestamp + '\0'
//    snprintf(input, sizeof(input), "%s@%lld", uuid, timestamp);
//
//    uLong crc = crc32(0L, Z_NULL, 0);
//    crc = crc32(crc, reinterpret_cast<const Bytef *>(input), sizeof(input) - 1);
//
//    InternalDebug("generate sync:%lx", crc);
//
//    out[0] = crc >> 24 & 0xFF;
//    out[1] = (0xFF0000 & crc) >> 16;
//    out[2] = (0xFF00 & crc) >> 8;
//    out[3] = 0xFF & crc;
//}

bool getFileSize(int fd, size_t &size) {
    struct stat st = {};
    if (::fstat(fd, &st) != -1) {
        size = (size_t) st.st_size;
        return true;
    }
    return false;
}

size_t getFileSize(const string &filepath) {
    struct stat st {};
    int rc = stat(filepath.c_str(), &st);
    return rc == 0 ? static_cast<size_t>(st.st_size) : 0;
}

size_t getPageSize() {
    return getpagesize();
}

bool isFileExists(const std::string &path) {
    struct stat st {};
    return (stat(path.c_str(), &st) == 0);
}

bool mkPath(const string &str) {
    unique_ptr<char> path{strdup(str.c_str())};

    struct stat sb = {};
    bool done = false;
    char *slash = path.get();

    while (!done) {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        done = (*slash == '\0');
        *slash = '\0';

        if (stat(path.get(), &sb) != 0) {
            if (errno != ENOENT || mkdir(path.get(), 0777) != 0) {
                InternalWarning("%s : %s", path.get(), strerror(errno));
                return false;
            }
        } else if (!S_ISDIR(sb.st_mode)) {
            InternalWarning("%s: %s", path.get(), strerror(ENOTDIR));
            return false;
        }

        *slash = '/';
    }
    return true;
}

bool endsWithSplash(const string &path) {
    const string SLASH = "/";
    return path.compare(path.size() - SLASH.size(), SLASH.size(), SLASH) == 0;
}

string filename(const string &path) {
    char sep = '/';

    size_t i = path.rfind(sep, path.length());
    if (i != string::npos) {
        return path.substr(i + 1, path.length() - i);
    }
    return "";
}

bool listFilesInDir(const string &path, vector<string> &files) {
    DIR *dir = ::opendir(path.c_str());
    if (!dir) {
        InternalError("fail to open dir:%s", path.c_str());
        return false;
    }
    struct dirent *ent;
    while ((ent = ::readdir(dir)) != nullptr) {
        files.emplace_back(string(ent->d_name));
    }
    closedir(dir);
    return true;
}

bool isDirectory(const string &path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        InternalWarning("%s : %s", path.c_str(), strerror(errno));
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool getFileCreateTime(const string &path, int64_t &epochMillis) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        InternalWarning("%s : %s", path.c_str(), strerror(errno));
        return false;
    }
#ifndef GLOG_ANDROID
    epochMillis = (int64_t) st.st_birthtimespec.tv_sec * 1000 + st.st_birthtimespec.tv_nsec / 1000000;
#else
    epochMillis = (int64_t) st.st_ctim.tv_sec * 1000 + st.st_ctim.tv_nsec / 1000000;
#endif // GLOG_ANDROID
    return true;
}

constexpr char HEX_TABLE[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

string hex2Str(const uint8_t *data, int len) {
    string s(len * 2, 0);

    for (int i = 0; i < len; ++i) {
        s[2 * i] = HEX_TABLE[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = HEX_TABLE[data[i] & 0x0F];
    }
    return s;
}

int char2int(char input) {
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    throw std::invalid_argument("Invalid input string");
}

bool str2Hex(const string &s, uint8_t *data) {
    const string::size_type len = s.length();
    if (s.empty() || data == nullptr || len % 2 != 0) {
        return false;
    }
    for (int i = 0; i < len; i += 2) {
        data[i / 2] = (char2int(s.at(i)) << 4) | char2int(s.at(i + 1));
    }
    return true;
}

int readSpecifySize(int fd, void *buf, size_t size) {
    if (fd < 0 || !buf || size == 0) {
        return -1;
    }
    ssize_t bytes;
    if ((bytes = ::read(fd, buf, size)) != size)
        return -1;
    return static_cast<int>(bytes);
}

} // namespace glog