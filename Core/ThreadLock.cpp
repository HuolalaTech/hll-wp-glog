//
// Created by issac on 2021/1/28.
//

#include "ThreadLock.h"
#include "InternalLog.h"
#include <cerrno>
#include <sys/time.h>

namespace glog {
ThreadLock::ThreadLock() {
    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);
    if (ret != 0) {
        InternalError("fail to init mutex attr %p, ret=%d, errno=%s", &attr, ret, strerror(errno));
        return;
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    ret = pthread_mutex_init(&m_lock, &attr);

    if (ret != 0) {
        InternalError("fail to init mutex %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }

    pthread_mutexattr_destroy(&attr);
}

ThreadLock::~ThreadLock() {
    pthread_mutex_destroy(&m_lock);
}

void ThreadLock::lock() {
    auto ret = pthread_mutex_lock(&m_lock);
    if (ret != 0) {
        InternalError("fail to lock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

void ThreadLock::unlock() {
    auto ret = pthread_mutex_unlock(&m_lock);
    if (ret != 0) {
        InternalError("fail to unlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }
}

ConditionVariable::ConditionVariable() {
    pthread_condattr_t attr;
    int ret = pthread_condattr_init(&attr);
    if (ret != 0) {
        InternalError("fail to init condition attr %p, ret=%d, errno=%s", &attr, ret, strerror(errno));
        return;
    }

    ret = pthread_cond_init(&m_condition, &attr);

    if (ret != 0) {
        InternalError("fail to init condition %p, ret=%d, errno=%s", &m_condition, ret, strerror(errno));
    }

    pthread_condattr_destroy(&attr);
}

ConditionVariable::~ConditionVariable() {
    pthread_cond_destroy(&m_condition);
}

void ConditionVariable::await(ThreadLock &lock) {
    int ret = pthread_cond_wait(&m_condition, &lock.m_lock);
    if (ret != 0) {
        InternalError("fail to await %p, ret=%d, errno=%s", &m_condition, ret, strerror(errno));
    }
}

bool ConditionVariable::awaitTimeout(ThreadLock &lock, long millis) {
    struct timespec ts {};
    makeTimeout(&ts, millis);

    int ret = pthread_cond_timedwait(&m_condition, &lock.m_lock, &ts);
    if (ret != 0 && ret != ETIMEDOUT) {
        InternalError("fail to await %p, ret=%d, errno=%s", &m_condition, ret, strerror(errno));
    }
    return ret != ETIMEDOUT;
}

void ConditionVariable::signal() {
    int ret = pthread_cond_signal(&m_condition);
    if (ret != 0) {
        InternalError("fail to signal %p, ret=%d, errno=%s", &m_condition, ret, strerror(errno));
    }
}

void ConditionVariable::makeTimeout(struct timespec *pts, long millisecond) {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    pts->tv_sec = millisecond / 1000 + tv.tv_sec;
    pts->tv_nsec = (millisecond % 1000) * 1000 * 1000 + tv.tv_usec * 1000;

    pts->tv_sec += pts->tv_nsec / (1000 * 1000 * 1000);
    pts->tv_nsec = pts->tv_nsec % (1000 * 1000 * 1000);
}

ReadWriteLock::ReadWriteLock() {
    pthread_rwlockattr_t attr;
    int ret = pthread_rwlockattr_init(&attr);
    if (ret != 0) {
        InternalError("fail to init rwlock attr %p, ret=%d, errno=%s", &attr, ret, strerror(errno));
        return;
    }

    ret = pthread_rwlock_init(&m_lock, &attr);

    if (ret != 0) {
        InternalError("fail to init rwlock %p, ret=%d, errno=%s", &m_lock, ret, strerror(errno));
    }

    pthread_rwlockattr_destroy(&attr);
}

ReadWriteLock::~ReadWriteLock() {
    pthread_rwlock_destroy(&m_lock);
}

} // namespace glog