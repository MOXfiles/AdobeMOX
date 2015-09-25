

#include "MOX_AEIO_Dialogs.h"

#import "MOX_Video_Out_Controller.h"

bool
MOX_AEIO_Video_Out_Dialog(
	DialogBitDepth	&bitDepth,
	bool			&lossless,
	int				&quality, // 1-100
	DialogCodec		&codec,
	const void		*plugHndl,
	const void		*mwnd)
{
	bool result = false;

	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"MOX_Video_Out_Controller"];

	if(ui_controller_class)
	{
		MOX_Video_Out_Controller *ui_controller = [[ui_controller_class alloc]
													init:(OutDialogBitDepth)bitDepth
														lossless:lossless
														quality:quality
														codec:(OutDialogCodec)codec];
		if(ui_controller)
		{
			NSWindow *my_window = [ui_controller getWindow];
			
			if(my_window)
			{
				NSInteger modal_result;
				OutDialogResult dialog_result;
				
				// because we're running a modal on top of a modal, we have to do our own							
				// modal loop that we can exit without calling [NSApp endModal], which would also							
				// kill AE's modal dialog.
				NSModalSession modal_session = [NSApp beginModalSessionForWindow:my_window];
				
				do{
					modal_result = [NSApp runModalSession:modal_session];

					dialog_result = [ui_controller getResult];
				}
				while(dialog_result == OUT_DIALOG_RESULT_CONTINUE && modal_result == NSRunContinuesResponse);
				
				[NSApp endModalSession:modal_session];
				
				if(dialog_result == OUT_DIALOG_RESULT_OK || modal_result == NSRunStoppedResponse)
				{
					bitDepth = (DialogBitDepth)[ui_controller getDepth];
					lossless = [ui_controller getLossless];
					quality = [ui_controller getQuality];
					codec = (DialogCodec)[ui_controller getCodec];
				
					result = true;
				}
				
				[my_window close];
			}
			
			[ui_controller release];
		}
	}
	
	return result;
}
