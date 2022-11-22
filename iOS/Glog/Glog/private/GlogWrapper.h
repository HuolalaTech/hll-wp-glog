//
//  GlogWrapper.h
//  glog
//
//  Created by peng on 2021/2/18.
//

#import <Foundation/Foundation.h>
#import "GlogConfigManager.h"

typedef enum : NSInteger {
    ConfigLogLevel_Debug = 0, // not available for release/product build
    ConfigLogLevel_Info = 1,  // default level
    ConfigLogLevel_Warning,
    ConfigLogLevel_Error,
    ConfigLogLevel_None, // special level used to disable all log messages
} ConfigLogLevel;

typedef enum : NSUInteger {
    CXXGlogType_OnLine = 0,
    CXXGlogType_OffLine = 1,
    CXXGlogType_Performance = 2,
} CXXGLogType;

typedef void(^GlogRemoveBlock)(NSMutableArray * _Nullable dataArray);
typedef void(^GlogSanpshotBlock)(NSString * _Nullable snapshotStr);

NS_ASSUME_NONNULL_BEGIN

@interface GlogWrapper : NSObject

+ (void)glogInitialize:(ConfigLogLevel )level;
#pragma mark ---------write read-------------------

+ (BOOL )glogWrite:(NSData *)data andGlog:(CXXGLogType )glogType;
+ (NSMutableArray *)glogReadOneBuffer:(CXXGLogType )glogType;
+ (NSMutableArray *)glogReadAllBuffer:(CXXGLogType )glogType;
+ (NSMutableArray *)glogReadCount:(NSInteger )count
                          andGlog:(CXXGLogType )glogType;
+ (NSMutableArray *)glogReadMemory:(NSInteger )memory
                           andGlog:(CXXGLogType )glogType;
+ (NSMutableArray *)glogUpload:(CXXGLogType )glogType
                 snapshotBlock:(GlogSanpshotBlock)snapshotStrBlock
               andRemoveHandel:(GlogRemoveBlock )removeHandel;
+ (void)resetExpireSeconds:(int32_t )seconds
                   andGlog:(CXXGLogType)glogType;
+ (NSArray *)getLogsPath:(NSInteger )epochSeconds;
@end

NS_ASSUME_NONNULL_END
