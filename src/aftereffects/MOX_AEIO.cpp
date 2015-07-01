

#include "MOX_AEIO.h"


#include <MoxFiles/InputFile.h>
#include <MoxFiles/OutputFile.h>
#include <MoxFiles/Thread.h>
#include <MoxMxf/PlatformIOStream.h>

#include <map>

#include <assert.h>
#include <time.h>
//#include <sys/timeb.h>

#ifdef MAC_ENV
	#include <mach/mach.h>
#endif


class ErrThrower : std::exception
{
  public:
	ErrThrower(A_Err err = A_Err_NONE) throw() : _err(err) {}
	ErrThrower(const ErrThrower &other) throw() : _err(other._err) {}
	virtual ~ErrThrower() throw() {}

	ErrThrower & operator = (A_Err err)
	{
		_err = err;
		
		if(_err != A_Err_NONE)
			throw *this;
		
		return *this;
	}

	A_Err err() const { return _err; }
	
	virtual const char* what() const throw() { return "AE Error"; }
	
  private:
	A_Err _err;
};



class AEInputFile
{
  public:
	AEInputFile(const A_PathType *file_pathZ);
	~AEInputFile();

	MoxFiles::InputFile & file();
	
	void closeIfStale(int timeout);
	
  private:
	A_PathType *_path;
	static size_t pathLen(const A_PathType *path);
  
	PlatformIOStream *_stream;
	MoxFiles::InputFile *_file;
	
	time_t _last_access;
	void updateAccessTime();
	double timeSinceAccess() const;
};

AEInputFile::AEInputFile(const A_PathType *file_pathZ) :
	_stream(NULL),
	_file(NULL),
	_path(NULL)
{
	const size_t len = pathLen(file_pathZ);
	
	if(len == 0)
		throw MoxMxf::ArgExc("Empty path");
	
	_path = new A_PathType[len + 1];
	
	memcpy(_path, file_pathZ, sizeof(A_PathType) * (len + 1));
	

	_stream = new PlatformIOStream(_path, PlatformIOStream::ReadOnly);
	
	_file = new MoxFiles::InputFile(*_stream);
	
	updateAccessTime();
}

AEInputFile::~AEInputFile()
{
	delete _file;
	
	delete _stream;
	
	delete [] _path;
}

MoxFiles::InputFile &
AEInputFile::file()
{
	if(_stream == NULL)
	{
		_stream = new PlatformIOStream(_path, PlatformIOStream::ReadOnly);
	}
	
	if(_file == NULL)
	{
		_file = new MoxFiles::InputFile(*_stream);
	}
	
	updateAccessTime();
	
	return *_file;
}

void
AEInputFile::closeIfStale(int timeout)
{
	if(_file != NULL)
	{
		assert(_stream != NULL);
	
		if(timeSinceAccess() > timeout)
		{
			delete _file;
			_file = NULL;
			
			delete _stream;
			_stream = NULL;
		}
	}
}

size_t
AEInputFile::pathLen(const A_PathType *path)
{
	const A_PathType *p = path;
	
	size_t len = 0;
	
	while(*p++ != '\0')
		len++;
		
	return len;
}

void
AEInputFile::updateAccessTime()
{
	_last_access = time(NULL);
}

double
AEInputFile::timeSinceAccess() const
{
	return difftime(time(NULL), _last_access);
}

static std::map<AEIO_InSpecH, AEInputFile *> g_infiles;


class AEOutputFile
{
  public:
	AEOutputFile(const A_PathType *file_pathZ, const MoxFiles::Header &header);
	~AEOutputFile();
	
	MoxFiles::OutputFile & file() { return *_file; }
	
  private:
	PlatformIOStream *_stream;
	MoxFiles::OutputFile *_file;
};

AEOutputFile::AEOutputFile(const A_PathType *file_pathZ, const MoxFiles::Header &header) :
	_stream(NULL),
	_file(NULL)
{
	_stream = new PlatformIOStream(file_pathZ, PlatformIOStream::ReadWrite);
	
	_file = new MoxFiles::OutputFile(*_stream, header);
}

AEOutputFile::~AEOutputFile()
{
	delete _file;
	
	delete _stream;
}

static std::map<AEIO_OutSpecH, AEOutputFile *> g_outfiles;


#pragma mark-


typedef struct {
	A_u_char	magic[4]; // "MOX?"
	A_u_long	version;
	A_Boolean	flat; // just for testing
	A_u_char	reserve[55];

} MOX_InOptions;

typedef struct {
	A_u_char	magic[4]; // "MOX!"
	A_u_long	version;
	A_Boolean	flat; // just for testing
	A_u_char	reserve[56];

} MOX_OutOptions;


static void
InitOptions(MOX_InOptions *options)
{
	memset(options, 0, sizeof(MOX_InOptions));

	options->magic[0] = 'M';
	options->magic[1] = 'O';
	options->magic[2] = 'X';
	options->magic[3] = '?';
	
	options->version = 1;
	
	options->flat = FALSE;
}

static void
InitOptions(MOX_OutOptions *options)
{
	memset(options, 0, sizeof(MOX_OutOptions));

	options->magic[0] = 'M';
	options->magic[1] = 'O';
	options->magic[2] = 'X';
	options->magic[3] = '!';
	
	options->version = 1;
	
	options->flat = FALSE;
}

static void
CopyOptions(MOX_InOptions *dest, const MOX_InOptions *source)
{
	if(source->magic[0] == 'M' && source->magic[1] == 'O' &&
		source->magic[2] == 'X' && source->magic[3] == '?')
	{
		if(source->version == 1)
		{
			dest->flat = source->flat;
		}
		else
			assert(false);
	}
	else
		assert(false);
}

static void
CopyOptions(MOX_OutOptions *dest, const MOX_OutOptions *source)
{
	if(source->magic[0] == 'M' && source->magic[1] == 'O' &&
		source->magic[2] == 'X' && source->magic[3] == '!')
	{
		if(source->version == 1)
		{
			dest->flat = source->flat;
		}
		else
			assert(false);
	}
	else
		assert(false);
}

static void
FlattenOptions(MOX_InOptions *options)
{
	assert(options->flat == FALSE);
	
	options->flat = TRUE;
}

static void
FlattenOptions(MOX_OutOptions *options)
{
	assert(options->flat == FALSE);
	
	options->flat = TRUE;
}

static void
InflateOptions(MOX_InOptions *options)
{
	assert(options->flat == TRUE);
	
	options->flat = FALSE;
}

static void
InflateOptions(MOX_OutOptions *options)
{
	assert(options->flat == TRUE);
	
	options->flat = FALSE;
}


#pragma mark-


static int gNumCPUs = 1;


static A_Err
InitHook(
	struct SPBasicSuite *pica_basicP)
{
#ifdef MAC_ENV
	// get number of CPUs using Mach calls
	host_basic_info_data_t hostInfo;
	mach_msg_type_number_t infoCount;
	
	infoCount = HOST_BASIC_INFO_COUNT;
	host_info(mach_host_self(), HOST_BASIC_INFO, 
			  (host_info_t)&hostInfo, &infoCount);
	
