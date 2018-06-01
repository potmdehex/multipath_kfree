//
//  ViewController.m
//  multipath_kfree
//
//  Created by q on 6/1/18.
//  Copyright © 2018 kjljkla. All rights reserved.
//

#import "ViewController.h"

#include "jailbreak.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    jb_go();
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
