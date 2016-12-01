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


#include "MOX_Premiere_Export_Params.h"


#include <assert.h>
#include <math.h>

#include <sstream>
#include <vector>

using std::string;

static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}


prMALError
exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP)
{
	prMALError result = malNoError;
	
	ExportSettings *privateData	= reinterpret_cast<ExportSettings *>(outputSettingsP->privateData);
	
	PrSDKExportParamSuite *paramSuite = privateData->exportParamSuite;
	
	const csSDK_uint32 exID = outputSettingsP->exporterPluginID;
	const csSDK_int32 mgroupIndex = 0;
	
	
	csSDK_uint32 videoBitrate = 0;
	
	
	if(outputSettingsP->inExportVideo)
	{
		PrTime ticksPerSecond = 0;
		privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
		
		
		exParamValues width, height, alpha, frameRate, pixelAspectRatio, fieldType;
	
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		
		outputSettingsP->outVideoWidth = width.value.intValue;
		outputSettingsP->outVideoHeight = height.value.intValue;
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
		
		
		exParamValues bitDepth, codec;
		
		paramSuite->GetParamValue(exID, mgroupIndex, MOXVideoBitDepth, &bitDepth);
		paramSuite->GetParamValue(exID, mgroupIndex, MOXVideoCodec, &codec);
		
		
		const MOX_VideoBitDepth depthVal = (MOX_VideoBitDepth)bitDepth.value.intValue;
		const size_t bits_per_sample = (depthVal == VideoBitDepth_8bit ? 8 :
										depthVal == VideoBitDepth_10bit ? 10 :
										depthVal == VideoBitDepth_12bit ? 12 :
										depthVal == VideoBitDepth_16bit ? 16 :
										depthVal == VideoBitDepth_16bit_Float ? 16 :
										depthVal == VideoBitDepth_32bit_Float ? 32 :
										8);
										
		const size_t channels = (alpha.value.intValue ? 4 : 3);
		const size_t bits_per_frame = width.value.intValue * height.value.intValue * channels * bits_per_sample;
		const float fps = (double)ticksPerSecond / (double)frameRate.value.timeValue;
		const size_t kbits_per_second = (double)bits_per_frame * fps / 1024.0;
		
		const float multiplier = (codec.value.intValue == VideoCodec_Uncompressed ? 1.0 : 0.5);
		
		videoBitrate += (double)kbits_per_second * multiplier;
	}
	
	
	if(outputSettingsP->inExportAudio)
	{
		exParamValues sampleRate, channelType;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		
		PrAudioChannelType audioFormat = (PrAudioChannelType)channelType.value.intValue;
		
		if(audioFormat < kPrAudioChannelType_Mono || audioFormat > kPrAudioChannelType_51)
			audioFormat = kPrAudioChannelType_Stereo;
			
		const int audioChannels = (audioFormat == kPrAudioChannelType_51 ? 6 :
									audioFormat == kPrAudioChannelType_Stereo ? 2 :
									audioFormat == kPrAudioChannelType_Mono ? 1 :
									2);
									
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		outputSettingsP->outAudioChannelType = audioFormat;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_Compressed;
		
		
		exParamValues bitDepth;
		paramSuite->GetParamValue(exID, mgroupIndex, MOXAudioBitDepth, &bitDepth);
		
		const MOX_AudioBitDepth depthVal = (MOX_AudioBitDepth)bitDepth.value.intValue;
		const size_t bits_per_sample = (depthVal == AudioBitDepth_8bit ? 8 :
										depthVal == AudioBitDepth_16bit ? 16 :
										depthVal == AudioBitDepth_24bit ? 24 :
										depthVal == AudioBitDepth_32bit ? 32 :
										depthVal == AudioBitDepth_32bit_Float ? 32 :
										24);
		
		videoBitrate += (sampleRate.value.floatValue * audioChannels * bits_per_sample / 1024);
	}
	
	// outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = videoBitrate;

#if EXPORTMOD_VERSION >= 5
	// always doing maximum precision
	outputSettingsP->outUseMaximumRenderPrecision = kPrTrue;
#endif

	return result;
}


