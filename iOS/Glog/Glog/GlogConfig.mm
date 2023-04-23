//
//  GlogConfig.m
//  Glog
//
//  Created by peng on 2022/5/15.
//

#import "GlogConfig.h"

@implementation GlogConfig

- (instancetype)initWithProtoName:(NSString *)protoName {
    if (self = [super init]) {
        self.protoName = protoName;
        self.rootDirectory = [[GlogConfig documentDirectory] stringByAppendingString:@"/glog"];
        self.async = true;
        self.expireSeconds = 7 * 24 * 60 * 60;         // 7 day
        self.totalArchiveSizeLimit = 16 * 1024 * 1024; // 16 MB
        self.compressMode = GlogCompressMode_Zlib;
        self.incrementalArchive = false;
        self.encryptMode = GlogEncryptMode_None;
        self.serverPublicKey = @"";
    }
    return self;
}

+ (NSString *)documentDirectory {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSString *documentPath = (NSString *) [paths firstObject];
    return documentPath;
}

@end