	gNumCPUs = hostInfo.max_cpus;
#else // WIN_ENV
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	gNumCPUs = systemInfo.dwNumberOfProcessors;
#endif

	return A_Err_NONE;
}


static A_Err	
AEIO_Idle(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig,
	AEIO_IdleFlags			*idle_flags0)
{
	// AE expects me to close after every call, but that seems slow.
	// Instead I'll hang around for just a little
	const int timeout = 10;

	for(std::map<AEIO_InSpecH, AEInputFile *>::iterator i = g_infiles.begin(); i != g_infiles.end(); ++i)
	{
		i->second->closeIfStale(timeout);
	}

	return A_Err_NONE; 
}


static A_Err
DeathHook(	
	AEGP_GlobalRefcon unused1 ,
	AEGP_DeathRefcon unused2)
{
	assert(g_infiles.size() == 0); // all files were closed, right?

	if( MoxFiles::supportsThreads() )
		MoxFiles::setGlobalThreadCount(0);

	return A_Err_NONE;
}


#pragma mark-


static A_Err	
AEIO_VerifyFileImportable(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig, 
	const A_PathType		*file_pathZ, 
	A_Boolean				*importablePB)
{ 
	// TODO: Come up with a quick way to check if the file is really importable
	
	*importablePB = TRUE;
	
	return A_Err_NONE; 
}


static MoxFiles::Rational
AEFrameRate(MoxFiles::Rational fps)
{
	using namespace MoxFiles;

	// AE uses slightly different Rational values internally
	// Would not be a problem if AEGP_SetInSpecNativeFPS took an A_Ratio or A_Time *ahem*
	// See SDK guide for AEIO_GetTime

	if(fps == Rational(30000, 1001))
	{
		return Rational(2997, 100);
	}
	else if(fps == Rational(60000, 1001))
	{
		return Rational(2997, 50);
	}
	else if(fps == Rational(24000, 1001))
	{
		return Rational(2997, 125);
	}
	else
		return fps;
}


static unsigned int
AEBits(MoxFiles::PixelType type)
{
	switch(type)
	{
		case MoxFiles::UINT8:
			return 8;
		
		case MoxFiles::UINT10:
		case MoxFiles::UINT12:
		case MoxFiles::UINT16:
		case MoxFiles::UINT16A:
			return 16;
		
		case MoxFiles::UINT32:
			assert(false);
			return 32;
		
		case MoxFiles::HALF:
		case MoxFiles::FLOAT:
			return 32;
	}
	
	throw MoxMxf::ArgExc("Unknown pixel type");
}


static A_Err	
AEIO_InitInSpecFromFile(
	AEIO_BasicData		*basic_dataP,
	const A_PathType	*file_pathZ,
	AEIO_InSpecH		specH)
{ 
	A_Err ae_err =	A_Err_NONE;
	
	AEIO_Handle optionsH = NULL;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		ErrThrower err;
	
		using namespace MoxFiles;
		
		
		// Create options handle
		err = suites.MemorySuite()->AEGP_NewMemHandle( basic_dataP->aegp_plug_id, "Input Options",
														sizeof(MOX_InOptions),
														AEGP_MemFlag_CLEAR, &optionsH);
		AEIO_Handle old_optionsH = NULL;
		
		err = suites.IOInSuite()->AEGP_SetInSpecOptionsHandle(specH, (void*)optionsH, (void**)&old_optionsH);
		
		MOX_InOptions *options = NULL;
		
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		
		InitOptions(options);
		
		if(old_optionsH)
		{
			MOX_InOptions *old_options = NULL;
		
			err = suites.MemorySuite()->AEGP_LockMemHandle(old_optionsH, (void**)&old_options);
			
			CopyOptions(options, old_options);
			
			suites.MemorySuite()->AEGP_UnlockMemHandle(old_optionsH);
		}
		
		
		// open file
		if( supportsThreads() )
			setGlobalThreadCount(gNumCPUs);

	
		if(g_infiles.find(specH) == g_infiles.end())
		{
		#ifdef AE_HFS_PATHS	
			A_char pathZ[AEGP_MAX_PATH_SIZE];

			// convert the path format
			if(A_Err_NONE != ConvertPath(file_pathZ, pathZ, AEGP_MAX_PATH_SIZE-1) )
				return AEIO_Err_BAD_FILENAME;
		#else
			const A_PathType *pathZ = file_pathZ;
		#endif
		
			g_infiles[specH] = new AEInputFile(pathZ);
		}

		if(g_infiles[specH] == NULL)
			throw MoxMxf::NullExc("File is NULL");
		
		
		InputFile &file = g_infiles[specH]->file();
		
		const Header &head = file.header();
		
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
				
				if(bit_depth < AEBits(chan.type))
				{
					bit_depth = AEBits(chan.type);
				}
			}
			
			
			Rational ae_fps = AEFrameRate(fps);
			
			const A_Time duration = {head.duration() * fps.Denominator, fps.Numerator};
			const A_Fixed fixed_fps = FLOAT2FIX((double)ae_fps.Numerator / (double)ae_fps.Denominator);
			const A_Ratio pixel_aspect = {par.Numerator, par.Denominator};
			
			
			err = suites.IOInSuite()->AEGP_SetInSpecDepth(specH, bit_depth * num_channels);
			err = suites.IOInSuite()->AEGP_SetInSpecDuration(specH, &duration);
			err = suites.IOInSuite()->AEGP_SetInSpecDimensions(specH, head.width(), head.height());
			err = suites.IOInSuite()->AEGP_SetInSpecNativeFPS(specH, fixed_fps);
			err = suites.IOInSuite()->AEGP_SetInSpecHSF(specH, &pixel_aspect);
		}

		
		const AudioChannelList &audioChannels = head.audioChannels();
		
		if(audioChannels.size() > 0)
		{
			int bit_depth = 0;
			
			for(AudioChannelList::ConstIterator ch = audioChannels.begin(); ch != audioChannels.end(); ++ch)
			{
				const AudioChannel &chan = ch.channel();
				
				if(bit_depth == 0)
				{
					bit_depth = SampleBits(chan.type);
				}
				else
				{
					assert(bit_depth == SampleBits(chan.type));
				}
			}
			
			const Rational &sample_rate = head.sampleRate();
			
			int num_channels = audioChannels.size();
			
			if(num_channels > 2)
			{
				num_channels = 2; // can only do mono or stereo
			}
			
			const AEIO_SndEncoding sound_encoding = bit_depth == 8 ? AEIO_E_UNSIGNED_PCM :
													bit_depth == 16 ? AEIO_E_SIGNED_PCM :
													AEIO_E_SIGNED_FLOAT;
			
			const AEIO_SndSampleSize sample_size = bit_depth == 8 ? AEIO_SS_1 :
													bit_depth == 16 ? AEIO_SS_2 :
													AEIO_SS_4;
			
			const AEIO_SndChannels sound_channels = num_channels == 1 ? AEIO_SndChannels_MONO :
													AEIO_SndChannels_STEREO;
													
			
			err = suites.IOInSuite()->AEGP_SetInSpecSoundRate(specH, (double)sample_rate.Numerator / (double)sample_rate.Denominator);
			err = suites.IOInSuite()->AEGP_SetInSpecSoundEncoding(specH, sound_encoding);
			err = suites.IOInSuite()->AEGP_SetInSpecSoundSampleSize(specH, sample_size);
			err = suites.IOInSuite()->AEGP_SetInSpecSoundChannels(specH, sound_channels);
		}
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
	
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	
	return ae_err;
}


