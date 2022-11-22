//
// Created by issac on 2021/1/23.
//

#include "ZlibCompress.h"

namespace glog {

bool ZlibCompressor::realCompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) {
    InternalDebug("zlib deflate start");
    m_stream.avail_in = inBuffer.getAvailLength();
    m_stream.next_in = static_cast<Bytef *>(inBuffer.getPtr());

    //    GlogBufferLength_t compressedSize = deflateBound(&m_stream, inBuffer.getPosition());
    //    unique_ptr<GlogBuffer> outBuffer(new GlogBuffer(compressedSize));

    m_stream.next_out = static_cast<Bytef *>(outBuffer.getPtr());
    m_stream.avail_out = outBuffer.getCapacity();

    int ret = deflate(&m_stream, Z_SYNC_FLUSH);

    if (ret != Z_OK) {
        InternalError("fail to zlib deflate, ret:%d", ret);
        outBuffer.setAvailLength(0);
        return false;
    }
    GlogBufferLength_t outLength = outBuffer.getCapacity() - m_stream.avail_out;
    outBuffer.setAvailLength(outLength);
    return true;
}

void ZlibCompressor::reset() {
    int ret = deflateReset(&m_stream);
    if (ret != Z_OK) {
        InternalError("fail to reset deflate, ret:%d", ret);
    }
}

bool ZlibDecompressor::realDecompress(const GlogBuffer &inBuffer, GlogBuffer &outBuffer) {
    m_stream.avail_in = inBuffer.getAvailLength();
    m_stream.next_in = static_cast<Bytef *>(inBuffer.getPtr());

    m_stream.next_out = static_cast<Bytef *>(outBuffer.getPtr());
    m_stream.avail_out = outBuffer.getCapacity();

    int ret = inflate(&m_stream, Z_SYNC_FLUSH);

    if (ret != Z_OK) {
        InternalError("fail to zlib inflate, ret:%d, msg:%s", ret, m_stream.msg);
        outBuffer.setAvailLength(0);
        return false;
    }
    GlogBufferLength_t outLength = outBuffer.getCapacity() - m_stream.avail_out;
    outBuffer.setAvailLength(outLength);
    return true;
}

void ZlibDecompressor::reset() {
    int ret = inflateReset(&m_stream);
    if (ret != Z_OK) {
        InternalError("fail to reset inflate, ret:%d", ret);
    }
}

} // namespace glog