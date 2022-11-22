//
// Created by issac on 2021/1/25.
//

#include "MessageQueue.h"
#include "InternalLog.h"
#include "ScopedLock.h"
#include <cerrno>
#include <memory>
#include <thread>
#include <utility>

namespace glog {

MessageQueue::MessageQueue(int capacity) : m_capacity(capacity), m_queueLock(new ThreadLock) {
    // start looper immediately
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = ::pthread_create(&m_worker, &attr, workerRunnable, this);
    if (ret != 0) {
        InternalError("fail to create thread, message queue quit. %s", ::strerror(errno));
        m_running = false;
    } else {
        m_running = true;
    }
    pthread_attr_destroy(&attr);
}

MessageQueue::~MessageQueue() {
    quit();
    ::pthread_join(m_worker, nullptr);
    {
        SCOPED_LOCK(m_queueLock);

        std::list<std::unique_ptr<Message>> empty;
        std::swap(m_queue, empty);
    }

    delete m_queueLock;
}

void *MessageQueue::workerRunnable(void *mqPtr) {
#ifdef GLOG_ANDROID
    int ret = ::pthread_setname_np(::pthread_self(), MESSAGE_QUEUE_THREAD_NAME);
#else
    int ret = ::pthread_setname_np(MESSAGE_QUEUE_THREAD_NAME);
#endif
    if (ret != 0) {
        InternalWarning("fail to set mq thread name, %s", strerror(errno));
    }
    auto *ptr = (MessageQueue *) (mqPtr);
    if (ptr->m_running) {
        ptr->loop();
    }
    return nullptr;
}

void MessageQueue::loop() {
#ifndef GLOG_ANDROID
    pthread_t current = ::pthread_self();
    static const int nameLen = 32;
    char threadName[nameLen];
    int ret = ::pthread_getname_np(current, threadName, nameLen);
    if (ret != 0) {
        InternalError("fail to get mq thread name");
    } else {
        InternalDebug("mq enter loop, thread name:%s", threadName);
    }
#else
    InternalDebug("mq enter loop");
#endif

    while (true) {
        std::unique_ptr<Message> msg;
        {
            SCOPED_LOCK(m_queueLock);

            if (m_running && m_queue.empty()) {
                m_queueNotEmptyCondition.await(*m_queueLock);
            }

            if (!m_running && m_queue.empty()) { // quit -> complete all task -> exit loop
                break;
            }
            msg = std::move(m_queue.front());
            m_queue.pop_front();
        }
        msg->m_callback();
    }
}

bool MessageQueue::enqueue(Message &&msg) {
    if (!m_running) {
        return false;
    }
    SCOPED_LOCK(m_queueLock);

    if (m_queue.size() >= m_capacity) {
        InternalWarning("mq reach capacity:%d", m_capacity);
        return false;
    }
    m_queue.emplace_back(msg.move());
    m_queueNotEmptyCondition.signal();

    return true;
}

void MessageQueue::quit() {
    SCOPED_LOCK(m_queueLock);

    InternalDebug("mq quit.");
    if (m_running) {
        m_running = false;
        m_queueNotEmptyCondition.signal();
    }
}

std::unique_ptr<Message> Message::move() {
    return std::make_unique<Message>(std::move(*this));
}

} // namespace glog