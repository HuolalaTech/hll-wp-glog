//
//  GlogBridge.h
//  Bridge
//
//  Created by 曾梓峰 on 2021/3/30.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, InternalLogLevel) {
    InternalLogLevelDebug = 0, // not available for release/product build
    InternalLogLevelInfo = 1,  // default level
    InternalLogLevelWarning = 2,
    InternalLogLevelError = 3,
    InternalLogLevelNone = 4 // special level used to disable all log messages
};

@interface Glog : NSObject

+ (void)initializeGlog:(InternalLogLevel)level;

+ (nullable instancetype)instanceWithProto:(nonnull NSString *)proto rootDirectory:(nonnull NSString *)rootDirectory async:(BOOL)async encrypt:(BOOL)encrypt;

- (bool)write:(nonnull NSData *)data;

- (NSString *_Nonnull)getCacheFileName;
@end
