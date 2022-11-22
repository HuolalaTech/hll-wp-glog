//
//  GlogReader.h
//  Glog
//
//  Created by peng on 2022/5/17.
//

#import <Foundation/Foundation.h>


NS_ASSUME_NONNULL_BEGIN

@interface GlogReader : NSObject

+ (GlogReader *)initialize:(NSString *)glogName archiveFile:(NSString *)archiveFile;

- (void)openReader:(NSString *)glogName archiveFile:(NSString *)archiveFile;

- (void)closeReader:(NSString *)glogName;

- (NSInteger )read:(NSData **)data;

@end

NS_ASSUME_NONNULL_END
