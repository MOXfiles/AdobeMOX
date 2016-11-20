//
//  MOX_Video_Out_Controller.h
//
//  Created by Brendan Bolles on 9/21/15.
//  Copyright 2015 fnord. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef enum {
	OUT_DIALOG_RESULT_CONTINUE,
	OUT_DIALOG_RESULT_OK,
	OUT_DIALOG_RESULT_CANCEL
} OutDialogResult;

typedef enum {
	OUT_DIALOG_BITDEPTH_AUTO = 0,
	OUT_DIALOG_BITDEPTH_8 = 8,
	OUT_DIALOG_BITDEPTH_10 = 10,
	OUT_DIALOG_BITDEPTH_12 = 12,
	OUT_DIALOG_BITDEPTH_16 = 16,
	OUT_DIALOG_BITDEPTH_16_FLOAT = 116,
	OUT_DIALOG_BITDEPTH_32_FLOAT = 132
} OutDialogBitDepth;

typedef enum {
	OUT_DIALOG_CODEC_AUTO,
	OUT_DIALOG_CODEC_DIRAC,
	OUT_DIALOG_CODEC_OPENEXR,
	OUT_DIALOG_CODEC_JPEG,
	OUT_DIALOG_CODEC_JPEG2000,
	OUT_DIALOG_CODEC_PNG,
	OUT_DIALOG_CODEC_DPX,
	OUT_DIALOG_CODEC_UNCOMPRESSED
} OutDialogCodec;

@interface MOX_Video_Out_Controller : NSObject {
	IBOutlet NSWindow *theWindow;
	IBOutlet NSPopUpButton *depthMenu;
	IBOutlet NSButton *losslessCheckbox;
	IBOutlet NSSlider *qualitySlider;
	IBOutlet NSTextField *qualityLabel;
	IBOutlet NSTextField *qualityReadout;
	IBOutlet NSPopUpButton *codecMenu;
	
	OutDialogResult theResult;
}

- (id)init:(OutDialogBitDepth)depth
	lossless:(BOOL)lossless
	quality:(int)quality
	codec:(OutDialogCodec)codec;

- (IBAction)clickedCancel:(id)sender;
- (IBAction)clickedOK:(id)sender;
- (IBAction)trackLossless:(id)sender;

- (NSWindow *)getWindow;
- (OutDialogResult)getResult;

- (void)setDepth:(OutDialogBitDepth)depth;
- (void)setLossless:(BOOL)lossless;
- (void)setQuality:(int)quality;
- (void)setCodec:(OutDialogCodec)codec;

- (OutDialogBitDepth)getDepth;
- (BOOL)getLossless;
- (int)getQuality;
- (OutDialogCodec)getCodec;

@end
