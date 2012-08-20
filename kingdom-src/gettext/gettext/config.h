/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define if mono is the preferred C# implementation. */
/* #undef CSHARP_CHOICE_MONO */

/* Define if pnet is the preferred C# implementation. */
/* #undef CSHARP_CHOICE_PNET */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#define ENABLE_NLS 1

/* Define to 1 if the package shall run at any location in the filesystem. */
/* #undef ENABLE_RELOCATABLE */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#ifdef _WIN32
/* #undef HAVE_ALLOCA_H */
#else
#define HAVE_ALLOCA_H 1
#endif

/* Define to 1 if you have the `argz_count' function. */
/* #undef HAVE_ARGZ_COUNT */

/* Define to 1 if you have the <argz.h> header file. */
/* #undef HAVE_ARGZ_H */

/* Define to 1 if you have the `argz_next' function. */
/* #undef HAVE_ARGZ_NEXT */

/* Define to 1 if you have the `argz_stringify' function. */
/* #undef HAVE_ARGZ_STRINGIFY */

#ifdef _WIN32
/* Define to 1 if you have the `asprintf' function. */
/* #undef HAVE_ASPRINTF */

/* Define to 1 if you have the `atexit' function. */
/* #undef HAVE_ATEXIT */

/* Define to 1 if the compiler understands __builtin_expect. */
/* #undef HAVE_BUILTIN_EXPECT */
#else
/* Define to 1 if you have the `asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the `atexit' function. */
#define HAVE_ATEXIT 1

/* Define to 1 if the compiler understands __builtin_expect. */
#define HAVE_BUILTIN_EXPECT 1
#endif
/* Define to 1 if you have the `canonicalize_file_name' function. */
/* #undef HAVE_CANONICALIZE_FILE_NAME */

#ifdef __APPLE__
/* Define to 1 if you have the MacOS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
#define HAVE_CFLOCALECOPYCURRENT 1

/* Define to 1 if you have the MacOS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
#define HAVE_CFPREFERENCESCOPYAPPVALUE 1
#else
/* Define to 1 if you have the MacOS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the MacOS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */
#endif

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

#ifdef _WIN32
/* Define to 1 if you have the declaration of `clearerr_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_CLEARERR_UNLOCKED */

/* Define to 1 if you have the declaration of `feof_unlocked', and to 0 if you
   don't. */
/* #undef HAVE_DECL_FEOF_UNLOCKED */

/* Define to 1 if you have the declaration of `ferror_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FERROR_UNLOCKED */
#else
/* Define to 1 if you have the declaration of `clearerr_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_CLEARERR_UNLOCKED 1

#ifdef ANDROID
/* Define to 1 if you have the declaration of `feof_unlocked', and to 0 if you
   don't. */
/* #undef HAVE_DECL_FEOF_UNLOCKED */
#else
/* Define to 1 if you have the declaration of `feof_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_FEOF_UNLOCKED 1
#endif

/* Define to 1 if you have the declaration of `ferror_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FERROR_UNLOCKED 1
#endif

/* Define to 1 if you have the declaration of `fflush_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FFLUSH_UNLOCKED */

/* Define to 1 if you have the declaration of `fgets_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FGETS_UNLOCKED */

/* Define to 1 if you have the declaration of `fputc_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FPUTC_UNLOCKED */

/* Define to 1 if you have the declaration of `fputs_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FPUTS_UNLOCKED */

/* Define to 1 if you have the declaration of `fread_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FREAD_UNLOCKED */

/* Define to 1 if you have the declaration of `fwrite_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_FWRITE_UNLOCKED */

#ifdef _WIN32
/* Define to 1 if you have the declaration of `getchar_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_GETCHAR_UNLOCKED */

/* Define to 1 if you have the declaration of `getc_unlocked', and to 0 if you
   don't. */
/* #undef HAVE_DECL_GETC_UNLOCKED */

/* Define to 1 if you have the declaration of `putchar_unlocked', and to 0 if
   you don't. */
/* #undef HAVE_DECL_PUTCHAR_UNLOCKED */

/* Define to 1 if you have the declaration of `putc_unlocked', and to 0 if you
   don't. */
/* #undef HAVE_DECL_PUTC_UNLOCKED */

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
/* #undef HAVE_DECL_STRERROR_R */

/* Define to 1 if you have the declaration of `_snprintf', and to 0 if you
   don't. */
#define HAVE_DECL__SNPRINTF 1

/* Define to 1 if you have the declaration of `_snwprintf', and to 0 if you
   don't. */
