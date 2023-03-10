//
//  GlogBridge.mm
//  Bridge
//
//  Created by 曾梓峰 on 2021/3/30.
//

#import "GlogBridge.h"
#import <GlogCore/GlogPredef.h>
#import <Foundation/Foundation.h>
#import <GlogCore/Glog.h>
#import <GlogCore/GlogBuffer.h>
#import <GlogCore/InternalLog.h>

static BOOL g_initialized = NO;
static NSMutableDictionary *g_instanceDic = nil;
static recursive_mutex *g_instanceMutex;

@implementation Glog {
    glog::Glog *m_glog;
}

+ (void)initializeGlog:(InternalLogLevel)level {
    glog::Glog::initialize((glog::InternalLogLevel) level);

    g_instanceMutex = new recursive_mutex();
    g_instanceDic = [[NSMutableDictionary alloc] init];
    g_initialized = YES;
}

+ (instancetype)instanceWithProto:(NSString *)proto rootDirectory:(NSString *)rootDirectory async:(BOOL)async encrypt:(BOOL)encrypt{
    if (!g_initialized) {
        glog::InternalError("Glog not initialized");
        [NSException raise:@"not initialized error" format:@"Glog not initialized"];
    }
    std::unique_lock<std::recursive_mutex> lock(*g_instanceMutex);

    Glog *gl = [g_instanceDic objectForKey:proto];
    if (gl == nil) {
        gl = [[Glog alloc] initWithProto:proto rootDirectory:rootDirectory async:async encrypt:encrypt];
        if (!gl->m_glog) {
            //            [gl release];
            return nil;
        }
        [g_instanceDic setObject:gl forKey:proto];
        //        [gl release];
    }
    return gl;
}

- (instancetype)initWithProto:(NSString *)proto rootDirectory:(NSString *)rootDirectory async:(BOOL)async encrypt:(BOOL) encrypt{
    if (self = [super init]) {
        string pubKey = "9CECA66EB62760F5A93406A6F98C1489EC69089C0A1F2608B18B72B0BF428CC9BAEB30CB946208A2BFC2255D0E77968CB0"
                        "7FE0E7E7042A3E638CDB553A94C650";

        glog::GlogConfig config{
            .m_protoName = proto.UTF8String,
            .m_rootDirectory = rootDirectory.UTF8String,
            .m_incrementalArchive = true,
            .m_async = async,
            .m_expireSeconds = 7 * 24 * 60 * 60,
            .m_totalArchiveSizeLimit = 16 * 1024 * 1024,
            .m_compressMode = glog::format::GlogCompressMode::Zlib,
            .m_encryptMode = encrypt ? glog::format::GlogEncryptMode::AES : glog::format::GlogEncryptMode::None,
            .m_serverPublicKey = encrypt ? &pubKey : nullptr
        };
        m_glog = glog::Glog::maybeCreateWithConfig(config);
    }
    return self;
}

- (bool)write:(NSData *)data {
    return m_glog->write(glog::GlogBuffer((void *) data.bytes, data.length));
}

- (NSString *)getCacheFileName {
    return [NSString stringWithCString:m_glog->getCacheFileName().c_str() encoding:[NSString defaultCStringEncoding]];
}

@end
