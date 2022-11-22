//
//  Glog.h
//  Glog
//
//  Created by peng on 2022/5/15.
//

#import <Foundation/Foundation.h>
#import "GlogHandler.h"
#import "GlogConfig.h"
#import "GlogReader.h"

NS_ASSUME_NONNULL_BEGIN

@interface Glog : NSObject

+ (nullable instancetype)defaultGlog;

+ (nullable instancetype)initialize:(GlogConfig *)config;

+ (nullable instancetype)initializeGlog:(nullable NSString *)glogName;

+ (nullable instancetype)initializeGlog:(nullable NSString *)glogName rootDir:(nullable NSString *)rootDir;

- (bool)write:(NSData *)data;

- (bool)writeString:(NSString *)dataString;

- (bool)writeDictionary:(NSDictionary *)dataDictionary;

- (NSArray *)readArchiveFile:(NSString *)archiveFile;

- (void)removeArchiveFile:(NSString *)archiveFile;

- (void)removeAll;

- (void)resetExpireSeconds:(int32_t )expireSeconds;

- (NSArray *)getArchiveSnapshot;

- (NSArray *)getArchiveSnapshot:(bool )flush minLogNum:(size_t )minLogNum totalLogSize:(size_t)totalLogSize;

- (NSArray *)getArchivesOfDate:(time_t )epochSeconds;

- (size_t)getCacheSize;

- (void)flush;

- (void)destroy;

- (GlogReader *)openReader:(NSString *)archiveFile;

- (void)closeReader:(GlogReader *)reader;

+ (void)destroyAll;

@end

NS_ASSUME_NONNULL_END
