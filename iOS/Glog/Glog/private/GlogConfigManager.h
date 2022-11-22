//
//  GlogConfigManager.h
//  glog
//
//  Created by peng on 2021/2/3.
// Version 1.1.0

#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN

@interface GlogConfigManager : NSObject

+ (instancetype)shareInstance;

@property(nonatomic) BOOL isOpenLog;
@property(nonatomic) bool isAsync;
@property(nonatomic) BOOL isCompress;
@property(nonatomic) BOOL isFirstUpload;
@property(nonatomic)NSInteger suggestCacheSize;
@property(nonatomic)int32_t expireSeconds;
@property(nonatomic)int32_t expireSecondsOffLine;
@property(nonatomic) NSInteger totalArchiveSizeLimit;
@property(nonatomic) NSInteger offLineTotalArchiveSizeLimit;
@property(nonatomic) unsigned long archiveSize;
@property(nonatomic) unsigned long performanceGlogArchiveSize;
@property(nonatomic,strong) NSString *glogVersion;

@end

NS_ASSUME_NONNULL_END
