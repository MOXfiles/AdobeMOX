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


#include "MOX_Premiere_Import.h"

#include <MoxFiles/InputFile.h>
#include <MoxFiles/Thread.h>
#include <MoxMxf/PlatformIOStream.h>

#include <assert.h>
#include <math.h>

#include <sstream>
#include <map>
#include <queue>

#ifdef PRMAC_ENV
	#include <mach/mach.h>
#endif

int g_num_cpus = 1;



#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef PrSDKPPixCacheSuite2 PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion2
#else
typedef PrSDKPPixCacheSuite PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion
#endif


using namespace MoxFiles;


typedef struct
{	
	csSDK_int32				importerID;
	csSDK_int32				fileType;
	csSDK_int32				width;
	csSDK_int32				height;
	csSDK_int32				frameRateNum;
	csSDK_int32				frameRateDen;
	
	PlatformIOStream		*stream;
	MoxFiles::InputFile		*file;
	
	csSDK_uint8				bit_depth;
	float					audioSampleRate;
	int						numChannels;
	
	
	PlugMemoryFuncsPtr		memFuncs;
	SPBasicSuite			*BasicSuite;
	PrSDKPPixCreatorSuite	*PPixCreatorSuite;
	PrCacheSuite			*PPixCacheSuite;
	PrSDKPPixSuite			*PPixSuite;
	PrSDKPPix2Suite			*PPix2Suite;
	PrSDKTimeSuite			*TimeSuite;
	PrSDKImporterFileManagerSuite *FileSuite;
} ImporterLocalRec8, *ImporterLocalRec8Ptr, **ImporterLocalRec8H;


// http://matroska.org/technical/specs/notes.html#TimecodeScale
// Time (in nanoseconds) = TimeCode * TimeCodeScale
// When we call finctions like GetTime, we're given Time in Nanoseconds.
//static const long long S2NS = 1000000000LL;


static prMALError 
SDKInit(
	imStdParms		*stdParms, 
	imImportInfoRec	*importInfo)
{
	int fourCC = 0;
	VersionInfo version = {0, 0, 0};
	
	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParms->piSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);

		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_Version, (void *)&version);
	
		stdParms->piSuites->utilFuncs->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		// use the AE plug-in in AE, please
		if(fourCC == kAppAfterEffects)
			return imUnsupported;
	}


	importInfo->canSave				= kPrFalse;		// Can 'save as' files to disk, real file only.
	importInfo->canDelete			= kPrFalse;		// File importers only, use if you only if you have child files
	importInfo->canCalcSizes		= kPrFalse;		// These are for importers that look at a whole tree of files so
													// Premiere doesn't know about all of them.
	importInfo->canTrim				= kPrFalse;
	
	importInfo->hasSetup			= kPrFalse;		// Set to kPrTrue if you have a setup dialog
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	
	importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	importInfo->priority			= 0;
	
	importInfo->avoidAudioConform	= kPrTrue;		// If I let Premiere conform the audio, I get silence when
													// I try to play it in the program.  Seems like a bug to me.

#ifdef PRMAC_ENV
	// get number of CPUs using Mach calls
	host_basic_info_data_t hostInfo;
	mach_msg_type_number_t infoCount;
	
	infoCount = HOST_BASIC_INFO_COUNT;
	host_info(mach_host_self(), HOST_BASIC_INFO, 
			  (host_info_t)&hostInfo, &infoCount);
	
	g_num_cpus = hostInfo.avail_cpus;
#else // WIN_ENV
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	g_num_cpus = systemInfo.dwNumberOfProcessors;
#endif

	return malNoError;
}


static prMALError
SDKShutdown()
{
	if( MoxFiles::supportsThreads() )
		MoxFiles::setGlobalThreadCount(0);
	
	return malNoError;
}


