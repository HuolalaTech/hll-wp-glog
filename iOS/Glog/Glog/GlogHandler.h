//
//  GlogHeader.h
//  Glog
//
//  Created by peng on 2022/5/15.
//

#ifndef GlogHeader_h
#define GlogHeader_h

typedef NS_ENUM(NSUInteger, GlogInternalLogLevel) {
    GlogInternalLog_Debug = 0, // not available for release/product build
    GlogInternalLog_Info = 1,  // default level
    GlogInternalLog_Warning,
    GlogInternalLog_Error,
    GlogInternalLog_None, // special level used to disable all log messages
};
typedef NS_ENUM(NSUInteger, GlogCompressMode) {
    GlogCompressMode_None = 1,
    GlogCompressMode_Zlib = 2
};

typedef NS_ENUM(NSUInteger, GlogEncryptMode) {
    GlogEncryptMode_None = 1,
    GlogEncryptMode_AES = 2
};

#endif /* GlogHeader_h */
