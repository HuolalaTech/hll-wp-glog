//
//  AppDelegate.m
//  Benchmark
//
//  Created by 曾梓峰 on 2021/4/12.
//

#import "AppDelegate.h"
#import "ViewController.h"
#import <Bridge/GlogBridge.h>
#import <mars/xlog/appender.h>
#import <mars/xlog/xlogger.h>
#import <sys/xattr.h>
#import "LoganHelper.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
#ifdef DEBUG
    NSLog(@"=== debug mode");
    [Glog initializeGlog:InternalLogLevelDebug];
#else
    NSLog(@"=== release mode");
    [Glog initializeGlog:InternalLogLevelInfo];
#endif

    [self initXlog];
    [LoganHelper doInitLogan];
    return YES;
}

- (void)initXlog {
    NSString *logPath = [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0] stringByAppendingString:@"/wlog"];
    // set do not backup for logpath
    const char *attrName = "com.apple.MobileBackup";
    u_int8_t attrValue = 1;
    setxattr([logPath UTF8String], attrName, &attrValue, sizeof(attrValue), 0, 0);

    // init xlog
#if DEBUG
    xlogger_SetLevel(kLevelDebug);
    appender_set_console_log(true);
#else
    xlogger_SetLevel(kLevelInfo);
    appender_set_console_log(false);
#endif

    XLogConfig config;
    config.mode_ = kAppenderAsync;
    config.logdir_ = [logPath UTF8String];
    config.nameprefix_ = "X";
    config.pub_key_ = "";
    config.compress_mode_ = kZlib;
    config.compress_level_ = 0;
    config.cachedir_ = "";
    config.cache_days_ = 0;
    appender_open(config);
}

@end