static prMALError 
SDKGetIndFormat(
	imStdParms		*stdParms, 
	csSDK_size_t	index, 
	imIndFormatRec	*SDKIndFormatRec)
{
	prMALError	result		= malNoError;
	char formatname[255]	= "NOX";
	char shortname[32]		= "NOX";
	char platformXten[256]	= "nox\0\0";

	switch(index)
	{
		//	Add a case for each filetype.
		
		case 0:		
			SDKIndFormatRec->filetype			= 'NOX ';

			SDKIndFormatRec->canWriteTimecode	= kPrFalse;
			SDKIndFormatRec->canWriteMetaData	= kPrFalse;

			SDKIndFormatRec->flags = xfCanImport | xfIsMovie;

			#ifdef PRWIN_ENV
			strcpy_s(SDKIndFormatRec->FormatName, sizeof (SDKIndFormatRec->FormatName), formatname);				// The long name of the importer
			strcpy_s(SDKIndFormatRec->FormatShortName, sizeof (SDKIndFormatRec->FormatShortName), shortname);		// The short (menu name) of the importer
			strcpy_s(SDKIndFormatRec->PlatformExtension, sizeof (SDKIndFormatRec->PlatformExtension), platformXten);	// The 3 letter extension
			#else
			strcpy(SDKIndFormatRec->FormatName, formatname);			// The Long name of the importer
			strcpy(SDKIndFormatRec->FormatShortName, shortname);		// The short (menu name) of the importer
			strcpy(SDKIndFormatRec->PlatformExtension, platformXten);	// The 3 letter extension
			#endif

			break;

		default:
			result = imBadFormatIndex;
	}

	return result;
}



prMALError 
SDKOpenFile8(
	imStdParms		*stdParms, 
	imFileRef		*SDKfileRef, 
	imFileOpenRec8	*SDKfileOpenRec8)
{
	prMALError			result = malNoError;

	ImporterLocalRec8H	localRecH = NULL;
	ImporterLocalRec8Ptr localRecP = NULL;

	if(SDKfileOpenRec8->privatedata)
	{
		localRecH = (ImporterLocalRec8H)SDKfileOpenRec8->privatedata;

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));

		localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *localRecH );
	}
	else
	{
		localRecH = (ImporterLocalRec8H)stdParms->piSuites->memFuncs->newHandle(sizeof(ImporterLocalRec8));
		SDKfileOpenRec8->privatedata = (PrivateDataPtr)localRecH;

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));

		localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *localRecH );
		
		localRecP->stream = NULL;
		localRecP->file = NULL;
		
		
		// Acquire needed suites
		localRecP->memFuncs = stdParms->piSuites->memFuncs;
		localRecP->BasicSuite = stdParms->piSuites->utilFuncs->getSPBasicSuite();
		if(localRecP->BasicSuite)
		{
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion, (const void**)&localRecP->PPixCreatorSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCacheSuite, PrCacheVersion, (const void**)&localRecP->PPixCacheSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion, (const void**)&localRecP->PPixSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion, (const void**)&localRecP->PPix2Suite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion, (const void**)&localRecP->TimeSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKImporterFileManagerSuite, kPrSDKImporterFileManagerSuiteVersion, (const void**)&localRecP->FileSuite);
		}

		localRecP->importerID = SDKfileOpenRec8->inImporterID;
		localRecP->fileType = SDKfileOpenRec8->fileinfo.filetype;
	}


	SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = reinterpret_cast<imFileRef>(imInvalidHandleValue);


	if(localRecP)
	{
		const prUTF16Char *path = SDKfileOpenRec8->fileinfo.filepath;
	
	#ifdef PRWIN_ENV
		HANDLE fileH = CreateFileW(path,
									GENERIC_READ,
									FILE_SHARE_READ,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);
									
		if(fileH != imInvalidHandleValue)
		{
			SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = fileH;
		}
		else
			result = imFileOpenFailed;
	#else
		FSIORefNum refNum = CAST_REFNUM(imInvalidHandleValue);
				
		CFStringRef filePathCFSR = CFStringCreateWithCharacters(NULL, path, prUTF16CharLength(path));
													
		CFURLRef filePathURL = CFURLCreateWithFileSystemPath(NULL, filePathCFSR, kCFURLPOSIXPathStyle, false);
		
		if(filePathURL != NULL)
		{
			FSRef fileRef;
			Boolean success = CFURLGetFSRef(filePathURL, &fileRef);
			
			if(success)
			{
				HFSUniStr255 dataForkName;
				FSGetDataForkName(&dataForkName);
			
				OSErr err = FSOpenFork(	&fileRef,
										dataForkName.length,
										dataForkName.unicode,
										fsRdWrPerm,
										&refNum);
			}
										
			CFRelease(filePathURL);
		}
									
		CFRelease(filePathCFSR);

		if(CAST_FILEREF(refNum) != imInvalidHandleValue)
		{
			SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = CAST_FILEREF(refNum);
		}
		else
			result = imFileOpenFailed;
	#endif

	}

	if(result == malNoError && localRecP->file == NULL)
	{
		assert(localRecP->stream == NULL);
		
		try
		{
			if( supportsThreads() )
				setGlobalThreadCount(g_num_cpus);
		
			localRecP->stream = new PlatformIOStream(CAST_REFNUM(*SDKfileRef));
			
			localRecP->file = new MoxFiles::InputFile(*localRecP->stream);
			
			assert(SDKfileOpenRec8->inReadWrite == kPrOpenFileAccess_ReadOnly);
		}
		catch(MoxMxf::IoExc &e)
		{	result = imFileOpenFailed;	}
		catch(MoxMxf::BaseExc &e)
		{	result = imBadHeader;	}
		catch(...)
		{	result = imOtherErr;	}
	}
	
	// close file and delete private data if we got a bad file
	if(result != malNoError)
	{
		if(SDKfileOpenRec8->privatedata)
		{
			stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(SDKfileOpenRec8->privatedata));
			SDKfileOpenRec8->privatedata = NULL;
		}
	}
	else
	{
		stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(SDKfileOpenRec8->privatedata));
	}

	return result;
}



