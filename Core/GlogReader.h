//
// Created by issac on 2021/1/26.
//

#ifndef CORE__GLOGREADER_H_
#define CORE__GLOGREADER_H_

#include "GlogBuffer.h"
#include "GlogPredef.h"
#include <cerrno>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

namespace glog {
class Decompressor;

class GlogReader {
    friend class Glog;

public:
    //    bool hasNext() const;

    bool seek(size_t position);

    size_t getPosition() const { return m_position; }

    int read(GlogBuffer &outBuffer);

private:
    string m_file;
    int m_fd;
    size_t m_size;
    size_t m_position;
    string m_protoName;
    bool m_loadFileCompleted;
    Decompressor *m_decompressor;
    uint8_t m_serverPrivateKey[ECC_PRIVATE_KEY_LEN] = {};
    bool m_cipherReady = false;

    GlogReader(string archiveFile, string protoName, const string *serverPrivateKey);

    ~GlogReader();

    bool openFile();

    void closeFile();

    bool isFileAlreadyOpen() const { return m_fd >= 0 && m_size > 0; }

    size_t spaceRemain() const {
        if (m_size <= m_position) {
            return 0;
        }
        return m_size - m_position;
    }
};
} // namespace glog
#endif //CORE__GLOGREADER_H_