#define HAVE_DECL__SNWPRINTF 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define if you have the declaration of environ. */
#define HAVE_ENVIRON_DECL 1
#else
/* Define to 1 if you have the declaration of `getchar_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_GETCHAR_UNLOCKED 1

/* Define to 1 if you have the declaration of `getc_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_GETC_UNLOCKED 1

/* Define to 1 if you have the declaration of `putchar_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_PUTCHAR_UNLOCKED 1

/* Define to 1 if you have the declaration of `putc_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_PUTC_UNLOCKED 1

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of `_snprintf', and to 0 if you
   don't. */
/* #undef HAVE_DECL__SNPRINTF */

/* Define to 1 if you have the declaration of `_snwprintf', and to 0 if you
   don't. */
/* #undef HAVE_DECL__SNWPRINTF */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if you have the declaration of environ. */
/* #undef HAVE_ENVIRON_DECL */
#endif

/* Define to 1 if you have the `fwprintf' function. */
#define HAVE_FWPRINTF 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

#ifdef _WIN32
/* Define to 1 if you have the `getegid' function. */
/* #undef HAVE_GETEGID */

/* Define to 1 if you have the `geteuid' function. */
/* #undef HAVE_GETEUID */

/* Define to 1 if you have the `getgid' function. */
/* #undef HAVE_GETGID */

/* Define to 1 if you have the <getopt.h> header file. */
/* #undef HAVE_GETOPT_H */

/* Define to 1 if you have the `getopt_long_only' function. */
/* #undef HAVE_GETOPT_LONG_ONLY */

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */
#else
/* Define to 1 if you have the `getegid' function. */
#define HAVE_GETEGID 1

/* Define to 1 if you have the `geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the `getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getopt_long_only' function. */
#define HAVE_GETOPT_LONG_ONLY 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1
#endif

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

#ifdef _WIN32
/* Define to 1 if you have the `getuid' function. */
/* #undef HAVE_GETUID */
#else
/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1
#endif

/* Define if you have the iconv() function. */
#define HAVE_ICONV 1

#ifdef _WIN32
/* Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>. */
/* #undef HAVE_INTMAX_T */

/* Define if <inttypes.h> exists and doesn't clash with <sys/types.h>. */
/* #undef HAVE_INTTYPES_H */

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
/* #undef HAVE_INTTYPES_H_WITH_UINTMAX */

/* Define to 1 if you have the `isascii' function. */
/* #undef HAVE_ISASCII */

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
/* #undef HAVE_LANGINFO_CODESET */

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */
#else
/* Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>. */
#define HAVE_INTMAX_T 1

/* Define if <inttypes.h> exists and doesn't clash with <sys/types.h>. */
#define HAVE_INTTYPES_H 1

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
#define HAVE_INTTYPES_H_WITH_UINTMAX 1

/* Define to 1 if you have the `isascii' function. */
#define HAVE_ISASCII 1

#ifdef ANDROID
/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
/* #undef HAVE_LANGINFO_CODESET */
#else
/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1
#endif

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1
#endif

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define if you have the 'long double' type. */
#define HAVE_LONG_DOUBLE 1

#ifdef _WIN32
/* Define if you have the 'long long' type. */
/* #undef HAVE_LONG_LONG */
#else
/* Define if you have the 'long long' type. */
#define HAVE_LONG_LONG 1
#endif

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
/* #undef HAVE_MACH_O_DYLD_H */

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

#ifdef _WIN32
/* Define to 1 if you have the <memory.h> header file. */
/* #undef HAVE_MEMORY_H */
#else
/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1
#endif

/* Define to 1 if you have the `mempcpy' function. */
/* #undef HAVE_MEMPCPY */

#ifdef _WIN32
/* Define to 1 if you have a working `mmap' system call. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the `munmap' function. */
/* #undef HAVE_MUNMAP */
#else
/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the `munmap' function. */
#define HAVE_MUNMAP 1
#endif

/* Define if you have <langinfo.h> and it defines the NL_LOCALE_NAME macro if
   _GNU_SOURCE is defined. */
/* #undef HAVE_NL_LOCALE_NAME */

#ifdef _WIN32
/* Define if your printf() function supports format strings with positions. */
/* #undef HAVE_POSIX_PRINTF */

/* Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE. */
/* #undef HAVE_PTHREAD_MUTEX_RECURSIVE */

/* Define if the POSIX multithreading library has read/write locks. */
/* #undef HAVE_PTHREAD_RWLOCK */
#else
/* Define if your printf() function supports format strings with positions. */
#define HAVE_POSIX_PRINTF 1

/* Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE. */
#define HAVE_PTHREAD_MUTEX_RECURSIVE 1

/* Define if the POSIX multithreading library has read/write locks. */
#define HAVE_PTHREAD_RWLOCK 1
#endif

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

#ifdef _WIN32
/* Define to 1 if you have the `readlink' function. */
#undef HAVE_READLINK
#else
/* Define to 1 if you have the `readlink' function. */
#define HAVE_READLINK 1
#endif

