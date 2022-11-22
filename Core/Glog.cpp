//
// Created by issac on 2021/1/11.
//

#include "Glog.h"
#include "GlogFile.h"
#include "GlogReader.h"
#include "MessageQueue.h"
#include "ScopedLock.h"
#include "StopWatch.hpp"
#include "ThreadLock.h"
#include "ZlibCompress.h"
#include "utilities.h"
#include <cerrno>
#include <fcntl.h>
#include <regex>
#include <unordered_map>

using namespace std;

namespace glog {

size_t SYS_PAGE_SIZE;
Endianness SYS_ENDIAN;
unordered_map<string, Glog *> *g_instanceMap;
recursive_mutex *g_instanceMutex;

pthread_once_t onceControl = PTHREAD_ONCE_INIT;

// partial match table for kmp forward and backward searching for sync marker
int g_forwardPartialMatchTable[format::SYNC_MARKER_LENGTH];
int g_backwardPartialMatchTable[format::SYNC_MARKER_LENGTH];
pthread_t *g_daemon;
ThreadLock *g_daemonPollLock;
ConditionVariable *g_daemonPollCond;
atomic_bool g_daemonRunning = true;

void preparePartialTables() {
    const uint8_t len = format::SYNC_MARKER_LENGTH;
    uint8_t reverse[len] = {};

    for (int i = 0; i < len; ++i) {
        reverse[len - i - 1] = format::SYNC_MARKER[i];
    }

    computePartialMatchTable(format::SYNC_MARKER, g_forwardPartialMatchTable, len);
    computePartialMatchTable(reverse, g_backwardPartialMatchTable, len);
}

void applyOnAll(const std::function<void(Glog *)> &action) {
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    for (auto &pair : *g_instanceMap) {
        Glog *gPtr = pair.second;
        if (!gPtr)
            continue;
        action(gPtr);
    }
}

void *daemonAction(void *) {
#ifdef GLOG_ANDROID
    int ret = ::pthread_setname_np(::pthread_self(), DAEMON_THREAD_NAME);
#else
    int ret = ::pthread_setname_np(DAEMON_THREAD_NAME);
#endif
    if (ret != 0) {
        InternalWarning("fail to set daemon thread name, %s", strerror(errno));
    }
    bool initial = true;

    while (true) {
        time_t now = wallTimeNow();
        time_t t0 = midnightEpochSeconds(); // epoch seconds of 00:00
        time_t t24 = t0 + 24 * 60 * 60;     // epoch seconds of 24:00

        applyOnAll([now, t0, t24](Glog *ptr) -> void {
            if (ptr->incrementalArchive()) {
                const int tolerance = 2 * 60 + 30;
                ptr->setCheckAcrossDay(t24 - now < tolerance || now - t0 < tolerance);
            }
        });

        {
            SCOPED_LOCK(g_daemonPollLock);

            if (g_daemonRunning) {
                g_daemonPollCond->awaitTimeout(*g_daemonPollLock, initial ? 5 * 1000 : 2 * 60 * 1000);
                initial = false;
            } else {
                InternalWarning("daemon quit.");
                break;
            }
        }
        InternalDebug("daemon tick...");

        applyOnAll([](Glog *ptr) -> void {
            ptr->cleanupSafely();
            if (ptr->incrementalArchive()) {
                ptr->flush();
            }
        });
    }
    return nullptr;
}

void prepareDaemonThread() {
    g_daemon = new pthread_t;
    g_daemonPollLock = new ThreadLock;
    g_daemonPollCond = new ConditionVariable;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = ::pthread_create(g_daemon, &attr, daemonAction, nullptr);
    if (ret != 0) {
        g_daemonRunning = false;
        InternalError("fail to create thread, message queue quit. %s", ::strerror(errno));
    } else {
        g_daemonRunning = true;
    }
    pthread_attr_destroy(&attr);
}

void doInitialize() {
    g_instanceMutex = new recursive_mutex();
    g_instanceMap = new unordered_map<string, Glog *>;

    SYS_PAGE_SIZE = getPageSize();

    const int i = 1;
    SYS_ENDIAN = ((*(char *) &i) == 0) ? Endianness::BigEndian : Endianness::LittleEndian;
    preparePartialTables();
    prepareDaemonThread();
}

void Glog::initialize(InternalLogLevel logLevel) {
    g_currentLogLevel = logLevel;
    pthread_once(&onceControl, doInitialize);
}

Glog *Glog::maybeCreateWithConfig(const GlogConfig &config) {
    if (config.m_protoName.empty()) {
        InternalError("fail to create glog instance with empty proto");
        return nullptr;
    }
    if (!g_instanceMutex) {
        InternalError("fail to create glog, should invoke Glog::initialize()");
        return nullptr;
    }

    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    auto itr = g_instanceMap->find(config.m_protoName);
    if (itr != g_instanceMap->end()) {
        Glog *gPtr = itr->second;
        return gPtr;
    }

    if (config.m_rootDirectory.empty()) {
        InternalError("glog instance with empty root directory");
        return nullptr;
    } else {
        if (!isFileExists(config.m_rootDirectory)) {
            if (!mkPath(config.m_rootDirectory)) {
                InternalError("fail to mkdir [%s]", config.m_rootDirectory.c_str());
                return nullptr;
            }
        }
    }

    auto gPtr = new Glog(config.m_protoName, config.m_rootDirectory, config.m_async, config.m_expireSeconds,
                         config.m_totalArchiveSizeLimit, config.m_compressMode, config.m_encryptMode,
                         config.m_incrementalArchive, config.m_serverPublicKey);
    (*g_instanceMap)[config.m_protoName] = gPtr;
    return gPtr;
}

Glog *Glog::getInstanceWithProto(const string &protoName) {
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    auto itr = g_instanceMap->find(protoName);
    if (itr != g_instanceMap->end()) {
        Glog *gPtr = itr->second;
        return gPtr;
    }
    return nullptr;
}

Glog::Glog(string protoName,
           const string &rootDirectory,
           bool async,
           int32_t expireSeconds,
           size_t totalArchiveSizeLimit,
           format::GlogCompressMode compressMode,
           format::GlogEncryptMode encryptMode,
           bool incrementalArchive,
           const string *serverPublicKey)
    : m_protoName(std::move(protoName))
    , m_async(async)
    , m_expireSeconds(expireSeconds)
    , m_totalArchiveSizeLimit(totalArchiveSizeLimit)
    , m_compressMode(compressMode)
    , m_encryptMode(encryptMode)
    , m_archiveLock(new ThreadLock)
    , m_syncWriteLock(new ThreadLock)
    , m_readingArchivesLock(new ThreadLock)
    , m_incrementalArchive(incrementalArchive)
    , m_shouldTryFlushAcrossDay(false) {

    m_cacheSize = calculateRoundCacheSize();
    m_rootDirectory = endsWithSplash(rootDirectory) ? rootDirectory : rootDirectory + "/";
    m_compressor = new ZlibCompressor();
    string cacheFilePath = makeCacheFilePath(m_protoName, m_rootDirectory);
    m_cacheFile = new GlogFile(cacheFilePath, m_protoName, m_cacheSize, m_compressor, m_compressMode, m_encryptMode,
                               serverPublicKey);
    // every message may take extra memory because a copy of log content made before enqueue
    // max extra memory = capacity * single log max size
    if (async) {
        m_writeQueue = new MessageQueue(INT_MAX);
        if (!m_writeQueue->isRunning()) {
            m_writeQueue = nullptr;
            m_async = false;
        }
    } else {
        m_writeQueue = nullptr;
    }
    makeArchiveNameRegex(m_incrementalArchive, m_protoName, m_archiveNameRegex);
    if (async) {
        if (m_writeQueue)
            m_writeQueue->enqueue(Message([this]() { tryFlushAcrossDay(); }));
    } else {
        SCOPED_LOCK(m_syncWriteLock);
        tryFlushAcrossDay();
    }
}

Glog::~Glog() {
    InternalDebug("glog dealloc");
    delete m_writeQueue;
    delete m_cacheFile;
    delete m_compressor;
    delete m_archiveLock;
    delete m_syncWriteLock;
    delete m_readingArchivesLock;
    m_readingArchives.clear();
}

void Glog::destroy(const string &protoName) {
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    auto itr = g_instanceMap->find(protoName);
    if (itr != g_instanceMap->end()) {
        Glog *gPtr = itr->second;
        delete gPtr;
        g_instanceMap->erase(itr);
    }
}
void Glog::removeAll(bool removeReadingFiles, bool reloadCache) {
    if (m_async) {
        m_writeQueue->enqueue(
            Message([this, removeReadingFiles, reloadCache]() { doRemoveAll(removeReadingFiles, reloadCache); }));
    } else {
        SCOPED_LOCK(m_syncWriteLock);

        doRemoveAll(removeReadingFiles, reloadCache);
    }
}

bool findLogFileOfDate(const string &rootDirectory, string &outFilename, const string &protoName, time_t epochSeconds) {
    ::tm tmTime{};
    //    ::gmtime_r(&nowStamp, &tmTime); // utc time
    ::localtime_r(&epochSeconds, &tmTime); // local time
    int year = 1900 + tmTime.tm_year;
    int mon = 1 + tmTime.tm_mon;
    int day = tmTime.tm_mday;
    // [protoName]-yyyyMMdd.glog + '\0'
    const size_t fixedLength = protoName.length() + strlen("-yyyyMMdd") + strlen(ARCHIVE_FILE_SUFFIX) + 1;
    char name[fixedLength];
    snprintf(name, fixedLength, "%s-%d%02d%02d%s", protoName.c_str(), year, mon, day, ARCHIVE_FILE_SUFFIX);
    outFilename = string(name);

    vector<string> filenames;
    bool ret = listFilesInDir(rootDirectory, filenames);
    if (ret) {
        auto predicate = [outFilename](string &filename) {
            return filename == outFilename;
        };
        auto logfile = std::find_if(filenames.begin(), filenames.end(), predicate);

        if (logfile != filenames.end()) {
            return true;
        }
    }
    return false;
}

void Glog::doRemoveAll(bool removeReadingFiles, bool reloadCache) {
    list<FileStat> files;
    static size_t useless;
    collectArchives(files, FileOrder::None, useless);
    if (files.empty()) {
        return;
    }

    auto removeFile = [](const string &file) {
        if (::remove(file.c_str()) < 0) {
            InternalError("fail to remove file [%s] %s", file.c_str(), ::strerror(errno));
        } else {
            InternalDebug("remove file [%s]", file.c_str());
        }
    };

    for (auto &file : files) {
        if (!removeReadingFiles) {
            SCOPED_LOCK(m_readingArchivesLock);

            bool reading = m_readingArchives.find(file.m_path) != m_readingArchives.end();
            if (!reading) {
                removeFile(file.m_path);
            }
        } else {
            removeFile(file.m_path);
        }
    }

    if (reloadCache && m_cacheFile->getTotalLogNum() > 0) {
        InternalDebug("removeAll, loadFromDisk cache");
        m_cacheFile->closeFile();
        removeFile(m_cacheFile->getPath());
        uint8_t depth = MAX_RECURSION_DEPTH;
        m_cacheFile->loadFromDisk(depth, m_cacheSize);
    }
}

void Glog::destroyAll() {
    {
        SCOPED_LOCK(g_daemonPollLock);
        if (g_daemonRunning) {
            g_daemonRunning = false;
            g_daemonPollCond->signal();
        }
    }

    ::pthread_join(*g_daemon, nullptr);

    delete g_daemonPollCond;
    delete g_daemonPollLock;
    delete g_daemon;

    {
        std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

        for (auto &pair : *g_instanceMap) {
            Glog *gPtr = pair.second;
            delete gPtr;
            pair.second = nullptr;
        }

        delete g_instanceMap;
        g_instanceMap = nullptr;
    }
    delete g_instanceMutex;
}

void Glog::registerLogHandler(LogHandler handler) {
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    g_logHandler = handler;
}

void Glog::unRegisterLogHandler() {
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    g_logHandler = nullptr;
}

bool Glog::appendLogFile(uint8_t &maxRecursionDepth, time_t epochSeconds) {
    // 1. get log file of date
    // 2. if flush to today's log file and it's not exists, recreate cache
    // 3. check file valid, if invalid, remove it
    // 4. truncate file size
    // 5. mmap
    // 6. copy content of cache file to it
    // 7. msync, munmap
    // 8. truncate file size
    // 9. remove cache file and reload, for the purpose of update its creation time
    // 10. reset compressor
    string logFilename;
    const bool exists = findLogFileOfDate(m_rootDirectory, logFilename, m_protoName, epochSeconds);

    if (exists) {
        InternalDebug("log of date:%s found", logFilename.c_str());
    } else {
        InternalDebug("log of date:%s not found", logFilename.c_str());
    }

    const string filepath = m_rootDirectory + logFilename;
    int fd = ::open(filepath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU);
    if (fd < 0) {
        InternalError("fail to open [%s], %s", filepath.c_str(), strerror(errno));
        return false;
    }
    size_t fileSize = getFileSize(filepath);

    InternalDebug("log of date file size:%zu", fileSize);

    bool recreate = fileSize == 0;

    size_t headerSize = 0;
    if (!recreate) {
        HeaderMismatchReason reason = readHeader(fd, filepath, fileSize, m_protoName, &headerSize);

        if (reason != HeaderMismatchReason::None) {
            int ret = ::remove(filepath.c_str());

            recreate = true;
            fileSize = 0;
            InternalDebug("file [%s] header mismatch reason:%d, remove ret:%d %s", filepath.c_str(), reason, ret,
                          ::strerror(errno));
            if (--maxRecursionDepth <= 0) {
                InternalError("appendLogFile() reach recursion upper limit");
                m_cacheFile->closeFile();
                return false;
            }
            return appendLogFile(maxRecursionDepth, epochSeconds);
        }
    }
    size_t mmapOffset; // mmap starting at offset in the file, must be a multiple of the page size
    size_t mmapSize;   // mmap size, must be a multiple of the page size (always <= 132 KB)
    size_t dstOffset;  // copy to offset of mmap area
    size_t copySize;   // copy size
    if (recreate) {
        mmapOffset = 0;
        dstOffset = 0;
        copySize = m_cacheFile->getPosition();
        mmapSize = m_cacheFile->getPosition();
    } else {
        copySize = m_cacheFile->getDataLength();
        mmapOffset = fileSize / SYS_PAGE_SIZE * SYS_PAGE_SIZE;
        dstOffset = fileSize - mmapOffset;
        mmapSize = copySize + fileSize - mmapOffset;
    }
    if (mmapSize % SYS_PAGE_SIZE != 0) {
        mmapSize = (mmapSize / SYS_PAGE_SIZE + 1) * SYS_PAGE_SIZE;
    }

    InternalDebug("mmapOffset:%zu, mmapSize:%zu, copyOffset:%zu, copySize:%zu", mmapOffset, mmapSize, dstOffset,
                  copySize);

    if (::ftruncate(fd, mmapOffset + mmapSize) != 0) {
        InternalError("fail to truncate [%s] to size %zu, %s", filepath.c_str(), mmapOffset + mmapSize,
                      ::strerror(errno));
        m_cacheFile->closeFile();
        return false;
    }

    if (!GlogFile::zeroFillFile(fd, fileSize, copySize)) {
        InternalError("fail to zeroFile [%s] to size %zu, %s", filepath.c_str(), mmapOffset + mmapSize,
                      strerror(errno));
        m_cacheFile->closeFile();
        return false;
    }

    void *mmapPtr = nullptr;
    mmapPtr = ::mmap(mmapPtr, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mmapOffset);
    if (mmapPtr == MAP_FAILED) {
        InternalError("fail to mmap [%s], %s", filepath.c_str(), strerror(errno));
        m_cacheFile->closeFile();
        return false;
    }

    auto *src = (uint8_t *) m_cacheFile->getMemoryPtr();

    if (src == nullptr || src == MAP_FAILED) {
        InternalError("fail to get cache file [%s] mmap ptr", filepath.c_str());
        m_cacheFile->closeFile();
        return false;
    }
    memcpy((uint8_t *) mmapPtr + dstOffset, src + headerSize, copySize);

    if (mmapPtr && mmapPtr != MAP_FAILED) {
        if (::msync(mmapPtr, mmapSize, MS_SYNC) != 0) {
            InternalError("fail to msync [%s] offset:%zu size:%zu %s", filepath.c_str(), mmapOffset, mmapSize,
                          strerror(errno));
        }

        if (::munmap(mmapPtr, mmapSize) != 0) {
            InternalError("fail to munmap [%s] offset:%zu size:%zu %s", filepath.c_str(), mmapOffset, mmapSize,
                          strerror(errno));
        }
    }

    if (::ftruncate(fd, fileSize + copySize) != 0) {
        InternalError("fail to truncate [%s] to size %zu, %s", filepath.c_str(), fileSize + copySize,
                      ::strerror(errno));
        m_cacheFile->closeFile();
        return false;
    }

    //    // reset file content to '0' except header
    //    memset((uint8_t *) m_cacheFile->getMemoryPtr() + m_cacheFile->getHeaderSize(), 0,
    //           m_cacheFile->getPosition() - m_cacheFile->getHeaderSize());
    //    m_cacheFile->resetInternalState();

    m_cacheFile->closeFile();
    if (::remove(m_cacheFile->getPath().c_str()) < 0) {
        InternalError("fail to remove file [%s] %s", m_cacheFile->getPath().c_str(), ::strerror(errno));
        return false;
    } else {
        InternalDebug("remove file [%s]", m_cacheFile->getPath().c_str());
    }
    uint8_t depth = MAX_RECURSION_DEPTH;
    return m_cacheFile->loadFromDisk(depth, m_cacheSize);
}

bool Glog::internalFlush() {
    InternalDebug("==> maybe start flush ==>");

    // 0. msync

    // if is non incremental flush
    // 1. truncate cache file to actual size
    // 2. close cache file
    // 3. delete duplicate archive file with same name if exists
    // 4. rename current cache file to archive file
    // 5. recreate cache file
    // 6. reset compressor status

    // if is incremental flush
    // 1. append to today log file

    if (!m_cacheFile->getTotalLogNum()) { // no log in cache
        return false;
    }
    InternalDebug("==> start flush ==>");

    m_cacheFile->msync();

    if (m_incrementalArchive) {
        uint8_t maxDepth = MAX_RECURSION_DEPTH;
        WallTime_t now = wallTimeNow();
        auto nowStamp = static_cast<time_t>(now);
        return appendLogFile(maxDepth, nowStamp); // flush to today's log file
    }

    size_t actualSize = m_cacheFile->getPosition();
    if (::ftruncate(m_cacheFile->getFd(), actualSize) != 0) {
        InternalError("fail to truncate [%s] to size %zu, %s", m_cacheFile->getPath().c_str(), actualSize,
                      ::strerror(errno));
        return false;
    }
    m_cacheFile->closeFile();
    string filePath = makeArchiveFilePath(m_protoName, m_rootDirectory);
    InternalDebug("archive file path:%s", filePath.c_str());

    if (isFileExists(filePath)) {
        InternalWarning("duplicate archive file [%s]", filePath.c_str());
        if (::remove(filePath.c_str()) < 0) {
            InternalError("fail to remove duplicate archive file [%s] %s", filePath.c_str(), ::strerror(errno));
        }
    }

    if (::rename(m_cacheFile->getPath().c_str(), filePath.c_str()) < 0) {
        InternalError("fail to rename file [%s] to %s, %s", m_cacheFile->getPath().c_str(), filePath.c_str(),
                      strerror(errno));
        return false;
    }
    uint8_t depth = MAX_RECURSION_DEPTH;
    bool loaded = m_cacheFile->loadFromDisk(depth, m_cacheSize);
    if (!loaded) {
        return false;
    }
    InternalDebug("<== end flush <==");
    return true;
}

string Glog::getArchiveSnapshot(vector<string> &archives, const ArchiveCondition &condition, FileOrder order) {
    bool flush = condition.m_flush && (condition.m_minLogNum <= m_cacheFile->getTotalLogNum() ||
                                       condition.m_totalLogSize <= m_cacheFile->getTotalLogSize());
    InternalDebug("get archive snapshot, log in cache num:%d, size:%d, need flush:%d", m_cacheFile->getTotalLogNum(),
                  m_cacheFile->getTotalLogSize(), flush);

    const size_t bufLen = 1024;
    char buf[bufLen];
    memset(buf, '\0', bufLen);

    snprintf(buf, bufLen, "get archive snapshot condition[flush:%s, totalLogSize:%zu, minLogNum:%zu, order:%s]",
             condition.m_flush ? "true" : "false", condition.m_totalLogSize, condition.m_minLogNum,
             order == FileOrder::CreateTimeAscending ? "Ascending" : "Descending");

    string message{buf};

    if (condition.m_flush && !flush) {
        message += ", skip flush";
        if (condition.m_minLogNum > m_cacheFile->getTotalLogNum()) {
            int num = static_cast<int>(m_cacheFile->getTotalLogNum());
            message += ", insufficient log num:" + std::to_string(num);
        }

        if (condition.m_totalLogSize > m_cacheFile->getTotalLogSize()) {
            int size = static_cast<int>(m_cacheFile->getTotalLogSize());
            message += ", insufficient log size:" + std::to_string(size);
        }
    }

    auto collect = [this, order](vector<string> &archives) {
        list<FileStat> stats;
        static size_t useless;
        collectArchives(stats, order, useless);
        archives.resize(stats.size());
        std::transform(stats.begin(), stats.end(), archives.begin(), [](const FileStat &st) { return st.m_path; });
    };

    if (flush) {
        if (m_async) {
            auto flushFinishCond = std::make_shared<ConditionVariable>();
            auto flushFinished = std::make_shared<bool>(false);

            m_writeQueue->enqueue(Message([this, flushFinishCond, flushFinished]() {
                SCOPED_LOCK(m_archiveLock);

                internalFlush();
                *flushFinished = true;
                flushFinishCond->signal();
            }));
            {
                SCOPED_LOCK(m_archiveLock);

                if (!*flushFinished) {
                    bool timeout = !flushFinishCond->awaitTimeout(*m_archiveLock, FLUSH_AWAIT_TIMEOUT_MILLIS);
                    if (timeout) {
                        message += ", flush timeout after [" + std::to_string(FLUSH_AWAIT_TIMEOUT_MILLIS) + "] ms";
                        InternalWarning("flush timeout after [%ld] ms", FLUSH_AWAIT_TIMEOUT_MILLIS);
                    }
                }
                START_WATCH("glog watch collect archives");
                collect(archives);
                LAP("collect");
                END_WATCH();
            }
        } else {
            SCOPED_LOCK(m_syncWriteLock);

            bool ret = internalFlush();
            InternalDebug("flush in current thread, result:%d", ret);
            collect(archives);
        }
    } else {
        SCOPED_LOCK(m_syncWriteLock);

        collect(archives);
    }
    InternalDebug("get archive snapshot, file num:%d", archives.size());
    message += ", snapshot files num:" + std::to_string(static_cast<int>(archives.size()));
    return message;
}

void Glog::collectArchives(list<FileStat> &archives, FileOrder order, size_t &totalSize) {
    vector<string> filenames;
    bool ret = listFilesInDir(m_rootDirectory, filenames);
    if (!ret) {
        return;
    }
    int fileNum = 0;
    for (const auto &filename : filenames) {
        string fullPath = m_rootDirectory + filename;

        if (/*!isDirectory(fullPath) &&*/ std::regex_match(filename, m_archiveNameRegex)) {
            int64_t createTime;
            if (!getFileCreateTime(fullPath, createTime)) {
                InternalError("fail to get create time of file [%s]", fullPath.c_str());
                continue;
            }
            FileStat st = {.m_path = fullPath, .m_createTime = createTime, .m_size = getFileSize(fullPath)};
            archives.emplace_back(st);
            totalSize += st.m_size;
            fileNum++;
        }
    }
    if (fileNum > 1 && order != FileOrder::None) {
        archives.sort([order](const FileStat &lhs, const FileStat &rhs) -> bool {
            return order == FileOrder::CreateTimeDescending ? lhs.m_createTime > rhs.m_createTime
                                                            : lhs.m_createTime < rhs.m_createTime;
        });
    }
}

void Glog::makeArchiveNameRegex(bool incrementalArchive, const string &protoName, regex &outRegex) {
    // [protoName]-yyyyMMdd.glog
    // or
    // [protoName]-yyyyMMddHHmmssSSS.glog
    const size_t timeLen = strlen(incrementalArchive ? "yyyyMMdd" : "yyyyMMddHHmmssSSS");
    const size_t fixedLength = protoName.size() + timeLen + strlen(ARCHIVE_FILE_SUFFIX) + 8;
    char regexBuf[fixedLength];

    snprintf(regexBuf, fixedLength, "%s-\\d{%zu}%s", protoName.c_str(), timeLen, ARCHIVE_FILE_SUFFIX);
    outRegex = regex{regexBuf};
}

string Glog::getCacheFileName() const {
    return filename(m_cacheFile->getPath());
}

void Glog::getArchivesOfDate(time_t epochSeconds, vector<string> &filePaths) {
    list<FileStat> archives;
    static size_t useless;
    collectArchives(archives, FileOrder::None, useless);
    if (archives.empty()) {
        return;
    }

    for (auto &archive : archives) {
        ::tm specified{};
        //    ::gmtime_r(&nowStamp, &tmTime); // utc time
        ::localtime_r(&epochSeconds, &specified); // local time

        // incremental archive's create time may modified by ftruncate, so get its create time from name
        if (m_incrementalArchive) {
            string name = filename(archive.m_path);
            // protoName-yyyyMMdd
            string date = name.substr(m_protoName.length() + 1, 8);
            int year = 1900 + specified.tm_year;
            int mon = 1 + specified.tm_mon;
            int day = specified.tm_mday;
            if (stoi(date) == year * 10000 + mon * 100 + day) {
                filePaths.emplace_back(archive.m_path);
            }
        } else {
            ::tm fileTm{};
            time_t fileEpochSecs = static_cast<time_t>(archive.m_createTime / 1000);
            ::localtime_r(&fileEpochSecs, &fileTm);

            if (specified.tm_year == fileTm.tm_year && specified.tm_mon == fileTm.tm_mon &&
                specified.tm_mday == fileTm.tm_mday) {
                filePaths.emplace_back(archive.m_path);
            }
        }
    }
}

#define __REMOVE_FILE(f)                                                                                               \
    if (::remove((f).m_path.c_str()) < 0) {                                                                            \
        InternalError("fail to remove archive file [%s] %s", (f).m_path.c_str(), ::strerror(errno));                   \
    } else {                                                                                                           \
        InternalDebug("remove file [%s]", (f).m_path.c_str());                                                         \
    }

void Glog::cleanupSafely() {
    // 1. collect archives, sort as create time ascending
    // 2. if total size > limit, remove files until total size < limit
    // 3. remove overdue files
    // 4. remove over limit files

    InternalDebug("start cleanup, proto:%s", m_protoName.c_str());
    list<FileStat> archives;
    size_t totalSize = 0;
    collectArchives(archives, FileOrder::CreateTimeAscending, totalSize);

    if (archives.size() <= 1) {
        return;
    }

    archives.pop_back(); // the newest archive may operating by Glog, skip it

    InternalDebug("prepare to clean archives, total file num:%d", archives.size());

    time_t now = wallTimeNow();
    const int32_t expireSeconds = m_expireSeconds;

    // if oldest file not overdue, skip overdue check
    bool anyOverdue = now - archives.front().m_createTime / 1000 > expireSeconds;

    SCOPED_LOCK(m_readingArchivesLock);

    for (auto itr = archives.begin(); itr != archives.end();) {
        FileStat file = *itr;
        bool removed = false;
        bool reading = m_readingArchives.find(file.m_path) != m_readingArchives.end();
        // this file is not being read, remove it
        if (totalSize > m_totalArchiveSizeLimit && !reading) {
            __REMOVE_FILE(file);
            // invoker of openReader may delete file, so ignore the result of remove, shrink total as if remove succeed
            totalSize -= file.m_size;
            removed = true;
            archives.erase(itr++);
        }

        if (anyOverdue && !removed && !reading && now - file.m_createTime / 1000 > expireSeconds) {
            // invoker of openReader may deleted file
            __REMOVE_FILE(file);
            removed = true;
            archives.erase(itr++);
        }
        if (!removed) {
            ++itr;
        }
    }

    while (archives.size() > DEFAULT_TOTAL_ARCHIVE_NUM_LIMIT) {
        FileStat f = archives.front(); // remove oldest first
        __REMOVE_FILE(f);
        archives.pop_front();
    }
}

GlogReader *Glog::openReader(const string &archiveFile, const string *serverPrivateKey) {
    {
        SCOPED_LOCK(m_readingArchivesLock);

        m_readingArchives.emplace(archiveFile);
    }
    return new GlogReader(archiveFile, m_protoName, serverPrivateKey);
}

void Glog::closeReader(GlogReader *reader) {
    {
        SCOPED_LOCK(m_readingArchivesLock);

        m_readingArchives.erase(reader->m_file);
    }

    delete reader;
}

void Glog::removeArchiveFile(const string &file, bool removeReadingFile) {
    auto removeFile = [](const string &file) {
        if (::remove(file.c_str()) < 0) {
            InternalError("fail to remove file [%s] %s", file.c_str(), ::strerror(errno));
        } else {
            InternalDebug("remove file [%s]", file.c_str());
        }
    };

    if (!removeReadingFile) {
        SCOPED_LOCK(m_readingArchivesLock);

        bool reading = m_readingArchives.find(file) != m_readingArchives.end();
        if (!reading) {
            removeFile(file);
        }
    } else {
        removeFile(file);
    }
}

void Glog::flush() {
    if (m_async) {
        auto flushFinishCond = std::make_shared<ConditionVariable>();
        auto flushFinished = std::make_shared<bool>(false);

        m_writeQueue->enqueue(Message([this, flushFinishCond, flushFinished]() {
            SCOPED_LOCK(m_archiveLock);

            internalFlush();
            *flushFinished = true;
            flushFinishCond->signal();
        }));
        {
            SCOPED_LOCK(m_archiveLock);

            if (!*flushFinished) {
                bool timeout = !flushFinishCond->awaitTimeout(*m_archiveLock, FLUSH_AWAIT_TIMEOUT_MILLIS);
                if (timeout) {
                    InternalWarning("flush timeout after [%ld] ms", FLUSH_AWAIT_TIMEOUT_MILLIS);
                }
            }
        }
    } else {
        SCOPED_LOCK(m_syncWriteLock);

        bool ret = internalFlush();
        InternalDebug("flush in current thread, result:%d", ret);
    }
}

// directory/[protoName].glogmmap
string makeCacheFilePath(const string &protoName, const string &directory) {
    const size_t fixedLength = protoName.size() + directory.size() + strlen(CACHE_FILE_SUFFIX) + 16;
    char buffer[fixedLength];
    const string format = "%s%s%s";
    snprintf(buffer, fixedLength, format.c_str(), directory.c_str(), protoName.c_str(), CACHE_FILE_SUFFIX);
    return string(buffer);
}

string makeArchiveFilePath(const string &protoName, const string &directory) {
    WallTime_t now = wallTimeNow();
    auto nowStamp = static_cast<time_t>(now);
    ::tm tmTime{};
    //    ::gmtime_r(&nowStamp, &tmTime); // utc time
    ::localtime_r(&nowStamp, &tmTime); // local time
    int year = 1900 + tmTime.tm_year;
    int mon = 1 + tmTime.tm_mon;
    int day = tmTime.tm_mday;
    int hour = tmTime.tm_hour;
    int min = tmTime.tm_min;
    int sec = tmTime.tm_sec;
    int millis = static_cast<int>((now - nowStamp) * 1000);

    const size_t fixedLength = protoName.size() + directory.size() + 32;
    char buffer[fixedLength];
    snprintf(buffer, fixedLength, "%d%02d%02d%02d%02d%02d%03d", year, mon, day, hour, min, sec, millis);
    string timeStamp(buffer);

    // directory/[protoName]-yyyyMMddHHmmssSSS.glog
    const string format = "%s%s-%s%s";
    snprintf(buffer, fixedLength, format.c_str(), directory.c_str(), protoName.c_str(), timeStamp.c_str(),
             ARCHIVE_FILE_SUFFIX);
    return string(buffer);
}

size_t calculateRoundCacheSize() {
    // ensure cache size is multiple of page size
    // usually = 128 KB
    return 32 * SYS_PAGE_SIZE;
}

template <typename T>
void computePartialMatchTable(const T match[], int outTable[], int size) {
    outTable[0] = -1;
    int i = 0, j = -1;

    while (i < size - 1) {
        if (j == -1 || match[i] == match[j]) {
            i++;
            j++;
            if (match[i] != match[j]) {
                outTable[i] = j;
            } else {
                outTable[i] = outTable[j];
            }
        } else {
            j = outTable[j];
        }
    }
}

bool Glog::tryFlushAcrossDay() {
    InternalDebug("start check if should flush cache to corresponding log file, proto:%s", m_protoName.c_str());
    if (!m_incrementalArchive) { // only incremental archive may need flush to corresponding file
        return false;
    }
    bool needFlush = false;
    int64_t cacheCreateMs = 0;
    if (!getFileCreateTime(m_cacheFile->getPath(), cacheCreateMs)) {
        InternalError("fail to get create time of file [%s]", m_cacheFile->getPath().c_str());
    } else {
        time_t cacheCreateSec = cacheCreateMs * 0.001;
        ::tm cacheCreateTm{};
        ::tm nowTm{};
        ::localtime_r(&cacheCreateSec, &cacheCreateTm);
        tmNow(&nowTm);

        // no need to flush cache which created today
        needFlush = !(nowTm.tm_year == cacheCreateTm.tm_year && nowTm.tm_mon == cacheCreateTm.tm_mon &&
                      nowTm.tm_mday == cacheCreateTm.tm_mday);

        // if there is no data in cache file which is not created today,
        // still need flush for the purpose of update cache file's creation time
        //            if (needFlush) {
        //                if (!m_cacheFile->getTotalLogNum()) { // no log in cache
        //                    needFlush = false;
        //                }
        //            }
    }
    if (needFlush) {
        InternalDebug("flush cache to correct day");
        m_cacheFile->msync();
        uint8_t maxDepth = MAX_RECURSION_DEPTH;
        return appendLogFile(maxDepth, static_cast<time_t>(cacheCreateMs * 0.001));
    }
    return false;
}
} // namespace glog
