//
//  GlogConfigManager.m
//  glog
//
//  Created by peng on 2021/2/3.
//

#import "GlogConfigManager.h"
@implementation GlogConfigManager
static GlogConfigManager* single = nil;

+ (instancetype)shareInstance
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        single = [[GlogConfigManager alloc] init];
        single.isAsync = true;
        single.suggestCacheSize = 256 * 1024;
        single.expireSeconds = 7 * 24 * 60 * 60;
        single.expireSecondsOffLine = 15 * 24 * 60 * 60;
        single.totalArchiveSizeLimit = 16 * 1024 *1024;
        single.offLineTotalArchiveSizeLimit = 2 * 256 * 1024 * 1024;
        single.isCompress = true;
        single.archiveSize = 4 * 1024;
        single.glogVersion = @"3.0.0";
        single.performanceGlogArchiveSize = 1*1024;
        single.isFirstUpload = YES;
    });
    return single;
}

@end
