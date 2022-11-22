//
// Created by issac on 2021/1/11.
//

#ifndef CORE__GLOG_H_
#define CORE__GLOG_H_

#include "GlogPredef.h"
#include <chrono>
#include <list>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace std;

namespace glog {
class Glog;
class GlogFile;
class GlogReader;
class GlogBuffer;
class Compressor;
class ThreadLock;
class ConditionVariable;
class MessageQueue;

extern size_t SYS_PAGE_SIZE;
extern Endianness SYS_ENDIAN;
extern unordered_map<string, Glog *> *g_instanceMap; // guide by g_instanceMutex
extern recursive_mutex *g_instanceMutex;
extern int g_forwardPartialMatchTable[format::SYNC_MARKER_LENGTH];
extern int g_backwardPartialMatchTable[format::SYNC_MARKER_LENGTH];
extern pthread_t *g_daemon;
extern ThreadLock *g_daemonPollLock;
extern ConditionVariable *g_daemonPollCond;
extern atomic_bool g_daemonRunning;

extern string makeCacheFilePath(const string &protoName, const string &directory);
extern string makeArchiveFilePath(const string &protoName, const string &directory);
extern size_t calculateRoundCacheSize();
extern bool
findLogFileOfDate(const string &rootDirectory, string &outFilename, const string &protoName, time_t epochSeconds);
template <typename T>
extern void computePartialMatchTable(const T match[], int outTable[], int size);

class Glog {
public:
    /**
     * MUST call initialize before any call of instanceWithProto
     * @param logLevel glog internal log level, JUST for glog internal use.
     */
    static void initialize(InternalLogLevel logLevel);

    /**
     * Create new Glog instance if necessary. Glog will maintain a internal map (key - protoName, value - glog instance) as holder.
     *
     * GlogConfig:
     *
     * protoName: proto name to identify Glog instance, will be part of file name.
     * rootDirectory: file root directory, Glog will create one cache file and multi archive files in it.
     * incrementalArchive: true - will flush cache to log archive of today, false - flush will just rename current cache to a new archive file and recreate cache
     * async: write file in async mode or sync mode. Write in sync mode will block your code execution, async mode will put every write action into a FIFO queue.
     * expireSeconds: file expires (now  - create time > expire seconds) will be deleted.
     * totalArchiveSizeLimit: total archive files size limit in bytes.
     * compressMode: current version (GlogStoreIVVersion) only support Zlib
     * encryptMode: current version (GlogStoreIVVersion) only support AES CBC-128
     * serverPublicKey: server public key for ECDH
     *
     * @return new Glog instance if not exists in internal map
     */
    static Glog *maybeCreateWithConfig(const GlogConfig &config);

    /**
     * Get Glog instance with proto name, may be null if not createWithConfig with corresponding proto name
     * @param protoName
     * @return
     */
    static Glog *getInstanceWithProto(const string &protoName);

    bool write(const GlogBuffer &log) { return m_async ? writeAsync(log) : writeSync(log); }

    /**
     * flush log in cache to archive file. Operation will BLOCK current thread.
     */
    void flush();

    static void destroy(const string &protoName);

    /**
     * destroy ALL glog instance
     */
    static void destroyAll();

    /**
     * redirect glog internal log for custom usage.
     */
    static void registerLogHandler(LogHandler handler);
    static void unRegisterLogHandler();

    /**
     * take snapshot of archives, archive mmap cache if necessary. Operation will BLOCK current thread.
     *
     * @param archives fill it with archive files
     * @param condition archive if one of the condition match
     * @param order archives file order
     *
     * @return status message
     */
    string getArchiveSnapshot(vector<string> &archives,
                              const ArchiveCondition &condition,
                              FileOrder order = FileOrder::CreateTimeDescending);

    /**
     * open archive file for reading, MUST call closeReader after read.
     */
    GlogReader *openReader(const string &archiveFile, const string *serverPrivateKey = nullptr);
    void closeReader(GlogReader *reader);

    /**
     * reset expire seconds, clean archives if new value < old value, return true if do clean
     */
    bool resetExpireSeconds(int32_t seconds) {
        if (seconds == m_expireSeconds) {
            return false;
        }
        int32_t oldValue = m_expireSeconds;
        m_expireSeconds = seconds;
        if (seconds < oldValue) {
            cleanupSafely();
            return true;
        }
        return false;
    }

    /**
     * get mmap cache size
     */
    size_t getCacheSize() const { return m_cacheSize; }

    /**
     * remove all archive files, if removeReadingFiles = true, remove files which is being read.
     * if reloadCache = true, remove the log content in cache file.
     */
    void removeAll(bool removeReadingFiles = false, bool reloadCache = false);

    /**
     * just remove file, be careful if multi reader is reading this file, or this file is being incremental archive
     */
    void removeArchiveFile(const string &file, bool removeReadingFile = false);

    string getCacheFileName() const;

    void getArchivesOfDate(time_t epochSeconds, vector<string> &filePaths);

    bool tryFlushAcrossDay();

    /**
     * check across day every write
     */
    void setCheckAcrossDay(bool check) {
        if (m_shouldTryFlushAcrossDay != check) {
            m_shouldTryFlushAcrossDay = check;
        }
    }

    /**
     * 1. if total size of archive files exceed limit, delete oldest files until total size less than limit.
     * 2. delete overdue files
     * 3. delete until file number < limit
     *
     * this action will NOT delete files which is being read
    */
    void cleanupSafely();

    bool incrementalArchive() const { return m_incrementalArchive; }

    bool async() const { return m_async; }

    // just forbid it for possibly misuse
    explicit Glog(const Glog &other) = delete;
    Glog &operator=(const Glog &other) = delete;

private:
    Glog(string protoName,
         const string &rootDirectory,
         bool async,
         int32_t expireSeconds,
         size_t totalArchiveSizeLimit,
         format::GlogCompressMode compressMode,
         format::GlogEncryptMode encryptMode,
         bool incrementalArchive,
         const string *serverPublicKey);

    ~Glog();

    const string m_protoName;
    string m_rootDirectory;
    size_t m_cacheSize;
    atomic_int32_t m_expireSeconds;
    const size_t m_totalArchiveSizeLimit;
    unordered_set<string> m_readingArchives; // guide by m_readingArchivesLock
    const format::GlogCompressMode m_compressMode;
    const format::GlogEncryptMode m_encryptMode;
    GlogFile *m_cacheFile;
    MessageQueue *m_writeQueue;
    Compressor *m_compressor;
    ThreadLock *m_archiveLock;
    ThreadLock *m_syncWriteLock;
    atomic_bool m_async;
    ThreadLock *m_readingArchivesLock;
    atomic_bool m_incrementalArchive;
    regex m_archiveNameRegex;
    atomic_bool m_shouldTryFlushAcrossDay;

    bool writeSync(const GlogBuffer &log);
    bool writeAsync(const GlogBuffer &log);

    bool internalFlush();

    /**
     * flush cache to log file of specified date,
     * remove and recreate cache file no matter it contains data or not,
     * if there is no data in cache, create a empty log file only contains header
     */
    bool appendLogFile(uint8_t &maxRecursionDepth, time_t epochSeconds);

    /**
     * collect archives with current proto name, may limit the file numbers for performance
     */
    void collectArchives(list<FileStat> &archives, FileOrder order, size_t &totalSize);

    void doRemoveAll(bool removeReadingFiles, bool reloadCache);

    static void makeArchiveNameRegex(bool incrementalArchive, const string &protoName, regex &outRegex);
};
} // namespace glog
#endif //CORE__GLOG_H_
