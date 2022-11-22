//
//  GlogConfig.h
//  Glog
//
//  Created by peng on 2022/5/15.
//

#import <Foundation/Foundation.h>
#import "GlogHandler.h"

NS_ASSUME_NONNULL_BEGIN

@interface GlogConfig : NSObject

+ (nullable instancetype) defaultConfig;

@property(nullable, nonatomic, strong) NSString *glogName;

@property(nullable, nonatomic, strong) NSString *rootDirectory;

@property(nonatomic, assign) bool incrementalArchive;

@property(nonatomic, assign) bool async;

@property(nonatomic, assign) int32_t expireSeconds;

@property(nonatomic, assign) size_t totalArchiveSizeLimit;

@property(nonatomic) GlogCompressMode compressMode;

@property(nonatomic) GlogEncryptMode encryptMode;

@property(nullable, nonatomic, strong) NSString *serverPublicKey;

@property(nonatomic) GlogInternalLogLevel logLevel;

@end

NS_ASSUME_NONNULL_END
