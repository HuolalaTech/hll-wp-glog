//
//  Glog_IO.cpp
//  core
//
//  Created by issac on 2021/1/11.
//

#include "Glog_IO.h"
#include "Compressor.h"
#include "Glog.h"
#include "GlogFile.h"
#include "GlogPredef.h"
#include "GlogReader.h"
#include "MessageQueue.h"
#include "ScopedLock.h"
#include "aes/AESCrypt.h"
#include "micro-ecc/uECC.h"
#include "utilities.h"
#include <cerrno>

using namespace glog::format;
/*
 * file format (all length store as little endian)
 *
 * sync marker: synchronization marker, 8 random bytes
 *
 * optional:
 * (write iv and key when the log should be encrypted)
 * AES random iv: AES crypt CFB-128 random initial vector
 * client ecc public key: make shared key with server private key
 *
+-----------------------------------------------------------------+
|                         magic number (4)                        |
+----------------+-------------------------------+----------------+
|   version (1)  |     proto name length (2)     |
+----------------+-------------------------------+----------------+
|       proto name (0...)       ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+================+================================================+
|  mode set (1)  |          optional: AES random iv (16)
+----------------+------------------------------------------------+
|                              ... AES random iv                  |
+-----------------------------------------------------------------+
|       optional: client ecc public key (64)
+-----------------------------------------------------------------+
|                              ... client ecc public key          |
+-------------------------------+---------------------------------+
|        log length (2)         |
+-------------------------------+----------------------------------
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+================+================================================+
|  mode set (1)  |          optional: AES random iv (16)
+----------------+------------------------------------------------+
|                              ... AES random iv                  |
+-----------------------------------------------------------------+
|       optional: client ecc public key (64)
+-----------------------------------------------------------------+
|                              ... client ecc public key          |
+-------------------------------+---------------------------------+
|        log length (2)         |
+-------------------------------+----------------------------------
|                           log data (0...)         ...
+-----------------------------------------------------------------+
|		sync marker (8)	...					                      |
+-----------------------------------------------------------------+
|                                       ... sync marker           |
+-----------------------------------------------------------------+
|                              ...
+-----------------------------------------------------------------+
*/

