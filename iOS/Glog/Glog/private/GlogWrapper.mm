//
//  GlogWrapper.m
//  glog
//
//  Created by peng on 2021/2/18.
//

#import "GlogWrapper.h"
#import "zlib.h"
#import <GlogCore/GlogPredef.h>
#import <GlogCore/Glog.h>
#import <GlogCore/GlogBuffer.h>
#import <GlogCore/InternalLog.h>
#import <GlogCore/GlogReader.h>

using namespace glog;

#define GLOG_CONFIG [GlogConfigManager shareInstance]
#define GlogManager [GlogWrapper shareInstance]

@interface GlogWrapper()

@property(nonatomic) glog::Glog *onlineGlog;
@property(nonatomic) glog::Glog *offlineGlog;
@property(nonatomic) glog::Glog *performanceGlog;

@end

@implementation GlogWrapper

static GlogWrapper* single = nil;

+ (instancetype)shareInstance
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        single = [[GlogWrapper alloc] init];
    });
    return single;
}



void glogCall(glog::InternalLogLevel level, const char *file, int line, const char *function, const std::string &message){
    if ([GlogConfigManager shareInstance].isOpenLog) {
//        NSLog(@"call level = %d , file = %c , message = %s",level,file,message);
        
    }
}

+ (void)glogInitialize:(ConfigLogLevel )level{
    Glog::initialize(InternalLogLevelInfo);
    GlogManager.onlineGlog = [self glogInstanceWithProto:@"onlineGlog"];
    GlogManager.offlineGlog = [self glogInstanceWithProto:@"offlineGlog"];
    GlogManager.performanceGlog = [self glogInstanceWithProto:@"performanceGlog"];
}

+ (glog::Glog *)glogInstanceWithProto:(NSString *)protoName{
    NSAssert(protoName != nil && [protoName isKindOfClass:[NSString class]], @"protoName Can't be empty");
    NSString *rootPath = [self createFile:protoName];
    if (GLOG_CONFIG.isOpenLog) {
        NSLog(@"protoname = %@ \n async = %d , suggestCacheSize = %ld  expireSeconds = %d totalArchiveSizeLimit = %ld",rootPath,GLOG_CONFIG.isAsync,(long)GLOG_CONFIG.suggestCacheSize,GLOG_CONFIG.expireSeconds,(long)GLOG_CONFIG.totalArchiveSizeLimit);
    }
    std::string logPath=  [rootPath UTF8String];
    std::string glogName = [protoName UTF8String];
    bool isoffline = [protoName isEqualToString:@"offlineGlog"];
    int32_t expireSeconds = isoffline ? GLOG_CONFIG.expireSecondsOffLine : GLOG_CONFIG.expireSeconds;
    GlogConfig config{
                .m_protoName = glogName,
                .m_rootDirectory = logPath,
                .m_incrementalArchive = isoffline,
                .m_async = GLOG_CONFIG.isAsync,
                .m_expireSeconds = expireSeconds,
                .m_totalArchiveSizeLimit = static_cast<size_t>(isoffline ? GLOG_CONFIG.offLineTotalArchiveSizeLimit : GLOG_CONFIG.totalArchiveSizeLimit),
                .m_compressMode = GLOG_CONFIG.isCompress ? format::GlogCompressMode::Zlib : format::GlogCompressMode::None,
                .m_encryptMode = format::GlogEncryptMode::None,
                .m_serverPublicKey = nullptr
    };
    return Glog::instanceWithProto(config);
}

+ (NSString *)createFile:(NSString *)filePath{
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSString *libraryPath = (NSString *) [paths firstObject];
    NSString *rootDir = [libraryPath stringByAppendingPathComponent:filePath];
    return rootDir;
}

+ (glog::Glog *)getTypeGlog:(CXXGLogType )glogType{
    Glog* resGlog = GlogManager.offlineGlog;
    switch (glogType) {
        case CXXGlogType_OnLine:
            resGlog = GlogManager.onlineGlog;
            break;
            
        case CXXGlogType_OffLine:
            resGlog = GlogManager.offlineGlog;
            break;

        case CXXGlogType_Performance:
            resGlog = GlogManager.performanceGlog;
            break;

        default:
            break;
    }
    
    return resGlog;
}

+ (BOOL )glogWrite:(NSData *)data andGlog:(CXXGLogType )glogType{
    Glog* resGlog = [self getTypeGlog:glogType];
    if(resGlog == nil){
        NSLog(@"glog is nil");
        return NO;
    }
    GlogBuffer buffer((void *)[data bytes], data.length,true);
    return resGlog->write(buffer);
}