static A_Err
AEIO_InitInSpecInteractive(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH)
{
	assert(FALSE); // shouldn't get this

	return A_Err_NONE; 
}


static A_Err	
AEIO_GetInSpecInfo(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	AEIO_Verbiage	*verbiageP)
{ 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	strcpy(verbiageP->name, "Made-up name");
	strcpy(verbiageP->type, "NOX");
	strcpy(verbiageP->sub_type, "NOX alpha plug-in");

	return A_Err_NONE;
}


static A_Err	
AEIO_SeqOptionsDlg(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,  
	A_Boolean		*user_interactedPB0)
{ 
	basic_dataP->msg_func(0, "IO: Here's my sequence options dialog!");
	return A_Err_NONE; 
}


static A_Err
AEIO_FlattenOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		*flat_optionsPH)
{ 
	A_Err ae_err =	A_Err_NONE;
	
	AEIO_Handle optionsH = NULL;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		ErrThrower err;
		
		err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH));
		
		if(optionsH)
		{
			err = suites.MemorySuite()->AEGP_NewMemHandle( basic_dataP->aegp_plug_id, "Flat Options",
															sizeof(MOX_InOptions),
															AEGP_MemFlag_CLEAR, flat_optionsPH);
			MOX_InOptions *options = NULL,
							*flat_options = NULL;
			
			err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
			err = suites.MemorySuite()->AEGP_LockMemHandle(*flat_optionsPH, (void**)&flat_options);
			
			InitOptions(flat_options);
			
			CopyOptions(flat_options, options);
			
			FlattenOptions(flat_options);
			
			err = suites.MemorySuite()->AEGP_UnlockMemHandle(*flat_optionsPH);
		}
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
	
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	
	return ae_err;
}

static A_Err
AEIO_InflateOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		flat_optionsH)
{ 
	A_Err ae_err =	A_Err_NONE;
	
	AEIO_Handle optionsH = NULL;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		ErrThrower err;
		
		if(flat_optionsH)
		{
			err = suites.MemorySuite()->AEGP_NewMemHandle( basic_dataP->aegp_plug_id, "Round Options",
															sizeof(MOX_InOptions),
															AEGP_MemFlag_CLEAR, &optionsH);
			MOX_InOptions *options = NULL,
							*flat_options = NULL;
			
			err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
			err = suites.MemorySuite()->AEGP_LockMemHandle(flat_optionsH, (void**)&flat_options);
			
			InflateOptions(flat_options);
			
			InitOptions(options);
			
			CopyOptions(options, flat_options);
			
			FlattenOptions(flat_options);
			
			err = suites.MemorySuite()->AEGP_UnlockMemHandle(flat_optionsH);
			
			AEIO_Handle old_optionsH = NULL;
			
			err = suites.IOInSuite()->AEGP_SetInSpecOptionsHandle(specH, (void*)optionsH, (void**)&old_optionsH);
			
			assert(old_optionsH == NULL);
			
			// AE will take care of flat_optionsH
		}
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
	
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	
	return ae_err;
}


static A_Err
AEIO_SynchInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Boolean		*changed0)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_GetActiveExtent(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,				/* >> */
	const A_Time	*tr,				/* >> */
	A_LRect			*extent)			/* << */
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
}


static A_Err	
AEIO_GetDimensions(
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			 specH, 
	const AEIO_RationalScale *rs0,
	A_long					 *width0, 
	A_long					 *height0)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK; 
}
			

static A_Err	
AEIO_GetDuration(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Time			*tr)
{
	assert(false); // don't think this is getting called

	return AEIO_Err_USE_DFLT_CALLBACK; 
}


static A_Err	
AEIO_GetTime(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	A_Time			*tr)
{
	// I would *love* for this to be called because it would let me
	// Return FPS as an A_Time.  Doesn't appear to be happening though.
	assert(false);

	return AEIO_Err_USE_DFLT_CALLBACK; 
}


static A_Err	
AEIO_InqNextFrameTime(
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			specH, 
	const A_Time			*base_time_tr,
	AEIO_TimeDir			time_dir, 
	A_Boolean				*found0,
	A_Time					*key_time_tr0)
{
	// this does get called, but don't think I need to use it

	return AEIO_Err_USE_DFLT_CALLBACK; 
}


