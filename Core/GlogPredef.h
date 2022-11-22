//
// Created by issac on 2021/1/19.
//

#ifndef CORE__GLOGPREDEF_H_
#define CORE__GLOGPREDEF_H_

#include <cstdint>
#include <string>

#ifdef __APPLE__
#    ifdef FORCE_POSIX
#        define GLOG_POSIX
#    else
#        define GLOG_APPLE
#    endif
#elif __ANDROID__
#    ifdef FORCE_POSIX
#        define GLOG_POSIX
#    else
#        define GLOG_ANDROID
#    endif
#endif

#ifdef DEBUG
#    define GLOG_DEBUG
#endif

#ifdef GLOG_APPLE
#    import <Foundation/Foundation.h>
typedef NSString *InternalLog_t;
#else
typedef const std::string &InternalLog_t;
#endif // GLOG_APPLE

namespace glog {

enum InternalLogLevel : int {
    InternalLogLevelDebug = 0, // not available for release/product build
    InternalLogLevelInfo = 1,  // default level
    InternalLogLevelWarning,
    InternalLogLevelError,
    InternalLogLevelNone, // special level used to disable all log messages
};

typedef void (*LogHandler)(
    glog::InternalLogLevel level, const char *file, int line, const char *function, InternalLog_t message);

typedef uint16_t GlogBufferLength_t;
typedef unsigned char GlogByte_t;

constexpr auto CACHE_FILE_SUFFIX = ".glogmmap";
constexpr auto ARCHIVE_FILE_SUFFIX = ".glog";
constexpr auto MESSAGE_QUEUE_THREAD_NAME = "glog-core-mq";
constexpr auto DAEMON_THREAD_NAME = "glog-core-dm";
// 7 days expires
const int32_t DEFAULT_EXPIRES_SECS = 7 * 24 * 60 * 60;
// 16 MB total archives
const size_t DEFAULT_TOTAL_ARCHIVE_SIZE_LIMIT = 16 * 1024 * 1024;

const int DEFAULT_TOTAL_ARCHIVE_NUM_LIMIT = 30;
namespace format {

/**
 * file with version < 0x3 will be removed while glog initializing
 */
enum class GlogVersion : GlogByte_t {
    /**
     * @deprecated initial version
     */
    GlogInitialVersion [[deprecated("incorrect initial write position")]] = 0x1,

    /**
     * @deprecated fix incorrect initial write position
     */
    GlogFixPositionVersion [[deprecated("incompatible format")]] = 0x2,

    /**
     * add sync marker to each log, support recover from broken file
     */
    GlogRecoveryVersion = 0x3,

    /**
     * store iv and public key in each log to support AES CFB-128 encrypt
     */
    GlogCipherVersion = 0x4,
};

const GlogByte_t MAGIC_NUMBER[] = {0x1B, 0xAD, 0xC0, 0xDE};
enum class GlogCompressMode : uint8_t { None = 1, Zlib = 2 };
enum class GlogEncryptMode : uint8_t { None = 1, AES = 2 };

typedef struct ModeSet {
    GlogCompressMode m_compressMode = GlogCompressMode::None;
    GlogEncryptMode m_encryptMode = GlogEncryptMode::None;
} ModeSet;

#pragma pack(push, 1)
typedef struct FixedHeader {
    GlogByte_t m_magicNumber[sizeof(MAGIC_NUMBER)];
    GlogByte_t m_versionCode;
    uint16_t m_protoNameLength;
} FixedHeader;
#pragma pack(pop)

const GlogByte_t HEADER_FIXED_LENGTH = sizeof(FixedHeader);

// log length width = 2 bytes
const GlogByte_t LOG_LENGTH_BYTES = 2;
// sync marker
const GlogByte_t SYNC_MARKER[] = {0xB7, 0xDB, 0xE7, 0xDB, 0x80, 0xAD, 0xD9, 0x57};
const GlogByte_t SYNC_MARKER_LENGTH = sizeof(SYNC_MARKER);
} // namespace format

// single log content max length = 16 KB (uncompressed)
// MUST < 2^16 - 1 = (64KB - 1) because length's width = 2 bytes
const GlogBufferLength_t SINGLE_LOG_CONTENT_MAX_LENGTH = 16 * 1024;

const uint8_t MAX_RECURSION_DEPTH = 5;

const size_t AES_KEY_LEN = 16;
const size_t AES_KEY_BITSET_LEN = 128;
const size_t ECC_PUBLIC_KEY_LEN = 64;
const size_t ECC_PRIVATE_KEY_LEN = 32;

enum class SyncFlag : bool { Glog_SYNC = true, Glog_ASYNC = false };

enum class Endianness : uint8_t { BigEndian = 0, LittleEndian = 1 };

enum class FileOrder : uint8_t { None = 0, CreateTimeAscending = 1, CreateTimeDescending = 2 };

const long FLUSH_AWAIT_TIMEOUT_MILLIS = 3000; // await flush at most 3s

typedef struct ArchiveCondition {
    bool m_flush;
    size_t m_minLogNum;
    size_t m_totalLogSize;
} ArchiveCondition;

typedef struct FileStat {
    std::string m_path;
    int64_t m_createTime; // millis since the epoch TODO in fact it's the file's last modified time
    size_t m_size;
} FileStat;

typedef struct GlogConfig {
    std::string m_protoName;
    std::string m_rootDirectory;
    bool m_incrementalArchive = false;
    bool m_async = true;
    int32_t m_expireSeconds = DEFAULT_EXPIRES_SECS;
    size_t m_totalArchiveSizeLimit = DEFAULT_TOTAL_ARCHIVE_SIZE_LIMIT;
    format::GlogCompressMode m_compressMode = format::GlogCompressMode::Zlib;
    format::GlogEncryptMode m_encryptMode = format::GlogEncryptMode::None;
    std::string *m_serverPublicKey = nullptr;
} GlogConfig;
} // namespace glog
#endif //CORE__GLOGPREDEF_H_
