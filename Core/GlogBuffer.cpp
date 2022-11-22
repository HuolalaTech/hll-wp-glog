//
// Created by issac on 2021/1/11.
//

#include "GlogBuffer.h"
#include "GlogPredef.h"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace glog {

GlogBuffer::GlogBuffer(GlogBufferLength_t size) : m_capacity(size), m_isDeepCopy(true), m_availLength(size) {
    m_ptr = malloc(size);
    if (!m_ptr) {
        throw std::runtime_error(strerror(errno));
    }
}

GlogBuffer::GlogBuffer(void *srcPtr, GlogBufferLength_t size, bool deepCopy)
    : m_capacity(size), m_isDeepCopy(deepCopy), m_availLength(size) {
    if (deepCopy) {
        m_ptr = malloc(size);
        if (!m_ptr) {
            throw std::runtime_error(strerror(errno));
        }
        ::memcpy(m_ptr, srcPtr, size);
    } else {
        m_ptr = srcPtr;
    }
}

GlogBuffer::~GlogBuffer() {
    if (m_ptr && m_isDeepCopy) {
        free(m_ptr);
    }
}

} // namespace glog