prMALError
exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec)
{
	prMALError				result				= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(generateDefaultParamRec->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = generateDefaultParamRec->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	
	// get current settings
	PrParam widthP, heightP, parN, parD, fieldTypeP, frameRateP, channelsTypeP, sampleRateP;
	
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoWidth, &widthP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoHeight, &heightP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectNumerator, &parN);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectDenominator, &parD);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFieldType, &fieldTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFrameRate, &frameRateP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioChannelsType, &channelsTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioSampleRate, &sampleRateP);
	
	if(widthP.mInt32 == 0)
	{
		widthP.mInt32 = 1920;
	}
	
	if(heightP.mInt32 == 0)
	{
		heightP.mInt32 = 1080;
	}



	prUTF16Char groupString[256];
	
	// Video Tab
	exportParamSuite->AddMultiGroup(exID, &gIdx);
	
	utf16ncpy(groupString, "Video Tab", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBETopParamGroup, ADBEVideoTabGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	

	// Image Settings group
	utf16ncpy(groupString, "Video Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEBasicVideoGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;
	widthValues.value.intValue = widthP.mInt32;
	widthValues.disabled = kPrFalse;
	widthValues.hidden = kPrFalse;
	
	exNewParamInfo widthParam;
	widthParam.structVersion = 1;
	strncpy(widthParam.identifier, ADBEVideoWidth, 255);
	widthParam.paramType = exParamType_int;
	widthParam.flags = exParamFlag_none;
	widthParam.paramValues = widthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &widthParam);


	// height
	exParamValues heightValues;
	heightValues.structVersion = 1;
	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	heightValues.value.intValue = heightP.mInt32;
	heightValues.disabled = kPrFalse;
	heightValues.hidden = kPrFalse;
	
	exNewParamInfo heightParam;
	heightParam.structVersion = 1;
	strncpy(heightParam.identifier, ADBEVideoHeight, 255);
	heightParam.paramType = exParamType_int;
	heightParam.flags = exParamFlag_none;
	heightParam.paramValues = heightValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &heightParam);


	// pixel aspect ratio
	exParamValues parValues;
	parValues.structVersion = 1;
	parValues.rangeMin.ratioValue.numerator = 10;
	parValues.rangeMin.ratioValue.denominator = 11;
	parValues.rangeMax.ratioValue.numerator = 2;
	parValues.rangeMax.ratioValue.denominator = 1;
	parValues.value.ratioValue.numerator = parN.mInt32;
	parValues.value.ratioValue.denominator = parD.mInt32;
	parValues.disabled = kPrFalse;
	parValues.hidden = kPrFalse;
	
	exNewParamInfo parParam;
	parParam.structVersion = 1;
	strncpy(parParam.identifier, ADBEVideoAspect, 255);
	parParam.paramType = exParamType_ratio;
	parParam.flags = exParamFlag_none;
	parParam.paramValues = parValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &parParam);
	
	
	// field order
	if(fieldTypeP.mInt32 == prFieldsUnknown)
		fieldTypeP.mInt32 = prFieldsNone;
	
	exParamValues fieldOrderValues;
	fieldOrderValues.structVersion = 1;
	fieldOrderValues.value.intValue = fieldTypeP.mInt32;
	fieldOrderValues.disabled = kPrFalse;
	fieldOrderValues.hidden = kPrFalse;
	
	exNewParamInfo fieldOrderParam;
	fieldOrderParam.structVersion = 1;
	strncpy(fieldOrderParam.identifier, ADBEVideoFieldType, 255);
	fieldOrderParam.paramType = exParamType_int;
	fieldOrderParam.flags = exParamFlag_none;
	fieldOrderParam.paramValues = fieldOrderValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fieldOrderParam);


	// frame rate
	exParamValues fpsValues;
	fpsValues.structVersion = 1;
	fpsValues.rangeMin.timeValue = 1;
	timeSuite->GetTicksPerSecond(&fpsValues.rangeMax.timeValue);
	fpsValues.value.timeValue = frameRateP.mInt64;
	fpsValues.disabled = kPrFalse;
	fpsValues.hidden = kPrFalse;
	
	exNewParamInfo fpsParam;
	fpsParam.structVersion = 1;
	strncpy(fpsParam.identifier, ADBEVideoFPS, 255);
	fpsParam.paramType = exParamType_ticksFrameRate;
	fpsParam.flags = exParamFlag_none;
	fpsParam.paramValues = fpsValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fpsParam);


	// Alpha channel
	exParamValues alphaValues;
	alphaValues.structVersion = 1;
	alphaValues.value.intValue = kPrFalse;
	alphaValues.disabled = kPrFalse;
	alphaValues.hidden = kPrFalse;
	
	exNewParamInfo alphaParam;
	alphaParam.structVersion = 1;
	strncpy(alphaParam.identifier, ADBEVideoAlpha, 255);
	alphaParam.paramType = exParamType_bool;
	alphaParam.flags = exParamFlag_none;
	alphaParam.paramValues = alphaValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &alphaParam);
	
	
	// Video Codec Settings Group
	utf16ncpy(groupString, "Codec settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEVideoCodecGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
									
	
	// Bit depth
	exParamValues videoBitDepthValues;
	videoBitDepthValues.structVersion = 1;
	videoBitDepthValues.rangeMin.intValue = VideoBitDepth_8bit;
	videoBitDepthValues.rangeMax.intValue = VideoBitDepth_32bit_Float;
	videoBitDepthValues.value.intValue = VideoBitDepth_8bit;
	videoBitDepthValues.disabled = kPrFalse;
	videoBitDepthValues.hidden = kPrFalse;
	
	exNewParamInfo videoBitDepthParam;
	videoBitDepthParam.structVersion = 1;
	strncpy(videoBitDepthParam.identifier, MOXVideoBitDepth, 255);
	videoBitDepthParam.paramType = exParamType_int;
	videoBitDepthParam.flags = exParamFlag_none;
	videoBitDepthParam.paramValues = videoBitDepthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &videoBitDepthParam);
	
	
	// Lossless
	exParamValues losslessValues;
	losslessValues.structVersion = 1;
	losslessValues.value.intValue = kPrTrue;
	losslessValues.disabled = kPrFalse;
	losslessValues.hidden = kPrFalse;
	
	exNewParamInfo losslessParam;
	losslessParam.structVersion = 1;
	strncpy(losslessParam.identifier, MOXLossless, 255);
	losslessParam.paramType = exParamType_bool;
	losslessParam.flags = exParamFlag_none;
	losslessParam.paramValues = losslessValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &losslessParam);
	
	
	// Quality
	exParamValues qualityValues;
	qualityValues.structVersion = 1;
	qualityValues.rangeMin.intValue = 1;
	qualityValues.rangeMax.intValue = 100;
	qualityValues.value.intValue = 80;
	qualityValues.disabled = kPrTrue;
	qualityValues.hidden = kPrFalse;
	
	exNewParamInfo qualityParam;
	qualityParam.structVersion = 1;
	strncpy(qualityParam.identifier, MOXQuality, 255);
	qualityParam.paramType = exParamType_int;
	qualityParam.flags = exParamFlag_slider;
	qualityParam.paramValues = qualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &qualityParam);
	
	
	// Codec
	exParamValues codecValues;
	codecValues.structVersion = 1;
	codecValues.rangeMin.intValue = VideoCodec_Auto;
	codecValues.rangeMax.intValue = VideoCodec_Uncompressed;
	codecValues.value.intValue = VideoCodec_Auto;
	codecValues.disabled = kPrFalse;
	codecValues.hidden = kPrFalse;
	
	exNewParamInfo codecParam;
	codecParam.structVersion = 1;
	strncpy(codecParam.identifier, MOXVideoCodec, 255);
	codecParam.paramType = exParamType_int;
	codecParam.flags = exParamFlag_none;
	codecParam.paramValues = codecValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &codecParam);

									
	// Version
	exParamValues versionValues;
	versionValues.structVersion = 1;
	versionValues.rangeMin.intValue = 0;
	versionValues.rangeMax.intValue = INT_MAX;
	versionValues.value.intValue = MOX_PLUGIN_VERSION_MAJOR << 16 |
									MOX_PLUGIN_VERSION_MINOR << 8 |
									MOX_PLUGIN_VERSION_BUILD;
	versionValues.disabled = kPrTrue;
	versionValues.hidden = kPrTrue;
	
	exNewParamInfo versionParam;
	versionParam.structVersion = 1;
	strncpy(versionParam.identifier, MOXPluginVersion, 255);
	versionParam.paramType = exParamType_int;
	versionParam.flags = exParamFlag_none;
	versionParam.paramValues = versionValues;
	
	exportParamSuite->AddParam(exID, gIdx, MOXPluginVersion, &versionParam);
	


	// Audio Tab
	utf16ncpy(groupString, "Audio Tab", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBETopParamGroup, ADBEAudioTabGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);


	// Audio Settings group
	utf16ncpy(groupString, "Audio Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEAudioTabGroup, ADBEBasicAudioGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// Sample rate
	exParamValues sampleRateValues;
	sampleRateValues.value.floatValue = sampleRateP.mFloat64;
	sampleRateValues.disabled = kPrFalse;
	sampleRateValues.hidden = kPrFalse;
	
	exNewParamInfo sampleRateParam;
	sampleRateParam.structVersion = 1;
	strncpy(sampleRateParam.identifier, ADBEAudioRatePerSecond, 255);
	sampleRateParam.paramType = exParamType_float;
	sampleRateParam.flags = exParamFlag_none;
	sampleRateParam.paramValues = sampleRateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicAudioGroup, &sampleRateParam);
	
	
	// Channel type
	exParamValues channelTypeValues;
	channelTypeValues.value.intValue = channelsTypeP.mInt32;
	channelTypeValues.disabled = kPrFalse;
	channelTypeValues.hidden = kPrFalse;
	
	exNewParamInfo channelTypeParam;
	channelTypeParam.structVersion = 1;
	strncpy(channelTypeParam.identifier, ADBEAudioNumChannels, 255);
	channelTypeParam.paramType = exParamType_int;
	channelTypeParam.flags = exParamFlag_none;
	channelTypeParam.paramValues = channelTypeValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicAudioGroup, &channelTypeParam);
	
	
	// Audio Codec Settings Group
	utf16ncpy(groupString, "Codec settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEAudioTabGroup, ADBEAudioCodecGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// Bit depth
	exParamValues audioBitDepthValues;
	audioBitDepthValues.structVersion = 1;
	audioBitDepthValues.rangeMin.intValue = AudioBitDepth_8bit;
	audioBitDepthValues.rangeMax.intValue = AudioBitDepth_32bit_Float;
	audioBitDepthValues.value.intValue = AudioBitDepth_24bit;
	audioBitDepthValues.disabled = kPrFalse;
	audioBitDepthValues.hidden = kPrFalse;
	
	exNewParamInfo audioBitDepthParam;
	audioBitDepthParam.structVersion = 1;
	strncpy(audioBitDepthParam.identifier, MOXAudioBitDepth, 255);
	audioBitDepthParam.paramType = exParamType_int;
	audioBitDepthParam.flags = exParamFlag_none;
	audioBitDepthParam.paramValues = audioBitDepthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioBitDepthParam);
	
	

	exportParamSuite->SetParamsVersion(exID, 1);
	
	
	return result;
}


