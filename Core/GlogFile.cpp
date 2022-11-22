//
// Created by issac on 2021/1/11.
//

#include "GlogFile.h"
#include "Glog.h"
#include "InternalLog.h"
#include "aes/AESCrypt.h"
#include "micro-ecc/uECC.h"
#include "utilities.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace glog {

GlogFile::GlogFile(std::string path,
                   std::string protoName,
                   size_t specifiedSize,
                   Compressor *compressor,
                   format::GlogCompressMode compressMode,
                   format::GlogEncryptMode encryptMode,
                   const string *serverPublicKey)
    : m_path(std::move(path))
    , m_protoName(std::move(protoName))
    , m_fd(-1)
    , m_ptr(nullptr)
    , m_size(0)
    , m_totalLogNum(0)
    , m_totalLogSize(0)
    , m_loadFileCompleted(false)
    , m_position(0)
    , m_compressor(compressor)
    , m_compressMode(compressMode)
    , m_encryptMode(encryptMode)
    , m_headerSize(0) {
    uint8_t depth = MAX_RECURSION_DEPTH;
    if (serverPublicKey && !serverPublicKey->empty()) {
        if (serverPublicKey->length() != ECC_PUBLIC_KEY_LEN * 2 || !str2Hex(*serverPublicKey, m_serverPublicKey)) {
            throw std::invalid_argument("illegal server public key");
        }
        AESCrypt::initRandomSeed();
        m_svrPubKeyReady = true;
    } else if (encryptMode == format::GlogEncryptMode::AES) {
        throw std::invalid_argument("should provide cipher key while encrypt mode = AES");
    }
    m_loadFileCompleted = loadFromDisk(depth, specifiedSize);
}

bool GlogFile::resetAesKey() {
    if (!m_svrPubKeyReady) {
        return false;
    }
    uint8_t clientPrivateKey[ECC_PRIVATE_KEY_LEN] = {};
    if (uECC_make_key(m_clientPublicKey, clientPrivateKey, uECC_secp256k1()) == 0) {
        InternalError("fail to make ecc key pair");
        return false;
    }

    uint8_t userKey[32] = {};
    if (uECC_shared_secret(m_serverPublicKey, clientPrivateKey, userKey, uECC_secp256k1()) == 0) {
        InternalError("fail to make aes usr key");
        return false;
    }

    if (AES_set_encrypt_key(userKey, AES_KEY_BITSET_LEN, &m_aesKey) != 0) {
        InternalError("fail to make aes key");
        return false;
    }
    return true;
}

