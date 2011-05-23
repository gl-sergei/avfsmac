//
//  MinimalFileSystem.h
//  avfsmac
//
//  Created by Quyllur on 10/20/10.
//  Copyright 2010 Quyllur. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface MinimalFileSystem : NSObject {
	
	NSString *baseFilePath;
	NSNumber *fileSystemNumber;
	NSString *mountPoint;
	
}

@property (retain) NSString* baseFilePath;
@property (retain) NSNumber* fileSystemNumber;
@property (retain) NSString* mountPoint;

- (id)initWithPath: (NSString*)aPath mountPoint:(NSString*)mtpt;
- (NSString*)canonizedArchivePath:(NSString*)path;

@end

// Category on NSError to  simplify creating an NSError based on posix errno.
@interface NSError (POSIX)
+ (NSError *)errorWithPOSIXCode:(int)code;
@end

@interface NSDate (TimeSpecTools)

+ (NSDate*) dateWithTimespec:(const struct timespec*) spec;

@end

