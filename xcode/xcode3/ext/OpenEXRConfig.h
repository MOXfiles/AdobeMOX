//
// Define and set to 1 if the target system supports a proc filesystem
// compatible with the Linux kernel's proc filesystem.  Note that this
// is only used by a program in the IlmImfTest test suite, it's not
// used by any OpenEXR library or application code.
//

#undef OPENEXR_IMF_HAVE_LINUX_PROCFS

//
// Define and set to 1 if the target system is a Darwin-based system
// (e.g., OS X).
//

#define OPENEXR_IMF_HAVE_DARWIN 1

//
// Define and set to 1 if the target system has a complete <iomanip>
// implementation, specifically if it supports the std::right
// formatter.
//

#define OPENEXR_IMF_HAVE_COMPLETE_IOMANIP 1

//
// Define and set to 1 if the target system has support for large
// stack sizes.
//

#undef OPENEXR_IMF_HAVE_LARGE_STACK

//
// Define if we can support GCC style inline asm with AVX instructions
//

#undef OPENEXR_IMF_HAVE_GCC_INLINE_ASM_AVX

//
// Define if we can use sysconf(_SC_NPROCESSORS_ONLN) to get CPU count
//

#define OPENEXR_IMF_HAVE_SYSCONF_NPROCESSORS_ONLN 1


//
// Version string for runtime access
//
#undef OPENEXR_VERSION_STRING
#undef OPENEXR_PACKAGE_STRING
