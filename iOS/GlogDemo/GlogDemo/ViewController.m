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

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    [self dataSource];
    [self tableView];
}

- (UITableView *)tableView{
    if (!_tableView) {
        _tableView = [[UITableView alloc]initWithFrame:CGRectMake(0, 0, SCREEN_W, SCREEN_H) style:UITableViewStyleGrouped];
        _tableView.delegate = self;
        _tableView.dataSource = self;
        [self.view addSubview:_tableView];
    }
    return _tableView;
}

-(NSMutableArray *)dataSource{
    if (!_dataSource) {
        _dataSource = [NSMutableArray new];
        [_dataSource addObjectsFromArray:@[@"initialize",
                                           @"write",
                                           @"read",
                                           @"getArchiveSnapshot",
                                           @"readArchiveFile",
                                           @"removeArchiveFile",
                                           @"flush",
                                           @"write 1000000 "
        
        ]];
    }
    return _dataSource;
}

-(NSInteger)numberOfSectionsInTableView:(UITableView *)tableView{
    return 1;
    
}
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return _dataSource.count;
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cellid"];
    if (cell == nil) {
        cell = [[UITableViewCell alloc]initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cellid"];
    }
    cell.textLabel.text = self.dataSource[indexPath.row];
      return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    switch (indexPath.row) {
        case 0:
        {
            self.glog = [Glog defaultGlog];
        }
            break;
        case 1:
        {
            [self.glog writeString:@"nnn"];
        }
            break;
        case 2:
        {
            @autoreleasepool {
                NSArray *logFiles = [self.glog getArchiveSnapshot:true minLogNum:2 totalLogSize:100 * 1024];
                NSMutableArray *dataArr = [NSMutableArray new];
                for (NSString *item in logFiles) {
                    GlogReader *reader = [self.glog openReader:item];
                    NSInteger length = 0;
                    NSData *data;
                    do {
                        length = [reader read:&data];
                        if (length==0) {
                            continue;
                        }else if (length < 0){
                            break;
                        }
                        if (data) {
                            NSString *dataString = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                            [dataArr addObject:dataString];
                        }
                    } while (length >= 0);
                    [self.glog closeReader:reader];
                }
                NSLog(@"all the data: %@",dataArr);
            }
        }
            break;
        case 3:
        {
            NSArray *logsList = [self.glog getArchiveSnapshot];
            NSLog(@"file:%@",logsList);
        }
            break;
        case 4:
        {
            NSArray *logsList = [self.glog getArchiveSnapshot];
            NSArray *fileData = [self.glog readArchiveFile:logsList[0]];
            for (NSData *data in fileData) {
                NSLog(@"fileData:%@",[[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding]);
            }
        }
            break;
            
        case 5:
        {
            NSArray *ar1 = [self.glog getArchiveSnapshot];
            [self.glog removeArchiveFile:ar1[0]];
            NSArray *ar2 = [self.glog getArchiveSnapshot];
            NSLog(@"删除前：%@ ， 删除后：%@",ar1,ar2);
        }
            break;
            
        case 6:
        {
            [self.glog flush];
            NSArray *ar = [self.glog getArchiveSnapshot];
            NSArray *fileData = [self.glog readArchiveFile:ar[0]];
            for (NSData *data in fileData) {
                NSLog(@"fileData:%@",[[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding]);
            }
     
        }
            break;
        case 7:
        {
            for (int i = 0; i < 100000; i ++) {
                [self.glog writeString:[NSString stringWithFormat:@"abc_%d",i]];
            }
        }
            break;
        default:
            break;
    }
}

@end
