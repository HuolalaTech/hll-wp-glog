//
// Created by issac on 2021/1/11.
//

#include "InternalLog.h"

namespace glog {

#ifdef GLOG_DEBUG
InternalLogLevel g_currentLogLevel = InternalLogLevelDebug;
#else
InternalLogLevel g_currentLogLevel = InternalLogLevelInfo;
#endif

LogHandler g_logHandler;

#ifdef ENABLE_INTERNAL_LOG

#    ifndef __FILE_NAME__
const char *_getFileName(const char *path) {
    const char *ptr = strrchr(path, '/');
    if (!ptr) {
        ptr = strrchr(path, '\\');
    }
    if (ptr) {
        return ptr + 1;
    } else {
        return path;
    }
}
#    endif // __FILE_NAME__

#    ifdef GLOG_ANDROID
#        include <android/log.h>
#        include <string>

const std::string APP_NAME = "glog-core";

static android_LogPriority GlogLevelDesc(InternalLogLevel level) {
    switch (level) {
        case InternalLogLevelDebug:
            return ANDROID_LOG_DEBUG;
        case InternalLogLevelInfo:
            return ANDROID_LOG_INFO;
        case InternalLogLevelWarning:
            return ANDROID_LOG_WARN;
        case InternalLogLevelError:
            return ANDROID_LOG_ERROR;
        default:
            return ANDROID_LOG_UNKNOWN;
    }
}

void internalLogWithLevel(
    InternalLogLevel level, const char *filename, const char *func, int line, const char *format, ...) {
    if (level >= g_currentLogLevel) {
        std::string message;
        char buffer[16];

        va_list args;
        va_start(args, format);
        auto length = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (length < 0) {
            message = {};
        } else if (length < sizeof(buffer)) {
            message = std::string(buffer, length);
        } else {
            message.resize(length, '\0');
            va_start(args, format);
            std::vsnprintf(message.data(), length + 1, format, args);
            va_end(args);
        }

        if (g_logHandler) {
            g_logHandler(level, filename, line, func, message);
        } else {
            auto desc = GlogLevelDesc(level);
            __android_log_print(desc, APP_NAME.c_str(), "<%s:%d::%s> %s", filename, line, func, message.c_str());
        }
    }
}

#    else
static const char *GlogLevelDesc(InternalLogLevel level) {
    switch (level) {
        case InternalLogLevelDebug:
            return "D";
        case InternalLogLevelInfo:
            return "I";
        case InternalLogLevelWarning:
            return "W";
        case InternalLogLevelError:
            return "E";
        default:
            return "N";
    }
}

#    endif // GLOG_ANDROID

#    ifdef GLOG_APPLE

void internalLogWithLevel(
    InternalLogLevel level, const char *filename, const char *func, int line, const char *format, ...) {
    if (level >= g_currentLogLevel) {
        NSString *nsFormat = [NSString stringWithUTF8String:format];
        va_list argList;
        va_start(argList, format);
        NSString *message = [[NSString alloc] initWithFormat:nsFormat arguments:argList];
        va_end(argList);

        if (g_logHandler) {
            g_logHandler(level, filename, line, func, message);
        } else {
            NSLog(@"[%s] <%s:%d::%s> %@", GlogLevelDesc(level), filename, line, func, message);
        }
    }
}

#    else
#        ifndef GLOG_ANDROID
void internalLogWithLevel(
    InternalLogLevel level, const char *filename, const char *func, int line, const char *format, ...) {
    if (level >= g_currentLogLevel) {
        std::string message;
        char buffer[16];

        va_list args;
        va_start(args, format);
        auto length = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (length < 0) { // something wrong
            message = {};
        } else if (length < sizeof(buffer)) {
            message = std::string(buffer, static_cast<unsigned long>(length));
        } else {
            message.resize(static_cast<unsigned long>(length), '\0');
            va_start(args, format);
            std::vsnprintf(const_cast<char *>(message.data()), static_cast<size_t>(length) + 1, format, args);
            va_end(args);
        }

        if (g_logHandler) {
            g_logHandler(level, filename, line, func, message);
        } else {
            printf("[%s] <%s:%d::%s> %s\n", GlogLevelDesc(level), filename, line, func, message.c_str());
        }
    }
}
#        endif
#    endif // GLOG_APPLE

#endif // ENABLE_INTERNAL_LOG
} // namespace glog