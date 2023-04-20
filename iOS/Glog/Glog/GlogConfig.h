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

@property(nonatomic) NSString *protoName;

@property(nonatomic) NSString *rootDirectory;

@property(nonatomic, assign) bool incrementalArchive;

@property(nonatomic, assign) bool async;

@property(nonatomic, assign) int32_t expireSeconds;

@property(nonatomic, assign) size_t totalArchiveSizeLimit;

@property(nonatomic) GlogCompressMode compressMode;

@property(nonatomic) GlogEncryptMode encryptMode;

@property(nonatomic) NSString *serverPublicKey;

- (instancetype)initWithProtoName:(NSString *)protoName;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
