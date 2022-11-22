//
// Created by issac on 2021/1/28.
//

#ifndef CORE__THREADLOCK_H_
#define CORE__THREADLOCK_H_

#include <pthread.h>

namespace glog {
class ConditionVariable;
class ThreadLock {
    friend class ConditionVariable;

private:
    pthread_mutex_t m_lock;

public:
    ThreadLock();
    ~ThreadLock();

    void lock();
    void unlock();

    // just forbid it for possibly misuse
    explicit ThreadLock(const ThreadLock &other) = delete;
    ThreadLock &operator=(const ThreadLock &other) = delete;
};

class ConditionVariable {
private:
    pthread_cond_t m_condition;
    static void makeTimeout(struct timespec *pts, long millisecond);

public:
    ConditionVariable();
    ~ConditionVariable();

    void signal();
    void await(ThreadLock &lock);
    /**
     * return false if timeout
     */
    bool awaitTimeout(ThreadLock &lock, long millis);

    // just forbid it for possibly misuse
    explicit ConditionVariable(const ConditionVariable &other) = delete;
    ConditionVariable &operator=(const ConditionVariable &other) = delete;
};

class ReadWriteLock {
    pthread_rwlock_t m_lock;

public:
    ReadWriteLock();
    ~ReadWriteLock();

    void readLock();
    void writeLock();
    void unlock();

    // just forbid it for possibly misuse
    explicit ReadWriteLock(const ReadWriteLock &other) = delete;
    ReadWriteLock &operator=(const ReadWriteLock &other) = delete;
};
} // namespace glog
#endif //CORE__THREADLOCK_H_
