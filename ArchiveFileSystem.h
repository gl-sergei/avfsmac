//
//  ArchiveFileSystem.h
//  ArchiveFS
//
//  Created by Quyllur on 10/18/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
// Filesystem operations.
//
#import <Foundation/Foundation.h>

// The core set of file system operations. This class will serve as the delegate
// for GMUserFileSystemFilesystem. For more details, see the section on 
// GMUserFileSystemOperations found in the documentation at:
// http://macfuse.googlecode.com/svn/trunk/core/sdk-objc/Documentation/index.html

#import "archivemount.h"
#import <MacFUSE/GMUserFileSystem.h>
#import "MinimalFileSystem.h"

@interface ArchiveFileSystem : MinimalFileSystem  {
	
	archive_fs_t fs;
	
}

- (id)initWithPath:(NSString*)archivePath mountPoint:(NSString*)mountPoint;
- (void)dealloc;

@end