+ (vector<string> )getArchiveSnapshot:(glog::Glog *)glog
                              andGlog:(CXXGLogType )glogType
                       snapshotString:(NSString **)snapshotStr {
    vector<string> archiveFiles;
    unsigned long archiveSize = GLOG_CONFIG.archiveSize;
    if (GLOG_CONFIG.isFirstUpload && glogType == CXXGlogType_OnLine) {
        archiveSize = 0;
    }
    ArchiveCondition condition{true /*start archive*/,
                                SIZE_T_MAX,
                                archiveSize};
    string cString = glog->getArchiveSnapshot(archiveFiles, condition);
    *snapshotStr = [NSString stringWithUTF8String:cString.c_str()];
    return archiveFiles;
}


+ (vector<string> )getArchiveSnapshotPerformance:(glog::Glog *)glog {
    vector<string> archiveFiles;
    ArchiveCondition condition{true /*start archive*/,
                                SIZE_T_MAX,
                                GLOG_CONFIG.performanceGlogArchiveSize};
    glog->getArchiveSnapshot(archiveFiles, condition);
    return archiveFiles;
}

+ (NSMutableArray *)glogReadOneBuffer:(CXXGLogType )glogType{
    return [self glogReadCount:1 andGlog:glogType];
}
+ (NSMutableArray *)glogReadAllBuffer:(CXXGLogType )glogType{
   return [self glogReadCount:0 andGlog:glogType];
}
+ (NSMutableArray *)glogReadCount:(NSInteger )count andGlog:(CXXGLogType )glogType{
    Glog *glog = [self getTypeGlog:glogType];
    if(glog == nil){
        NSLog(@"glog is nil");
        return [NSMutableArray new];
    }
    BOOL isCount = !(count == 0);
    NSMutableArray *muArr = [NSMutableArray array];
    NSString *str = @"";
    vector<string> archiveFiles = [self getArchiveSnapshot:glog andGlog:glogType snapshotString:&str];
    for(auto &file:archiveFiles) {
        NSString *fileString = [NSString stringWithUTF8String:file.c_str()];
        GlogReader *reader = glog->openReader(file);
        int n = 0;
        GlogBuffer outBuf(8 * 1024);
        NSMutableArray *mArr = [NSMutableArray new];
        do {
            n = reader->read(outBuf);
            if (n==0) {
                continue;
            }else if (n < 0){
                break;
            }
            NSData *resData = [NSData dataWithBytes:outBuf.getPtr() length:n];
            if (n > 0) {
                [mArr addObject:resData];
            }
            if (isCount) {
                count -- ;
                if (count == 0) {
                    [muArr addObject:mArr];
                    return muArr;
                }
            }
        } while (n >= 0);
        glog->closeReader(reader);
        NSFileManager *fileManager = [NSFileManager defaultManager];
        [fileManager removeItemAtPath:fileString error:nil];
        [muArr addObject:mArr];
      
    }
    return muArr;
}

