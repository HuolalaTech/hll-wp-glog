//
// Created by issac on 2021/1/11.
//

#ifndef CORE_LOG_BUFFER_H_
#define CORE_LOG_BUFFER_H_

#include "GlogPredef.h"
#include <cstdlib>

namespace glog {

// use as class member may cause free() same m_ptr multi times.
class GlogBuffer {

public:
    explicit GlogBuffer(GlogBufferLength_t size);

    GlogBuffer(void *srcPtr, GlogBufferLength_t size, bool deepCopy = false);

    GlogBuffer(GlogBuffer &&) = delete;
    GlogBuffer &operator=(GlogBuffer &&) = delete;

    GlogBuffer(const GlogBuffer &) = delete;
    GlogBuffer &operator=(const GlogBuffer &) = delete;

    ~GlogBuffer();

    void *getPtr() const { return m_ptr; }

    GlogBufferLength_t getCapacity() const { return m_capacity; }

    GlogBufferLength_t getAvailLength() const { return m_availLength; }

    void setAvailLength(GlogBufferLength_t availLength) { m_availLength = availLength; }

    //    size_t spaceLeft() const {
    //        if (m_capacity <= m_availLength) {
    //            return 0;
    //        }
    //        return m_capacity - m_availLength;
    //    }

private:
    void *m_ptr;
    GlogBufferLength_t m_capacity;
    // CAUTION at the very beginning, m_availLength = m_capacity
    GlogBufferLength_t m_availLength;
    bool m_isDeepCopy;
};

} // namespace glog
#endif // CORE_LOG_BUFFER_H_