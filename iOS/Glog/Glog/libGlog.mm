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


@interface Glog()

@property(nonatomic) glog::Glog *glog;

@property(nonatomic, readwrite, strong) GlogConfig *config;

@property(nonatomic) glog::GlogReader *reader;

@end

@implementation Glog

#pragma mark ------------------- initialize -------------------

+ (instancetype)defaultGlog{
    return [self initialize:[GlogConfig defaultConfig]];
}

+ (nullable instancetype)initialize:(GlogConfig *)config{
    Glog *glog = [[Glog alloc]initWith:config];
    return glog;
}

+ (nullable instancetype)initializeGlog:(nullable NSString *)glogName{
    GlogConfig *config = [GlogConfig defaultConfig];
    config.glogName = glogName;
    return [self initialize:config];
}

+ (nullable instancetype)initializeGlog:(nullable NSString *)glogName rootDir:(nullable NSString *)rootDir{
    GlogConfig *config = [GlogConfig defaultConfig];
    config.glogName = glogName;
    config.rootDirectory = rootDir;
    return [self initialize:config];
}

#pragma mark ------------------- init -------------------

- (instancetype)initWith:(GlogConfig *)glogConfig
{
    self = [super init];
    if (self) {
        self.glog = [self glogInstanceWithConfig:glogConfig];
        self.config = glogConfig;
    }
    return self;
}

- (glog::Glog *)glogInstanceWithConfig:(GlogConfig *)config{
    glog::Glog::initialize((glog::InternalLogLevel)config.logLevel);
    NSString *rootDic = [config.rootDirectory stringByAppendingPathComponent:config.glogName];
    glog::GlogConfig config_struct{
        .m_protoName = [config.glogName UTF8String],
        .m_rootDirectory = [rootDic UTF8String],
        .m_incrementalArchive = config.incrementalArchive,
        .m_async = config.async,
        .m_expireSeconds = config.expireSeconds,
        .m_totalArchiveSizeLimit = config.totalArchiveSizeLimit,
        .m_compressMode = (glog::format::GlogCompressMode)config.compressMode,
        .m_encryptMode = (glog::format::GlogEncryptMode)config.encryptMode,
        .m_serverPublicKey = nullptr
    };
    return glog::Glog::maybeCreateWithConfig(config_struct);
}

#pragma mark ------------------- glog write -------------------

- (bool)writeString:(NSString *)dataString{
    NSData *data =[dataString dataUsingEncoding:NSUTF8StringEncoding];
    if (!data) {
        NSLog(@"write error:%@",dataString);
        return NO;
    }
    return [self write:data];
}

- (bool)writeDictionary:(NSDictionary *)dataDictionary{
    NSError *error;
    NSData *data= [NSJSONSerialization dataWithJSONObject:dataDictionary options:NSJSONWritingPrettyPrinted error:&error];
    if (error || !data) {
        NSLog(@"write error:%@",error);
        return NO;
    }
    return [self write:data];
}

- (bool)writeData:(NSData *)data{
    if (!data) {
        NSLog(@"write error:%@",data);
        return NO;
    }
    return [self write:data];
}

- (bool)write:(NSData *)data{
    if (!data) {
        NSLog(@"data is empty");
        return NO;
    }
    glog::GlogBuffer buffer((void *)[data bytes], data.length,true);
    return self.glog->write(buffer);
}

#pragma mark ------------------- glog read -------------------

- (NSArray *)readArchiveFile:(NSString *)archiveFile{
    @autoreleasepool {
        if ([Glog isBlankString:archiveFile]) {
            NSLog(@"The file cannot be empty");
            return [NSArray new];
        }
        glog::GlogReader *reader = self.glog->openReader([archiveFile UTF8String]);
        NSInteger length = 0;
        glog::GlogBuffer outBuf(16 * 1024);
        NSMutableArray *dataArr = [NSMutableArray new];
        do {
            length = reader->read(outBuf);
            if (length==0) {
                continue;
            }else if (length < 0){
                break;
            }
            NSData *data = [NSData dataWithBytes:outBuf.getPtr() length:length];
            [dataArr addObject:data];
        } while (length >= 0);
        self.glog->closeReader(reader);
        return dataArr;
    }
}
- (GlogReader *)openReader:(NSString *)archiveFile{
    GlogReader *reader = [GlogReader initialize:self.config.glogName archiveFile:archiveFile];
    return reader;
}
- (void)closeReader:(GlogReader *)reader{
    [reader closeReader:self.config.glogName];
}


#pragma mark ------------------- setting -------------------

- (void)flush{
    self.glog->flush();
}

- (void)resetExpireSeconds:(int32_t )expireSeconds{
    self.glog->resetExpireSeconds(expireSeconds);
}

#pragma mark ------------------- remove archive -------------------

- (void)removeArchiveFile:(NSString *)archiveFile{
    if ([Glog isBlankString:archiveFile]) {
        NSLog(@"The file cannot be empty");
        return ;
    }
    self.glog->removeArchiveFile([archiveFile UTF8String]);
}

- (void)removeAll{
    self.glog->removeAll();
}

#pragma mark ------------------- get archive -------------------

- (NSArray *)getArchiveSnapshot{
    return [self getArchiveSnapshot:true minLogNum:SIZE_T_MAX totalLogSize:0];
}

- (NSArray *)getArchiveSnapshot:(bool )flush minLogNum:(size_t )minLogNum totalLogSize:(size_t)totalLogSize{
    vector<string> archiveFiles;
    glog::ArchiveCondition condition{flush,minLogNum,totalLogSize};
    self.glog->getArchiveSnapshot(archiveFiles, condition,glog::FileOrder::CreateTimeAscending);
    return [self getArrayFromVector:archiveFiles];
}

- (NSArray *)getArchivesOfDate:(time_t )epochSeconds{
    NSMutableArray *archiveArr = [[NSMutableArray alloc]init];
    vector<string> archiveFiles;
    self.glog->getArchivesOfDate(epochSeconds, archiveFiles);
    for(auto &archiveFile:archiveFiles) {
        NSString *filePath = [NSString stringWithUTF8String:archiveFile.c_str()];
        [archiveArr addObject:filePath];
    }
    return archiveArr;
}

- (size_t)getCacheSize{
    return self.glog->getCacheSize();
}

#pragma mark ------------------- destroy -------------------

- (void)destroy{
    glog::Glog::destroy([self.config.glogName UTF8String]);
}

+ (void)destroyAll{
    glog::Glog::destroyAll();
}

#pragma mark ------------------- other -------------------

-(NSArray *)getArrayFromVector:(vector<string> )vec{
    NSMutableArray *verArr = [[NSMutableArray alloc]init];
    for(auto &item:vec) {
        NSString *value = [NSString stringWithUTF8String:item.c_str()];
        if ([value isKindOfClass:[NSString class]]) {
            [verArr addObject:value];
        }
    }
    return verArr;
}

+(BOOL) isBlankString:(NSString *)string {
    if (string == nil || string == NULL || [string isKindOfClass:[NSNull class]]) {
        return YES;
    }
    if (![string isKindOfClass:[NSString class]]) {
        return YES;
    }
    if ([[string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length]==0) {
        return YES;
    }
    return NO;
}

void glogLogHandler(glog::InternalLogLevel level, const char *file, int line, const char *function, const std::string &message){
    NSLog(@"file:%s, line:%d, message:%@",file,line,[NSString stringWithUTF8String:message.c_str()]);
}
@end

