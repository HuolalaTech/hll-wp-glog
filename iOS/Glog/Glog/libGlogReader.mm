//
//  GlogReader.m
//  Glog
//
//  Created by peng on 2022/5/17.
//

#import "GlogReader.h"
#import <GlogCore/GlogReader.h>
#import <GlogCore/Glog.h>
#import "Glog.h"

@interface GlogReader()

@property(nonatomic) glog::GlogReader *m_reader;
@property(nonatomic) glog::Glog *m_glog;

@end

@implementation GlogReader

- (instancetype)initWithGlog:(Glog *)glog archiveFile:(NSString *)archiveFile key:(NSString *)key {
    if (self = [super init]) {
        if (glog && glog.m_glog) {
            std::string priKey = key ? string{[key UTF8String]} : "";
            self.m_glog = (glog::Glog *)glog.m_glog;
            self.m_reader = self.m_glog->openReader([archiveFile UTF8String], &priKey);
        }
    }
    return self;
}

- (void)dealloc {
    [self close];
}

- (void)close {
    if (self.m_glog && self.m_reader) {
        self.m_glog->closeReader(self.m_reader);
    }
}

- (NSInteger )read:(NSData **)data{
    if (!self.m_reader) {
        NSLog(@"fail to open reader");
        return -1;
    }
    glog::GlogBuffer outBuf(glog::SINGLE_LOG_CONTENT_MAX_LENGTH);
    NSInteger length = self.m_reader->read(outBuf);
    if (length <= 0) {
        return length;
    }
    *data = [NSData dataWithBytes:outBuf.getPtr() length:length];
    return length;
}

@end