prMALError
exSDKPostProcessParams(
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP)
{
	prMALError		result	= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(postProcessParamsRecP->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	//PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = postProcessParamsRecP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	prUTF16Char paramString[256];
	
	
	// Image Settings group
	utf16ncpy(paramString, "Image Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEBasicVideoGroup, paramString);
	
									
	// width
	utf16ncpy(paramString, "Width", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoWidth, paramString);
	
	exParamValues widthValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthValues);

	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;

	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoWidth, &widthValues);
	
	
	// height
	utf16ncpy(paramString, "Height", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoHeight, paramString);
	
	exParamValues heightValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightValues);

	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	
	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoHeight, &heightValues);
	
	
	// pixel aspect ratio
	utf16ncpy(paramString, "Pixel Aspect Ratio", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAspect, paramString);
	
	csSDK_int32	PARs[][2] = {{1, 1}, {10, 11}, {40, 33}, {768, 702}, 
							{1024, 702}, {2, 1}, {4, 3}, {3, 2}};
							
	const char *PARStrings[] = {"Square pixels (1.0)",
								"D1/DV NTSC (0.9091)",
								"D1/DV NTSC Widescreen 16:9 (1.2121)",
								"D1/DV PAL (1.0940)", 
								"D1/DV PAL Widescreen 16:9 (1.4587)",
								"Anamorphic 2:1 (2.0)",
								"HD Anamorphic 1080 (1.3333)",
								"DVCPRO HD (1.5)"};


	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoAspect);
	
	exOneParamValueRec tempPAR;
	
	for(csSDK_int32 i=0; i < sizeof (PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		utf16ncpy(paramString, PARStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoAspect, &tempPAR, paramString);
	}
	
	
	// field type
	utf16ncpy(paramString, "Field Type", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFieldType, paramString);
	
	csSDK_int32	fieldOrders[] = {	prFieldsUpperFirst,
									prFieldsLowerFirst,
									prFieldsNone};
	
	const char *fieldOrderStrings[]	= {	"Upper First",
										"Lower First",
										"None"};

	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFieldType);
	
	exOneParamValueRec tempFieldOrder;
	for(int i=0; i < 3; i++)
	{
		tempFieldOrder.intValue = fieldOrders[i];
		utf16ncpy(paramString, fieldOrderStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFieldType, &tempFieldOrder, paramString);
	}
	
	
	// frame rate
	utf16ncpy(paramString, "Frame Rate", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFPS, paramString);
	
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	static const char *frameRateStrings[] = {	"10",
												"15",
												"23.976",
												"24",
												"25 (PAL)",
												"29.97 (NTSC)",
												"30",
												"47.952",
												"48",
												"50",
												"59.94",
												"60"};
	
	PrTime ticksPerSecond = 0;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		// is there overflow potential here?
		assert(frameRates[i] == ticksPerSecond * frameRateNumDens[i][1] / frameRateNumDens[i][0]);
	}
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFPS);
	
	exOneParamValueRec tempFrameRate;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		tempFrameRate.timeValue = frameRates[i];
		utf16ncpy(paramString, frameRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFPS, &tempFrameRate, paramString);
	}
	
	
	// Alpha channel
	utf16ncpy(paramString, "Include Alpha Channel", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAlpha, paramString);
	
	
	// Video codec settings
	utf16ncpy(paramString, "Codec settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoCodecGroup, paramString);
	
	
	// Video bit depth
	utf16ncpy(paramString, "Bit Depth", 255);
	exportParamSuite->SetParamName(exID, gIdx, MOXVideoBitDepth, paramString);
	
	const MOX_VideoBitDepth videoBitDepths[] = {	VideoBitDepth_8bit,
													VideoBitDepth_16bit,
													VideoBitDepth_16bit_Float,
													VideoBitDepth_32bit_Float };
	
	const char *videoBitDepthStrings[] = {	"8-bit",
											"16-bit",
											"16-bit float",
											"32-bit float" };
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, MOXVideoBitDepth);
	
	exOneParamValueRec tempVideoBitDepth;
	
	for(csSDK_int32 i=0; i < sizeof(videoBitDepths) / sizeof(MOX_VideoBitDepth); i++)
	{
		tempVideoBitDepth.intValue = videoBitDepths[i];
		utf16ncpy(paramString, videoBitDepthStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, MOXVideoBitDepth, &tempVideoBitDepth, paramString);
	}
	
	
	// Lossless
	utf16ncpy(paramString, "Lossless", 255);
	exportParamSuite->SetParamName(exID, gIdx, MOXLossless, paramString);
	
	
	// Quality
	utf16ncpy(paramString, "Quality", 255);
	exportParamSuite->SetParamName(exID, gIdx, MOXQuality, paramString);
	
	exParamValues qualityValues;
	exportParamSuite->GetParamValue(exID, gIdx, MOXQuality, &qualityValues);
	
	qualityValues.rangeMin.intValue = 1;
	qualityValues.rangeMax.intValue = 100;
	
	exportParamSuite->ChangeParam(exID, gIdx, MOXQuality, &qualityValues);

	
	
	// Video codec
	utf16ncpy(paramString, "Codec", 255);
	exportParamSuite->SetParamName(exID, gIdx, MOXVideoCodec, paramString);
	
	const MOX_VideoCodec videoCodecs[] = {	VideoCodec_Auto,
											VideoCodec_Dirac,
											VideoCodec_OpenEXR,
											VideoCodec_JPEG,
											VideoCodec_JPEG2000,
											VideoCodec_JPEGLS,
											VideoCodec_PNG,
											VideoCodec_DPX,
											VideoCodec_Uncompressed };
	
	const char *videoCodecStrings[] = { "Auto",
										"Dirac",
										"OpenEXR",
										"JPEG",
										"JPEG 2000",
										"JPEG-LS",
										"PNG",
										"DPX",
										"Uncompressed" };
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, MOXVideoCodec);
	
	exOneParamValueRec tempVideoCodec;
	
	for(csSDK_int32 i=0; i < sizeof(videoCodecs) / sizeof(MOX_VideoCodec); i++)
	{
		tempVideoCodec.intValue = videoCodecs[i];
		utf16ncpy(paramString, videoCodecStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, MOXVideoCodec, &tempVideoCodec, paramString);
	}
	

	// Audio Settings group
	utf16ncpy(paramString, "Audio Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEBasicAudioGroup, paramString);
	
	
	// Sample rate
	utf16ncpy(paramString, "Sample Rate", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioRatePerSecond, paramString);
	
	float sampleRates[] = { 8000.0f, 16000.0f, 32000.0f, 44100.0f, 48000.0f, 96000.0f };
	
	const char *sampleRateStrings[] = { "8000 Hz", "16000 Hz", "32000 Hz", "44100 Hz", "48000 Hz", "96000 Hz" };
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEAudioRatePerSecond);
	
	exOneParamValueRec tempSampleRate;
	
	for(csSDK_int32 i=0; i < sizeof(sampleRates) / sizeof(float); i++)
	{
		tempSampleRate.floatValue = sampleRates[i];
		utf16ncpy(paramString, sampleRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEAudioRatePerSecond, &tempSampleRate, paramString);
	}

	
	// Channels
	utf16ncpy(paramString, "Channels", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioNumChannels, paramString);
	
	csSDK_int32 channelTypes[] = { kPrAudioChannelType_Mono,
									kPrAudioChannelType_Stereo,
									kPrAudioChannelType_51 };
	
	const char *channelTypeStrings[] = { "Mono", "Stereo", "Dolby 5.1" };
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEAudioNumChannels);
	
	exOneParamValueRec tempChannelType;
	
	for(csSDK_int32 i=0; i < sizeof(channelTypes) / sizeof(csSDK_int32); i++)
	{
		tempChannelType.intValue = channelTypes[i];
		utf16ncpy(paramString, channelTypeStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEAudioNumChannels, &tempChannelType, paramString);
	}
	
	
	// Audio codec settings
	utf16ncpy(paramString, "Codec settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioCodecGroup, paramString);
	
	
	// Audio bit depth
	utf16ncpy(paramString, "Bit Depth", 255);
	exportParamSuite->SetParamName(exID, gIdx, MOXAudioBitDepth, paramString);
	
	MOX_AudioBitDepth audioBitDepths[] = { AudioBitDepth_8bit, AudioBitDepth_16bit, AudioBitDepth_24bit };
	
	const char *audioBitDepthStrings[] = { "8-bit", "16-bit", "24-bit" };
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, MOXAudioBitDepth);
	
	exOneParamValueRec tempAudioBitDepth;
	
	for(csSDK_int32 i=0; i < sizeof(audioBitDepths) / sizeof(MOX_AudioBitDepth); i++)
	{
		tempAudioBitDepth.intValue = audioBitDepths[i];
		utf16ncpy(paramString, audioBitDepthStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, MOXAudioBitDepth, &tempAudioBitDepth, paramString);
	}
	

	return result;
}


