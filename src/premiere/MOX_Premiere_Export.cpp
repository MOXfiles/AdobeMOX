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



#include "MOX_Premiere_Export.h"

#include "MOX_Premiere_Export_Params.h"

#include <MoxFiles/OutputFile.cpp>

#include <MoxFiles/Thread.h>

#ifdef PRMAC_ENV
	#include <mach/mach.h>
#else
	#include <assert.h>
	#include <time.h>
	#include <math.h>

	//#define LONG_LONG_MAX LLONG_MAX
#endif






static const csSDK_int32 MOX_ID = 'MOX ';
static const csSDK_int32 MOX_Export_Class = 'MOX ';

extern int g_num_cpus;



class PrIOStream : public MoxMxf::IOStream
{
  public:
	PrIOStream(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject);
	virtual ~PrIOStream();
	
	virtual int FileSeek(MoxMxf::UInt64 offset);
	virtual MoxMxf::UInt64 FileRead(unsigned char *dest, MoxMxf::UInt64 size);
	virtual MoxMxf::UInt64 FileWrite(const unsigned char *source, MoxMxf::UInt64 size);
	virtual MoxMxf::UInt64 FileTell();
	virtual void FileFlush();
	virtual void FileTruncate(MoxMxf::Int64 newsize);
	virtual MoxMxf::Int64 FileSize();

  private:
	PrSDKExportFileSuite *_fileSuite;
	const csSDK_uint32 _fileObject;
};

PrIOStream::PrIOStream(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject) :
	_fileSuite(fileSuite),
	_fileObject(fileObject)
{
	prSuiteError err = _fileSuite->Open(_fileObject);
	
	if(err != suiteError_NoError)
		throw MoxMxf::IoExc("Error opening file for writing.");
}

PrIOStream::~PrIOStream()
{
	prSuiteError err = _fileSuite->Close(_fileObject);
	
	assert(err == suiteError_NoError);
}

int
PrIOStream::FileSeek(MoxMxf::UInt64 offset)
{
	prInt64 pos = 0;

	prSuiteError err = _fileSuite->Seek(_fileObject, offset, pos, fileSeekMode_Begin);
	
	return err;
}

MoxMxf::UInt64
PrIOStream::FileRead(unsigned char *dest, MoxMxf::UInt64 size)
{
	assert(false); // Premiere can't read files it's writing

	return 0;
}

MoxMxf::UInt64
PrIOStream::FileWrite(const unsigned char *source, MoxMxf::UInt64 size)
{
	prSuiteError err = _fileSuite->Write(_fileObject, (void *)source, size);
	
	return (err == suiteError_NoError ? size : 0);
}

MoxMxf::UInt64
PrIOStream::FileTell()
{
	prInt64 pos = 0;

// son of a gun, fileSeekMode_End and fileSeekMode_Current are flipped inside Premiere!
#define PR_SEEK_CURRENT fileSeekMode_End

	prSuiteError err = _fileSuite->Seek(_fileObject, 0, pos, PR_SEEK_CURRENT);
	
	assert(err == suiteError_NoError);
	
	return pos;
}

void
PrIOStream::FileFlush()
{
	// can't tell Premiere to flush
}

void
PrIOStream::FileTruncate(MoxMxf::Int64 newsize)
{
	assert(false); // can't truncate
}

MoxMxf::Int64
PrIOStream::FileSize()
{
	assert(false); // hopefully writers don't have to know this
	
	prInt64 pos = 0;

// son of a gun, fileSeekMode_End and fileSeekMode_Current are flipped inside Premiere!
#define PR_SEEK_END fileSeekMode_Current

	prSuiteError err = _fileSuite->Seek(_fileObject, 0, pos, PR_SEEK_END);
	
	assert(err == suiteError_NoError);
	
	return pos;
}


static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}
/*
static void
ncpyUTF16(char *dest, const prUTF16Char *src, int max_len)
{
	char *d = dest;
	const prUTF16Char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}

static int
mylog2(int val)
{
	int ret = 0;
	
	while( pow(2.0, ret) < val )
	{
		ret++;
	}
	
	return ret;
}
*/

