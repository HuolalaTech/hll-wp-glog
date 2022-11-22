#ifndef CORE_SCOPEDLOCK_H
#define CORE_SCOPEDLOCK_H

namespace glog {

template <typename T>
class ScopedLock {
    T *m_lock;

    void lock() {
        if (m_lock) {
            m_lock->lock();
        }
    }

    void unlock() {
        if (m_lock) {
            m_lock->unlock();
        }
    }

public:
    explicit ScopedLock(T *oLock) : m_lock(oLock) { lock(); }

    ~ScopedLock() {
        unlock();
        m_lock = nullptr;
    }

    // just forbid it for possibly misuse
    explicit ScopedLock(const ScopedLock<T> &other) = delete;
    ScopedLock &operator=(const ScopedLock<T> &other) = delete;
};

} // namespace glog

#include <type_traits>

#define SCOPED_LOCK(lock) _SCOPEDLOCK(lock, __COUNTER__)
#define _SCOPEDLOCK(lock, counter) __SCOPEDLOCK(lock, counter)
#define __SCOPEDLOCK(lock, counter)                                                                                    \
    glog::ScopedLock<std::remove_pointer<decltype(lock)>::type> __scopedLock##counter(lock)

#endif //CORE_SCOPEDLOCK_H
