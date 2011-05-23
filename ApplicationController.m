//
//  ApplicationController.m
//  avfsmac
//
//  Created by Qoyllur on 10/16/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
#import "ApplicationController.h"
#import "MinimalFileSystem.h"
#import "AVFileSystem.h"
#import "ArchiveFileSystem.h"
#import <MacFUSE/MacFUSE.h>
#import "7z-objc/SQSevenZip.h"

@implementation ApplicationController

- (ApplicationController*)awakeFromNib {
	
	fileSystems = [[NSMutableArray alloc] init];
	userFileSystems = [[NSMutableArray alloc] init];
	avfs = nil;
	
	SQSevenZip *sz = [[SQSevenZip alloc] initWithFile:@"/Users/urijgagarin/Projects/avfsmac.7z"];
	[sz release];
	
	[super awakeFromNib];
	
	return self;
	
}

- (void)refreshBadgingLabel {
	if ([fileSystems count] > 0)
		[[NSApp dockTile] setBadgeLabel:[NSString stringWithFormat:@"%d", [fileSystems count]]];
	else
		[[NSApp dockTile] setBadgeLabel:@""];
}

- (void)mountFailed:(NSNotification *)notification {
	NSDictionary* userInfo = [notification userInfo];
	NSError* error = [userInfo objectForKey:kGMUserFileSystemErrorKey];
	NSLog(@"kGMUserFileSystem Error: %@, userInfo=%@", error, [error userInfo]);  
	NSRunAlertPanel(@"Mount Failed", [error localizedDescription], nil, nil, nil);
	
	[fileSystems removeLastObject];
	[userFileSystems removeLastObject];
	
	[self refreshBadgingLabel];
}

- (void)didMount:(NSNotification *)notification {
	NSDictionary* userInfo = [notification userInfo];
	NSString* mountPath = [userInfo objectForKey:kGMUserFileSystemMountPathKey];
	NSString* parentPath = [mountPath stringByDeletingLastPathComponent];
	[[NSWorkspace sharedWorkspace] selectFile:mountPath
					 inFileViewerRootedAtPath:parentPath];
	if (avfs) {
		NSNumber *fileSystemNumber = [[[[[NSFileManager alloc] init] autorelease] attributesOfFileSystemForPath:mountPath error: nil] objectForKey: NSFileSystemNumber];
		[avfs setFileSystemNumber:fileSystemNumber];
		avfs = nil;
	}
	[self refreshBadgingLabel];
}

- (void)didUnmount:(NSNotification*)notification {
	//	[[NSApplication sharedApplication] terminate:nil];
	NSDictionary* userInfo = [notification userInfo];
	NSString* mountPath = [userInfo objectForKey:kGMUserFileSystemMountPathKey];
	
	for (int i = 0; i < [fileSystems count]; ++i)
	{
		if ([[[fileSystems objectAtIndex:i] mountPoint] isEqualToString: mountPath])
		{
			[fileSystems removeObjectAtIndex:i];
			[userFileSystems removeObjectAtIndex:i];
			break;
		}
	}
	[self refreshBadgingLabel];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
	NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
	[center addObserver:self
			   selector:@selector(mountFailed:)
				   name:kGMUserFileSystemMountFailed object:nil];
	[center addObserver:self selector:@selector(didMount:)
				   name:kGMUserFileSystemDidMount object:nil];
	[center addObserver:self selector:@selector(didUnmount:)
				   name:kGMUserFileSystemDidUnmount object:nil];
	
}

- (BOOL)tryMount:(NSString*)fileName at:(NSString*)mountPath {
	
	NSError *error = nil;
	if ([avfs attributesOfItemAtPath:@"/" userData:nil error:&error])
	{
		GMUserFileSystem *ufs = [[[GMUserFileSystem alloc] initWithDelegate:avfs isThreadSafe:NO] autorelease];
		
		NSMutableArray* options = [NSMutableArray array];
		NSString* volArg = 
		[NSString stringWithFormat:@"volicon=%@", 
		 [[NSBundle mainBundle] pathForResource:@"avfsmac" ofType:@"icns"]];
		[options addObject:volArg];
		[options addObject:[NSString stringWithFormat:@"volname=%@", fileName]];
		[options addObject:@"local"];
		[fileSystems addObject:avfs];
		[userFileSystems addObject:ufs];
		[ufs mountAtPath:mountPath withOptions:options];
		return YES;
	}
	else {
		NSLog(@"error mounting %@: %ld (%@)", fileName, [error code], [error localizedDescription]);
		return NO;
	}
}

- (void)mountFile:(NSString*)fullPath {

	NSString *fileName = [fullPath lastPathComponent];
	NSLog(@"fileName is %@", fileName);
	if (!fileName)
	{
		NSLog(@"Bad fileName");
		[[NSApplication sharedApplication] terminate:nil];
	}
	
	NSString *archiveBasePath = [[fullPath copy] autorelease];
	NSNumber *fsId = [[[[[NSFileManager alloc] init] autorelease] attributesOfFileSystemForPath:archiveBasePath error:nil] objectForKey:NSFileSystemNumber];
	for (MinimalFileSystem *fs in fileSystems)
		if ([[fs fileSystemNumber] isEqualTo:fsId])
			archiveBasePath = [archiveBasePath stringByReplacingOccurrencesOfString:[fs mountPoint] withString:[NSString stringWithFormat:@"@%#", [fs baseFilePath]]];
	
	NSString* mountPath = [[NSString stringWithFormat:@"/Volumes/%@", fileName] stringByStandardizingPath];
	avfs = nil;
	if ([fullPath hasSuffix:@".tar.bz2"]) {
		avfs = [[[AVFileSystem alloc] initWithPath:fullPath mountPoint:mountPath] autorelease];
		NSLog(@"giving avfs plugin a chance");
		if (![self tryMount:fileName at:mountPath])
		{
			NSLog(@"avfs plugin failed");
			avfs = nil;
		}
	}
	if (!avfs) {
		avfs = [[[ArchiveFileSystem alloc] initWithPath:fullPath mountPoint:mountPath] autorelease];
		NSLog(@"giving chance to libarchive");
		if (![self tryMount:fileName at:mountPath])
		{
			NSLog(@"libarchive failed");
			avfs = nil;
		}
	}
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	for (GMUserFileSystem* ufs in userFileSystems)
		[ufs unmount];
	[userFileSystems release];
	[fileSystems release];
	return NSTerminateNow;
}

- (BOOL) application: (NSApplication*) anApplication
			openFile: (NSString*    ) aFileName
{
	NSLog(@"file name is %@", aFileName);
	[self mountFile:[aFileName stringByStandardizingPath]];
	return YES;
}

@end
