/*
 *  MOX_AEIO_Dialogs.h
 *  MOX_AE
 *
 *  Created by Brendan Bolles on 9/11/15.
 *  Copyright 2015 fnord. All rights reserved.
 *
 */


#ifndef MOX_AEIO_DIALOGS_H
#define MOX_AEIO_DIALOGS_H

typedef enum {
	DIALOG_BITDEPTH_AUTO = 0,
	DIALOG_BITDEPTH_8 = 8,
	DIALOG_BITDEPTH_10 = 10,
	DIALOG_BITDEPTH_12 = 12,
	DIALOG_BITDEPTH_16 = 16,
	DIALOG_BITDEPTH_16_FLOAT = 116,
	DIALOG_BITDEPTH_32_FLOAT = 132
} DialogBitDepth;

typedef enum {
	DIALOG_CODEC_AUTO,
	DIALOG_CODEC_DIRAC,
	DIALOG_CODEC_OPENEXR,
	DIALOG_CODEC_JPEG,
	DIALOG_CODEC_JPEG2000,
	DIALOG_CODEC_JPEGLS,
	//DIALOG_CODEC_JPEGXT,
	DIALOG_CODEC_PNG,
	DIALOG_CODEC_DPX,
	DIALOG_CODEC_UNCOMPRESSED
} DialogCodec;


bool
MOX_AEIO_Video_Out_Dialog(
	DialogBitDepth	&bitDepth,
	bool			&lossless,
	int				&quality, // 1-100
	DialogCodec		&codec,
	const void		*plugHndl,
	const void		*mwnd);


#endif // MOX_AEIO_DIALOGS_H