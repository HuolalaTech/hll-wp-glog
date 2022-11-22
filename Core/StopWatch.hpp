#ifndef CORE_STOPWATCH_H
#define CORE_STOPWATCH_H

#include <utility>

#include "InternalLog.h"
#include "utilities.h"

using namespace std;

namespace glog {

class Lap {
    friend class StopWatch;
    int64_t m_time;
    string m_label;
    Lap *m_next;

public:
    Lap(string label, int64_t time) : m_label(std::move(label)), m_time(time), m_next(nullptr) {}
};

class StopWatch {
    string m_tag;
    Lap *m_first;

public:
    explicit StopWatch(const string &tag) : m_tag(tag) {
        InternalInfo("[%s] start", tag.c_str());
        m_first = new Lap("Start", cycleClockNow());
    }

    ~StopWatch() {
        while (m_first) {
            Lap *next = m_first->m_next;
            delete m_first;
            m_first = next;
        }
    }

    void lap(const string &label) {
        Lap *lap = new Lap(label, cycleClockNow());

        InternalInfo("[%s] %s done", m_tag.c_str(), label.c_str());

        Lap *last = m_first;
        while (last->m_next) {
            last = last->m_next;
        }
        last->m_next = lap;
    }

    void showCosts() {
        double totalMillis = 0;

        for (Lap *lap = m_first; lap->m_next; lap = lap->m_next) {
            double lapMillis = lap->m_next->m_time * 0.001 - lap->m_time * 0.001;
            totalMillis += lapMillis;
            InternalInfo("[%s] %s cost %.3f ms", m_tag.c_str(), lap->m_next->m_label.c_str(), lapMillis);
        }
        InternalInfo("[%s] total cost: %.3f ms", m_tag.c_str(), totalMillis);
    }
};

} // namespace glog

#ifdef GLOG_DEBUG
#    define START_WATCH(tag) StopWatch sw((tag));
#    define LAP(label) sw.lap((label));
#    define END_WATCH() sw.showCosts();
#else
#    define START_WATCH(tag)                                                                                           \
        {}
#    define LAP(label)                                                                                                 \
        {}
#    define END_WATCH()                                                                                                \
        {}
#endif

#endif //CORE_STOPWATCH_H