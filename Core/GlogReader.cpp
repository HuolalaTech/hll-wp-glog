//
// Created by issac on 2021/1/26.
//

#include "GlogReader.h"

#include "Glog.h"
#include "GlogFile.h"
#include "GlogPredef.h"
#include "InternalLog.h"
#include "ZlibCompress.h"
#include "utilities.h"
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utility>

namespace glog {
GlogReader::GlogReader(string archiveFile, string protoName, const string *serverPrivateKey)
    : m_file(std::move(archiveFile))
    , m_fd(-1)
    , m_size(0)
    , m_protoName(std::move(protoName))
    , m_decompressor(nullptr)
    , m_position(0)
    , m_loadFileCompleted(false) {
    if (serverPrivateKey && !serverPrivateKey->empty()) {
        if (serverPrivateKey->length() != ECC_PRIVATE_KEY_LEN * 2 || !str2Hex(*serverPrivateKey, m_serverPrivateKey)) {
            throw std::invalid_argument("illegal svr pri key");
        }
        m_cipherReady = true;
    }
    m_loadFileCompleted = openFile();
}

GlogReader::~GlogReader() {
    closeFile();
    delete m_decompressor;
}

bool GlogReader::openFile() {
    m_fd = ::open(m_file.c_str(), O_RDONLY | O_CLOEXEC, S_IRWXU);
    if (m_fd < 0) {
        InternalError("fail to open [%s], %s", m_file.c_str(), strerror(errno));
        return false;
    }

    getFileSize(m_fd, m_size);

    size_t headerSize = 0;
    HeaderMismatchReason reason = readHeader(m_fd, m_file, m_size, m_protoName, &headerSize, nullptr);

    if (reason != HeaderMismatchReason::None) {
        int ret = ::remove(m_file.c_str());
        InternalDebug("file [%s] header mismatch reason:%d, remove ret:%d %s", m_file.c_str(), reason, ret,
                      ::strerror(errno));
        return false;
    }
    m_decompressor = new ZlibDecompressor();
    m_position = headerSize;
    return true;
}

void GlogReader::closeFile() {
    if (m_fd >= 0) {
        if (::close(m_fd) != 0) {
            InternalError("fail to close [%s], %s", m_file.c_str(), strerror(errno));
        }
    }
    m_fd = -1;
    m_size = 0;
    m_position = 0;
}

bool GlogReader::seek(size_t position) {
    if (::lseek(m_fd, position, SEEK_SET) < 0) {
        InternalError("fail to lseek file [%s], %s", m_file.c_str(), strerror(errno));
        return false;
    }
    m_position = position;
    return true;
}

//bool GlogReader::hasNext() const {
//    return spaceRemain() > format::LOG_LENGTH_BYTES;
//}
} // namespace glog