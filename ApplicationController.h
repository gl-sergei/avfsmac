//
//  ApplicationController.h
//  avfsmac
//
//  Created by Quyllur on 10/16/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
#import <Cocoa/Cocoa.h>

@class GMUserFileSystem;
@class MinimalFileSystem;

@interface ApplicationController : NSObject {
	NSMutableArray *fileSystems;
	NSMutableArray* userFileSystems;
	MinimalFileSystem *avfs;
}

@end
