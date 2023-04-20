//
//  Glog.m
//  Glog
//
//  Created by peng on 2022/5/15.
//

#import "Glog.h"
#import "zlib.h"
#import <GlogCore/GlogPredef.h>
#import <GlogCore/Glog.h>
#import <GlogCore/GlogBuffer.h>
#import <GlogCore/InternalLog.h>
#import <GlogCore/GlogReader.h>

static BOOL g_initialized = NO;
static NSMutableDictionary *g_instanceDic = nil;
static recursive_mutex *g_instanceMutex;

@interface Glog()

@property(nonatomic) NSString *m_protoName;

@end

@implementation Glog

#pragma mark ------------------- initialize -------------------

+ (void)initialize:(GlogInternalLogLevel)level {
    if (g_initialized) {
        return;
    }
    glog::Glog::initialize((glog::InternalLogLevel) level);

    g_instanceMutex = new recursive_mutex();
    g_instanceDic = [[NSMutableDictionary alloc] init];
    g_initialized = YES;
}

+ (instancetype)glogWithConfig:(GlogConfig *)config {
    assert(g_initialized);
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    Glog *gl = [g_instanceDic objectForKey:config.protoName];
    if (gl == nil) {
        gl = [[Glog alloc] initWithConfig:config];
        if (![gl m_glog]) {
            return nil;
        }
        [g_instanceDic setObject:gl forKey:config.protoName];
    }
    return gl;
}

#pragma mark ------------------- init -------------------

- (instancetype)initWithConfig:(GlogConfig *)config {
    if (self = [super init]) {
        std::string serverPubKey = config.serverPublicKey ? std::string([config.serverPublicKey UTF8String]) : "";
        glog::GlogConfig cfg {
            .m_protoName = [config.protoName UTF8String],
            .m_rootDirectory = [config.rootDirectory UTF8String],
            .m_incrementalArchive = config.incrementalArchive,
            .m_async = config.async,
            .m_expireSeconds = config.expireSeconds,
            .m_totalArchiveSizeLimit = config.totalArchiveSizeLimit,
            .m_compressMode = (glog::format::GlogCompressMode) config.compressMode,
            .m_encryptMode = (glog::format::GlogEncryptMode) config.encryptMode,
            .m_serverPublicKey = &serverPubKey
        };
        
        self.m_glog = glog::Glog::maybeCreateWithConfig(cfg);
    }
    return self;
}

#pragma mark ------------------- glog write -------------------

- (bool)write:(NSData *)data {
    if (!data) {
        return NO;
    }
    glog::GlogBuffer buffer((void *) [data bytes], data.length, true);
    return ((glog::Glog *)self.m_glog)->write(buffer);
}

#pragma mark ------------------- glog read -------------------

- (NSArray *)readArchiveFile:(NSString *)archiveFile key:(NSString *)key {
    @autoreleasepool {
        if ([Glog isBlankString:archiveFile]) {
            NSLog(@"archive file path cannot be empty");
            return [NSArray new];
        }
        std::string privKey = key ? string{[key UTF8String]} : "";
        glog::GlogReader *reader = ((glog::Glog *)self.m_glog)->openReader([archiveFile UTF8String], &privKey);
        NSInteger length = 0;
        glog::GlogBuffer outBuf(glog::SINGLE_LOG_CONTENT_MAX_LENGTH);
        NSMutableArray *dataArr = [NSMutableArray new];
        do {
            length = reader->read(outBuf);
            if (length == 0) {
                continue;
            } else if (length < 0) {
                break;
            }
            NSData *data = [NSData dataWithBytes:outBuf.getPtr() length:length];
            [dataArr addObject:data];
        } while (true);
        ((glog::Glog *)self.m_glog)->closeReader(reader);
        return dataArr;
    }
}

- (GlogReader *)openReader:(NSString *)archiveFile{
    return [self openReader:archiveFile key:@""];
}

- (GlogReader *)openReader:(NSString *)archiveFile key:(NSString *)key {
    return [[GlogReader alloc] initWithGlog:self archiveFile:archiveFile key:key];
}

#pragma mark ------------------- others -------------------

- (void)flush {
    ((glog::Glog *)self.m_glog)->flush();
}

- (void)resetExpireSeconds:(int32_t )expireSeconds{
    ((glog::Glog *)self.m_glog)->resetExpireSeconds(expireSeconds);
}

#pragma mark ------------------- remove archive -------------------

- (void)removeArchiveFile:(NSString *)archiveFile{
    if ([Glog isBlankString:archiveFile]) {
        NSLog(@"The file cannot be empty");
        return ;
    }
    ((glog::Glog *)self.m_glog)->removeArchiveFile([archiveFile UTF8String]);
}

- (void)removeAll{
    ((glog::Glog *)self.m_glog)->removeAll();
}

#pragma mark ------------------- get archive -------------------

- (NSArray *)getArchiveSnapshot{
    return [self getArchiveSnapshot:true minLogNum:0 totalLogSize:0];
}

- (NSArray *)getArchiveSnapshot:(bool )flush minLogNum:(size_t )minLogNum totalLogSize:(size_t)totalLogSize{
    vector<string> archiveFiles;
    glog::ArchiveCondition condition{flush,minLogNum,totalLogSize};
    ((glog::Glog *)self.m_glog)->getArchiveSnapshot(archiveFiles, condition,glog::FileOrder::CreateTimeAscending);
    return [self getArrayFromVector:archiveFiles];
}

- (NSArray *)getArchivesOfDate:(time_t)epochSeconds {
    NSMutableArray *archiveArr = [[NSMutableArray alloc] init];
    vector<string> archiveFiles;
    ((glog::Glog *)self.m_glog)->getArchivesOfDate(epochSeconds, archiveFiles);
    for (auto &archiveFile : archiveFiles) {
        NSString *filePath = [NSString stringWithUTF8String:archiveFile.c_str()];
        [archiveArr addObject:filePath];
    }
    return archiveArr;
}

- (size_t)getCacheSize {
    return ((glog::Glog *)self.m_glog)->getCacheSize();
}

#pragma mark ------------------- destroy -------------------

- (void)destroy {
    glog::Glog::destroy([self.m_protoName UTF8String]);
}

+ (void)destroyAll {
    glog::Glog::destroyAll();
}

#pragma mark ------------------- other -------------------

- (NSArray *)getArrayFromVector:(vector<string>)vector {
    NSMutableArray *array = [[NSMutableArray alloc] init];
    for (auto &item : vector) {
        NSString *value = [NSString stringWithUTF8String:item.c_str()];
        if ([value isKindOfClass:[NSString class]]) {
            [array addObject:value];
        }
    }
    return array;
}

+ (BOOL)isBlankString:(NSString *)string {
    if (string == nil || string == NULL || [string isKindOfClass:[NSNull class]]) {
        return YES;
    }
    if (![string isKindOfClass:[NSString class]]) {
        return YES;
    }
    if ([[string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length] == 0) {
        return YES;
    }
    return NO;
}

@end
