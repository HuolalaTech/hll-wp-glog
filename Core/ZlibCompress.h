//
// Created by issac on 2021/1/23.
//

#ifndef CORE__ZLIBCOMPRESS_H_
#define CORE__ZLIBCOMPRESS_H_

#include "Compressor.h"
#include "InternalLog.h"
#include "zlib.h"

namespace glog {
class ZlibCompressor : public Compressor {
public:
    ZlibCompressor() : Compressor(), m_stream(z_stream{.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL}) {
        // windowBits can also be -8..-15 for raw deflate. In this case, -windowBits determines the window size.
        // deflate() will then generate raw deflate data with no zlib header or trailer, and will not compute a check value.
        int ret =
            deflateInit2(&m_stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);

        if (ret != Z_OK) {
            InternalError("fail to init zlib compressor, ret:%d", ret);
            m_init = false;
        } else {
            m_init = true;
        }
    }

    ~ZlibCompressor() override { deflateEnd(&m_stream); }

    bool realCompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) override;

    void reset() override;

private:
    z_stream m_stream;
};

class ZlibDecompressor : public Decompressor {
public:
    explicit ZlibDecompressor()
        : Decompressor()
        , m_stream(z_stream{
              .zalloc = Z_NULL,
              .zfree = Z_NULL,
              .opaque = Z_NULL,
          }) {

        int ret = inflateInit2(&m_stream, -MAX_WBITS);

        if (ret != Z_OK) {
            InternalError("fail to init zlib decompressor, ret:%d", ret);
            m_init = false;
        } else {
            m_init = true;
        }
    }

    ~ZlibDecompressor() override { inflateEnd(&m_stream); }

    bool realDecompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) override;

    void reset() override;

private:
    z_stream m_stream;
};
} // namespace glog
#endif //CORE__ZLIBCOMPRESS_H_
