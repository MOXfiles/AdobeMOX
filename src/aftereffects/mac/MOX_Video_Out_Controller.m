//
//  MOX_Video_Out_Controller.m
//
//  Created by Brendan Bolles on 9/21/15.
//  Copyright 2015 fnord. All rights reserved.
//

#import "MOX_Video_Out_Controller.h"

@implementation MOX_Video_Out_Controller

- (id)init:(OutDialogBitDepth)depth
	lossless:(BOOL)lossless
	quality:(int)quality
	codec:(OutDialogCodec)codec
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"MOX_Video_Out_Dialog" owner:self]))
		return nil;
	
	[theWindow center];
	
	theResult = OUT_DIALOG_RESULT_CONTINUE;
	
	[self setDepth:depth];
	[self setLossless:lossless];
	[self setQuality:quality];
	[self setCodec:codec];
	
	return self;
}

- (IBAction)clickedCancel:(id)sender {
    theResult = OUT_DIALOG_RESULT_CANCEL;
}

- (IBAction)clickedOK:(id)sender {
    theResult = OUT_DIALOG_RESULT_OK;
}

- (IBAction)trackLossless:(id)sender {
	BOOL lossless = [self getLossless];
	
	[qualitySlider setEnabled:!lossless];
	[qualityLabel setTextColor:(lossless ? [NSColor disabledControlTextColor] : [NSColor textColor])];
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (OutDialogResult)getResult {
	return theResult;
}

- (void)setDepth:(OutDialogBitDepth)depth {
	[depthMenu selectItemWithTag:(NSInteger)depth];
}

- (void)setLossless:(BOOL)lossless {
	[losslessCheckbox setState:(lossless ? NSOnState : NSOffState)];
	[self trackLossless:nil];
}

- (void)setQuality:(int)quality {
	[qualitySlider setIntValue:quality];
	[qualityReadout setIntValue:quality];
}

- (void)setCodec:(OutDialogCodec)codec {
	[codecMenu selectItemWithTag:(NSInteger)codec];
}

- (OutDialogBitDepth)getDepth {
	return (OutDialogBitDepth)[[depthMenu selectedItem] tag];
}

- (BOOL)getLossless {
	return ([losslessCheckbox state] == NSOnState);
}

- (int)getQuality {
	return [qualitySlider intValue];
}

- (OutDialogCodec)getCodec {
	return (OutDialogCodec)[[codecMenu selectedItem] tag];
}

@end
