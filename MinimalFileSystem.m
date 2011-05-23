//
//  MinimalFileSystem.m
//  avfsmac
//
//  Created by Quyllur on 10/20/10.
//  Copyright 2010 Quyllur. All rights reserved.
//

#import "MinimalFileSystem.h"

@implementation NSError (POSIX)
+ (NSError *)errorWithPOSIXCode:(int) code {
	return [NSError errorWithDomain:NSPOSIXErrorDomain code:code userInfo:nil];
}
@end

@implementation MinimalFileSystem

@synthesize baseFilePath;
@synthesize fileSystemNumber;
@synthesize mountPoint;

- (id)initWithPath: (NSString*)aPath mountPoint:(NSString*)mtpt {
	
	if (self = [super init]) {
		
		[self setBaseFilePath:aPath];
		[self setMountPoint:mtpt];
		
	}
	
	return self;
	
}

- (NSString*)canonizedArchivePath:(NSString*)path {
	return [[path copy] autorelease];
}


@end

@implementation NSDate (TimeSpecTools)

#pragma mark Tools
+ (NSDate*) dateWithTimespec:(const struct timespec*) spec {
	const double kNanoSecondsPerSecond = 1000000000.0;
	const NSTimeInterval time_ns = spec->tv_nsec;
	const NSTimeInterval time_sec = spec->tv_sec + (time_ns / kNanoSecondsPerSecond);
	return [NSDate dateWithTimeIntervalSince1970:time_sec];
}

@end

