//
//  GlogReader.h
//  Glog
//
//  Created by peng on 2022/5/17.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class Glog;

@interface GlogReader : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithGlog:(Glog *)glog archiveFile:(NSString *)archiveFile key:(NSString *)key;

- (NSInteger )read:(NSData **)data;

@end

NS_ASSUME_NONNULL_END
