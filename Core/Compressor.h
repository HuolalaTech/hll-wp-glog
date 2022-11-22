//
// Created by issac on 2021/1/23.
//

#ifndef CORE__COMPRESSOR_H_
#define CORE__COMPRESSOR_H_

#include "GlogBuffer.h"
#include "InternalLog.h"

using namespace std;

namespace glog {

class Compressor {
public:
    explicit Compressor() : m_init(false){};

    virtual ~Compressor() = default;

    virtual void reset() = 0;

    bool compress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) {
        InternalDebug("compress start, init:%d", m_init);
        if (!m_init)
            return false;
        return realCompress(inBuffer, outBuffer);
    }

protected:
    virtual bool realCompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) = 0;

    bool m_init;
};

class Decompressor {
public:
    explicit Decompressor() : m_init(false) {}

    virtual ~Decompressor() = default;

    virtual void reset() = 0;

    bool decompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) {
        if (!m_init)
            return false;
        return realDecompress(inBuffer, outBuffer);
    }

protected:
    virtual bool realDecompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) = 0;

    bool m_init;
};
} // namespace glog

#endif //CORE__COMPRESSOR_H_