namespace glog {

#define __CHECK_RET(ret)                                                                                               \
    if ((ret) < 0) {                                                                                                   \
        InternalError("fail to read file [%s], %s", path.c_str(), strerror(errno));                                    \
        return HeaderMismatchReason::ReadFail;                                                                         \
    } else if ((ret) == 0) {                                                                                           \
        InternalDebug("invalid file [%s], too small size:%zu", path.c_str(), fileSize);                                \
        return HeaderMismatchReason::SizeTooSmall;                                                                     \
    }

HeaderMismatchReason readHeader(int fd,
                                const string &path,
                                size_t fileSize,
                                const string &protoName,
                                size_t *outHeaderSize,
                                GlogByte_t *outVersionCode) {
    InternalDebug("start read file's [%s] header", path.c_str());
    if (fileSize < HEADER_FIXED_LENGTH) {
        InternalDebug("invalid file [%s], too small size:%zu", path.c_str(), fileSize);
        return HeaderMismatchReason::SizeTooSmall;
    }

    FixedHeader fixedHeader{};

    ssize_t ret = readSpecifySize(fd, &fixedHeader, HEADER_FIXED_LENGTH);
    __CHECK_RET(ret);

    if (::memcmp(fixedHeader.m_magicNumber, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0) {
        InternalDebug("invalid file [%s], magic number mismatching", path.c_str());
        return HeaderMismatchReason::MagicNumMismatch;
    }
    if (outVersionCode) {
        *outVersionCode = fixedHeader.m_versionCode;
    }
    if (fixedHeader.m_versionCode != static_cast<GlogByte_t>(GlogVersion::GlogCipherVersion)) {
        InternalDebug("invalid file [%s], version code [%d] mismatching", path.c_str(), fixedHeader.m_versionCode);
        return HeaderMismatchReason::VersionCodeMismatch;
    }

    const GlogBufferLength_t nameLen = readU16Le(fixedHeader.m_protoNameLength);

    if (fileSize < HEADER_FIXED_LENGTH + nameLen + SYNC_MARKER_LENGTH) {
        InternalDebug("invalid file [%s], too small size:%zu", path.c_str(), fileSize);
        return HeaderMismatchReason::ProtoNameMismatch;
    }

    char protoNameBuf[nameLen + 1];
    ret = readSpecifySize(fd, protoNameBuf, nameLen);
    __CHECK_RET(ret);

    protoNameBuf[nameLen] = '\0';
    if (strcmp(protoName.c_str(), protoNameBuf) != 0) {
        InternalDebug("invalid file [%s], proto name mismatching", path.c_str());
        return HeaderMismatchReason::ProtoNameMismatch;
    }

    uint8_t syncMarker[SYNC_MARKER_LENGTH] = {};
    ret = readSpecifySize(fd, (void *) syncMarker, SYNC_MARKER_LENGTH);
    __CHECK_RET(ret);

    if (::memcmp(syncMarker, format::SYNC_MARKER, SYNC_MARKER_LENGTH) != 0) {
        InternalDebug("invalid file [%s], sync marker invalid", path.c_str());
        return HeaderMismatchReason::SyncMarkerMismatch;
    }

    if (outHeaderSize) {
        *outHeaderSize = HEADER_FIXED_LENGTH + nameLen + SYNC_MARKER_LENGTH;
    }

    return HeaderMismatchReason::None;
}

bool Glog::writeAsync(const GlogBuffer &log) {
    GlogBufferLength_t length = log.getAvailLength();
    if (unlikely(length == 0 || length > SINGLE_LOG_CONTENT_MAX_LENGTH)) {
        InternalWarning("illegal log length [%d], skip write", length);
        return false;
    }

    // make a copy in case of content in log's pointer address change before action executed
    auto *copy = new GlogBuffer(log.getPtr(), log.getAvailLength(), true);
    bool enqueued = m_writeQueue->enqueue(Message([this, copy]() {
        writeSync(*copy);
        delete copy;
    }));
    return enqueued;
}

bool Glog::writeSync(const GlogBuffer &log) {
    SCOPED_LOCK(m_syncWriteLock);

    GlogBufferLength_t length = log.getAvailLength();
    if (unlikely(length == 0 || length > SINGLE_LOG_CONTENT_MAX_LENGTH)) {
        InternalWarning("illegal log length [%d], skip write", length);
        return false;
    }

    if (m_shouldTryFlushAcrossDay && m_incrementalArchive) {
        tryFlushAcrossDay();
    }

    return m_cacheFile->writeLogData(log, [this](GlogBufferLength_t length) { return internalFlush(); });
}

bool GlogFile::writeHeader() {
    if (!isFileAlreadyOpen()) {
        InternalWarning("fail to write header because the file [%s] is not open", m_path.c_str());
        return false;
    }
    m_headerSize = HEADER_FIXED_LENGTH + m_protoName.size() + SYNC_MARKER_LENGTH;
    if (m_headerSize > spaceLeft()) {
        InternalError("file left space [%d] not enough for header", spaceLeft());
        return false;
    }
    FixedHeader fixedHeader{.m_versionCode = static_cast<GlogByte_t>(GlogVersion::GlogCipherVersion),
                            .m_protoNameLength = toLittleEndianU16(m_protoName.size())};
    memcpy(fixedHeader.m_magicNumber, MAGIC_NUMBER, sizeof(MAGIC_NUMBER));

    GlogBuffer headerBuff(&fixedHeader, sizeof(fixedHeader));
    bool ret = writeRawData(headerBuff);
    if (!ret) {
        return false;
    }

    GlogBuffer protoNameBuff((void *) m_protoName.data(), m_protoName.size());
    ret = writeRawData(protoNameBuff);
    if (!ret) {
        return false;
    }

    // in order to keep sync marker in cache same as today's log file,
    // do not generate it by uuid, just use constant.
    // generateSync(m_syncMarker);
    GlogBuffer syncMarkerBuff((void *) format::SYNC_MARKER, SYNC_MARKER_LENGTH);
    return GlogFile::writeRawData(syncMarkerBuff);
}

// get next sync marker's head position, move m_position to next sync marker's end
#define __TRY_RECOVER_CAL(offset)                                                                                      \
    int64_t syncPos = searchSyncMarker(basePtr + (offset), m_size - m_position - (offset));                            \
    InternalDebug("found sync marker at position:%lld", syncPos);                                                      \
    if (syncPos == -1) {                                                                                               \
        break;                                                                                                         \
    } else {                                                                                                           \
        m_position += static_cast<size_t>((offset) + syncPos + SYNC_MARKER_LENGTH);                                    \
        basePtr += (offset) + syncPos + SYNC_MARKER_LENGTH;                                                            \
        continue;                                                                                                      \
    }

void GlogFile::calculatePosition() {
    auto *basePtr = static_cast<uint8_t *>(m_ptr);

    basePtr += m_headerSize;
    m_position += m_headerSize;

    // calculate total log size
    while (spaceLeft() > LOG_LENGTH_BYTES + 1 + SYNC_MARKER_LENGTH) {
        ModeSet ms = toModeSet(*basePtr);
        if (ms.m_compressMode < GlogCompressMode::None || ms.m_compressMode > GlogCompressMode::Zlib ||
            ms.m_encryptMode < format::GlogEncryptMode::None || ms.m_encryptMode > format::GlogEncryptMode::AES) {
            InternalDebug("illegal mode set at position [%zu], compress mode:%d, encrypt mode:%d", m_position.load(),
                          ms.m_compressMode, ms.m_encryptMode);
            __TRY_RECOVER_CAL(1);
        }
        const bool cipher = ms.m_encryptMode == format::GlogEncryptMode::AES;
        if (spaceLeft() < logStoreSize(1, cipher)) {
            break;
        }
        size_t offset = 1 + (cipher ? AES_KEY_LEN + ECC_PUBLIC_KEY_LEN : 0);
        basePtr += offset;
        m_position += offset;

        auto *lenPtr = reinterpret_cast<GlogBufferLength_t *>(basePtr);
        GlogBufferLength_t logLength = readU16Le(*lenPtr);

        // skip broken logs by search next sync marker
        if (logLength == 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
            InternalWarning("file [%s] broken at position:%d", m_path.c_str(), m_position.load());
            __TRY_RECOVER_CAL(LOG_LENGTH_BYTES);
        }

        offset = logLength + LOG_LENGTH_BYTES;
        basePtr += offset;
        m_position += offset;

        if (SYNC_MARKER_LENGTH > spaceLeft()) {
            InternalWarning("file [%s] end unexpectedly", m_path.c_str());
            break;
        }

        if (::memcmp(basePtr, format::SYNC_MARKER, SYNC_MARKER_LENGTH) != 0) {
            InternalWarning("file [%s] broken at position:%d", m_path.c_str(), m_position.load());
            __TRY_RECOVER_CAL(SYNC_MARKER_LENGTH);
        }

        m_totalLogNum++;
        m_totalLogSize += logLength;
        basePtr += SYNC_MARKER_LENGTH;
        m_position += SYNC_MARKER_LENGTH;
    }
    InternalDebug("total log num:%zu, log size:%zu", m_totalLogNum.load(), m_totalLogSize.load());
}

bool GlogFile::writeRawData(const GlogBuffer &data) {
    if (!isFileAlreadyOpen()) {
        InternalWarning("fail to write raw data because the file [%s] is not open", m_path.c_str());
        return false;
    }
    InternalDebug("write raw len:%d pos:%zu", data.getAvailLength(), m_position.load());

    auto *basePtr = static_cast<uint8_t *>(m_ptr);
    ::memcpy(basePtr + m_position, data.getPtr(), data.getAvailLength());
    m_position += data.getAvailLength();
    return true;
}

bool GlogFile::writeLogData(const GlogBuffer &data,
                            const std::function<bool(uint32_t length)> &insufficientSpaceCallback) {
    if (!isFileAlreadyOpen()) {
        InternalDebug("fail to write log data because the file [%s] is not open", m_path.c_str());
        return false;
    }
    if (!m_loadFileCompleted) {
        InternalWarning("fail to write log data because the file loading [%s] is not completed", m_path.c_str());
        return false;
    }
    GlogBuffer compressBuffer(SINGLE_LOG_CONTENT_MAX_LENGTH);

    bool needCompress = m_compressMode != format::GlogCompressMode::None && m_compressor;
    bool needEncrypt = m_encryptMode != format::GlogEncryptMode::None && m_cipherReady;
    ModeSet modeSet{.m_compressMode = this->m_compressMode, .m_encryptMode = this->m_encryptMode};
    uint8_t iv[AES_KEY_LEN] = {};

    if (needCompress) {
        InternalDebug("before compress len:%d", data.getAvailLength());
        // m_compressor->reset();
        bool ret = m_compressor->compress(data, compressBuffer);
        InternalDebug("after compress len:%d", compressBuffer.getAvailLength());
        if (!ret) {
            return false;
        }
    }

    GlogBufferLength_t length = needCompress ? compressBuffer.getAvailLength() : data.getAvailLength();

    if (logStoreSize(length, needEncrypt) > spaceLeft()) {
        if (insufficientSpaceCallback) {
            if (!insufficientSpaceCallback(logStoreSize(length, needEncrypt))) {
                return false;
            }
            if (logStoreSize(length, needEncrypt) > spaceLeft()) {
                return false; // still no space to write
            }
            // recompress because compressor reset dictionary after archive
            if (needCompress) {
                bool ret = m_compressor->compress(data, compressBuffer);
                InternalDebug("after recompress len:%d", compressBuffer.getAvailLength());
                if (!ret) {
                    return false;
                }
                length = compressBuffer.getAvailLength();
                if (logStoreSize(length, needEncrypt) > spaceLeft()) {
                    return false;
                }
            }
            // redo encrypt because cipher may reset after archive
            if (needEncrypt) { // AES CFB-128 won't change data size
                if (needCompress) {
                    AESCrypt::encryptOnce(compressBuffer.getPtr(), compressBuffer.getPtr(),
                                          compressBuffer.getAvailLength(), &m_aesKey, iv);
                } else {
                    AESCrypt::encryptOnce(data.getPtr(), data.getPtr(), data.getAvailLength(), &m_aesKey, iv);
                }
            }
        }
    } else if (needEncrypt) {
        if (needCompress) {
            AESCrypt::encryptOnce(compressBuffer.getPtr(), compressBuffer.getPtr(), compressBuffer.getAvailLength(),
                                  &m_aesKey, iv);
        } else {
            AESCrypt::encryptOnce(data.getPtr(), data.getPtr(), data.getAvailLength(), &m_aesKey, iv);
        }
    }

    // write log header
    auto *basePtr = static_cast<uint8_t *>(m_ptr);
    basePtr += m_position;

    GlogByte_t msNum = fromModeSet(modeSet);
    memcpy(basePtr, &msNum, sizeof(GlogByte_t));
    basePtr += sizeof(GlogByte_t);
    m_position += sizeof(GlogByte_t);

    if (needEncrypt) {
        memcpy(basePtr, iv, AES_KEY_LEN);
        basePtr += AES_KEY_LEN;
        m_position += AES_KEY_LEN;

        memcpy(basePtr, m_clientPublicKey, ECC_PUBLIC_KEY_LEN);
        basePtr += ECC_PUBLIC_KEY_LEN;
        m_position += ECC_PUBLIC_KEY_LEN;
    }

    GlogBufferLength_t lengthLe = toLittleEndianU16(length);

    ::memcpy(basePtr, &lengthLe, LOG_LENGTH_BYTES);
    basePtr += LOG_LENGTH_BYTES;
    m_position += LOG_LENGTH_BYTES;

    bool ret;
    if (needCompress) {
        ret = writeRawData(compressBuffer);
    } else {
        ret = writeRawData(data);
    }

    if (ret) {
        m_totalLogNum++;
        m_totalLogSize += needCompress ? compressBuffer.getAvailLength() : data.getAvailLength();
        InternalDebug("write sync marker at position:%d", m_position.load());
        ::memcpy(basePtr + length, format::SYNC_MARKER, SYNC_MARKER_LENGTH);
        m_position += SYNC_MARKER_LENGTH;
    }
    return ret;
}

#define __TRY_RECOVER_READ(offset, ret)                                                                                \
    int64_t syncPos = searchSyncMarkerInFile(m_file, m_fd, m_position + (offset), m_size);                             \
    InternalDebug("found sync marker at position:%lld", syncPos);                                                      \
    if (syncPos == -1) {                                                                                               \
        return ret;                                                                                                    \
    } else {                                                                                                           \
        m_position = static_cast<size_t>(syncPos + SYNC_MARKER_LENGTH);                                                \
        return 0;                                                                                                      \
    }

int GlogReader::read(GlogBuffer &outBuffer) {
    if (!m_loadFileCompleted || outBuffer.getCapacity() <= 0 || !isFileAlreadyOpen()) {
        return -1;
    }
    if (spaceRemain() <= LOG_LENGTH_BYTES + 1 + SYNC_MARKER_LENGTH) {
        return -2;
    }
    GlogByte_t msNum = 0;
    int bytes = readSpecifySize(m_fd, &msNum, 1);
    if (bytes <= 0) {
        return -3;
    }
    m_position += bytes;
    ModeSet ms = toModeSet(msNum);
    if (ms.m_compressMode < GlogCompressMode::None || ms.m_compressMode > GlogCompressMode::Zlib ||
        ms.m_encryptMode < format::GlogEncryptMode::None || ms.m_encryptMode > format::GlogEncryptMode::AES) {
        InternalDebug("illegal mode set at position [%zu], compress mode:%d, encrypt mode:%d", m_position - 1,
                      ms.m_compressMode, ms.m_encryptMode);
        __TRY_RECOVER_READ(0, -4);
    }
    const bool needDecrypt = ms.m_encryptMode == format::GlogEncryptMode::AES;
    if (needDecrypt && !m_cipherReady) {
        InternalDebug("cipher not ready");
        return -5;
    }
    const bool needDecompress = ms.m_compressMode == format::GlogCompressMode::Zlib;

    if (spaceRemain() < logStoreSize(1, needDecrypt)) {
        return -6;
    }

    uint8_t iv[AES_KEY_LEN] = {};
    uint8_t clientPubKey[ECC_PUBLIC_KEY_LEN] = {};

    if (needDecrypt) {
        bytes = readSpecifySize(m_fd, iv, AES_KEY_LEN);
        if (bytes <= 0) {
            return -7;
        }
        m_position += bytes;

        bytes = readSpecifySize(m_fd, clientPubKey, ECC_PUBLIC_KEY_LEN);
        if (bytes <= 0) {
            return -8;
        }
        m_position += bytes;
    }

    GlogBufferLength_t logLength = 0;
    bytes = readSpecifySize(m_fd, &logLength, LOG_LENGTH_BYTES);
    if (bytes <= 0) {
        return -9;
    }
    m_position += bytes;

    if (logLength == 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
        // try recover
        InternalWarning("file [%s] broken at position:%d", m_file.c_str(), m_position - bytes);
        __TRY_RECOVER_READ(0, -10);
    }

    if (outBuffer.getCapacity() < logLength || spaceRemain() < logLength + SYNC_MARKER_LENGTH) {
        return -11;
    }

    InternalDebug("need decompress:%d, need decrypt:%d", needDecompress, needDecrypt);
    if (needDecompress) {
        GlogBuffer inBuffer(logLength);
        bytes = readSpecifySize(m_fd, inBuffer.getPtr(), logLength);
        if (bytes <= 0) {
            return -12;
        }
        if (needDecrypt) {
            uint8_t userKey[32] = {};
            openssl::AES_KEY aesKey = {};

            if (uECC_shared_secret(clientPubKey, m_serverPrivateKey, userKey, uECC_secp256k1()) == 0) {
                return -13;
            }

            if (AES_set_encrypt_key(userKey, AES_KEY_BITSET_LEN, &aesKey) != 0) {
                return -14;
            }
            AESCrypt::decryptOnce(inBuffer.getPtr(), inBuffer.getPtr(), bytes, &aesKey, iv);
        }
        // m_decompressor->reset();
        bool ret = m_decompressor->decompress(inBuffer, outBuffer);
        if (!ret) {
            InternalWarning("fail to decompress [%s] offset:%zu length:%zu", m_file.c_str(), m_position, logLength);
            __TRY_RECOVER_READ(logLength, -15);
            // if chunk in next position compressed with initial state, should invoke m_decompressor->reset();
            // but there's no way to know if next chunk was compressed with initial state or inflight state.
        }
        m_position += logLength;
        bytes = outBuffer.getAvailLength();
    } else {
        bytes = readSpecifySize(m_fd, outBuffer.getPtr(), logLength);
        if (bytes <= 0) {
            return -16;
        }
        outBuffer.setAvailLength(bytes);
        if (needDecrypt) {
            uint8_t userKey[32] = {};
            openssl::AES_KEY aesKey = {};

            if (uECC_shared_secret(clientPubKey, m_serverPrivateKey, userKey, uECC_secp256k1()) == 0) {
                return -17;
            }
            if (AES_set_encrypt_key(userKey, AES_KEY_BITSET_LEN, &aesKey) != 0) {
                return -18;
            }
            AESCrypt::decryptOnce(outBuffer.getPtr(), outBuffer.getPtr(), bytes, &aesKey, iv);
        }
        m_position += logLength;
    }

    uint8_t syncMarker[SYNC_MARKER_LENGTH] = {};
    int n = readSpecifySize(m_fd, syncMarker, SYNC_MARKER_LENGTH);
    if (n <= 0) {
        return -19;
    }

    if (::memcmp(syncMarker, format::SYNC_MARKER, SYNC_MARKER_LENGTH) != 0) {
        InternalWarning("file [%s] broken at position:%d", m_file.c_str(), m_position);
        __TRY_RECOVER_READ(SYNC_MARKER_LENGTH, -20);
    }

    m_position += SYNC_MARKER_LENGTH;
    return bytes;
}

ModeSet toModeSet(GlogByte_t value) {
    ModeSet st{};
    st.m_compressMode = static_cast<GlogCompressMode>(value >> 4);
    st.m_encryptMode = static_cast<GlogEncryptMode>(value & 0x0F);
    return st;
}

GlogByte_t fromModeSet(const ModeSet st) {
    auto compressMode = static_cast<uint8_t>(st.m_compressMode);
    auto encryptMode = static_cast<uint8_t>(st.m_encryptMode);
    return encryptMode | compressMode << 4;
}

uint32_t logStoreSize(GlogBufferLength_t logLength, bool cipher) {
    return logHeaderSize(cipher) + logLength + format::SYNC_MARKER_LENGTH;
}

uint8_t logHeaderSize(bool cipher) {
    return format::LOG_LENGTH_BYTES + 1 + (cipher ? AES_KEY_LEN + ECC_PUBLIC_KEY_LEN : 0);
}

} // namespace glog