static A_Err	
AEIO_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	AEIO_InSpecH					specH, 
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP)
{
	A_Err ae_err =	A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		ErrThrower err;
	
		using namespace MoxFiles;
	
		if(g_infiles.find(specH) == g_infiles.end())
		{
		#ifdef AE_UNICODE_PATHS
			AEGP_MemHandle pathH = NULL;
			A_PathType *file_nameZ = NULL;
			
			suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
			
			if(pathH)
			{
				suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
			}
			else
				return AEIO_Err_BAD_FILENAME; 
		#else
			A_PathType file_nameZ[AEGP_MAX_PATH_SIZE];
			
			suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
		#endif
			
		#ifdef AE_HFS_PATHS	
			// convert the path format
			if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
				return AEIO_Err_BAD_FILENAME; 
		#endif
			
			
			g_infiles[specH] = new AEInputFile(file_nameZ);
			
			
		#ifdef AE_UNICODE_PATHS
			if(pathH)
				suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
		#endif
		}

		if(g_infiles[specH] == NULL)
			throw MoxMxf::NullExc("File is NULL");
		
		
		InputFile &file = g_infiles[specH]->file();
		
		const Header &head = file.header();
		
		
		PF_PixelFormat pixel_format;
		err = suites.PFWorldSuite()->PF_GetPixelFormat(wP, &pixel_format);
		
		
		PF_EffectWorld temp_World_data;
		PF_EffectWorld *temp_World = &temp_World_data;
		
		PF_EffectWorld *active_World = NULL;
		
		if(wP->width == head.width() && wP->height == head.height())
		{
			active_World = wP;
			
			temp_World = NULL;
		}
		else
		{
			// make our own PF_EffectWorld
			err = suites.PFWorldSuite()->PF_NewWorld(NULL, head.width(), head.height(), FALSE,
														pixel_format, temp_World);
													
			active_World = temp_World;
		}
		
		
		const MoxFiles::PixelType pixel_type = (pixel_format == PF_PixelFormat_ARGB32 ? MoxFiles::UINT8 :
												pixel_format == PF_PixelFormat_ARGB64 ? MoxFiles::UINT16A :
												pixel_format == PF_PixelFormat_ARGB128 ? MoxFiles::FLOAT :
												MoxFiles::UINT8);
		
		const size_t subpixel_size = PixelSize(pixel_type);
		const size_t pixel_size = 4 * subpixel_size;
		const ptrdiff_t rowbytes = active_World->rowbytes;
		
		const double rgb_fill = 0;
		const double alpha_fill = (pixel_type == MoxFiles::UINT8 ? 255 :
									pixel_type == MoxFiles::UINT16A ? 32768 :
									pixel_type == MoxFiles::FLOAT ? 1.0 :
									0);
		
		char *origin = (char *)active_World->data;
		
		FrameBuffer frame_buffer(active_World->width, active_World->height);
		
		frame_buffer.insert("A", Slice(pixel_type, origin + (subpixel_size * 0), pixel_size, rowbytes, 1, 1, alpha_fill));
		frame_buffer.insert("R", Slice(pixel_type, origin + (subpixel_size * 1), pixel_size, rowbytes, 1, 1, rgb_fill));
		frame_buffer.insert("G", Slice(pixel_type, origin + (subpixel_size * 2), pixel_size, rowbytes, 1, 1, rgb_fill));
		frame_buffer.insert("B", Slice(pixel_type, origin + (subpixel_size * 3), pixel_size, rowbytes, 1, 1, rgb_fill));
		
		
		const Rational &fps = head.frameRate();
		const Rational ae_fps = AEFrameRate(fps);
		
		const A_Time &frame_time = sparse_framePPB->tr;
		const Rational frame_time_rat(frame_time.value, frame_time.scale);
		const Rational frame_rat = frame_time_rat * ae_fps;
		
		assert(frame_rat.Numerator % frame_rat.Denominator == 0);
		assert(frame_rat.Denominator == 1);
		assert(frame_time.scale == ae_fps.Numerator || frame_time.value == 0);
		
		const int frame = (frame_rat.Denominator == 1 ? frame_rat.Numerator :
							(double)frame_rat.Numerator / (double)frame_rat.Denominator);
		
		file.getFrame(frame, frame_buffer);
		
		
		if(temp_World != NULL)
		{
			if(sparse_framePPB->qual == AEIO_Qual_HIGH)
				err = suites.PFWorldTransformSuite()->copy_hq(NULL, temp_World, wP, NULL, NULL);
			else
				err = suites.PFWorldTransformSuite()->copy(NULL, temp_World, wP, NULL, NULL);
				
			err = suites.PFWorldSuite()->PF_DisposeWorld(NULL, temp_World);
		}
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}

	return ae_err;
}


static A_Err	
AEIO_GetSound(
	AEIO_BasicData				*basic_dataP,	
	AEIO_InSpecH				specH,
	AEIO_SndQuality				quality,
	const AEIO_InterruptFuncs	*interrupt_funcsP0,	
	const A_Time				*startPT,	
	const A_Time				*durPT,	
	A_u_long					start_sampLu,
	A_u_long					num_samplesLu,
	void						*dataPV)
{ 
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		ErrThrower err;
		
		using namespace MoxFiles;
	
		if(g_infiles.find(specH) == g_infiles.end())
		{
		#ifdef AE_UNICODE_PATHS
			AEGP_MemHandle pathH = NULL;
			A_PathType *file_nameZ = NULL;
			
			suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
			
			if(pathH)
			{
				suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
			}
			else
				return AEIO_Err_BAD_FILENAME; 
		#else
			A_PathType file_nameZ[AEGP_MAX_PATH_SIZE];
			
			suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
		#endif
			
		#ifdef AE_HFS_PATHS	
			// convert the path format
			if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
				return AEIO_Err_BAD_FILENAME; 
		#endif
			
			
			g_infiles[specH] = new AEInputFile(file_nameZ);
			
			
		#ifdef AE_UNICODE_PATHS
			if(pathH)
				suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
		#endif
		}

		if(g_infiles[specH] == NULL)
			throw MoxMxf::NullExc("File is NULL");
		
		
		InputFile &file = g_infiles[specH]->file();
		
		
		A_FpLong sample_rate;
		AEIO_SndEncoding sound_encoding;
		AEIO_SndSampleSize sample_size;
		AEIO_SndChannels num_channels;
		
		err = suites.IOInSuite()->AEGP_GetInSpecSoundRate(specH, &sample_rate);
		err = suites.IOInSuite()->AEGP_GetInSpecSoundEncoding(specH, &sound_encoding);
		err = suites.IOInSuite()->AEGP_GetInSpecSoundSampleSize(specH, &sample_size);
		err = suites.IOInSuite()->AEGP_GetInSpecSoundChannels(specH, &num_channels);
		
		const SampleType sample_type = (sample_size == AEIO_SS_1 ? MoxFiles::UNSIGNED8 :
										sample_size == AEIO_SS_2 ? MoxFiles::SIGNED16 :
										sample_size == AEIO_SS_4 ? MoxFiles::AFLOAT :
										MoxFiles::AFLOAT);
		
		if(sample_size == AEIO_SS_1)
		{
			assert(sound_encoding == AEIO_E_UNSIGNED_PCM);
		}
		else if(sample_size == AEIO_SS_4)
		{
			assert(sound_encoding == AEIO_E_SIGNED_FLOAT);
		}
		else
			assert(sound_encoding == AEIO_E_SIGNED_PCM);
		
		
		const int number_channels = (num_channels == AEIO_SndChannels_MONO ? 1 : 2);
		const size_t channel_size = (sample_size == AEIO_SS_1 ? 1 :
										sample_size == AEIO_SS_2 ? 2 :
										sample_size == AEIO_SS_4 ? 4 :
										1);
		const ptrdiff_t stride = number_channels * channel_size;
		
		char *origin = (char *)dataPV;
		
		
		AudioBuffer audio_buffer(num_samplesLu);
		
		if(num_channels == AEIO_SndChannels_MONO)
		{
			audio_buffer.insert("Mono", AudioSlice(sample_type, origin + (channel_size * 0), stride));
		}
		else
		{
			audio_buffer.insert("Left", AudioSlice(sample_type, origin + (channel_size * 0), stride));
			audio_buffer.insert("Right", AudioSlice(sample_type, origin + (channel_size * 1), stride));
		}
		
		
		assert(start_sampLu == (A_u_long)((double)sample_rate * (double)startPT->value / (double)startPT->scale));
		assert(num_samplesLu == (A_u_long)((double)sample_rate * (double)durPT->value / (double)durPT->scale));
		
		file.seekAudio(start_sampLu);
		
		file.readAudio(num_samplesLu, audio_buffer);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}

	return ae_err;
}


static A_Err
AEIO_DisposeInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH)
{
	A_Err ae_err =	A_Err_NONE;

	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);	

	try
	{
		std::map<AEIO_InSpecH, AEInputFile *>::iterator file = g_infiles.find(specH);
	
		if(file != g_infiles.end())
		{
			delete file->second;
		
			g_infiles.erase(file);
		}
		else
			assert(false); // where's the file?
		
		
		ErrThrower err;
		
		AEIO_Handle optionsH = NULL;
		
		err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH));
		
		if(optionsH)
		{
			err = suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
		}
		else
			assert(false);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_INAPPROPRIATE_ACTION;
	}
	
	return ae_err;
}