/* Define to 1 if you have the <search.h> header file. */
/* #undef HAVE_SEARCH_H */

#ifdef _WIN32
/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */
#else
/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1
#endif

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

#ifdef _WIN32
/* Define to 1 if stdbool.h conforms to C99. */
/* #undef HAVE_STDBOOL_H */
#else
/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1
#endif

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

#ifdef _WIN32
/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HAVE_STDINT_H */

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
/* #undef HAVE_STDINT_H_WITH_UINTMAX */
#else
/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
#define HAVE_STDINT_H_WITH_UINTMAX 1
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

#ifdef _WIN32
/* Define to 1 if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */

/* Define to 1 if you have the `strcasecmp' function. */
/* #undef HAVE_STRCASECMP */
#else
#ifdef ANDROID
/* Define to 1 if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */
#else
/* Define to 1 if you have the `stpcpy' function. */
#define HAVE_STPCPY 1
#endif

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1
#endif

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

#ifdef _WIN32
/* Define to 1 if you have the `strerror_r' function. */
/* #undef HAVE_STRERROR_R */

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */
#else
/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1
#endif

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

#ifdef _WIN32
/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */
#else
/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1
#endif

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `tsearch' function. */
/* #undef HAVE_TSEARCH */

#ifdef _WIN32
/* Define if you have the 'uintmax_t' type in <stdint.h> or <inttypes.h>. */
/* #undef HAVE_UINTMAX_T */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the `unsetenv' function. */
/* #undef HAVE_UNSETENV */

/* Define if you have the 'unsigned long long' type. */
/* #undef HAVE_UNSIGNED_LONG_LONG */

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
/* #undef HAVE_VISIBILITY */
#else
/* Define if you have the 'uintmax_t' type in <stdint.h> or <inttypes.h>. */
#define HAVE_UINTMAX_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define if you have the 'unsigned long long' type. */
#define HAVE_UNSIGNED_LONG_LONG 1

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#define HAVE_VISIBILITY 1
#endif

/* Define if you have the 'wchar_t' type. */
#define HAVE_WCHAR_T 1

/* Define to 1 if you have the `wcslen' function. */
#define HAVE_WCSLEN 1

/* Define if you have the 'wint_t' type. */
#define HAVE_WINT_T 1

#ifdef _WIN32
/* Define to 1 if the system has the type `_Bool'. */
/* #undef HAVE__BOOL */
#else
/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1
#endif

/* Define to 1 if you have the `_NSGetExecutablePath' function. */
/* #undef HAVE__NSGETEXECUTABLEPATH */

/* Define to 1 if you have the `__fsetlocking' function. */
/* #undef HAVE___FSETLOCKING */

#ifdef _WIN32
/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST const
#else
#define ICONV_CONST
#endif

/* Define to the value of ${prefix}, as a string. */

#ifdef _WIN32
/* Define if integer division by zero raises signal SIGFPE. */
/* #undef INTDIV0_RAISES_SIGFPE */
#else
/* Define if integer division by zero raises signal SIGFPE. */
#define INTDIV0_RAISES_SIGFPE 1
#endif

/* If malloc(0) is != NULL, define this to 1. Otherwise define this to 0. */
#define MALLOC_0_IS_NONNULL 1

/* Name of package */
#define PACKAGE "gettext-runtime"

#ifdef _WIN32
/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */
#else
/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""
#endif

/* Define if <inttypes.h> exists and defines unusable PRI* macros. */
/* #undef PRI_MACROS_BROKEN */

/* Define if the pthread_in_use() detection is hard. */
/* #undef PTHREAD_IN_USE_DETECTION_HARD */

#ifdef _WIN32
/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
#define SIZE_MAX 4294967295U

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
#define STACK_DIRECTION -1
#else
/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
/* #undef SIZE_MAX */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */
#endif

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
/* #undef STRERROR_R_CHAR_P */