+ (NSMutableArray *)glogReadMemory:(NSInteger )memory andGlog:(CXXGLogType )glogType{
    Glog *glog = [self getTypeGlog:glogType];
    if(glog == nil){
        NSLog(@"glog is nil");
        return [NSMutableArray new];
    }
    BOOL isMemory = !(memory == 0);
    NSInteger memorySize = 0;
    NSMutableArray *muArr = [NSMutableArray array];
    NSString *str = @"";
    vector<string> archiveFiles = [self getArchiveSnapshot:glog andGlog:glogType snapshotString:&str];
    for(auto &file:archiveFiles) {
        NSString *fileString = [NSString stringWithUTF8String:file.c_str()];
        GlogReader *reader = glog->openReader(file);
        int n = 0;
        GlogBuffer outBuf(glog::SINGLE_LOG_CONTENT_MAX_LENGTH);
        NSMutableArray *mArr = [NSMutableArray new];
        do {
            n = reader->read(outBuf);
            if (n==0) {
                continue;
            }else if (n < 0){
                break;
            }
            NSData *resData = [NSData dataWithBytes:outBuf.getPtr() length:n];
            [mArr addObject:resData];
            if (isMemory) {
                memorySize += n;
                if (memorySize >= memory) {
                    [muArr addObject:mArr];
                    glog->closeReader(reader);
                    return muArr;
                }
            }
        } while (n >= 0);
        
        glog->closeReader(reader);
        NSFileManager *fileManager = [NSFileManager defaultManager];
        [fileManager removeItemAtPath:fileString error:nil];
        
        [muArr addObject:mArr];
    }
    return muArr;
}
+ (NSMutableArray *)glogUpload:(CXXGLogType )glogType
                 snapshotBlock:(GlogSanpshotBlock)snapshotStrBlock
               andRemoveHandel:(GlogRemoveBlock )removeHandel{
    Glog *glog = [self getTypeGlog:glogType];
    if(glog == nil){
        NSLog(@"glog is nil");
        return [NSMutableArray new];
    }
    NSInteger memory = GLOG_CONFIG.suggestCacheSize;
    BOOL isMemory = !(memory == 0);
    NSInteger memorySize = 0;
    NSMutableArray *totalArr = [NSMutableArray array];
    
    vector<string> archiveFiles;
    if (glogType == CXXGlogType_Performance) {
        archiveFiles = [self getArchiveSnapshotPerformance:glog];
    } else {
        NSString *snapshotStr = @"";
        archiveFiles = [self getArchiveSnapshot:glog andGlog:glogType snapshotString:&snapshotStr];
        if (snapshotStrBlock != nil && snapshotStr.length > 0) {
            snapshotStrBlock(snapshotStr);
        }
        if (glogType == CXXGlogType_OnLine && GLOG_CONFIG.isFirstUpload) {
            GLOG_CONFIG.isFirstUpload = NO;
        }
    }
    
    NSInteger totalCount = 0;
    NSMutableArray *filePathArr = [[NSMutableArray alloc]init];
    
    for(auto &file:archiveFiles) {
        NSString *fileString = [NSString stringWithUTF8String:file.c_str()];
        GlogReader *reader = glog->openReader(file);
        int n = 0;
        GlogBuffer outBuf(glog::SINGLE_LOG_CONTENT_MAX_LENGTH);
        NSMutableArray *fileArr = [NSMutableArray new];
        do {
            n = reader->read(outBuf);
            if (n == 0) {
                continue;
            }else if (n < 0){
                break;
            }
            NSData *resData = [NSData dataWithBytes:outBuf.getPtr() length:n];
            if (resData.length > 0) {
                [fileArr addObject:resData];
                totalCount ++;
            }
            if (isMemory) {
                memorySize += n;
                if (memorySize >= memory && n <= 0) {
                    [totalArr addObject:fileArr];
                    glog->closeReader(reader);
//                    NSFileManager *fileManager = [NSFileManager defaultManager];
//                    [fileManager removeItemAtPath:fileString error:nil];
                    [filePathArr addObject:fileString];
                    if ([GlogConfigManager shareInstance].isOpenLog) {
                        NSLog(@" totalmemory = %ld,n = %d, file = %@, totalCount = %ld fileCount = %lu",(long)memorySize,n ,fileString,(long)totalCount,(unsigned long)fileArr.count);
                    }
                    removeHandel(filePathArr);
                    return totalArr;
                }
            }
        } while (n > 0);
        glog->closeReader(reader);
        [filePathArr addObject:fileString];
        [totalArr addObject:fileArr];
    
    }
    
    if ([GlogConfigManager shareInstance].isOpenLog) {
        NSLog(@" totalmemory = %ldfileArr, totalCount = %ld",(long)memorySize ,(long)totalCount);
    }
    removeHandel(filePathArr);
    return totalArr;
}

+ (void)resetExpireSeconds:(int32_t )seconds
                   andGlog:(CXXGLogType )glogType {
    if(GlogManager.onlineGlog != nil && glogType == CXXGlogType_OnLine){
        GlogManager.onlineGlog->resetExpireSeconds(seconds);
    }
    if(GlogManager.offlineGlog != nil && glogType == CXXGlogType_OffLine){
        GlogManager.offlineGlog->resetExpireSeconds(seconds);
    }
    if(GlogManager.performanceGlog != nil && glogType == CXXGlogType_Performance){
        GlogManager.performanceGlog->resetExpireSeconds(seconds);
    }
}

+ (NSArray *)getLogsPath:(NSInteger )epochSeconds{
    if (GlogManager.offlineGlog == nil) {
        return [NSArray new];
    }
    GlogManager.offlineGlog->flush();
    NSMutableArray *pathArr = [NSMutableArray new];
    vector<string> archiveFiles;
    GlogManager.offlineGlog->getArchivesOfDate(epochSeconds, archiveFiles);
    for(auto &file:archiveFiles) {
        NSString *fileString = [NSString stringWithUTF8String:file.c_str()];
        [pathArr addObject:fileString];
    }
    return pathArr;
}
@end
