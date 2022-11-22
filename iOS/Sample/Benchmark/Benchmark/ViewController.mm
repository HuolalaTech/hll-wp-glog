//
//  ViewController.m
//  Benchmark
//
//  Created by 曾梓峰 on 2021/4/12.
//

#import "LogUtil.h"
#import "ViewController.h"
#import <Bridge/GlogBridge.h>
#import "LoganHelper.h"

@interface ViewController ()

@end

@implementation ViewController
typedef NS_ENUM(NSUInteger, LogQuantity) {
    _10K = 10000,
    _100K = 100000
};

NSMutableArray<NSString *> *rawLogs10k = [NSMutableArray arrayWithCapacity:10000];
NSMutableArray<NSString *> *rawLogs100k = [NSMutableArray arrayWithCapacity:100000];
NSString *logDirectory = nil;

+ (NSString *)convertToString:(LogQuantity)quantity {
    NSString *result = nil;
    
    switch (quantity) {
        case _10K:
            result = @"10K";
            break;
        case _100K:
            result = @"100K";
            break;
        default:
            result = @"unknown";
    }
    
    return result;
}

#pragma mark - cocoa callback

typedef NS_ENUM(NSUInteger, ButtonType) {
    GlogSync10K = 0,
    GlogSync100K = 1,
    GlogAsync10K = 2,
    GlogAsync100K = 3,
    Xlog10K = 4,
    Xlog100K = 5,
    Cleanup = 6,
    Logan10K = 7,
    Logan100K = 8,
};

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self prepareRawLogs];
}

- (IBAction)onButtonClick:(id)sender {
    NSLog(@"=== onButtonClick, tag:%ld", (long) [sender tag]);
    switch ((NSUInteger)[sender tag]) {
        case GlogSync10K:
            [self glogWriteAsync:NO quantity:_10K];
            break;
        case GlogSync100K:
            [self glogWriteAsync:NO quantity:_100K];
            break;
        case GlogAsync10K:
            [self glogWriteAsync:YES quantity:_10K];
            break;
        case GlogAsync100K:
            [self glogWriteAsync:YES quantity:_100K];
            break;
        case Xlog10K:
            [self xlogWrite:_10K];
            break;
        case Xlog100K:
            [self xlogWrite:_100K];
            break;
        case Cleanup:
            [self cleanup];
            break;
        case Logan10K:
            [self loganWrite:_10K];
            break;
        case Logan100K:
            [self loganWrite:_100K];
            break;
    }
}

#pragma mark - prepare test data

- (NSMutableArray<NSString *> *)getLogsInBundle:(NSString *)name {
    NSString *filePath = [[NSBundle mainBundle] pathForResource:name ofType:@"log"];
    FILE *file = fopen([filePath UTF8String], [@"r" UTF8String]);
    size_t len;
    char *ptr;
    int num = 0;
    NSMutableArray<NSString *> *res = [NSMutableArray new];
    
    do {
        ptr = fgetln(file, &len); // including '\n'
        
        if (ptr) {
            char line[len];
            strncpy(line, ptr, len - 1); // ignore '\n' at last position
            line[len - 1] = '\0';
            [res addObject:[NSString stringWithFormat:@"%s", line]];
        }
        num++;
    } while (len > 0);
    
    unsigned long long fileSize = [[[NSFileManager defaultManager] attributesOfItemAtPath:filePath error:nil] fileSize];
    NSString *sizeString = [NSByteCountFormatter stringFromByteCount:fileSize countStyle:NSByteCountFormatterCountStyleFile];
    NSLog(@"read log in file:%@ finished, log num:%d, file size:%@", filePath, --num, sizeString);
    return res;
}

- (void)prepareRawLogs {
    rawLogs10k = [self getLogsInBundle:@"raw_10k"];
    rawLogs100k = [self getLogsInBundle:@"raw_100k"];
    
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentPath = [paths firstObject];
    
    if ([documentPath length] <= 0) {
        NSLog(@"=== documentPath length <= 0");
        return;
    }
    
    logDirectory = [documentPath stringByAppendingString:@"/wlog"];
    [self cleanup];
    NSLog(@"prepare finished, logDirectory:%@", logDirectory);
}