static prMALError
exSDKStartup(
	exportStdParms		*stdParmsP, 
	exExporterInfoRec	*infoRecP)
{
	int fourCC = 0;
	VersionInfo version = {0, 0, 0};

	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParmsP->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);

		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_Version, (void *)&version);
	
		stdParmsP->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		// not a good idea to try to run a MediaCore exporter in AE
		if(fourCC == kAppAfterEffects)
			return exportReturn_IterateExporterDone;
	}
	

	infoRecP->fileType			= MOX_ID;
	
	utf16ncpy(infoRecP->fileTypeName, "NOX", 255);
	utf16ncpy(infoRecP->fileTypeDefaultExtension, "nox", 255);
	
	infoRecP->classID = MOX_Export_Class;
	
	infoRecP->exportReqIndex	= 0;
	infoRecP->wantsNoProgressBar = kPrFalse;
	infoRecP->hideInUI			= kPrFalse;
	infoRecP->doesNotSupportAudioOnly = kPrFalse;
	infoRecP->canExportVideo	= kPrTrue;
	infoRecP->canExportAudio	= kPrTrue;
	infoRecP->singleFrameOnly	= kPrFalse;
	
	infoRecP->interfaceVersion	= EXPORTMOD_VERSION;
	
	infoRecP->isCacheable		= kPrFalse;
	
	if(stdParmsP->interfaceVer >= 6 &&
		((fourCC == kAppPremierePro && version.major >= 9) ||
		 (fourCC == kAppMediaEncoder && version.major >= 9)))
	{
	#if EXPORTMOD_VERSION >= 6
		infoRecP->canConformToMatchParams = kPrTrue;
	#else
		// in earlier SDKs, we'll cheat and set this ourselves
		csSDK_uint32 *info = &infoRecP->isCacheable;
		info[1] = kPrTrue; // one spot past isCacheable
	#endif
	}

	return malNoError;
}


static prMALError
exSDKBeginInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result				= malNoError;
	SPErr					spError				= kSPNoError;
	ExportSettings			*mySettings;
	PrSDKMemoryManagerSuite	*memorySuite;
	csSDK_int32				exportSettingsSize	= sizeof(ExportSettings);
	SPBasicSuite			*spBasic			= stdParmsP->getSPBasicSuite();
	
	if(spBasic != NULL)
	{
		spError = spBasic->AcquireSuite(
			kPrSDKMemoryManagerSuite,
			kPrSDKMemoryManagerSuiteVersion,
			const_cast<const void**>(reinterpret_cast<void**>(&memorySuite)));
			
		mySettings = reinterpret_cast<ExportSettings *>(memorySuite->NewPtrClear(exportSettingsSize));

		if(mySettings)
		{
			mySettings->spBasic		= spBasic;
			mySettings->memorySuite	= memorySuite;
			
			spError = spBasic->AcquireSuite(
				kPrSDKExportParamSuite,
				kPrSDKExportParamSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportParamSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportFileSuite,
				kPrSDKExportFileSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportFileSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportInfoSuite,
				kPrSDKExportInfoSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportInfoSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportProgressSuite,
				kPrSDKExportProgressSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportProgressSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixCreatorSuite,
				kPrSDKPPixCreatorSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixCreatorSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixSuite,
				kPrSDKPPixSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPix2Suite,
				kPrSDKPPix2SuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppix2Suite))));
			spError = spBasic->AcquireSuite(
				kPrSDKSequenceRenderSuite,
				kPrSDKSequenceRenderSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceRenderSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKSequenceAudioSuite,
				kPrSDKSequenceAudioSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceAudioSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKTimeSuite,
				kPrSDKTimeSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->timeSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKWindowSuite,
				kPrSDKWindowSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->windowSuite))));
		}


		instanceRecP->privateData = reinterpret_cast<void*>(mySettings);
	}
	else
	{
		result = exportReturn_ErrMemory;
	}
	
	return result;
}


static prMALError
exSDKEndInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result		= malNoError;
	ExportSettings			*lRec		= reinterpret_cast<ExportSettings *>(instanceRecP->privateData);
	SPBasicSuite			*spBasic	= stdParmsP->getSPBasicSuite();
	PrSDKMemoryManagerSuite	*memorySuite;
	if(spBasic != NULL && lRec != NULL)
	{
		if (lRec->exportParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion);
		}
		if (lRec->exportFileSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion);
		}
		if (lRec->exportInfoSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion);
		}
		if (lRec->exportProgressSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportProgressSuite, kPrSDKExportProgressSuiteVersion);
		}
		if (lRec->ppixCreatorSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion);
		}
		if (lRec->ppixSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		}
		if (lRec->ppix2Suite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		}
		if (lRec->sequenceRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion);
		}
		if (lRec->sequenceAudioSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceAudioSuite, kPrSDKSequenceAudioSuiteVersion);
		}
		if (lRec->timeSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		}
		if (lRec->windowSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}
		if (lRec->memorySuite)
		{
			memorySuite = lRec->memorySuite;
			memorySuite->PrDisposePtr(reinterpret_cast<PrMemoryPtr>(lRec));
			result = spBasic->ReleaseSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion);
		}
	}

	return result;
}



static prMALError
exSDKFileExtension(
	exportStdParms					*stdParmsP, 
	exQueryExportFileExtensionRec	*exportFileExtensionRecP)
{
	utf16ncpy(exportFileExtensionRecP->outFileExtension, "nox", 255);
		
	return malNoError;
}


