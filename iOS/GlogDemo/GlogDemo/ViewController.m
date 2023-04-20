//
//  ViewController.m
//  GlogDemo
//
//  Created by peng on 2022/5/17.
//

#import "ViewController.h"
#import <Glog/GlogReader.h>
#import <Glog/Glog.h>
#define SCREEN_W [UIScreen mainScreen].bounds.size.width
#define SCREEN_H [UIScreen mainScreen].bounds.size.height
@interface ViewController ()<UITableViewDelegate,UITableViewDataSource>

@property(nonatomic, strong)UITableView *tableView;

@property(nonatomic, strong)NSMutableArray *dataSource;

@property(nonatomic, strong)Glog *glog;

@end

@implementation ViewController

NSString* svrPubKey = @"41B5F5F9A53684A1C09B931B7BDF7D7C3959BC7FB31827ADBE1524DDC8F2D90AD4978891385D956CE817B293FC57CF07A4EC3DAF03F63852D75A32A956B84176";
NSString* svrPrivKey = @"9C8B23A406216B7B93AA94C66AA5451CCE41DD57A8D5ADBCE8D9F1E7F3D33F45";

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    [self dataSource];
    [self tableView];
    
    [Glog initialize:GlogInternalLog_Info];
    
    GlogConfig *cfg = [[GlogConfig alloc] initWithProtoName:@"demo"];
    cfg.encryptMode = GlogEncryptMode_AES;
    cfg.serverPublicKey = svrPubKey;
    self.glog = [Glog glogWithConfig:cfg];
}

- (UITableView *)tableView {
    if (!_tableView) {
        _tableView = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, SCREEN_W, SCREEN_H) style:UITableViewStyleGrouped];
        _tableView.delegate = self;
        _tableView.dataSource = self;
        [self.view addSubview:_tableView];
    }
    return _tableView;
}

- (NSMutableArray *)dataSource {
    if (!_dataSource) {
        _dataSource = [NSMutableArray new];
        [_dataSource addObjectsFromArray:@[ @"",
                                        @"write",
                                        @"read",
                                        @"getArchiveSnapshot",
                                        @"readArchiveFile",
                                        @"removeArchiveFile",
                                        @"flush",
                                        @"write 100000 "

        ]];
    }
    return _dataSource;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return _dataSource.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cellid"];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cellid"];
    }
    cell.textLabel.text = self.dataSource[indexPath.row];
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    switch (indexPath.row) {
        case 0: {
            break;
        }
        case 1: {
            NSString* content = @"hello glog";
            [self.glog write:[content dataUsingEncoding:NSUTF8StringEncoding]];
            break;
        }
        case 2: {
            @autoreleasepool {
                NSArray *logFiles = [self.glog getArchiveSnapshot:true minLogNum:2 totalLogSize:100 * 1024];
                for (NSString *item in logFiles) {
                    GlogReader *reader = [self.glog openReader:item key:svrPrivKey];
                    NSInteger length = 0;
                    NSData *data;
                    do {
                        length = [reader read:&data];
                        if (length == 0) {
                            continue;
                        }else if (length < 0){
                            break;
                        }
                        if (data) {
                            NSString *log = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                            NSLog(@"log: %@", log);
                        }
                    } while (true);
                }
            }
        }
        break;
            
        case 3: {
            NSArray *files = [self.glog getArchiveSnapshot];
            NSLog(@"files:%@", files);
        }
        break;
            
        case 4: {
            NSArray *files = [self.glog getArchiveSnapshot];
            NSArray *fileData = [self.glog readArchiveFile:files[0] key:svrPrivKey];
            for (NSData *data in fileData) {
                NSLog(@"log:%@",[[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding]);
            }
        }
        break;
            
        case 5: {
            NSArray *logFiles = [self.glog getArchiveSnapshot];
            [self.glog removeArchiveFile:logFiles[0]];
            NSArray *files = [self.glog getArchiveSnapshot];
            NSLog(@"删除前：%@ ， 删除后：%@", logFiles, files);
        }
        break;
            
        case 6: {
            [self.glog flush];
            NSArray *files = [self.glog getArchiveSnapshot];
            NSArray *logs = [self.glog readArchiveFile:files[0] key:svrPrivKey];
            for (NSData *data in logs) {
                NSLog(@"log:%@",[[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding]);
            }
     
        }
        break;
            
        case 7: {
            for (int i = 0; i < 100000; i++) {
                [self.glog write:[[NSString stringWithFormat:@"glog_%d",i] dataUsingEncoding:NSUTF8StringEncoding]];
            }
        }
        break;
        
        default:
            break;
    }
}

@end
