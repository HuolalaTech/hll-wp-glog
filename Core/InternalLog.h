//
// Created by issac on 2021/1/11.
//

#ifndef CORE__INTERNALLOG_H_
#define CORE__INTERNALLOG_H_

#include "GlogPredef.h"
#include <string>

namespace glog {

void internalLogWithLevel(
    InternalLogLevel level, const char *filename, const char *func, int line, const char *format, ...);

extern InternalLogLevel g_currentLogLevel;

extern LogHandler g_logHandler;

// enable logging
#define ENABLE_INTERNAL_LOG

#ifdef ENABLE_INTERNAL_LOG

#    ifdef __FILE_NAME__
#        define __GLOG_FILE_NAME__ __FILE_NAME__
#    else
const char *_getFileName(const char *path);
#        define __GLOG_FILE_NAME__ _getFileName(__FILE__)
#    endif // __FILE_NAME__

#    define InternalError(format, ...)                                                                                 \
        internalLogWithLevel(glog::InternalLogLevelError, __GLOG_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#    define InternalWarning(format, ...)                                                                               \
        internalLogWithLevel(glog::InternalLogLevelWarning, __GLOG_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#    define InternalInfo(format, ...)                                                                                  \
        internalLogWithLevel(glog::InternalLogLevelInfo, __GLOG_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#    ifdef GLOG_DEBUG
#        define InternalDebug(format, ...)                                                                             \
            internalLogWithLevel(glog::InternalLogLevelDebug, __GLOG_FILE_NAME__, __func__, __LINE__, format, ##__VA_ARGS__)
#    else
#        define InternalDebug(format, ...)                                                                             \
            {}
#    endif // GLOG_DEBUG

#else

#    define InternalError(format, ...)                                                                                 \
        {}
#    define InternalWarning(format, ...)                                                                               \
        {}
#    define InternalInfo(format, ...)                                                                                  \
        {}
#    define InternalDebug(format, ...)                                                                                 \
        {}
#endif // ENABLE_INTERNAL_LOG

} // namespace glog
#endif //CORE__INTERNALLOG_H_