static MoxFiles::Rational get_framerate(PrTime ticksPerSecond, PrTime ticks_per_frame)
{
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	int frameRateIndex = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(ticks_per_frame == frameRates[i])
			frameRateIndex = i;
	}
	
	MoxFiles::Rational fps(24, 1);
	
	if(frameRateIndex >= 0)
	{
		fps.Numerator = frameRateNumDens[frameRateIndex][0];
		fps.Denominator = frameRateNumDens[frameRateIndex][1];
	}
	else
	{
		fps.Numerator = 1001 * ticksPerSecond / ticks_per_frame;
		fps.Denominator = 1001;
	}
	
	return fps;
}


static prMALError
exSDKExport(
	exportStdParms	*stdParmsP,
	exDoExportRec	*exportInfoP)
{
	prMALError					result					= malNoError;
	ExportSettings				*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	PrSDKExportParamSuite		*paramSuite				= mySettings->exportParamSuite;
	PrSDKExportInfoSuite		*exportInfoSuite		= mySettings->exportInfoSuite;
	PrSDKSequenceRenderSuite	*renderSuite			= mySettings->sequenceRenderSuite;
	PrSDKSequenceAudioSuite		*audioSuite				= mySettings->sequenceAudioSuite;
	PrSDKMemoryManagerSuite		*memorySuite			= mySettings->memorySuite;
	PrSDKPPixCreatorSuite		*pixCreatorSuite		= mySettings->ppixCreatorSuite;
	PrSDKPPixSuite				*pixSuite				= mySettings->ppixSuite;
	PrSDKPPix2Suite				*pix2Suite				= mySettings->ppix2Suite;


	PrTime ticksPerSecond = 0;
	mySettings->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	
	csSDK_uint32 exID = exportInfoP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	exParamValues widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP, alphaP;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	
	const bool alpha = alphaP.value.intValue;
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);
	
	PrAudioChannelType audioFormat = (PrAudioChannelType)channelTypeP.value.intValue;
	
	if(audioFormat < kPrAudioChannelType_Mono || audioFormat > kPrAudioChannelType_51)
		audioFormat = kPrAudioChannelType_Stereo;
	
	const int audioChannels = (audioFormat == kPrAudioChannelType_51 ? 6 :
								audioFormat == kPrAudioChannelType_Stereo ? 2 :
								audioFormat == kPrAudioChannelType_Mono ? 1 :
								2);
	
		
	SequenceRender_ParamsRec renderParms;
	PrPixelFormat pixelFormats[] = { PrPixelFormat_BGRA_4444_8u };
									
	renderParms.inRequestedPixelFormatArray = pixelFormats;
	renderParms.inRequestedPixelFormatArrayCount = 1;
	renderParms.inWidth = widthP.value.intValue;
	renderParms.inHeight = heightP.value.intValue;
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatioP.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatioP.value.ratioValue.denominator;
	renderParms.inRenderQuality = kPrRenderQuality_High;
	renderParms.inFieldType = fieldTypeP.value.intValue;
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = kPrRenderQuality_High;
	renderParms.inCompositeOnBlack = kPrTrue;
	
	
	csSDK_uint32 videoRenderID = 0;
	
	if(exportInfoP->exportVideo)
	{
		result = renderSuite->MakeVideoRenderer(exID, &videoRenderID, frameRateP.value.timeValue);
	}
	
	csSDK_uint32 audioRenderID = 0;
	
	if(exportInfoP->exportAudio && result == malNoError)
	{
		result = audioSuite->MakeAudioRenderer(exID,
												exportInfoP->startTime,
												audioFormat,
												kPrAudioSampleType_32BitFloat,
												sampleRateP.value.floatValue, 
												&audioRenderID);
	}

	
	if(result == malNoError)
	{
		try
		{
			using namespace MoxFiles;
			
			if( supportsThreads() )
				setGlobalThreadCount(g_num_cpus);
		
			const Rational par(pixelAspectRatioP.value.ratioValue.numerator, pixelAspectRatioP.value.ratioValue.denominator);
			const Rational fps = get_framerate(ticksPerSecond, frameRateP.value.timeValue);
		
			Header head(widthP.value.intValue, heightP.value.intValue, par, fps, UNCOMPRESSED);
			
			MoxFiles::ChannelList &channels = head.channels();
			
			channels.insert("R", Channel(MoxFiles::UINT8));
			channels.insert("G", Channel(MoxFiles::UINT8));
			channels.insert("B", Channel(MoxFiles::UINT8));
			
			if(alpha)
				channels.insert("A", Channel(MoxFiles::UINT8));
			
			PrIOStream outstream(mySettings->exportFileSuite, exportInfoP->fileObject);
			
			OutputFile outfile(outstream, head);
			
			
			PrTime videoTime = exportInfoP->startTime;
			
			while(videoTime <= exportInfoP->endTime && result == malNoError)
			{
				SequenceRender_GetFrameReturnRec renderResult;
				
				result = renderSuite->RenderVideoFrame(videoRenderID,
														videoTime,
														&renderParms,
														kRenderCacheType_None,
														&renderResult);
			
				if(result == suiteError_NoError)
				{
					prRect bounds;
					csSDK_uint32 parN, parD;
					
					pixSuite->GetBounds(renderResult.outFrame, &bounds);
					pixSuite->GetPixelAspectRatio(renderResult.outFrame, &parN, &parD);
					
					const int width = bounds.right - bounds.left;
					const int height = bounds.bottom - bounds.top;
					
					assert(width == widthP.value.intValue);
					assert(height == heightP.value.intValue);
					assert(parN == pixelAspectRatioP.value.ratioValue.numerator);
					assert(parD == pixelAspectRatioP.value.ratioValue.denominator);
					
									
					PrPixelFormat pixFormat;
					char *frameBufferP = NULL;
					csSDK_int32 rowbytes = 0;
					
					pixSuite->GetPixelFormat(renderResult.outFrame, &pixFormat);
					pixSuite->GetPixels(renderResult.outFrame, PrPPixBufferAccess_ReadOnly, &frameBufferP);
					pixSuite->GetRowBytes(renderResult.outFrame, &rowbytes);
					
					if(pixFormat == PrPixelFormat_BGRA_4444_8u)
					{
						FrameBufferPtr frame = new FrameBuffer(width, height);
						
						char *origin = frameBufferP + ((height - 1) * rowbytes);
						
						frame->insert("B", Slice(MoxFiles::UINT8, origin + 0, sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
						frame->insert("G", Slice(MoxFiles::UINT8, origin + 1, sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
						frame->insert("R", Slice(MoxFiles::UINT8, origin + 2, sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
						
						if(alpha)
							frame->insert("A", Slice(MoxFiles::UINT8, origin + 3, sizeof(unsigned char) * 4, -rowbytes, 1, 1, 255));
						
						
						outfile.pushFrame(frame);
					}
					else
						assert(false);
				
					pixSuite->Dispose(renderResult.outFrame);
					
					
					
					const float progress = (double)(videoTime - exportInfoP->startTime) / (double)(exportInfoP->endTime - exportInfoP->startTime);

					result = mySettings->exportProgressSuite->UpdateProgressPercent(exID, progress);
					
					if(result == suiteError_ExporterSuspended)
					{
						result = mySettings->exportProgressSuite->WaitForResume(exID);
					}
				}
			
				videoTime += frameRateP.value.timeValue;
			}
		}
		catch(...)
		{
			result = exportReturn_InternalError;
		}
	}
	
	
	
	if(exportInfoP->exportVideo)
		renderSuite->ReleaseVideoRenderer(exID, videoRenderID);

	if(exportInfoP->exportAudio)
		audioSuite->ReleaseAudioRenderer(exID, audioRenderID);
	

	return result;
}




DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParmsP, 
	void			*param1, 
	void			*param2)
{
	prMALError result = exportReturn_Unsupported;
	
	switch (selector)
	{
		case exSelStartup:
			result = exSDKStartup(	stdParmsP, 
									reinterpret_cast<exExporterInfoRec*>(param1));
			break;

		case exSelBeginInstance:
			result = exSDKBeginInstance(stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelEndInstance:
			result = exSDKEndInstance(	stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelGenerateDefaultParams:
			result = exSDKGenerateDefaultParams(stdParmsP,
												reinterpret_cast<exGenerateDefaultParamRec*>(param1));
			break;

		case exSelPostProcessParams:
			result = exSDKPostProcessParams(stdParmsP,
											reinterpret_cast<exPostProcessParamsRec*>(param1));
			break;

		case exSelGetParamSummary:
			result = exSDKGetParamSummary(	stdParmsP,
											reinterpret_cast<exParamSummaryRec*>(param1));
			break;

		case exSelQueryOutputSettings:
			result = exSDKQueryOutputSettings(	stdParmsP,
												reinterpret_cast<exQueryOutputSettingsRec*>(param1));
			break;

		case exSelQueryExportFileExtension:
			result = exSDKFileExtension(stdParmsP,
										reinterpret_cast<exQueryExportFileExtensionRec*>(param1));
			break;

		case exSelValidateParamChanged:
			result = exSDKValidateParamChanged(	stdParmsP,
												reinterpret_cast<exParamChangedRec*>(param1));
			break;

		case exSelValidateOutputSettings:
			result = malNoError;
			break;

		case exSelExport:
			result = exSDKExport(	stdParmsP,
									reinterpret_cast<exDoExportRec*>(param1));
			break;
	}
	
	return result;
}