static prMALError 
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	void				*privateData)
{
	// "Quiet File" really means close the file handle, but we're still
	// using it and might open it again, so hold on to any stored data
	// structures you don't want to re-create.

	// If file has not yet been closed
	if(SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );
		
		if(localRecP->file != NULL)
		{
			delete localRecP->file;
			
			localRecP->file = NULL;
		}
		
		if(localRecP->stream != NULL)
		{
			delete localRecP->stream;
			
			localRecP->stream = NULL;
		}

		stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	#ifdef PRWIN_ENV
		CloseHandle(*SDKfileRef);
	#else
		FSCloseFork( CAST_REFNUM(*SDKfileRef) );
	#endif
	
		*SDKfileRef = imInvalidHandleValue;
	}

	return malNoError; 
}


static prMALError 
SDKCloseFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef,
	void				*privateData) 
{
	ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);
	
	// If file has not yet been closed
	if(SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		SDKQuietFile(stdParms, SDKfileRef, privateData);
	}

	// Remove the privateData handle.
	// CLEANUP - Destroy the handle we created to avoid memory leaks
	if(ldataH && *ldataH && (*ldataH)->BasicSuite)
	{
		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCacheSuite, PrCacheVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKImporterFileManagerSuite, kPrSDKImporterFileManagerSuiteVersion);

		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(ldataH));
	}

	return malNoError;
}


static prMALError 
SDKGetIndPixelFormat(
	imStdParms			*stdParms,
	csSDK_size_t		idx,
	imIndPixelFormatRec	*SDKIndPixelFormatRec) 
{
	prMALError	result	= malNoError;
	
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKIndPixelFormatRec->privatedata);
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

	if(idx == 0)
	{
		const PrPixelFormat pix_fmt = (localRecP->bit_depth == 8 ? PrPixelFormat_BGRA_4444_8u :
										localRecP->bit_depth == 16 ? PrPixelFormat_BGRA_4444_16u :
										localRecP->bit_depth == 32 ? PrPixelFormat_BGRA_4444_32f :
										PrPixelFormat_BGRA_4444_8u);
										
		SDKIndPixelFormatRec->outPixelFormat = pix_fmt;
	}
	else if(idx == 1)
	{
		SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_BGRA_4444_8u;
	}
	else
		result = imBadFormatIndex;
	

	return result;	
}


// TODO: Support imDataRateAnalysis and we'll get a pretty graph in the Properties panel!
// Sounds like a good task for someone who wants to contribute to this open source project.


