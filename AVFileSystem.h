//
//  AVFileSystem.h
//  avfsmac
//
//  Created by Quyllur on 10/16/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
// Filesystem operations.
//
#import <Foundation/Foundation.h>
#import "MinimalFileSystem.h"

// The core set of file system operations. This class will serve as the delegate
// for GMUserFileSystemFilesystem. For more details, see the section on 
// GMUserFileSystemOperations found in the documentation at:
// http://macfuse.googlecode.com/svn/trunk/core/sdk-objc/Documentation/index.html

@interface AVFileSystem : MinimalFileSystem  {
	
}

- (id)initWithPath: (NSString*)aPath mountPoint:(NSString*)mtpt;

@end
