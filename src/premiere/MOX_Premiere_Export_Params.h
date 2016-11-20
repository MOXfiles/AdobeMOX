///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015, Brendan Bolles
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *	   Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *	   Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
//
// MOX plug-in for Premiere
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------


#ifndef MOX_PREMIERE_EXPORT_PARAMS_H
#define MOX_PREMIERE_EXPORT_PARAMS_H


#include "MOX_Premiere_Export.h"


#ifndef ADBEVideoAlpha
#define ADBEVideoAlpha		"ADBEVideoAlpha"
#endif

#define MOXPluginVersion	"MOXPluginVersion"


typedef enum {
	VideoBitDepth_8bit = 8,
	VideoBitDepth_10bit = 10,
	VideoBitDepth_12bit = 12,
	VideoBitDepth_16bit = 16,
	VideoBitDepth_16bit_Float = 253,
	VideoBitDepth_32bit_Float = 254
} MOX_VideoBitDepth;

typedef enum {
	VideoCodec_Auto = 0,
	VideoCodec_Dirac,
	VideoCodec_OpenEXR,
	VideoCodec_JPEG,
	VideoCodec_JPEG2000,
	VideoCodec_PNG,
	VideoCodec_DPX,
	VideoCodec_Uncompressed
} MOX_VideoCodec;

typedef enum {
	AudioBitDepth_8bit = 8,
	AudioBitDepth_16bit = 16,
	AudioBitDepth_24bit = 24,
	AudioBitDepth_32bit = 32,
	AudioBitDepth_32bit_Float = 254
} MOX_AudioBitDepth;


#define MOXVideoBitDepth	"MOXVideoBitDepth"
#define MOXLossless			"MOXLossless"
#define MOXQuality			"MOXQuality"
#define MOXVideoCodec		"MOXVideoCodec"
#define MOXAudioBitDepth	"MOXAudioBitDepth"


prMALError
exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP);

prMALError
exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec);

prMALError
exSDKPostProcessParams(
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP);

prMALError
exSDKGetParamSummary(
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP);

prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP);
	



#endif // WEBM_PREMIERE_EXPORT_PARAMS_H