bool GlogFile::loadFromDisk(uint8_t &maxRecursionDepth, size_t specifiedSize) {
    // 1. open file
    // 2. delete if file is invalid, then reload it
    // 3. enlarge file to appropriate size
    // 4. try mmap
    // 5. write header if is a new file
    // 6. set m_position to data size (exclude zeros suffix)

    InternalDebug("start load file [%s]", m_path.c_str());

    if (isFileAlreadyOpen()) {
        InternalWarning("loadFromDisk from file while the file [%s] is already open", m_path.c_str());
        closeFile();
    }

    bool isBrandNewFile = false;

    m_fd = ::open(m_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    if (m_fd < 0) {
        InternalError("fail to open [%s], %s", m_path.c_str(), strerror(errno));
        return false;
    }

    size_t fileSize = 0;
    getFileSize(m_fd, fileSize);
    m_size = fileSize;
    // verify file header format, delete broken file then recreate
    if (m_size == 0) {
        isBrandNewFile = true;
    } else if (m_size > 0) {
        HeaderMismatchReason reason = readHeader(m_fd, m_path, m_size, m_protoName, &m_headerSize, nullptr);
        InternalDebug("cache file [%s] already exist", m_path.c_str());
        if (reason != HeaderMismatchReason::None) {
            int ret = ::remove(m_path.c_str());
            InternalDebug("file [%s] header mismatch reason:%d, remove ret:%d %s", m_path.c_str(), reason, ret,
                          ::strerror(errno));
            if (--maxRecursionDepth <= 0) {
                InternalError("loadFromDisk() reach recursion upper limit");
                return false;
            }
            return loadFromDisk(maxRecursionDepth, specifiedSize);
        }
    } else {
        InternalError("fail get file [%s] size", m_path.c_str());
        return false;
    }
    bool truncated = true;
    if (m_size != specifiedSize) {
        truncated = GlogFile::truncate(specifiedSize);
    }
    if (!truncated) {
        InternalDebug("fail to truncate");
        return false;
    }

    if (!GlogFile::mmap()) {
        if (m_ptr && m_ptr != MAP_FAILED) {
            if (::munmap(m_ptr, m_size) != 0) {
                InternalError("fail to munmap [%s], %s", m_path.c_str(), strerror(errno));
            }
        }
        return false;
    }
#ifdef GLOG_APPLE
    tryResetFileProtection(m_path);
#endif // GLOG_APPLE
    if (isBrandNewFile) {
        if (isFileAlreadyOpen()) {
            if (!zeroFillFile(m_fd, 0, m_size)) {
                InternalError("fail to zeroFile [%s] to size %zu, %s", m_path.c_str(), m_size.load(), strerror(errno));
                closeFile();
                return false;
            }
        }

        bool ret = writeHeader();
        if (!ret) {
            InternalError("fail to write header [%s], %s", m_path.c_str(), strerror(errno));
            closeFile();
            return false;
        }
        m_totalLogNum = 0;
        m_totalLogSize = 0;
        m_position = m_headerSize;
    } else {
        // calculate current position
        calculatePosition();
    }
    if (m_compressor) {
        m_compressor->reset();
    }
    m_cipherReady = resetAesKey();
    InternalDebug("end load file [%s] actual size:%zu, file disk size:%zu", m_path.c_str(), m_position.load(),
                  m_size.load());
    return true;
}

bool GlogFile::truncate(size_t size) {
    if (m_fd < 0) {
        InternalError("fail to truncate [%s] because m_fd < 0, m_fd:%d", m_path.c_str(), m_fd);
        return false;
    }
    size_t oldSize = m_size;
    m_size = size;
    if (::ftruncate(m_fd, m_size) != 0) {
        InternalError("fail to truncate [%s] to size %zu, %s", m_path.c_str(), m_size.load(), ::strerror(errno));
        m_size = oldSize;
        return false;
    }

    if (m_ptr && m_ptr != MAP_FAILED) {
        if (::munmap(m_ptr, oldSize) != 0) {
            InternalError("fail to munmap [%s], %s", m_path.c_str(), strerror(errno));
        }
    }
    return true;
}

bool GlogFile::zeroFillFile(int fd, size_t startPos, size_t size) {
    if (fd < 0) {
        return false;
    }

    if (::lseek(fd, static_cast<off_t>(startPos), SEEK_SET) < 0) {
        InternalError("fail to lseek fd[%d], error:%s", fd, strerror(errno));
        return false;
    }

    const char zeros[4096] = {};
    int ret = -1;
    while (size >= sizeof(zeros)) {
        if ((ret = ::write(fd, zeros, sizeof(zeros))) != sizeof(zeros)) {
            InternalError("fail to write fd[%d], count:%d, error:%s", fd, ret, strerror(errno));
            return false;
        }
        size -= sizeof(zeros);
    }
    if (size > 0) {
        if ((ret = ::write(fd, zeros, size)) != size) {
            InternalError("fail to write fd[%d], count:%d, error:%s", fd, ret, strerror(errno));
            return false;
        }
    }
    return true;
}

bool GlogFile::mmap() {
    m_ptr = ::mmap(m_ptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (m_ptr == MAP_FAILED) {
        InternalError("fail to mmap [%s], %s", m_path.c_str(), strerror(errno));
        m_ptr = nullptr;
        return false;
    }
    return true;
}

bool GlogFile::msync(SyncFlag syncFlag) {
    if (m_ptr && m_ptr != MAP_FAILED) {
        auto ret = ::msync(m_ptr, m_size, syncFlag == SyncFlag::Glog_SYNC ? MS_SYNC : MS_ASYNC);
        if (ret == 0) {
            return true;
        }
        InternalError("fail to msync [%s], %s", m_path.c_str(), strerror(errno));
    }
    return false;
}

void GlogFile::closeFile() {
    if (m_ptr && m_ptr != MAP_FAILED) {
        if (::munmap(m_ptr, m_size) != 0) {
            InternalError("fail to munmap [%s], %s", m_path.c_str(), strerror(errno));
        }
    }
    m_ptr = nullptr;

    if (m_fd >= 0) {
        if (::close(m_fd) != 0) {
            InternalError("fail to close [%s], %s", m_path.c_str(), strerror(errno));
        }
    }
    m_fd = -1;
    m_size = 0;
    m_position = m_headerSize = 0;
    m_totalLogNum = m_totalLogSize = 0;
}

#ifdef GLOG_APPLE
void GlogFile::tryResetFileProtection(const string &path) {
    @autoreleasepool {
        NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
        NSDictionary *attr = [[NSFileManager defaultManager] attributesOfItemAtPath:nsPath error:nullptr];
        NSString *protection = [attr valueForKey:NSFileProtectionKey];
        InternalDebug("protection on [%@] is %@", nsPath, protection);
        if ([protection isEqualToString:NSFileProtectionCompleteUntilFirstUserAuthentication] == NO) {
            NSMutableDictionary *newAttr = [NSMutableDictionary dictionaryWithDictionary:attr];
            [newAttr setObject:NSFileProtectionCompleteUntilFirstUserAuthentication forKey:NSFileProtectionKey];
            NSError *err = nil;
            [[NSFileManager defaultManager] setAttributes:newAttr ofItemAtPath:nsPath error:&err];
            if (err != nil) {
                InternalError("fail to set attribute %@ on [%@]: %@",
                              NSFileProtectionCompleteUntilFirstUserAuthentication, nsPath, err);
            }
        }
    }
}
#endif // GLOG_APPLE

int64_t GlogFile::searchSyncMarker(const uint8_t *src, size_t len) {
    return searchNeedle(src, len, format::SYNC_MARKER, format::SYNC_MARKER_LENGTH, false);
}

// When comparing signed with unsigned, the compiler converts the signed value to unsigned
// so use int64_t length
int64_t
searchNeedle(const uint8_t *haystack, int64_t haystackLen, const uint8_t *needle, int64_t needleLen, bool backward) {
    if (needleLen == 0 || haystackLen == 0 || needleLen > haystackLen) {
        return -1;
    }
    if (backward) {
        int64_t i = haystackLen - 1, j = needleLen - 1;
        while (i >= 0 && j >= 0) {
            if (j == needleLen || haystack[i] == needle[j]) {
                i--;
                j--;
            } else {
                j = needleLen - 1 - g_backwardPartialMatchTable[needleLen - 1 - j];
            }
        }
        if (j == -1) {
            return i - j;
        }
    } else {
        int64_t i = 0, j = 0;
        while (i < haystackLen && j < needleLen) {
            if (j == -1 || haystack[i] == needle[j]) {
                i++;
                j++;
            } else {
                j = g_forwardPartialMatchTable[j];
            }
        }
        if (j == needleLen) {
            return i - j;
        }
    }
    return -1;
}

int64_t searchSyncMarkerInFile(const string &path, int fd, off_t offset, size_t size) {
    if (size <= offset || format::SYNC_MARKER_LENGTH > size - offset) {
        return -1;
    }
    int64_t i = offset, j = 0;
    while (i < size && j < format::SYNC_MARKER_LENGTH) {
        uint8_t haystackElement;
        if (::lseek(fd, i, SEEK_SET) < 0) {
            InternalError("fail to lseek file [%s], %s", path.c_str(), strerror(errno));
            return -1;
        }
        if (::read(fd, &haystackElement, 1) <= 0) {
            InternalError("fail to read file [%s], %s", path.c_str(), strerror(errno));
            return -1;
        }
        if (j == -1 || haystackElement == format::SYNC_MARKER[j]) {
            i++;
            j++;
        } else {
            j = g_forwardPartialMatchTable[j];
        }
    }
    if (j == format::SYNC_MARKER_LENGTH) {
        return i - j;
    }
    return -1;
}

} // namespace glog