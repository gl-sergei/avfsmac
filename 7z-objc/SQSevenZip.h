//
//  SQSevenZip.h
//  avfsmac
//
//  Created by Qoyllur on 11/5/10.
//  Copyright 2010 Qoyllur. All rights reserved.
//

#import <Cocoa/Cocoa.h>

struct sq_seven_zip_implementation;

@interface SQSevenZip : NSObject {

	struct sq_seven_zip_implementation *impl;
	
	NSString *fileName;
	
}

@property (retain, nonatomic) NSString* fileName;

-(SQSevenZip*)initWithFile:(NSString*)aFileName;
	-(void)dealloc;

@end
