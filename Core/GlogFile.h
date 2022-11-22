//
// Created by issac on 2021/1/11.
//

#ifndef CORE_LOG_FILE_H_
#define CORE_LOG_FILE_H_

#include "Compressor.h"
#include "GlogBuffer.h"
#include "GlogPredef.h"
#include "Glog_IO.h"
#include "InternalLog.h"
#include "openssl/openssl_aes.h"
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace glog {

class GlogFile {
    friend class Glog;

public:
    GlogFile(std::string path,
             std::string protoName,
             size_t specifiedSize,
             Compressor *compressor,
             format::GlogCompressMode compressMode,
             format::GlogEncryptMode encryptMode,
             const string *serverPublicKey);

    ~GlogFile() {
        if (isFileAlreadyOpen()) {
            GlogFile::msync();
            GlogFile::closeFile();
        }
    }

    bool mmap();

    bool msync(SyncFlag syncFlag = SyncFlag::Glog_SYNC);

    bool loadFromDisk(uint8_t &maxRecursionDepth, size_t specifiedSize);

    void closeFile();

    bool isFileAlreadyOpen() const { return m_fd >= 0 && m_size > 0 && m_ptr != MAP_FAILED; }

    void *getMemoryPtr() const { return m_ptr; }

    int getFd() const { return m_fd; }

    std::string getPath() { return m_path; }

    bool writeHeader();

    // write data as it is
    bool writeRawData(const GlogBuffer &data);

    // write log and its length in front of it
    bool writeLogData(const GlogBuffer &data, const std::function<bool(uint32_t length)> &insufficientSpaceCallback);

    // calculate header size + log data size after mmap
    void calculatePosition();

    // write position
    size_t getPosition() const { return m_position; }

    // data length exclude header
    size_t getDataLength() const { return m_position - m_headerSize; }

    size_t spaceLeft() const {
        if (m_size <= m_position) {
            return 0;
        }
        return m_size - m_position;
    }

    size_t getTotalLogNum() const { return m_totalLogNum; }

    size_t getTotalLogSize() const { return m_totalLogSize; }

    size_t getHeaderSize() const { return m_headerSize; }

    void resetInternalState() {
        m_position = m_headerSize;
        m_totalLogNum = m_totalLogSize = 0;
    }

    size_t getSize() const { return m_size; }

private:
    std::string m_protoName;
    std::string m_path;
    int m_fd;
    void *m_ptr;
    atomic_size_t m_size;
    size_t m_headerSize; // assign while calling [writeHeader, readHeader]
    atomic_size_t m_position;
    atomic_size_t m_totalLogNum;  // total log number
    atomic_size_t m_totalLogSize; // total log data size in this file on disk
    atomic_bool m_loadFileCompleted;
    Compressor *m_compressor; // should not free, glog will free it
    format::GlogCompressMode m_compressMode;
    format::GlogEncryptMode m_encryptMode;
    uint8_t m_clientPublicKey[ECC_PUBLIC_KEY_LEN] = {};
    uint8_t m_serverPublicKey[ECC_PUBLIC_KEY_LEN] = {};
    openssl::AES_KEY m_aesKey = {};
    bool m_cipherReady = false;
    bool m_svrPubKeyReady = false;

    static int64_t searchSyncMarker(const uint8_t *src, size_t len);

    // truncate size and redo mmap if needed
    bool truncate(size_t size);

    bool resetAesKey();

    static bool zeroFillFile(int fd, size_t startPos, size_t size);

#ifdef GLOG_APPLE
    void tryResetFileProtection(const string &path);
#endif // GLOG_APPLE
};

/**
 * read glog file header do some check
 */
extern HeaderMismatchReason readHeader(int fd,
                                       const string &path,
                                       size_t fileSize,
                                       const string &protoName,
                                       size_t *outHeaderSize = nullptr,
                                       GlogByte_t *outVersionCode = nullptr);

/*
 * kmp search needle in haystack, backward means search from haystack's last position.
 * for example
 * haystack = "efdacbcdac", needle = "dac"
 * backward = true, return 7
 * backward = false, return 2
 */
extern int64_t
searchNeedle(const uint8_t *haystack, int64_t haystackLen, const uint8_t *needle, int64_t needleLen, bool backward);
extern int64_t searchSyncMarkerInFile(const string &path, int fd, off_t offset, size_t size);
} // namespace glog
#endif //CORE_LOG_FILE_H_
