/* libjpeg-turbo build number */
/* Making this up */
#define BUILD "MoxJPEG"

/* How to obtain function inlining. */
#ifndef INLINE
#if defined(__GNUC__)
#define INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define INLINE __forceinline
#else
#define INLINE
#endif
#endif

/* Define to the full name of this package. */
/* Making this up */
#define PACKAGE_NAME "libjpeg-turbo"

/* Version number of package */
/* Making this up */
#define VERSION "1.5.2"

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 8