prMALError
exSDKGetParamSummary(
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(summaryRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	std::string summary1, summary2, summary3;

	csSDK_uint32	exID	= summaryRecP->exporterPluginID;
	csSDK_int32		gIdx	= 0;
	
	// Standard settings
	exParamValues width, height, frameRate;
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRate);
	
	exParamValues bitDepth, lossless, quality, codec;
	paramSuite->GetParamValue(exID, gIdx, MOXVideoBitDepth, &bitDepth);
	paramSuite->GetParamValue(exID, gIdx, MOXLossless, &lossless);
	paramSuite->GetParamValue(exID, gIdx, MOXQuality, &quality);
	paramSuite->GetParamValue(exID, gIdx, MOXVideoCodec, &codec);
	
	exParamValues sampleRateP, channelTypeP, audioBitDepthP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);
	paramSuite->GetParamValue(exID, gIdx, MOXAudioBitDepth, &audioBitDepthP);

	
	
	// oh boy, figure out frame rate
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	static const char *frameRateStrings[] = {	"10",
												"15",
												"23.976",
												"24",
												"25 (PAL)",
												"29.97 (NTSC)",
												"30",
												"47.952",
												"48",
												"50",
												"59.94",
												"60"};
	
	PrTime ticksPerSecond = 0;
	privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	csSDK_int32 frame_rate_index = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(frameRates[i] == frameRate.value.timeValue)
			frame_rate_index = i;
	}


	std::stringstream stream1;
	
	stream1 << width.value.intValue << "x" << height.value.intValue;
	
	if(frame_rate_index >= 0 && frame_rate_index < 12) 
		stream1 << ", " << frameRateStrings[frame_rate_index] << " fps";
	
	summary1 = stream1.str();
	
	
	std::stringstream stream2;
	
	stream2 << (int)sampleRateP.value.floatValue << " Hz";
	stream2 << ", " << (channelTypeP.value.intValue == kPrAudioChannelType_51 ? "Dolby 5.1" :
						channelTypeP.value.intValue == kPrAudioChannelType_Mono ? "Mono" :
						"Stereo");

	const MOX_AudioBitDepth audioDepthVal = (MOX_AudioBitDepth)audioBitDepthP.value.intValue;
	const std::string audioDepth_str = (audioDepthVal == AudioBitDepth_8bit ? "8-bit" :
										audioDepthVal == AudioBitDepth_16bit ? "16-bit" :
										audioDepthVal == AudioBitDepth_24bit ? "24-bit" :
										audioDepthVal == AudioBitDepth_32bit ? "32-bit" :
										audioDepthVal == AudioBitDepth_32bit_Float ? "32-bit float" :
										"Unknown");
	stream2 << ", " << audioDepth_str;
	
	summary2 = stream2.str();
	
	
	const MOX_VideoBitDepth videoDepth = (MOX_VideoBitDepth)bitDepth.value.intValue;
	const bool videoLossless = lossless.value.intValue;
	const int videoQuality = quality.value.intValue;
	const MOX_VideoCodec videoCodec = (MOX_VideoCodec)codec.value.intValue;
	
	const std::string bitDepth_str = (videoDepth == VideoBitDepth_8bit ? "8-bit" :
								videoDepth == VideoBitDepth_10bit ? "10-bit" :
								videoDepth == VideoBitDepth_12bit ? "12-bit" :
								videoDepth == VideoBitDepth_16bit ? "16-bit" :
								videoDepth == VideoBitDepth_16bit_Float ? "16-bit float" :
								videoDepth == VideoBitDepth_32bit_Float ? "32-bit float" :
								"Unknown");
	
	std::stringstream quality_str;
	
	if(videoLossless)
	{
		quality_str << "Lossless";
	}
	else
	{
		quality_str << "Quality " << videoQuality;
	}
	
	const std::string codec_str = (videoCodec == VideoCodec_Dirac ? "Dirac" :
									videoCodec == VideoCodec_OpenEXR ? "OpenEXR" :
									videoCodec == VideoCodec_JPEG ? "JPEG" :
									videoCodec == VideoCodec_JPEG2000 ? "JPEG 2000" :
									videoCodec == VideoCodec_JPEGLS ? "JPEG-LS" :
									videoCodec == VideoCodec_PNG ? "PNG" :
									videoCodec == VideoCodec_DPX ? "DPX" :
									videoCodec == VideoCodec_Uncompressed ? "Uncompressed" :
									"Auto");
	
	std::stringstream stream3;
	
	stream3 << bitDepth_str << ", " << quality_str.str() << ", "<< codec_str << " codec";
	
	summary3 = stream3.str();
	
	

	utf16ncpy(summaryRecP->Summary1, summary1.c_str(), 255);
	utf16ncpy(summaryRecP->Summary2, summary2.c_str(), 255);
	utf16ncpy(summaryRecP->Summary3, summary3.c_str(), 255);
	
	return malNoError;
}


prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(validateParamChangedRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	csSDK_int32 exID = validateParamChangedRecP->exporterPluginID;
	csSDK_int32 gIdx = validateParamChangedRecP->multiGroupIndex;
	
	std::string param = validateParamChangedRecP->changedParamIdentifier;
	
	if(param == MOXLossless)
	{
		exParamValues losslessValue, qualityValue;
		
		paramSuite->GetParamValue(exID, gIdx, MOXLossless, &losslessValue);
		paramSuite->GetParamValue(exID, gIdx, MOXQuality, &qualityValue);
		
		qualityValue.disabled = !!losslessValue.value.intValue;
		
		paramSuite->ChangeParam(exID, gIdx, MOXQuality, &qualityValue);
	}

	return malNoError;
}