static A_Err	
AEIO_CloseSourceFiles(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			seqH)
{ 
	return A_Err_NONE; 
}		// TRUE for close, FALSE for unclose


#pragma mark-


static A_Err	
AEIO_GetNumAuxChannels(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			*num_channelsPL)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_GetAuxChannelDesc(	
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			chan_indexL,
	PF_ChannelDesc	*descP)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;	
}


static A_Err	
AEIO_DrawAuxChannel(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			specH,
	A_long					chan_indexL,
	const AEIO_DrawFramePB	*pbP,
	PF_ChannelChunk			*chunkP)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_FreeAuxChannel(	
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			specH,
	PF_ChannelChunk			*chunkP)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_NumAuxFiles(		
	AEIO_BasicData		*basic_dataP,
	AEIO_InSpecH		seqH,
	A_long				*files_per_framePL0)
{ 
	*files_per_framePL0 = 0;
	
	return A_Err_NONE; 
}


static A_Err	
AEIO_GetNthAuxFileSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			seqH,
	A_long					frame_numL, 
	A_long					n,
#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
	A_PathType				*file_pathZ)
#else
	AEGP_MemHandle			*pathPH)
#endif
{
	assert(FALSE); // no aux files
	
	return A_Err_NONE; 
}


#pragma mark-


static A_Err	
AEIO_InitOutputSpec(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_Boolean				*user_interacted)
{
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	assert(*user_interacted == FALSE);
	
	try
	{
		ErrThrower err;
		
		AEIO_Handle		optionsH		=	NULL,
						old_optionsH	=	NULL;
		MOX_OutOptions	*options			=	NULL,
						*old_options		=	NULL;
						
		// make new options handle
		err = suites.MemorySuite()->AEGP_NewMemHandle( basic_dataP->aegp_plug_id, "Output Options",
												sizeof(MOX_OutOptions),
												AEGP_MemFlag_CLEAR, &optionsH);

		// set the options handle
		err = suites.IOOutSuite()->AEGP_SetOutSpecOptionsHandle(outH, (void*)optionsH, (void**)&old_optionsH);
		
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		
		InitOptions(options);
		
		if(old_optionsH)
		{
			err = suites.MemorySuite()->AEGP_LockMemHandle(old_optionsH, (void**)&old_options);
			
			InflateOptions(old_options);
			
			CopyOptions(options, old_options);
			
			err = suites.MemorySuite()->AEGP_FreeMemHandle(old_optionsH);
		}
		
		err = suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
  	return ae_err;
}


static A_Err	
AEIO_UserOptionsDialog(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	const PF_EffectWorld	*sample0,
	A_Boolean				*user_interacted0)
{ 
	basic_dataP->msg_func(1, "No options dialog to see here!");
	return A_Err_NONE;
}


static A_Err
AEIO_UserAudioOptionsDialog(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH, 
	A_Boolean			*user_interacted0)
{
	basic_dataP->msg_func(1, "No audio options dialog to see here!");
	return A_Err_NONE;
}


static A_Err	
AEIO_OutputInfoChanged(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH)
{
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	A_Boolean missing = FALSE;
	
	ae_err = suites.IOOutSuite()->AEGP_GetOutSpecIsMissing(outH, &missing);
	
	assert(missing == FALSE);

	return ae_err;
}


static A_Err	
AEIO_GetFlatOutputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	AEIO_Handle		*flat_optionsPH)
{
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	try
	{
		ErrThrower err;
		
		AEIO_Handle		optionsH		=	NULL;
		MOX_OutOptions	*options		=	NULL;
		
		err = suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, reinterpret_cast<void**>(&optionsH));
		
		if(optionsH)
		{
			err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		
			err = suites.MemorySuite()->AEGP_NewMemHandle( basic_dataP->aegp_plug_id, "Flat Options",
															sizeof(MOX_OutOptions),
															AEGP_MemFlag_CLEAR, flat_optionsPH);
													
			MOX_OutOptions *flat_options = NULL;
			
			err = suites.MemorySuite()->AEGP_LockMemHandle(*flat_optionsPH, (void**)&flat_options);
			
			InitOptions(flat_options);
			
			CopyOptions(flat_options, options);
			
			FlattenOptions(flat_options);
			
			err = suites.MemorySuite()->AEGP_UnlockMemHandle(*flat_optionsPH);
			
			err = suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		}
		else
			assert(false);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
  	return ae_err;
}


static A_Err	
AEIO_GetOutputInfo(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH,
	AEIO_Verbiage		*verbiageP)
{ 
	A_Err err			= A_Err_NONE;
	//AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	strcpy(verbiageP->name, "filename");
	strcpy(verbiageP->type, "NOX");
	strcpy(verbiageP->sub_type, "no options yet");
	
	return err;
}


static A_Err	
AEIO_DisposeOutputOptions(
	AEIO_BasicData	*basic_dataP,
	void			*optionsPV)
{ 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	AEIO_Handle			optionsH	=	static_cast<AEIO_Handle>(optionsPV);
	A_Err				err			=	A_Err_NONE;
	
	if(optionsH)
	{
		ERR(suites.MemorySuite()->AEGP_FreeMemHandle(optionsH));
	}
	
	return err;
}


static A_Err	
AEIO_GetDepths(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	AEIO_SupportedDepthFlags	*which)
{ 
	*which =	AEIO_SupportedDepthFlags_DEPTH_24	|
				AEIO_SupportedDepthFlags_DEPTH_32	|
				AEIO_SupportedDepthFlags_DEPTH_48	|
				AEIO_SupportedDepthFlags_DEPTH_64	|
				AEIO_SupportedDepthFlags_DEPTH_96	|
				AEIO_SupportedDepthFlags_DEPTH_128;

	return A_Err_NONE; 
}


static A_Err	
AEIO_GetOutputSuffix(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	A_char			*suffix)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_SetOutputFile(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
	A_PathType	*file_pathZ)
#else
	const A_UTF16Char	*file_pathZ)