static prMALError 
SDKAnalysis(
	imStdParms		*stdParms,
	imFileRef		SDKfileRef,
	imAnalysisRec	*SDKAnalysisRec)
{
	// Is this all I'm supposed to do here?
	// The string shows up in the properties dialog.
	assert(SDKAnalysisRec->privatedata);
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKAnalysisRec->privatedata);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

	std::stringstream stream;

	stream << "Nothing here yet";
	
	if(SDKAnalysisRec->buffersize > stream.str().size())
		strcpy(SDKAnalysisRec->buffer, stream.str().c_str());
	
	
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return malNoError;
}


prMALError 
SDKGetInfo8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileAccessInfo8, 
	imFileInfoRec8		*SDKFileInfo8)
{
	prMALError					result				= malNoError;


	SDKFileInfo8->vidInfo.supportsAsyncIO			= kPrFalse;
	SDKFileInfo8->vidInfo.supportsGetSourceVideo	= kPrTrue;
	SDKFileInfo8->vidInfo.hasPulldown				= kPrFalse;
	SDKFileInfo8->hasDataRate						= kPrFalse;


	// private data
	assert(SDKFileInfo8->privatedata);
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKFileInfo8->privatedata);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

	if(localRecP == NULL || localRecP->file == NULL)
	{
		assert(false);
		return imOtherErr;
	}
	
	SDKFileInfo8->hasVideo = kPrFalse;
	SDKFileInfo8->hasAudio = kPrFalse;
	
	
	try
	{
		const Header &head = localRecP->file->header();
		
		const ChannelList &channels = head.channels();
		
		if(channels.size() > 0)
		{
			const Rational &fps = head.frameRate();
			const Rational &par = head.pixelAspectRatio();
			
			const bool has_alpha = (channels.findChannel("A") != NULL);
			const int num_channels = (has_alpha ? 4 : 3);
			
			int bit_depth = 8;
			
			for(ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
			{
				const Channel &chan = i.channel();
				
				if(bit_depth < PixelBits(chan.type))
				{
					bit_depth = PixelBits(chan.type);
				}
			}
		
			// Video information
			SDKFileInfo8->hasVideo				= kPrTrue;
			SDKFileInfo8->vidInfo.subType		= PrPixelFormat_BGRA_4444_8u;
			SDKFileInfo8->vidInfo.imageWidth	= head.width();
			SDKFileInfo8->vidInfo.imageHeight	= head.height();
			SDKFileInfo8->vidInfo.depth			= bit_depth * num_channels;
			SDKFileInfo8->vidInfo.fieldType		= prFieldsUnknown;
			SDKFileInfo8->vidInfo.isStill		= kPrFalse;
			SDKFileInfo8->vidInfo.noDuration	= imNoDurationFalse;
			SDKFileInfo8->vidDuration			= head.duration() * fps.Denominator;
			SDKFileInfo8->vidScale				= fps.Numerator;
			SDKFileInfo8->vidSampleSize			= fps.Denominator;

			SDKFileInfo8->vidInfo.alphaType		= (has_alpha ? alphaStraight : alphaNone);

			SDKFileInfo8->vidInfo.pixelAspectNum = par.Numerator;
			SDKFileInfo8->vidInfo.pixelAspectDen = par.Denominator;

			// store some values we want to get without going to the file
			localRecP->width = SDKFileInfo8->vidInfo.imageWidth;
			localRecP->height = SDKFileInfo8->vidInfo.imageHeight;

			localRecP->frameRateNum = SDKFileInfo8->vidScale;
			localRecP->frameRateDen = SDKFileInfo8->vidSampleSize;
			
			localRecP->bit_depth = SDKFileInfo8->vidInfo.depth;
		}

		
		const AudioChannelList &audioChannels = head.audioChannels();
		
		if(audioChannels.size() > 0)
		{
			PrAudioSampleType sample_type = kPrAudioSampleType_Other;
			
			for(AudioChannelList::ConstIterator ch = audioChannels.begin(); ch != audioChannels.end(); ++ch)
			{
				const AudioChannel &chan = ch.channel();
				
				PrAudioSampleType chan_depth;
				
				switch(chan.type)
				{
					case UNSIGNED8:		chan_depth = kPrAudioSampleType_8BitInt;	break;
					case SIGNED16:		chan_depth = kPrAudioSampleType_16BitInt;	break;
					case SIGNED24:		chan_depth = kPrAudioSampleType_24BitInt;	break;
					case SIGNED32:		chan_depth = kPrAudioSampleType_32BitInt;	break;
					case AFLOAT:		chan_depth = kPrAudioSampleType_32BitFloat;	break;
					
					default:
						throw MoxMxf::NoImplExc("");
				}
				
				if(sample_type == kPrAudioSampleType_Other)
				{
					sample_type = chan_depth;
				}
				else
				{
					assert(sample_type == chan_depth);
				}
			}
			
			const Rational &sample_rate = head.sampleRate();
			const float float_sample_rate = (float)sample_rate.Numerator / (float)sample_rate.Denominator;
			
			int num_channels = audioChannels.size();
			
			if(num_channels > 2 && num_channels != 6)
			{
				num_channels = 2; // can only do mono, stereo, and 5.1
			}
			
			// Audio information
			SDKFileInfo8->hasAudio				= kPrTrue;
			SDKFileInfo8->audInfo.numChannels	= num_channels;
			SDKFileInfo8->audInfo.sampleRate	= float_sample_rate;
			SDKFileInfo8->audInfo.sampleType	= sample_type;
			
			
			localRecP->audioSampleRate			= SDKFileInfo8->audInfo.sampleRate;
			localRecP->numChannels				= SDKFileInfo8->audInfo.numChannels;
		}
	}
	catch(...)
	{
		result = imOtherErr;
	}
	
			
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


static prMALError 
SDKPreferredFrameSize(
	imStdParms					*stdparms, 
	imPreferredFrameSizeRec		*preferredFrameSizeRec)
{
	prMALError			result	= imIterateFrameSizes;
	ImporterLocalRec8H	ldataH	= reinterpret_cast<ImporterLocalRec8H>(preferredFrameSizeRec->inPrivateData);

	stdparms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	// TODO: Make sure it really isn't possible to decode a smaller frame
	bool can_shrink = false;

	if(preferredFrameSizeRec->inIndex == 0)
	{
		preferredFrameSizeRec->outWidth = localRecP->width;
		preferredFrameSizeRec->outHeight = localRecP->height;
	}
	else
	{
		// we store width and height in private data so we can produce it here
		const int divisor = pow(2.0, preferredFrameSizeRec->inIndex);
		
		if(can_shrink &&
			preferredFrameSizeRec->inIndex < 4 &&
			localRecP->width % divisor == 0 &&
			localRecP->height % divisor == 0 )
		{
			preferredFrameSizeRec->outWidth = localRecP->width / divisor;
			preferredFrameSizeRec->outHeight = localRecP->height / divisor;
		}
		else
			result = malNoError;
	}


	stdparms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}



static prMALError 
SDKGetSourceVideo(
	imStdParms			*stdParms, 
	imFileRef			fileRef, 
	imSourceVideoRec	*sourceVideoRec)
{
	prMALError		result		= malNoError;

	// privateData
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(sourceVideoRec->inPrivateData);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	PrTime ticksPerSecond = 0;
	localRecP->TimeSuite->GetTicksPerSecond(&ticksPerSecond);

	csSDK_int32		theFrame	= 0;
	
	if(localRecP->frameRateDen == 0) // i.e. still frame
	{
		theFrame = 0;
	}
	else
	{
		PrTime ticksPerFrame = (ticksPerSecond * (PrTime)localRecP->frameRateDen) / (PrTime)localRecP->frameRateNum;
		theFrame = sourceVideoRec->inFrameTime / ticksPerFrame;
	}

	// Check to see if frame is already in cache
	result = localRecP->PPixCacheSuite->GetFrameFromCache(	localRecP->importerID,
															0,
															theFrame,
															1,
															sourceVideoRec->inFrameFormats,
															sourceVideoRec->outFrame,
															NULL,
															NULL);

	// If frame is not in the cache, read the frame and put it in the cache; otherwise, we're done
	if(result != suiteError_NoError)
	{
		// ok, we'll read the file - clear error
		result = malNoError;
		
		imFrameFormat *frameFormat = &sourceVideoRec->inFrameFormats[0];
		
		prRect theRect;
		if(frameFormat->inFrameWidth == 0 && frameFormat->inFrameHeight == 0)
		{
			frameFormat->inFrameWidth = localRecP->width;
			frameFormat->inFrameHeight = localRecP->height;
		}
		
		// Windows and MacOS have different definitions of Rects, so use the cross-platform prSetRect
		prSetRect(&theRect, 0, 0, frameFormat->inFrameWidth, frameFormat->inFrameHeight);
		
		
		
		try
		{
			InputFile *infile = localRecP->file;
			
			const Header &head = infile->header();
			
			const PrPixelFormat pix_fmt = frameFormat->inPixelFormat;
			
			
			PPixHand ppix;
			localRecP->PPixCreatorSuite->CreatePPix(&ppix, PrPPixBufferAccess_ReadWrite, pix_fmt, &theRect);
			

			char *frameBufferP = NULL;
			csSDK_int32 rowbytes = 0;
			
			localRecP->PPixSuite->GetPixels(ppix, PrPPixBufferAccess_ReadWrite, &frameBufferP);
			localRecP->PPixSuite->GetRowBytes(ppix, &rowbytes);
			
			char *origin = frameBufferP + (rowbytes * (frameFormat->inFrameHeight - 1));
			
			
			FrameBuffer frameBuffer(frameFormat->inFrameWidth, frameFormat->inFrameHeight);
			
			if(pix_fmt == PrPixelFormat_BGRA_4444_8u)
			{
				frameBuffer.insert("B", Slice(MoxFiles::UINT8, origin + (sizeof(unsigned char) * 0), sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("G", Slice(MoxFiles::UINT8, origin + (sizeof(unsigned char) * 1), sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("R", Slice(MoxFiles::UINT8, origin + (sizeof(unsigned char) * 2), sizeof(unsigned char) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("A", Slice(MoxFiles::UINT8, origin + (sizeof(unsigned char) * 3), sizeof(unsigned char) * 4, -rowbytes, 1, 1, 255));
			}
			else if(pix_fmt == PrPixelFormat_BGRA_4444_16u)
			{
				frameBuffer.insert("B", Slice(MoxFiles::UINT16A, origin + (sizeof(unsigned short) * 0), sizeof(unsigned short) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("G", Slice(MoxFiles::UINT16A, origin + (sizeof(unsigned short) * 1), sizeof(unsigned short) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("R", Slice(MoxFiles::UINT16A, origin + (sizeof(unsigned short) * 2), sizeof(unsigned short) * 4, -rowbytes, 1, 1, 0));
				frameBuffer.insert("A", Slice(MoxFiles::UINT16A, origin + (sizeof(unsigned short) * 3), sizeof(unsigned short) * 4, -rowbytes, 1, 1, 32768));
			}
			else if(pix_fmt == PrPixelFormat_BGRA_4444_32f)
			{
				frameBuffer.insert("B", Slice(MoxFiles::FLOAT, origin + (sizeof(float) * 0), sizeof(float) * 4, -rowbytes, 1, 1, 0.0));
				frameBuffer.insert("G", Slice(MoxFiles::FLOAT, origin + (sizeof(float) * 1), sizeof(float) * 4, -rowbytes, 1, 1, 0.0));
				frameBuffer.insert("R", Slice(MoxFiles::FLOAT, origin + (sizeof(float) * 2), sizeof(float) * 4, -rowbytes, 1, 1, 0.0));
				frameBuffer.insert("A", Slice(MoxFiles::FLOAT, origin + (sizeof(float) * 3), sizeof(float) * 4, -rowbytes, 1, 1, 1.0));
			}
			else
				assert(false);
			
			
			infile->getFrame(theFrame, frameBuffer);


			localRecP->PPixCacheSuite->AddFrameToCache(	localRecP->importerID,
														0,
														ppix,
														theFrame,
														NULL,
														NULL);
														
			*sourceVideoRec->outFrame = ppix;
			
			// Premiere will handle the disposing of the frame buffer
		}
		catch(...)
		{
			result = imOtherErr;
		}
	}


	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


												

static prMALError 
SDKImportAudio7(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imImportAudioRec7	*audioRec7)
{
	prMALError		result		= malNoError;

	// privateData
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(audioRec7->privateData);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );
		
	assert(audioRec7->position >= 0); // Do they really want contiguous samples?

	try
	{
		InputFile *infile = localRecP->file;
		
		const AudioChannelList &chans = infile->header().audioChannels();
		
		
		AudioBuffer buffer(audioRec7->size);
		
		const ptrdiff_t stride = sizeof(float);
		
		if(chans.size() == 1)
		{
			assert(localRecP->numChannels == 1);
		
			buffer.insert("Mono", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[0], stride));
		}
		if(chans.size() == 2)
		{
			assert(localRecP->numChannels == 2);
		
			buffer.insert("Left", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[0], stride));
			buffer.insert("Right", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[1], stride));
		}
		if(chans.size() == 6)
		{
			assert(localRecP->numChannels == 6);
		
			buffer.insert("Left", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[0], stride));
			buffer.insert("Right", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[1], stride));
			buffer.insert("RearLeft", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[2], stride));
			buffer.insert("RearRight", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[3], stride));
			buffer.insert("Center", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[4], stride));
			buffer.insert("LFE", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[5], stride));
		}
		else
		{
			// what we go to when there's some large number of channels
			assert(localRecP->numChannels == 2);
		
			buffer.insert("Channel1", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[0], stride));
			buffer.insert("Channel2", AudioSlice(MoxFiles::AFLOAT, (char *)audioRec7->buffer[1], stride));
		}
				
		
		infile->seekAudio(audioRec7->position);
		
		infile->readAudio(audioRec7->size, buffer);
	}
	catch(...)
	{
		result = imOtherErr;
	}
	
					
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));
	
	assert(result == malNoError);
	
	return result;
}


