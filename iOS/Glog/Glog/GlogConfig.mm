//
//  GlogConfig.m
//  Glog
//
//  Created by peng on 2022/5/15.
//

#import "GlogConfig.h"

#define defaultRootDic @"glogDefaults"

@implementation GlogConfig

+ (instancetype)defaultConfig{
    GlogConfig *glogConfig = [[GlogConfig alloc]init];
    glogConfig.glogName = defaultRootDic;
    glogConfig.rootDirectory = [self getRootDirectory:@"glogDefault"];
    glogConfig.incrementalArchive = false;
    glogConfig.async = false;
    glogConfig.expireSeconds = 7 * 24 * 60 * 60;//默认7天
    glogConfig.totalArchiveSizeLimit = 16 * 1024 *1024;//默认16 M
    glogConfig.compressMode = GlogCompressMode_None;
    glogConfig.encryptMode = GlogEncryptMode_None;
    glogConfig.serverPublicKey = nil;
    glogConfig.logLevel = GlogInternalLog_Info;
    return glogConfig;
}
+ (NSString *)getRootDirectory:(NSString *)glogName{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSString *rootDir = (NSString *) [paths firstObject];
    NSLog(@"%@",rootDir);
    return rootDir;
}

@end
