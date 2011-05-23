//
//  AVFileSystem.m
//  avfsmac
//
//  Created by Quyllur on 10/16/10.
//  Copyright 2010 Quyllur. All rights reserved.
//
#import <sys/xattr.h>
#import <sys/stat.h>
#import "virtual.h"
#import "AVFileSystem.h"
#import <MacFUSE/MacFUSE.h>


// NOTE: It is fine to remove the below sections that are marked as 'Optional'.
// To create a working write-able file system, you must implement all non-optional
// methods fully and have them return errors correctly.

// The core set of file system operations. This class will serve as the delegate
// for GMUserFileSystemFilesystem. For more details, see the section on 
// GMUserFileSystemOperations found in the documentation at:
// http://macfuse.googlecode.com/svn/trunk/core/sdk-objc/Documentation/index.html
@implementation AVFileSystem

#pragma mark Directory Contents

- (id)initWithPath:(NSString *)aPath mountPoint:(NSString*)mtpt {
	
	self = [super initWithPath:aPath mountPoint:mtpt];
	
	return self;
	
}

- (NSString*)canonizedArchivePath:(NSString*)path {
	return [path stringByReplacingOccurrencesOfString:[self mountPoint]
										   withString:[NSString stringWithFormat:@"@%#", [self baseFilePath]]
											  options:0
												range:NSMakeRange(0, [[self mountPoint] length] - 1)];
}

- (NSString*)makeAbsolutePath: (NSString*)path {
	return [NSString stringWithFormat:@"%@#%@", [self baseFilePath], path];
}

- (NSArray *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error {
	DIR *dp;
    struct dirent *de;

    dp = virt_opendir([[self makeAbsolutePath:path] fileSystemRepresentation]);
    if (dp == NULL)
	{
		if (error)
			*error = [NSError errorWithPOSIXCode:errno];
        return nil;
	}

	NSMutableArray *dirList = [[[NSMutableArray alloc] init] autorelease];
    while((de = virt_readdir(dp)) != NULL) {
		if (strcmp(".", de->d_name) && strcmp("..", de->d_name))
			[dirList addObject:[NSString stringWithUTF8String:de->d_name]];
    }
	
    virt_closedir(dp);

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
    res = virt_lstat([[self makeAbsolutePath:path] fileSystemRepresentation], &st);
	if (error)
		*error = [NSError errorWithPOSIXCode:res == -1 ? errno : res];
	if (res)
		return nil;
	NSMutableDictionary *attrs = [[[NSMutableDictionary alloc] init] autorelease];
	
	NSObject *fileType = NSFileTypeUnknown;
	if (S_ISCHR(st.st_mode))
		fileType = NSFileTypeCharacterSpecial;
	if (S_ISDIR(st.st_mode))
		fileType = NSFileTypeDirectory;
	if (S_ISBLK(st.st_mode))
		fileType = NSFileTypeBlockSpecial;
	if (S_ISREG(st.st_mode))
		fileType = NSFileTypeRegular;
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
	
	NSMutableDictionary *attrs = [[[NSMutableDictionary alloc] init] autorelease];
	
	NSError *err = nil;
	NSDictionary *archAttrs = [[[[NSFileManager alloc] init] autorelease] attributesOfItemAtPath:[self baseFilePath] error:&err];
	
	[attrs setObject:[NSNumber numberWithUnsignedLongLong:[[archAttrs objectForKey:NSFileSize] unsignedLongLongValue]] forKey:NSFileSystemSize];
	[attrs setObject:[NSNumber numberWithUnsignedLongLong:0] forKey:NSFileSystemFreeSize];
	
	return attrs;
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
	
	int fd = virt_open([[self makeAbsolutePath:path] fileSystemRepresentation], mode, 0);
	if (fd == -1)
	{
		if (error)
			*error = [NSError errorWithPOSIXCode:errno];
		return NO;
	}
	*userData = [NSNumber numberWithInt:fd];
	return YES;
}

- (void)releaseFileAtPath:(NSString *)path userData:(id)userData {
	
	int fd = [userData intValue];
	virt_close(fd);
}

- (int)readFileAtPath:(NSString *)path 
             userData:(id)userData
               buffer:(char *)buffer 
                 size:(size_t)size 
               offset:(off_t)offset
                error:(NSError **)error {
    int res;
	int fd = [userData intValue];
	
    if (virt_lseek(fd, offset, SEEK_SET) == -1) {
		if (error)
			*error = [NSError errorWithPOSIXCode:errno];
		return NO;
    }
	
    res = virt_read(fd, buffer, size);
	
    if (res == -1)
	{
		if (error)
			*error = [NSError errorWithPOSIXCode:errno];
		return NO;
	}
	
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

	const int MAXPATHLEN=1024;
	int res;
	char buf[MAXPATHLEN];
	
    res = virt_readlink([[self makeAbsolutePath:path] fileSystemRepresentation], buf, MAXPATHLEN - 1);
    if (res == -1)
	{
		if (error)
			*error = [NSError errorWithPOSIXCode:errno];
        return nil;
	}
	
    buf[res] = '\0';

	return [NSString stringWithUTF8String:buf];
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

@end
