//
//  GlogReader.m
//  Glog
//
//  Created by peng on 2022/5/17.
//

#import "GlogReader.h"
#import <GlogCore/GlogReader.h>
#import <GlogCore/Glog.h>

@interface GlogReader()

@property(nonatomic, strong)NSString *archiveFile;

@property(nonatomic) glog::GlogReader *reader;

@end

@implementation GlogReader
+ (GlogReader *)initialize:(NSString *)glogName archiveFile:(NSString *)archiveFile{
    GlogReader *reader = [[GlogReader alloc]init];
    [reader openReader:glogName archiveFile:archiveFile];
    return reader;
}
- (void)openReader:(NSString *)glogName archiveFile:(NSString *)archiveFile{
    glog::Glog *glog = [self getGlog:glogName];
    self.reader = glog->openReader([archiveFile UTF8String]);
}

- (void)closeReader:(NSString *)glogName{
    glog::Glog *glog = [self getGlog:glogName];
    glog->closeReader(self.reader);
    self.reader = nil;
}

- (glog::Glog *)getGlog:(NSString *)glogName{
    std::unique_lock<std::recursive_mutex> lock(*glog::g_instanceMutex);
    auto itr = glog::g_instanceMap->find([glogName UTF8String]);
    if (itr != glog::g_instanceMap->end()) {
        glog::Glog *glog = itr->second;
        return glog;
    }else{
        return nil;
    }
}

- (NSInteger )read:(NSData **)data{
    if (!self.reader) {
        NSLog(@"read error");
        return -1;
    }
    glog::GlogBuffer outBuf(glog::SINGLE_LOG_CONTENT_MAX_LENGTH);
    NSInteger length = self.reader->read(outBuf);
    if (length <= 0) {
        return length;
    }
    *data = [NSData dataWithBytes:outBuf.getPtr() length:length];
    return length;
}

@end