PREMPLUGENTRY DllExport xImportEntry (
	csSDK_int32		selector, 
	imStdParms		*stdParms, 
	void			*param1, 
	void			*param2)
{
	prMALError	result				= imUnsupported;

	try{

	switch (selector)
	{
		case imInit:
			result =	SDKInit(stdParms, 
								reinterpret_cast<imImportInfoRec*>(param1));
			break;

		case imShutdown:
			result =	SDKShutdown();
			break;

		case imGetInfo8:
			result =	SDKGetInfo8(stdParms, 
									reinterpret_cast<imFileAccessRec8*>(param1), 
									reinterpret_cast<imFileInfoRec8*>(param2));
			break;

		case imOpenFile8:
			result =	SDKOpenFile8(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<imFileOpenRec8*>(param2));
			break;
		
		case imQuietFile:
			result =	SDKQuietFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2); 
			break;

		case imCloseFile:
			result =	SDKCloseFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2);
			break;

		case imAnalysis:
			result =	SDKAnalysis(	stdParms,
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imAnalysisRec*>(param2));
			break;

		case imGetIndFormat:
			result =	SDKGetIndFormat(stdParms, 
										reinterpret_cast<csSDK_size_t>(param1),
										reinterpret_cast<imIndFormatRec*>(param2));
			break;

		case imGetIndPixelFormat:
			result = SDKGetIndPixelFormat(	stdParms,
											reinterpret_cast<csSDK_size_t>(param1),
											reinterpret_cast<imIndPixelFormatRec*>(param2));
			break;

		// Importers that support the Premiere Pro 2.0 API must return malSupports8 for this selector
		case imGetSupports8:
			result = malSupports8;
			break;

		case imGetPreferredFrameSize:
			result =	SDKPreferredFrameSize(	stdParms,
												reinterpret_cast<imPreferredFrameSizeRec*>(param1));
			break;

		case imGetSourceVideo:
			result =	SDKGetSourceVideo(	stdParms,
											reinterpret_cast<imFileRef>(param1),
											reinterpret_cast<imSourceVideoRec*>(param2));
			break;
			
		case imImportAudio7:
			result =	SDKImportAudio7(	stdParms,
											reinterpret_cast<imFileRef>(param1),
											reinterpret_cast<imImportAudioRec7*>(param2));
			break;

		case imCreateAsyncImporter:
			result =	imUnsupported;
			break;
	}
	
	}catch(...) { result = imOtherErr; }

	return result;
}

