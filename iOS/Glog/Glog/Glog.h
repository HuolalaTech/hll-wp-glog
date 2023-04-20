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

@property(nonatomic) void *m_glog;

- (instancetype)init NS_UNAVAILABLE;

+ (void)initialize:(GlogInternalLogLevel)level;

+ (instancetype)glogWithConfig:(GlogConfig *)config;

- (bool)write:(NSData *)data;

- (NSArray *)readArchiveFile:(NSString *)archiveFile key:(NSString *)key;

- (void)removeArchiveFile:(NSString *)archiveFile;

- (void)removeAll;

- (void)resetExpireSeconds:(int32_t )expireSeconds;

- (NSArray *)getArchiveSnapshot;

/**
 * get archive files snapshot, if flush == true, either total log number in cache >= minLogNum or
 * total log size in cache >= totalLogSize will trigger flush.
 */
- (NSArray *)getArchiveSnapshot:(bool )flush minLogNum:(size_t )minLogNum totalLogSize:(size_t)totalLogSize;

- (NSArray *)getArchivesOfDate:(time_t )epochSeconds;

- (size_t)getCacheSize;

- (void)flush;

- (void)destroy;

- (GlogReader *)openReader:(NSString *)archiveFile;

- (GlogReader *)openReader:(NSString *)archiveFile key:(NSString*)key;

+ (void)destroyAll;

@end

NS_ASSUME_NONNULL_END
