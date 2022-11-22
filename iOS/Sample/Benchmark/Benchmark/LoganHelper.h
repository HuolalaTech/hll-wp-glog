//
//  LoganHelper.m
//  Benchmark
//
//  Created by 曾梓峰 on 2022/1/14.
//

#import <Foundation/Foundation.h>
#import <Logan.h>

@interface LoganHelper : NSObject

+ (void)doInitLogan;

+ (void)write:(NSString *)log;

+ (void)getLogFile:(LoganFilePathBlock)block;

@end