- (void)cleanup {
    NSArray<NSString *> *oldLogfiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:logDirectory error:nil];
    for (NSString *old : oldLogfiles) {
        BOOL removed = [[NSFileManager defaultManager] removeItemAtPath:[logDirectory stringByAppendingFormat:@"/%@", old] error:nil];
        if (!removed) {
            NSLog(@"=== remove file [%@] fail", [logDirectory stringByAppendingFormat:@"/%@", old]);
        }
    }
    NSLog(@"=== cleanup finished");
}

#pragma mark - glog write test

- (void)glogWriteAsync:(BOOL)async quantity:(LogQuantity)quantity {
    NSString *quantityDesc = [ViewController convertToString:quantity];
    NSString *protoName = [NSString stringWithFormat:@"glog-benchmark-%d-%@", async, quantityDesc];
    Glog *glog = [Glog instanceWithProto:protoName rootDirectory:logDirectory async:async encrypt:true];
    NSMutableArray<NSString*> *logs = quantity == _100K ? rawLogs100k : rawLogs10k;
    auto totalMillis = 0.0;
    
    NSLog(@"glog write start, file:%@", [glog getCacheFileName]);
    
    for (NSString *log in logs) {
        NSData *data = [log dataUsingEncoding:NSUTF8StringEncoding];
        NSDate *start = [NSDate date];
        [glog write:data];
        NSDate *end = [NSDate date];
        totalMillis += [end timeIntervalSinceDate:start] * 1000;
    }
    
    void (^printBlock)(void) = ^{
        NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:logDirectory error:nil];
        files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id item, NSDictionary *bindings) {
            return [item containsString:protoName];
        }]];
        long totalSize = 0;
        for (NSString *file in files) {
            totalSize += [[[NSFileManager defaultManager] attributesOfItemAtPath:[logDirectory stringByAppendingFormat:@"/%@", file] error:nil] fileSize];
        }
        NSLog(@"glog write finished, cost:%f millis, total size:%@", totalMillis, [NSByteCountFormatter stringFromByteCount:totalSize countStyle:NSByteCountFormatterCountStyleFile]);
    };
    if (async) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), dispatch_get_main_queue(), printBlock);
    } else {
        printBlock();
    }
}

- (void)xlogWrite:(LogQuantity)quantity {
    NSLog(@"xlog write start, quantity:%@", [ViewController convertToString:quantity]);
    
    auto totalMillis = 0.0;
    NSMutableArray *logs = quantity == _100K ? rawLogs100k : rawLogs10k;
    
    for (NSString *log in logs) {
//        NSData *data = [log dataUsingEncoding:NSUTF8StringEncoding];
        NSDate *start = [NSDate date];
        LOG_INFO("", log);
        NSDate *end = [NSDate date];
        totalMillis += [end timeIntervalSinceDate:start] * 1000;
    }
    
    void (^printBlock)(void) = ^{
        NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:logDirectory error:nil];
        files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id item, NSDictionary *bindings) {
            return [item containsString:@"X"];
        }]];
        long totalSize = 0;
        for (NSString *file in files) {
            totalSize += [[[NSFileManager defaultManager] attributesOfItemAtPath:[logDirectory stringByAppendingFormat:@"/%@", file] error:nil] fileSize];
        }
        NSLog(@"xlog write finished, cost:%f millis, total size:%@", totalMillis, [NSByteCountFormatter stringFromByteCount:totalSize countStyle:NSByteCountFormatterCountStyleFile]);
    };
    printBlock();
}

- (void)loganWrite:(LogQuantity)quantity{
    NSLog(@"logan write start, quantity:%@", [ViewController convertToString:quantity]);
    
    auto totalMillis = 0.0;
    NSMutableArray *logs = quantity == _100K ? rawLogs100k : rawLogs10k;
    
    for (NSString *log in logs) {
//        NSData *data = [log dataUsingEncoding:NSUTF8StringEncoding];
        NSDate *start = [NSDate date];
        [LoganHelper write:log];
        NSDate *end = [NSDate date];
        totalMillis += [end timeIntervalSinceDate:start] * 1000;
    }
    
    void (^printBlock)(void) = ^{
        [LoganHelper getLogFile:^(NSString *_Nullable filePath) {
            long totalSize = [[NSFileManager defaultManager] attributesOfItemAtPath:filePath error:nil].fileSize;
            NSLog(@"logan write finished, cost:%f millis, total size:%@", totalMillis, [NSByteCountFormatter stringFromByteCount:totalSize countStyle:NSByteCountFormatterCountStyleFile]);
        }];
    };
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), dispatch_get_main_queue(), printBlock);
}

@end
