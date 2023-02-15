//
// Created by issac on 2021/1/20.
//

#ifndef CORE__UTILITIES_H_
#define CORE__UTILITIES_H_

#include "GlogPredef.h"
#include <cstdint>

#ifndef likely
#    define unlikely(x) (__builtin_expect(bool(x), 0))
#    define likely(x) (__builtin_expect(bool(x), 1))
#endif

using namespace std;

namespace glog {
typedef double WallTime_t;

/**
 * microseconds since the epoch
 */
int64_t cycleClockNow();

/**
 * seconds since the epoch
 */
WallTime_t wallTimeNow();

void tmNow(::tm *outTm);

/**
 * epoch millis of today's 00:00
 */
time_t midnightEpochSeconds();

uint16_t reverseU16(uint16_t value);

uint32_t reverseU32(uint32_t value);

uint16_t toLittleEndianU16(uint16_t value);

uint32_t toLittleEndianU32(uint32_t value);

uint16_t readU16Le(uint16_t value);

//void generateSync(uint8_t out[]);

bool getFileSize(int fd, size_t &size);

size_t getFileSize(const string &filepath);

size_t getPageSize();

bool isFileExists(const std::string &path);

bool mkPath(const string &path);

bool endsWithSplash(const string &path);

string filename(const string &path);

// list files in dir (not recursive)
bool listFilesInDir(const string &path, vector<string> &files);

bool isDirectory(const string &path);

// TODO in fact it's the file's last modified time
bool getFileCreateTime(const string &path, int64_t &epochMillis);

string hex2Str(const uint8_t *data, int len);

int char2int(char input);

bool str2Hex(const string &s, uint8_t *data);

int readSpecifySize(int fd, void *buf, size_t size);

bool closeFile(int fd, const char *filepath = nullptr);
} // namespace glog
#endif //CORE__UTILITIES_H_
