//
//  LoganHelper.m
//  Benchmark
//
//  Created by 曾梓峰 on 2022/1/14.
//

#import <Foundation/Foundation.h>
#import "LoganHelper.h"

@implementation LoganHelper

+ (void)doInitLogan{
    NSData *keydata = [@"0123456789012345" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *ivdata = [@"0123456789012345" dataUsingEncoding:NSUTF8StringEncoding];
    uint64_t file_max = 10 * 1024 * 1024;
    // logan init，incoming 16-bit key，16-bit iv，largest written to the file size(byte)
    loganInit(keydata, ivdata, file_max);

    //#if DEBUG
    //loganUseASL(YES);
    //#endif
}

+ (void)write:(NSString *)log {
    logan(1, log);
}

+ (void)getLogFile:(LoganFilePathBlock)block {
    loganUploadFilePath(loganTodaysDate(), block);
}

@end