#endif
{
	// this is in case you wanted to take the path AE
	// came up with and change it to something else
	
  	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_StartAdding(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH, 
	A_long				flags)
{
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	AEIO_Handle		optionsH = NULL;
	MOX_OutOptions	*options = NULL;
	
	try
	{
		ErrThrower err;
		
		using namespace MoxFiles;
	
		if( supportsThreads() )
			setGlobalThreadCount(gNumCPUs);
		

		// get file path
	#ifdef AE_UNICODE_PATHS
		AEGP_MemHandle pathH = NULL;
		A_PathType *file_pathZ = NULL;
		
		A_Boolean file_reservedPB = FALSE; // WTF?
		err = suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, &pathH, &file_reservedPB);
		
		if(pathH)
		{
			suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_pathZ);
		}
		else
			return AEIO_Err_BAD_FILENAME; 
	#else
		A_PathType file_pathZ[AEGP_MAX_PATH_SIZE+1];
		
		A_Boolean file_reservedPB = FALSE; // WTF?
		err = suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, file_pathZ, &file_reservedPB);
	#endif
		
	#ifdef AE_HFS_PATHS	
		// convert the path format
		if(A_Err_NONE != ConvertPath(file_pathZ, file_pathZ, AEGP_MAX_PATH_SIZE-1) )
			return AEIO_Err_BAD_FILENAME; 
	#endif

		
		
		err = suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, (void**)&optionsH);
		
		if(optionsH)
		{
			err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		}
		else
			throw MoxMxf::NullExc("No options");
		
		
		A_long width, height;
		A_short depth;
		AEIO_AlphaLabel alpha_label;
		A_Fixed fps;
		A_Ratio par;
		A_Time duration;
		A_FpLong sample_rate;
		AEIO_SndEncoding sound_encoding;
		AEIO_SndSampleSize sound_sample_size;
		AEIO_SndChannels sound_channels;
		A_Boolean is_still, missing;
		
		err = suites.IOOutSuite()->AEGP_GetOutSpecDimensions(outH, &width, &height);
		err = suites.IOOutSuite()->AEGP_GetOutSpecDepth(outH, &depth);
		err = suites.IOOutSuite()->AEGP_GetOutSpecAlphaLabel(outH, &alpha_label);
		err = suites.IOOutSuite()->AEGP_GetOutSpecFPS(outH, &fps);
		err = suites.IOOutSuite()->AEGP_GetOutSpecDuration(outH, &duration);
		err = suites.IOOutSuite()->AEGP_GetOutSpecHSF(outH, &par);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundRate(outH, &sample_rate);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundEncoding(outH, &sound_encoding);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundSampleSize(outH, &sound_sample_size);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundChannels(outH, &sound_channels);
		err = suites.IOOutSuite()->AEGP_GetOutSpecIsStill(outH, &is_still);
		err = suites.IOOutSuite()->AEGP_GetOutSpecIsMissing(outH, &missing);
		
		assert(is_still == FALSE);
		assert(missing == FALSE);
		
		
		Rational frameRate;
		
		if(fps % 65536 == 0)
		{
			frameRate.Numerator = fps / 65536;
			frameRate.Denominator = 1;
		}
		else
		{
			// note this is slightly different from what AE uses internally,
			// using {2997, 100} for 29.97 fps instead of the proper {30000, 1001}
			
			frameRate.Numerator = (FIX_2_FLOAT(fps) * 1001.0) + 0.5;
			frameRate.Denominator = 1001;
		}
		
		Rational sampleRate(sample_rate, 1);
		
		
		const VideoCompression vid_compression = (depth >= 96 ? MoxFiles::OPENEXR : MoxFiles::PNG);
		const AudioCompression aud_compression = MoxFiles::PCM;
		
		Header head(width, height, frameRate, sampleRate, vid_compression, aud_compression);
		
		
		if(depth > 0)
		{
			ChannelList &channels = head.channels();
			
			const size_t bytes_per_pixel = (depth >> 3);
			const bool have_alpha = (bytes_per_pixel == 4 || bytes_per_pixel == 8 || bytes_per_pixel == 16);
			const size_t bytes_per_subpixel = (bytes_per_pixel / (have_alpha ? 4 : 3));
			
			const MoxFiles::PixelType pixel_type = (bytes_per_subpixel == 1 ? MoxFiles::UINT8 :
													bytes_per_subpixel == 2 ? MoxFiles::UINT16 :
													bytes_per_subpixel == 4 ? MoxFiles::HALF :
													MoxFiles::UINT8);
											
			channels.insert("R", Channel(pixel_type));
			channels.insert("G", Channel(pixel_type));
			channels.insert("B", Channel(pixel_type));
			
			if(have_alpha)
				channels.insert("A", Channel(pixel_type));
		}
		
		
		if(sound_channels > 0)
		{
			if(sound_sample_size == AEIO_SS_1)
			{
				assert(sound_encoding == AEIO_E_UNSIGNED_PCM);
			}
			else if(sound_sample_size == AEIO_SS_4)
			{
				assert(sound_encoding == AEIO_E_SIGNED_FLOAT);
			}
			else
				assert(sound_encoding == AEIO_E_SIGNED_PCM);
			
			
			const SampleType sample_type = (sound_sample_size == AEIO_SS_1 ? MoxFiles::UNSIGNED8 :
											sound_sample_size == AEIO_SS_2 ? MoxFiles::SIGNED16 :
											sound_sample_size == AEIO_SS_4 ? MoxFiles::SIGNED24 : // not handling float audio yet
											MoxFiles::SIGNED24);
			
			
			AudioChannelList &audio_channels = head.audioChannels();
			
			if(sound_channels == AEIO_SndChannels_MONO)
			{
				audio_channels.insert("Mono", AudioChannel(sample_type));
			}
			else
			{
				assert(sound_channels == AEIO_SndChannels_STEREO);
				
				audio_channels.insert("Left", AudioChannel(sample_type));
				audio_channels.insert("Right", AudioChannel(sample_type));
			}
		}
		
		
		if(g_outfiles.find(outH) == g_outfiles.end())
		{
			g_outfiles[outH] = new AEOutputFile(file_pathZ, head);
		}
		else
			throw MoxMxf::LogicExc("File already exists");
		
		
	#ifdef AE_UNICODE_PATHS
		if(pathH)
			suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
	#endif
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(...)
	{
		ae_err = AEIO_Err_INITIALIZE_FAILED;
	}
	
	
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	
  	return ae_err;
}


static A_Err	
AEIO_GetSizes(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	A_u_longlong	*free_space, 
	A_u_longlong	*file_size)
{
	return AEIO_Err_USE_DFLT_CALLBACK; // maybe AEIO_Err_USE_DFLT_GETSIZES_FREESPACE ?
}