#ifdef _WIN32
/* Define if the POSIX multithreading library can be used. */
/* #undef USE_POSIX_THREADS */
#else
/* Define if the POSIX multithreading library can be used. */
#define USE_POSIX_THREADS 1
#endif

/* Define if references to the POSIX multithreading library should be made
   weak. */
/* #undef USE_POSIX_THREADS_WEAK */

/* Define if the GNU Pth multithreading library can be used. */
/* #undef USE_PTH_THREADS */

/* Define if references to the GNU Pth multithreading library should be made
   weak. */
/* #undef USE_PTH_THREADS_WEAK */

/* Define if the old Solaris multithreading library can be used. */
/* #undef USE_SOLARIS_THREADS */

/* Define if references to the old Solaris multithreading library should be
   made weak. */
/* #undef USE_SOLARIS_THREADS_WEAK */

/* Define to 1 if you want getc etc. to use unlocked I/O if available.
   Unlocked I/O can improve performance in unithreaded apps, but it is not
   safe for multithreaded apps. */
#define USE_UNLOCKED_IO 1

#ifdef _WIN32
/* Define if the Win32 multithreading API can be used. */
#define USE_WIN32_THREADS 1
#else
/* Define if the Win32 multithreading API can be used. */
/* #undef USE_WIN32_THREADS */
#endif

/* Version number of package */
#define VERSION "0.15"

/* Define if unsetenv() returns void, not int. */
/* #undef VOID_UNSETENV */

#ifndef _WIN32
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
#endif

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

#ifdef _WIN32
/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#define inline __inline
#endif
#endif

/* Define as the type of the result of subtracting two pointers, if the system
   doesn't define it. */
/* #undef ptrdiff_t */

/* Define to empty if the C compiler doesn't support this keyword. */
/* #undef signed */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

#ifdef _WIN32
/* Define as a signed type of the same size as size_t. */
#define ssize_t int

/* Define to unsigned long or unsigned long long if <stdint.h> and
   <inttypes.h> don't define. */
#define uintmax_t unsigned long
#else
/* Define as a signed type of the same size as size_t. */
/* #undef ssize_t */

/* Define to unsigned long or unsigned long long if <stdint.h> and
   <inttypes.h> don't define. */
/* #undef uintmax_t */
#endif

#define __libc_lock_t                   gl_lock_t
#define __libc_lock_define              gl_lock_define
#define __libc_lock_define_initialized  gl_lock_define_initialized
#define __libc_lock_init                gl_lock_init
#define __libc_lock_lock                gl_lock_lock
#define __libc_lock_unlock              gl_lock_unlock
#define __libc_lock_recursive_t                   gl_recursive_lock_t
#define __libc_lock_define_recursive              gl_recursive_lock_define
#define __libc_lock_define_initialized_recursive  gl_recursive_lock_define_initialized
#define __libc_lock_init_recursive                gl_recursive_lock_init
#define __libc_lock_lock_recursive                gl_recursive_lock_lock
#define __libc_lock_unlock_recursive              gl_recursive_lock_unlock
#define glthread_in_use  libintl_thread_in_use
#define glthread_lock_init     libintl_lock_init
#define glthread_lock_lock     libintl_lock_lock
#define glthread_lock_unlock   libintl_lock_unlock
#define glthread_lock_destroy  libintl_lock_destroy
#define glthread_rwlock_init     libintl_rwlock_init
#define glthread_rwlock_rdlock   libintl_rwlock_rdlock
#define glthread_rwlock_wrlock   libintl_rwlock_wrlock
#define glthread_rwlock_unlock   libintl_rwlock_unlock
#define glthread_rwlock_destroy  libintl_rwlock_destroy
#define glthread_recursive_lock_init     libintl_recursive_lock_init
#define glthread_recursive_lock_lock     libintl_recursive_lock_lock
#define glthread_recursive_lock_unlock   libintl_recursive_lock_unlock
#define glthread_recursive_lock_destroy  libintl_recursive_lock_destroy
#define glthread_once                 libintl_once
#define glthread_once_call            libintl_once_call
#define glthread_once_singlethreaded  libintl_once_singlethreaded



/* On Windows, variables that may be in a DLL must be marked specially.  */
#if (defined _MSC_VER && defined _DLL)
# define DLL_VARIABLE __declspec (dllimport)
#else
# define DLL_VARIABLE
#endif

/* Extra OS/2 (emx+gcc) defines.  */
#ifdef __EMX__
# include "intl/os2compat.h"
#endif

