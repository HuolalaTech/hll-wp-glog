//
// Created by issac on 2021/1/25.
//

#ifndef CORE__MESSAGEQUEUE_H_
#define CORE__MESSAGEQUEUE_H_

#include "ThreadLock.h"
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <utility>

namespace glog {

class Message;

class MessageQueue {
public:
    explicit MessageQueue(int capacity);
    bool enqueue(Message &&msg);
    void quit();
    bool isRunning() const { return m_running; }
    ~MessageQueue();

private:
    std::list<std::unique_ptr<Message>> m_queue;
    int m_capacity;
    pthread_t m_worker;
    ThreadLock *m_queueLock;
    ConditionVariable m_queueNotEmptyCondition;
    std::atomic_bool m_running;
    void loop();

    // entrance of worker thread
    static void *workerRunnable(void *mqPtr);
};

class Message {
    friend class MessageQueue;

public:
    explicit Message(std::function<void()> callback) { m_callback = std::move(callback); }

    std::unique_ptr<Message> move();

    Message(const Message &) = delete;
    Message &operator=(const Message &) = delete;

    Message(Message &&) = default;
    Message &operator=(Message &&) = default;

private:
    std::function<void()> m_callback;
};

} // namespace glog
#endif //CORE__MESSAGEQUEUE_H_