static A_Err	
AEIO_AddFrame(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_long					frame_index, 
	A_long					frames,
	const PF_EffectWorld	*wP, 
	const A_LPoint			*origin0,
	A_Boolean				was_compressedB,	
	AEIO_InterruptFuncs		*inter0)
{ 
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	try
	{
		ErrThrower err;
		
		using namespace MoxFiles;
	
		if(g_outfiles.find(outH) == g_outfiles.end())
		{
			throw MoxMxf::LogicExc("No output file");
		}
		else if(g_outfiles[outH] == NULL)
		{
			throw MoxMxf::NullExc("File is NULL");
		}
					
		
		PF_PixelFormat pixel_format;
		err = suites.PFWorldSuite()->PF_GetPixelFormat(wP, &pixel_format);
		
		const MoxFiles::PixelType pixel_type = (pixel_format == PF_PixelFormat_ARGB32 ? MoxFiles::UINT8 :
												pixel_format == PF_PixelFormat_ARGB64 ? MoxFiles::UINT16A :
												pixel_format == PF_PixelFormat_ARGB128 ? MoxFiles::FLOAT :
												MoxFiles::UINT8);
		
		const size_t subpixel_size = PixelSize(pixel_type);
		const size_t pixel_size = 4 * subpixel_size;
		const ptrdiff_t rowbytes = wP->rowbytes;
		
		const double rgb_fill = 0;
		const double alpha_fill = (pixel_type == MoxFiles::UINT8 ? 255 :
									pixel_type == MoxFiles::UINT16A ? 32768 :
									pixel_type == MoxFiles::FLOAT ? 1.0 :
									0);
		
		char *origin = (char *)wP->data;
		
		
		FrameBuffer frame_buffer(wP->width, wP->height);
		
		frame_buffer.insert("A", Slice(pixel_type, origin + (subpixel_size * 0), pixel_size, rowbytes, 1, 1, alpha_fill));
		frame_buffer.insert("R", Slice(pixel_type, origin + (subpixel_size * 1), pixel_size, rowbytes, 1, 1, rgb_fill));
		frame_buffer.insert("G", Slice(pixel_type, origin + (subpixel_size * 2), pixel_size, rowbytes, 1, 1, rgb_fill));
		frame_buffer.insert("B", Slice(pixel_type, origin + (subpixel_size * 3), pixel_size, rowbytes, 1, 1, rgb_fill));
		
		
		OutputFile &file = g_outfiles[outH]->file();
		
		assert(wP->width == file.header().width());
		assert(wP->height == file.header().height());
		assert(frames == 1);
		
		file.pushFrame(frame_buffer);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(MoxMxf::IoExc &e)
	{
		ae_err = AEIO_Err_DISK_FULL;
	}
	catch(...)
	{
		ae_err = AEIO_Err_INAPPROPRIATE_ACTION;
	}
	
  	return ae_err;
}


static A_Err	
AEIO_OutputFrame(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	const PF_EffectWorld	*wP)
{ 
	assert(false); // this is for still formats only

	return A_Err_NONE;
}


static A_Err	
AEIO_AddSoundChunk(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	const A_Time	*start, 
	A_u_long		num_samplesLu,
	const void		*dataPV)
{ 
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	try
	{
		ErrThrower err;
		
		using namespace MoxFiles;
	
		if(g_outfiles.find(outH) == g_outfiles.end())
		{
			throw MoxMxf::LogicExc("No output file");
		}
		else if(g_outfiles[outH] == NULL)
		{
			throw MoxMxf::NullExc("File is NULL");
		}
		
		
		A_FpLong sample_rate;
		AEIO_SndEncoding sound_encoding;
		AEIO_SndSampleSize sample_size;
		AEIO_SndChannels num_channels;
		
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundRate(outH, &sample_rate);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundEncoding(outH, &sound_encoding);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundSampleSize(outH, &sample_size);
		err = suites.IOOutSuite()->AEGP_GetOutSpecSoundChannels(outH, &num_channels);
		
		const SampleType sample_type = (sample_size == AEIO_SS_1 ? MoxFiles::UNSIGNED8 :
										sample_size == AEIO_SS_2 ? MoxFiles::SIGNED16 :
										sample_size == AEIO_SS_4 ? MoxFiles::AFLOAT :
										MoxFiles::AFLOAT);
		
		if(sample_size == AEIO_SS_1)
		{
			assert(sound_encoding == AEIO_E_UNSIGNED_PCM);
		}
		else if(sample_size == AEIO_SS_4)
		{
			assert(sound_encoding == AEIO_E_SIGNED_FLOAT);
		}
		else
			assert(sound_encoding == AEIO_E_SIGNED_PCM);
		
		
		const int number_channels = (num_channels == AEIO_SndChannels_MONO ? 1 : 2);
		const size_t channel_size = (sample_size == AEIO_SS_1 ? 1 :
										sample_size == AEIO_SS_2 ? 2 :
										sample_size == AEIO_SS_4 ? 4 :
										1);
		const ptrdiff_t stride = number_channels * channel_size;
		
		char *origin = (char *)dataPV;
		
		
		AudioBuffer audio_buffer(num_samplesLu);
		
		if(num_channels == AEIO_SndChannels_MONO)
		{
			audio_buffer.insert("Mono", AudioSlice(sample_type, origin + (channel_size * 0), stride));
		}
		else
		{
			audio_buffer.insert("Left", AudioSlice(sample_type, origin + (channel_size * 0), stride));
			audio_buffer.insert("Right", AudioSlice(sample_type, origin + (channel_size * 1), stride));
		}
		
		
		OutputFile &file = g_outfiles[outH]->file();
		
		file.pushAudio(audio_buffer);
	}
	catch(ErrThrower &err)
	{
		ae_err = err.err();
	}
	catch(MoxMxf::IoExc &e)
	{
		ae_err = AEIO_Err_DISK_FULL;
	}
	catch(...)
	{
		ae_err = AEIO_Err_INAPPROPRIATE_ACTION;
	}
	
  	return ae_err;
}


static A_Err	
AEIO_WriteLabels(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	AEIO_LabelFlags	*written)
{ 
	return AEIO_Err_USE_DFLT_CALLBACK;
}


static A_Err	
AEIO_EndAdding(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_long					flags)
{ 
	A_Err ae_err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	try
	{
		std::map<AEIO_OutSpecH, AEOutputFile *>::iterator file = g_outfiles.find(outH);
	
		if(file != g_outfiles.end())
		{
			delete file->second;
		
			g_outfiles.erase(file);
		}
		else
			assert(false); // where is the file?
	}
	catch(...)
	{
		ae_err = AEIO_Err_PARSING;
	}
	
  	return ae_err;
}


static A_Err	
AEIO_Flush(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH)
{ 
	//	free any temp buffers you kept around for
	//	writing.
	
	return A_Err_NONE; 
}


#pragma mark-


static A_Err	
AEIO_CountUserData(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			inH,
	A_u_long 				typeLu,
	A_u_long				max_sizeLu,
	A_u_long				*num_of_typePLu)
{ 
	return A_Err_NONE; 
}

static A_Err	
AEIO_SetUserData(    
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH			outH,
	A_u_long				typeLu,
	A_u_long				indexLu,
	const AEIO_Handle		dataH)
{ 
	return A_Err_NONE; 
}

static A_Err	
AEIO_GetUserData(   
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			inH,
	A_u_long 				typeLu,
	A_u_long				indexLu,
	A_u_long				max_sizeLu,
	AEIO_Handle				*dataPH)
{ 
	return A_Err_NONE; 
}
                            
static A_Err	
AEIO_AddMarker(		
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH 			outH, 
	A_long 					frame_index, 
	AEIO_MarkerType 		marker_type, 
	void					*marker_dataPV, 
	AEIO_InterruptFuncs		*inter0)
{ 
	/*	The type of the marker is in marker_type,
		and the text they contain is in 
		marker_dataPV.
	*/
	return A_Err_NONE; 
}


#pragma mark-


static A_Err
ConstructModuleInfo(
	SPBasicSuite		*pica_basicP,			
	AEIO_ModuleInfo		*info)
{
	if (info) {
		info->sig						=	'NOX_';
		info->max_width					=	2147483647;
		info->max_height				=	2147483647;
		info->num_filetypes				=	1;
		info->num_extensions			=	1;
		info->num_clips					=	0;
		
		info->create_kind.type			=	'NOX_';
		info->create_kind.creator		=	'FNRD';

		info->create_ext.pad			=	'.';
		info->create_ext.extension[0]	=	'n';
		info->create_ext.extension[1]	=	'o';
		info->create_ext.extension[2]	=	'x';

		strcpy(info->name, "NOX");
		
		info->num_aux_extensionsS		=	0;

		info->flags						=	AEIO_MFlag_INPUT			| 
											AEIO_MFlag_OUTPUT			| 
											AEIO_MFlag_FILE				|
											AEIO_MFlag_VIDEO			| 
											AEIO_MFlag_AUDIO			|
											//AEIO_MFlag_NO_TIME			| 
											AEIO_MFlag_NO_OPTIONS		| 
											//AEIO_MFlag_CAN_DO_MARKERS	|
											AEIO_MFlag_HSF_AWARE		|
											AEIO_MFlag_CAN_DRAW_DEEP;
		
		info->flags2					=	AEIO_MFlag2_CAN_DRAW_FLOAT	|
											AEIO_MFlag2_CAN_DO_AUDIO_32	|
											AEIO_MFlag2_SUPPORTS_ICC_PROFILES;

		info->read_kinds[0].mac.type			=	'NOX_';
		info->read_kinds[0].mac.creator			=	AEIO_ANY_CREATOR;
		info->read_kinds[1].ext					=	info->create_ext;
		
		return A_Err_NONE;
	}
	else
		return A_Err_STRUCT;
}

static A_Err
ConstructFunctionBlock(
	Current_AEIO_FunctionBlock	*funcs)
{
	if (funcs)
	{
		funcs->AEIO_InitInSpecFromFile		=	AEIO_InitInSpecFromFile;
		funcs->AEIO_InitInSpecInteractive	=	AEIO_InitInSpecInteractive;
		funcs->AEIO_DisposeInSpec			=	AEIO_DisposeInSpec;
		funcs->AEIO_FlattenOptions			=	AEIO_FlattenOptions;
		funcs->AEIO_InflateOptions			=	AEIO_InflateOptions;
		funcs->AEIO_SynchInSpec				=	AEIO_SynchInSpec;
		funcs->AEIO_GetActiveExtent			=	AEIO_GetActiveExtent;
		funcs->AEIO_GetInSpecInfo			=	AEIO_GetInSpecInfo;
		funcs->AEIO_DrawSparseFrame			=	AEIO_DrawSparseFrame;
		funcs->AEIO_GetDimensions			=	AEIO_GetDimensions;
		funcs->AEIO_GetDuration				=	AEIO_GetDuration;
		funcs->AEIO_GetTime					=	AEIO_GetTime;
		funcs->AEIO_GetSound				=	AEIO_GetSound;
		funcs->AEIO_InqNextFrameTime		=	AEIO_InqNextFrameTime;
		
		funcs->AEIO_InitOutputSpec			=	AEIO_InitOutputSpec;
		funcs->AEIO_GetFlatOutputOptions	=	AEIO_GetFlatOutputOptions;
		funcs->AEIO_DisposeOutputOptions	=	AEIO_DisposeOutputOptions;
		funcs->AEIO_UserOptionsDialog		=	AEIO_UserOptionsDialog;
		funcs->AEIO_GetOutputInfo			=	AEIO_GetOutputInfo;
		funcs->AEIO_OutputInfoChanged		=	AEIO_OutputInfoChanged;
		funcs->AEIO_SetOutputFile			=	AEIO_SetOutputFile;
		funcs->AEIO_StartAdding				=	AEIO_StartAdding;
		funcs->AEIO_AddFrame				=	AEIO_AddFrame;
		funcs->AEIO_EndAdding				=	AEIO_EndAdding;
		funcs->AEIO_OutputFrame				=	AEIO_OutputFrame;
		funcs->AEIO_WriteLabels				=	AEIO_WriteLabels;
		funcs->AEIO_GetSizes				=	AEIO_GetSizes;
		funcs->AEIO_Flush					=	AEIO_Flush;
		funcs->AEIO_AddSoundChunk			=	AEIO_AddSoundChunk;
		funcs->AEIO_Idle					=	AEIO_Idle;
		funcs->AEIO_GetDepths				=	AEIO_GetDepths;
		funcs->AEIO_GetOutputSuffix			=	AEIO_GetOutputSuffix;
		
		funcs->AEIO_SeqOptionsDlg			=	AEIO_SeqOptionsDlg;
		
		funcs->AEIO_GetNumAuxChannels		=	AEIO_GetNumAuxChannels;
		funcs->AEIO_GetAuxChannelDesc		=	AEIO_GetAuxChannelDesc;
		funcs->AEIO_DrawAuxChannel			=	AEIO_DrawAuxChannel;
		funcs->AEIO_FreeAuxChannel			=	AEIO_FreeAuxChannel;
		funcs->AEIO_NumAuxFiles				=	AEIO_NumAuxFiles;
		funcs->AEIO_GetNthAuxFileSpec		=	AEIO_GetNthAuxFileSpec;
		
		funcs->AEIO_CloseSourceFiles		=	AEIO_CloseSourceFiles;
		
		funcs->AEIO_CountUserData			=	AEIO_CountUserData;
		funcs->AEIO_SetUserData				=	AEIO_SetUserData;
		funcs->AEIO_GetUserData				=	AEIO_GetUserData;
		funcs->AEIO_AddMarker				=	AEIO_AddMarker;
		
		funcs->AEIO_VerifyFileImportable	=	AEIO_VerifyFileImportable;
		
		funcs->AEIO_UserAudioOptionsDialog	=	AEIO_UserAudioOptionsDialog;
		
		funcs->AEIO_AddMarker2				=	NULL;
		funcs->AEIO_AddMarker3				=	NULL;
		
		funcs->AEIO_GetMimeType				=	NULL;

		return A_Err_NONE;
	}
	else
		return A_Err_STRUCT;
}


A_Err
GPMain_IO(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */
#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
	const A_char		 	*file_pathZ,			/* >> platform-specific delimiters */
	const A_char		 	*res_pathZ,				/* >> platform-specific delimiters */
#endif
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	void					*global_refconPV)		/* << */
{
	A_Err 				err					= A_Err_NONE;
	AEIO_ModuleInfo		info;
	Current_AEIO_FunctionBlock	funcs;
	AEGP_SuiteHandler	suites(pica_basicP);	

	AEFX_CLR_STRUCT(info);
	AEFX_CLR_STRUCT(funcs);
	
	ERR(InitHook(pica_basicP));
	ERR(suites.RegisterSuite()->AEGP_RegisterDeathHook(aegp_plugin_id, DeathHook, 0));
	ERR(ConstructModuleInfo(pica_basicP, &info));
	ERR(ConstructFunctionBlock(&funcs));

	ERR(suites.RegisterSuite()->AEGP_RegisterIO(aegp_plugin_id,
												0,
												&info, 
												&funcs));
	
	return err;
}
