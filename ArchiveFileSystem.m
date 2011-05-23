//
//  ArchiveFilesystem.m
//  ArchiveFS
//
//  Created by Quyllur on 10/18/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
#import <sys/xattr.h>
#import <sys/stat.h>
#import "ArchiveFileSystem.h"
#import <MacFUSE/MacFUSE.h>

// NOTE: It is fine to remove the below sections that are marked as 'Optional'.
// To create a working write-able file system, you must implement all non-optional
// methods fully and have them return errors correctly.

// The core set of file system operations. This class will serve as the delegate
// for GMUserFileSystemFilesystem. For more details, see the section on 
// GMUserFileSystemOperations found in the documentation at:
// http://macfuse.googlecode.com/svn/trunk/core/sdk-objc/Documentation/index.html
@implementation ArchiveFileSystem

- (ArchiveFileSystem*)initWithPath:(NSString *)archivePath mountPoint:(NSString *)mtpt {
	
	if (self = [super initWithPath:archivePath mountPoint:mtpt]) {
		ar_init(&fs, [archivePath fileSystemRepresentation], [mountPoint fileSystemRepresentation]);
	}
	
	return self;
}

- (void)dealloc {
	ar_free(&fs);
	[super dealloc];
}

#pragma mark Directory Contents

- (NSArray *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error {
	
	NSMutableArray *dirList = [[[NSMutableArray alloc] init] autorelease];
	
	int res = ar_readdir(&fs, [path fileSystemRepresentation], NULL,
						 ^ int (const char* name, struct stat* st) {
							 (void) st;
							 if (strcmp(".", name) && strcmp("..", name))
								 [dirList addObject:[NSString stringWithUTF8String:name]];
							 return 0;
						 }, 0);
	*error = [NSError errorWithPOSIXCode:-res];
	
	if (res)
		return nil;
	return dirList;
}

#pragma mark Getting and Setting Attributes

- (NSDictionary *)attributesOfItemAtPath:(NSString *)path
                                userData:(id)userData
                                   error:(NSError **)error {
	
    int res;
	
	uid_t uid = getuid();
	gid_t gid = getgid();

	struct stat st;
	res = ar_getattr(&fs, [path fileSystemRepresentation], &st);
	if (error)
		*error = [NSError errorWithPOSIXCode:-res];
	if (res)
		return nil;
	NSMutableDictionary *attrs = [[[NSMutableDictionary alloc] init] autorelease];
	
	NSObject *fileType = NSFileTypeUnknown;
	if (S_ISCHR(st.st_mode))
		fileType = NSFileTypeCharacterSpecial;
	if (S_ISDIR(st.st_mode)) {
		fileType = NSFileTypeDirectory;
	}
	if (S_ISBLK(st.st_mode))
		fileType = NSFileTypeBlockSpecial;
	if (S_ISREG(st.st_mode)) {
		fileType = NSFileTypeRegular;
	}
	if (S_ISLNK(st.st_mode))
		fileType = NSFileTypeSymbolicLink;
	if (S_ISSOCK(st.st_mode))
		fileType = NSFileTypeSocket;

	[attrs setObject:fileType forKey:NSFileType];
	[attrs setObject:[NSNumber numberWithUnsignedLongLong:st.st_size] forKey:NSFileSize];
	[attrs setObject:[NSDate dateWithTimespec:(&st.st_mtimespec)] forKey:NSFileModificationDate];
	[attrs setObject:[NSNumber numberWithUnsignedLong:st.st_nlink] forKey:NSFileReferenceCount];
	[attrs setObject:[NSNumber numberWithUnsignedLong:(st.st_mode & 07777)] forKey:NSFilePosixPermissions];
	[attrs setObject:[NSNumber numberWithUnsignedLong:uid/*st.st_uid*/] forKey:NSFileOwnerAccountID];
	[attrs setObject:[NSNumber numberWithUnsignedLong:gid/*st.st_gid*/] forKey:NSFileGroupOwnerAccountID];
	[attrs setObject:[NSNumber numberWithUnsignedLongLong:st.st_ino] forKey:NSFileSystemFileNumber];
	[attrs setObject:[NSDate dateWithTimespec:(&st.st_mtimespec)] forKey:NSFileCreationDate];
	
	return attrs;
}

- (NSDictionary *)attributesOfFileSystemForPath:(NSString *)path
                                          error:(NSError **)error {
	return [NSDictionary dictionary];  // Default file system attributes.
}

- (BOOL)setAttributes:(NSDictionary *)attributes 
         ofItemAtPath:(NSString *)path
             userData:(id)userData
                error:(NSError **)error {
	return NO; 
}

#pragma mark File Contents

- (BOOL)openFileAtPath:(NSString *)path 
                  mode:(int)mode
              userData:(id *)userData
                 error:(NSError **)error {
	
	int res = ar_open(&fs, [path fileSystemRepresentation], mode);
	
	if (res) {
		if (error)
			*error = [NSError errorWithPOSIXCode:-res];
		return NO;
	}
	
	return YES;
}

- (void)releaseFileAtPath:(NSString *)path userData:(id)userData {
	ar_release(&fs, [path fileSystemRepresentation]);
}

- (int)readFileAtPath:(NSString *)path 
             userData:(id)userData
               buffer:(char *)buffer 
                 size:(size_t)size 
               offset:(off_t)offset
                error:(NSError **)error {
	
	int res = ar_read(&fs, [path fileSystemRepresentation], buffer, size, offset);
	if (error)
		*error = [NSError errorWithPOSIXCode:res < 0 ? res : 0];
	return res;
}

- (int)writeFileAtPath:(NSString *)path 
              userData:(id)userData
                buffer:(const char *)buffer
                  size:(size_t)size 
                offset:(off_t)offset
				 error:(NSError **)error {
	return size;
}

// (Optional)
- (BOOL)exchangeDataOfItemAtPath:(NSString *)path1
                  withItemAtPath:(NSString *)path2
                           error:(NSError **)error {
	return NO;
}

#pragma mark Creating an Item

- (BOOL)createDirectoryAtPath:(NSString *)path 
                   attributes:(NSDictionary *)attributes
                        error:(NSError **)error {
	return NO;
}

- (BOOL)createFileAtPath:(NSString *)path 
              attributes:(NSDictionary *)attributes
                userData:(id *)userData
                   error:(NSError **)error {
	return NO;
}

#pragma mark Moving an Item

- (BOOL)moveItemAtPath:(NSString *)source 
				toPath:(NSString *)destination
				 error:(NSError **)error {
	return NO;
}

#pragma mark Removing an Item

// Optional
- (BOOL)removeDirectoryAtPath:(NSString *)path error:(NSError **)error {
	return NO;
}

- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error {
	return NO;
}

#pragma mark Linking an Item (Optional)

- (BOOL)linkItemAtPath:(NSString *)path
                toPath:(NSString *)otherPath
                 error:(NSError **)error {
	return NO; 
}

#pragma mark Symbolic Links (Optional)

- (BOOL)createSymbolicLinkAtPath:(NSString *)path 
             withDestinationPath:(NSString *)otherPath
                           error:(NSError **)error {
	return NO;
}

- (NSString *)destinationOfSymbolicLinkAtPath:(NSString *)path
                                        error:(NSError **)error {
	
	char *buf = calloc(1, 4096);
	int res = ar_readlink(&fs, [path fileSystemRepresentation], buf, 4096);
	buf[4095] = 0;
	NSString *result = [NSString stringWithUTF8String:buf];
	free(buf);
	
	if (error)
		*error = [NSError errorWithPOSIXCode:-res];
	if (res)
		return nil;
	return result;
}

#pragma mark Extended Attributes (Optional)

- (NSArray *)extendedAttributesOfItemAtPath:(NSString *)path error:(NSError **)error {
	return [NSArray array];  // No extended attributes.
}

- (NSData *)valueOfExtendedAttribute:(NSString *)name 
                        ofItemAtPath:(NSString *)path
                            position:(off_t)position
                               error:(NSError **)error {
	if (error)
		*error = [NSError errorWithPOSIXCode:ENOATTR];
	return nil;
}

- (BOOL)setExtendedAttribute:(NSString *)name
                ofItemAtPath:(NSString *)path
                       value:(NSData *)value
                    position:(off_t)position
                     options:(int)options
                       error:(NSError **)error {
	return NO;
}

- (BOOL)removeExtendedAttribute:(NSString *)name
                   ofItemAtPath:(NSString *)path
                          error:(NSError **)error {
	return NO;
}

#pragma mark FinderInfo and ResourceFork (Optional)

- (NSDictionary *)finderAttributesAtPath:(NSString *)path 
                                   error:(NSError **)error {
	return [NSDictionary dictionary];
}

- (NSDictionary *)resourceAttributesAtPath:(NSString *)path
                                     error:(NSError **)error {
	return [NSDictionary dictionary];
}

#pragma mark mount/umount

- (void)willMount {
}

- (void)willUnmount {
}

@end
