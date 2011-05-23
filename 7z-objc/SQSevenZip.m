//
//  SQSevenZip.m
//  avfsmac
//
//  Created by Qoyllur on 11/5/10.
//  Copyright 2010 Qoyllur. All rights reserved.
//

#import "SQSevenZip.h"

#import "../7z/Archive/7z/7zIn.h"
#include "../7z/Archive/7z/7zAlloc.h"
#include "../7z/7zCrc.h"
#include "../7z/7zFile.h"


struct sq_seven_zip_implementation {
	
	CFileInStream archiveStream;
	CLookToRead lookStream;
	CSzArEx db;
	ISzAlloc allocImp;
	ISzAlloc allocTempImp;
	
};

@implementation SQSevenZip

@synthesize fileName;

-(SQSevenZip*)initWithFile:(NSString*)aFileName {
	
	self = [super init];
	
	if (self) {
	
		impl = malloc(sizeof(struct sq_seven_zip_implementation));
		
		if (impl) {
		
			[self setFileName:aFileName];
		
			if (!InFile_Open(&impl->archiveStream.file, [[self fileName] fileSystemRepresentation])) {
				
				FileInStream_CreateVTable(&impl->archiveStream);
				LookToRead_CreateVTable(&impl->lookStream, False);
				
				impl->lookStream.realStream = &impl->archiveStream.s;
				LookToRead_Init(&impl->lookStream);
				
				impl->allocImp.Alloc = SzAlloc;
				impl->allocImp.Free = SzFree;
				
				impl->allocTempImp.Alloc = SzAllocTemp;
				impl->allocTempImp.Free = SzFreeTemp;
				
				CrcGenerateTable();

				SzArEx_Init(&impl->db);
				
				SRes res = SzArEx_Open(&impl->db, &impl->lookStream.s, &impl->allocImp, &impl->allocTempImp);
				if (res != SZ_OK) {
					SzArEx_Free(&impl->db, &impl->allocImp);
					File_Close(&impl->archiveStream.file);
					free(impl);
					impl = NULL;
					[self release];
					self = nil;
				} else {
					UInt32 i;
					for (i = 0; i < impl->db.db.NumFiles; i++) {
						
						CSzFileItem *f = impl->db.db.Files + i;
						NSLog(@"{%s}", f->Name);
					}
				}
				
			}
			else {
				NSLog(@"could not open file %@", [self fileName]);
				free(impl);
				impl = NULL;
				[self release];
				self = nil;
			}
		} else {
			[self release];
			self = nil;
		}
		
	}

	return self;
	
}

-(void)dealloc {
	if (impl) {
		NSLog(@"SzArEx_Free");
		SzArEx_Free(&impl->db, &impl->allocImp);
		NSLog(@"File_Close");
		File_Close(&impl->archiveStream.file);
		free(impl);
	}
	
	[super dealloc];
	
}

@end
