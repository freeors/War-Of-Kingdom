#ifndef __POSIX_H_
#define __POSIX_H_
  
#define ARCH_IS_32BIT

// #define ARCH_IS_GENERIC
#define ARCH_IS_IA32

#if !defined ARCH_IS_IA32 && !defined ARCH_IS_GENERIC
#	error "Caller must define ARCH_IS_32 or ARCH_IS_GENERIC before including this file."
#endif
  
/*
 * macro            program type
 *
 * _LINUX && !__KERNEL__:        linux     application
 * _LINUX && __KERNEL__:         linux     driver
 * _WIN32 && !_DRIVER:           windows   application
 * _WIN32 && _DRIVER:            windows   driver
 *
 * above macro, _WIN32 is defined by _MSC. __KERNEL__ must be defined in linux's driver.
 * _LINUX and _DRIVER must be defined yourself.
 */

/*
 * now, only MSC(version must lager than VC6.0) and GCC are supported. 
 */

#if !defined(_MSC_VER) && !defined(__GNUC__)
#  error "now, only MSC and GCC complier are supported."
#endif

#if defined(_MSC_VER) && _MSC_VER < 1200
#	error "this is MSC, but version is less than VC6.0."
#endif

/*
 * include header file needed for this platform
 */
#include <SDL_types.h>

/*
 * include header file needed for this platform
 */
#ifdef _WIN32 
	#ifndef  _DRIVER
		#include <windows.h>
		#include <assert.h>
	#else
		extern "C" {
			#include <wdm.h>
		}
	#endif
	#include <stdio.h>
	#include <windef.h> // HIBYTE etc.
	#include <crtdefs.h> // intptr_t
	//
	// warning C4200: nonstandard extension used : zero-sized array in struct/union
    //    Cannot generate copy-ctor or copy-assignment operator when UDT contains a zero-sized array
	// driver: this warning must be disabled, or five functions cann't be linked 
	// application: I think using zero-sized is useful, so disable this warning 
	#pragma warning( disable: 4200 )
#elif defined(_LINUX)
	#include <linux/version.h>
	#ifdef __KERNEL__
		#include <linux/types.h>	// include uint32_t etc.
	#else
		#include <stdio.h>
	#endif
#elif defined(ANDROID)
	#include <android/log.h>
	#include <stdio.h>

#else
	#include <stdio.h>
#endif

/*
 *  Things that must be sorted by compiler and then by architecture
 */

#define DBG_BUFF_SIZE			1024

// define _MAX_PATH, linux isn't define it
#ifndef _MAX_PATH
#define _MAX_PATH		255
#endif

// file extended name maximal length
#ifndef _MAX_EXTNAME
#define _MAX_EXTNAME	7
#endif

/*
 * some data type.
 */
#ifndef __KERNEL__
// stdio.h has thiose definition
    #if !defined(_SIZE_T_DEFINEDR) && !defined(_SIZE_T)
        typedef uint32_t size_t;
    #define _SIZE_T_DEFINED
	#define _SIZE_T
    #endif
#else
    #ifndef _SIZE_T
        #define _SIZE_T
        typedef uint32	size_t;
    #endif
#endif

// mmsystem.h has this definition
#if defined(_WIN32) 
	#if defined(_DRIVER) || defined(_INC_MMSYSTEM)
	#endif
	// in mmsystem.h, when define FOURCC and mmioFOURCC, it doesn't dectcte whether this macro is defined.
#else
	typedef uint32_t  FOURCC;

	#ifndef mmioFOURCC
		#define mmioFOURCC(ch0, ch1, ch2, ch3)  \
		((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
	#endif
#endif

/*
 * (API level) file process macro
 */
#ifndef _FA_CREATE_T
#define _FA_CREATE_T
typedef enum {
	fa_unknown		= 0,
	fa_create		= 1,
	fa_open			= 2,
	fa_read			= 3,
	fa_write		= 4,
	fa_seek			= 5,
	fa_close		= 6,
	fa_remove		= 7,
	fa_rename		= 8,
} file_action_t;
#endif

#ifdef _WIN32

typedef HANDLE			posix_file_t;	
#define INVALID_FILE	INVALID_HANDLE_VALUE

#define posix_fopen(name, desired_access, create_disposition, file)   \
    file = CreateFile(name, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, \
			create_disposition, 0, NULL) 

#define posix_fseek(file, low_part_pos, high_part_pos)  do { \
	LONG __high_part_pos = high_part_pos;	\
	SetFilePointer(file, low_part_pos, (LONG*)&__high_part_pos, FILE_BEGIN); \
} while (0)

#define posix_fwrite(file, buf, size, bytertd)  do { \
	BOOL __ok;	\
	__ok = WriteFile(file, buf, size, (DWORD*)&(bytertd), NULL); \
	bytertd = __ok? bytertd: 0; \
} while (0)

#define posix_fread(file, buf, size, bytertd)  do { \
	BOOL __ok;	\
	__ok = ReadFile(file, buf, size, (DWORD*)&bytertd, NULL); \
	bytertd = __ok? bytertd: 0; \
} while (0)

#define posix_fclose(file)			\
	CloseHandle(file)

#define posix_fsize(file, fsizelow, fsizehigh)	do { \
    fsizelow = GetFileSize(file, (DWORD*)&fsizehigh);	\
} while(0)

#define posix_fsize_byname(name, fsizelow, fsizehigh) do { \
    HANDLE __h; \
    __h = CreateFile(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, \
			OPEN_EXISTING, 0, NULL);  \
	if (__h == INVALID_HANDLE_VALUE) {  \
		fsizelow = fsizehigh = 0;		\
	} else {		\
		fsizelow = GetFileSize(__h, (LPDWORD)&fsizehigh);	\
		CloseHandle(__h);	\
    } \
} while(0)

#elif !defined(__KERNEL__)

// 根据winapi定义，create_disposition定义没使用bit形式，TEXT模式只好放在desired_access
#define CONTENT_TXT			(0x00800000L)		

#ifndef GENERIC_READ
#define GENERIC_READ        (0x80000000L)
#define GENERIC_WRITE       (0x40000000L)

#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#endif

#define FILE_ATTRIBUTE_READONLY             0x00000001  
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  
#define FILE_ATTRIBUTE_ENCRYPTED            0x00000040  
#define FILE_ATTRIBUTE_NORMAL               0x00000080  
#define FILE_ATTRIBUTE_TEMPORARY            0x00000100  
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200  
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400  
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800  
#define FILE_ATTRIBUTE_OFFLINE              0x00001000  
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000  

typedef FILE*			posix_file_t;
#define INVALID_FILE	NULL

#define posix_fopen(name, desired_access, create_disposition, file)	do { \
    char __mode[5];	\
	int __mode_pos = 0;	\
	if (desired_access & GENERIC_WRITE) {	\
		__mode[__mode_pos ++] = 'w';	\
	} else {	\
		__mode[__mode_pos ++] = 'r';	\
	}	\
	__mode_pos = 1;	\
	if (!(desired_access & CONTENT_TXT)) {	\
		__mode[__mode_pos ++] = 'b';	\
	}	\
	if (create_disposition == CREATE_ALWAYS) {	\
		__mode[__mode_pos ++] = '+';	\
	}	\
	__mode[__mode_pos] = 0;	\
	file = fopen(name, __mode);	\
} while(0)

#define posix_fseek(file, low_part_pos, high_part_pos) fseek((FILE*)(file), low_part_pos, SEEK_SET)	
//	MAKEDWORDLONG(low_part_pos, high_part_pos);
//	fseeko64((FILE*)(file), pos, SEEK_SET);	

#define posix_fwrite(file, buf, size, bytertd)  do { \
	size_t __retval;	\
	__retval = fwrite(buf, 1, size, file); \
	bytertd = __retval; \
} while (0)

#define posix_fread(file, buf, size, bytertd)  do { \
	size_t __retval;	\
	__retval = fread(buf, 1, size, file); \
	bytertd = __retval; \
} while (0)

#define posix_fclose(file)			\
	fclose(file)

#define posix_fsize(file, fsizelow, fsizehigh) do { \
	fseek(file, 0, SEEK_END);	\
    fsizelow = ftell(file);	\
	fsizehigh = 0;	\
} while(0)

#define posix_fsize_byname(name, fsizelow, fsizehigh) do { \
    FILE *__h; \
    __h = fopen(name, "rb");  \
	if (!__h) {  \
		fsizelow = fsizehigh = 0;		\
	} else {		\
		fseek(__h, 0, SEEK_END);	\
		fsizelow = ftell(__h);	\
		fclose(__h);	\
    } \
} while(0)

#endif		

/*
 * alloc and free relative to heap memory 
 */
#ifdef __KERNEL__
    #define posix_malloc(size)    rvmalloc(size)

    #define posix_free(ptr, size) rvfree(ptr, size)
#elif defined(_WIN32) && defined(_DRIVER)
	#define posix_malloc(size)    ExAllocatePool(NonPagedPool, size)

	#define posix_free(ptr, size) ExFreePool(ptr);
#else
	#define posix_malloc(size)    malloc(size)

    #define posix_free(ptr, size) free(ptr)
#endif

#ifdef __KERNEL__
	#define posix_memcmp(buf1, buf2, count)	memcmp(buf1, buf2, count)
	#define posix_memcpy(dst, src, count) memcpy(dst, src, count)
#elif defined(_WIN32) 
	#ifdef _DRIVER
		#define posix_memcmp(buf1, buf2, count) (RtlCompareMemory(buf1, buf2, count) == count)
		#define posix_memcpy(dst, src, count) RtlCopyMemory(dst, src, count)
	#else
		#define posix_memcmp(buf1, buf2, count) memcmp(buf1, buf2, count)
		#define posix_memcpy(dst, src, count) memcpy(dst, src, count)
	#endif
#endif

/*
 * string 
 */
#ifdef _WIN32
	#ifndef strncasecmp
	#define strncasecmp(s1, s2, c)		_strnicmp(s1, s2, c)
	#endif

	#ifndef strcasecmp
	#define strcasecmp(s1, s2)			_stricmp(s1, s2)
	#endif
#endif

/*
 *  spin lock semantics : mutual exclusion even on SMB; no schedule allowed
 */
#ifdef _WIN32 
    #ifndef  _DRIVER
        #define spinlock_t(splock)            CRITICAL_SECTION  _##splock		
        #define init_spinlock(p, splock)      InitializeCriticalSection(&(p)->_##splock)
        #define uninit_spinlock(p, splock)    DeleteCriticalSection(&(p)->_##splock)
        #define lock_spinlock(p, splock)      EnterCriticalSection(&(p)->_##splock)
        #define unlock_spinlock(p, splock)    LeaveCriticalSection(&(p)->_##splock)
    #else
        #define spinlock_t(splock)            KSPIN_LOCK _##splock; \
										      KIRQL _##splock_irql		
        #define init_spinlock(p, splock)      KeInitializeSpinLock(&(p)->_##splock)
        #define uninit_spinlock(p, splock)    /* nothing */
        #define lock_spinlock(p, splock)      KeAcquireSpinLock(&(p)->_##splock, &(p)->_##splock_irql)
        #define unlock_spinlock(p, splock)    KeReleaseSpinLock(&(p)->_##splock, (p)->_##splock_irql)
	#endif
#else

	#ifdef __KERNEL__
		#include <linux/spinlock.h>
		#define spinlock_t(splock)				  spinlock_t _##splock; \
										          unsigned long _save##splock
		#define init_spinlock(p, splock)          spin_lock_init(&((p)->_##splock))
		#define uninit_spinlock(p, splock)        /* nothing */
//    #define lock_spinlock(p, splock)          spin_lock_irqsave(&p->_##splock, &p->_save##splock)	
//    #define unlock_spinlock(p, splock)        spin_unlock_irqrestore(&p->_##splock, p->_save##splock)
		#define lock_spinlock(p, splock)
		#define unlock_spinlock(p, splock)
	#else
		#define spinlock_t(splock)				  uint32_t _##splock 
		#define init_spinlock(p, splock)          
		#define uninit_spinlock(p, splock)        
		#define lock_spinlock(p, splock)
		#define unlock_spinlock(p, splock)
	#endif
#endif

/*
 * some macro used frequency
 */
// 1.mask must be integer of like xxx1000
// 2.if val is 0, align_ceil will return 0. even if mask is any value.
#ifndef align_ceil
#define align_ceil( val, mask)     (((val) + (mask) - 1) & ~((mask) - 1)) 
#endif  

// assistant macro of byte,word,dword,qword opration. windef.h has this definition.
// for make macro, first parameter is low part, second parameter is high part.
#ifndef posix_mku64
	#define posix_mku64(l, h)		((uint64_t)(((uint32_t)(l)) | ((uint64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mku32
	#define posix_mku32(l, h)		((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mku16
	#define posix_mku16(l, h)		((uint16_t)(((uint8_t)(l)) | ((uint16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_mki64
	#define posix_mki64(l, h)		((int64_t)(((uint32_t)(l)) | ((int64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mki32
	#define posix_mki32(l, h)		((int32_t)(((uint16_t)(l)) | ((int32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mki16
	#define posix_mki16(l, h)		((int16_t)(((uint8_t)(l)) | ((int16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_lo32
	#define posix_lo32(v64)			((uint32_t)(v64))
#endif

#ifndef posix_hi32
	#define posix_hi32(v64)			((uint32_t)(((uint64_t)(v64) >> 32) & 0xFFFFFFFF))
#endif

#ifndef posix_lo16
	#define posix_lo16(v32)			((uint16_t)(v32))
#endif

#ifndef posix_hi16
	#define posix_hi16(v32)			((uint16_t)(((uint32_t)(v32) >> 16) & 0xFFFF))
#endif

#ifndef posix_lo8
	#define posix_lo8(v16)			((uint8_t)(v16))
#endif

#ifndef posix_hi8
	#define posix_hi8(v16)			((uint8_t)(((uint16_t)(v16) >> 8) & 0xFF))
#endif


#ifndef posix_max
#define posix_max(a,b)            (((a) > (b))? (a) : (b))
#endif

#ifndef posix_min
#define posix_min(a,b)            (((a) < (b))? (a) : (b))
#endif

#ifndef posix_clip
#define	posix_clip(x, min, max)	  posix_max(posix_min((x), (max)), (min))
#endif

#ifndef posix_abs
#define posix_abs(a)            (((a) >= 0)? (a) : (-(a)))
#endif

/*
 * some macro that assist struct data type 
 */
#if defined(_WIN32) && defined(_DRIVER) /* && defined(_NTDEF_) */
    // in ntdef.h, when define FIELD_OFFSET and CONTAINING_RECORD, it doesn't dectcte whether this macro is defined.
#else
    #ifndef FIELD_OFFSET
    #    define FIELD_OFFSET(type, member)    ((int32_t)&(((type *)0)->member))
    #endif
	
    #ifndef CONTAINING_RECORD
    #    define CONTAINING_RECORD(ptr, type, member) ((type *)((uint8_t*)(ptr) - (uint32_t)(&((type *)0)->member)))
    #endif
#endif


#ifndef container_of
#define container_of(ptr, type, member)  CONTAINING_RECORD(ptr, type, member)
#endif

/*
 * print debug information 
 */

#ifdef _WIN32
    #ifndef _DRIVER
		static _inline void posix_print(const char *formt, ...)
		{
			// SYSTEMTIME SystemTime;
			//support the file size is less than 4GB
			va_list va_param;
			char   szBuf[DBG_BUFF_SIZE];
			char   szBuf2[DBG_BUFF_SIZE];

			// GetSystemTime(&SystemTime);
			
			va_start(va_param, formt);
			vsprintf(szBuf, formt, va_param);
			va_end(va_param);
/*
			sprintf(szBuf2, "%-14s %02u:%02u:%02u:%03u  %s",
				PROG_NAME, 
				SystemTime.wHour, 
				SystemTime.wMinute,
				SystemTime.wSecond,
				SystemTime.wMilliseconds, 
				szBuf);
*/
#ifdef PROG_NAME
			sprintf(szBuf2, "%-14s %s", PROG_NAME, szBuf);
#else 
			strcpy(szBuf2, szBuf);
#endif

			OutputDebugString((LPCSTR)szBuf2);
		}
	    static _inline void posix_print_mb(const char *formt, ...)
		{
			//support the file size is less than 4GB
			va_list va_param;
			char   szBuf[DBG_BUFF_SIZE];
			
			va_start(va_param, formt);
			vsprintf(szBuf, formt, va_param);
			va_end(va_param);
#ifdef PROG_NAME
			MessageBox(NULL, szBuf, PROG_NAME, MB_OK);
#else
			MessageBox(NULL, (LPCSTR)szBuf, "module", MB_OK);
#endif
		}
    #else
        #define posix_print DbgPrint
        static __inline void posix_print_mb(char *format, ...) {}
    #endif

#elif defined(__KERNEL__)  // linux section
	#define posix_print(format, arg...)  printk(KERN_ALERT format, ##arg)
	#define posix_print_mb(format, arg...)  printk(KERN_ALERT format, ##arg)

#elif defined(__APPLE__) // mac os x/ios
	#define posix_print(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
	#define posix_print_mb(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

#elif defined(ANDROID) // android
	#define posix_print(format, ...) __android_log_print(ANDROID_LOG_INFO, "kingdom", format, ##__VA_ARGS__)
	#define posix_print_mb(format, ...) __android_log_print(ANDROID_LOG_INFO, "kingdom", format, ##__VA_ARGS__)

#else
	#define posix_print(format, arg...) printf(format, ## arg)
	#define posix_print_mb(format, arg...) printf(format, ## arg)
#endif

/*
 * assert section, linux doesn't support
 */
#if defined(_WIN32) 
	#ifdef _DRIVER
		#define posix_assert(exp) if (!(exp)) DbgBreakPoint()
	#else
		#include <assert.h>
		#define posix_assert(exp) assert(exp)
	#endif
#else
	#define posix_assert(exp)
#endif

/*
 * posix_msleep
 */
#ifdef _WIN32
#define posix_msleep(ms)			Sleep(ms)
#else
#define posix_msleep(ms)			usleep(1000 * (ms))
#endif

/*
 * some program uses MSB of uint32_t to indicate special function
 */ 
#ifndef BIT32_MSB_MASK
#define		BIT32_MSB_MASK			0x80000000
#endif

#ifndef BIT32_NONMSB_MASK
#define		BIT32_NONMSB_MASK		0x7fffffff
#endif

/*
 * reference to limits.h
 */
#ifndef INT16_MIN
#define INT16_MIN   (-32768)        /* minimum (signed) short value */
#endif

#ifndef INT16_MAX
#define INT16_MAX      32767         /* maximum (signed) short value */
#endif

#ifndef UINT16_MAX
#define UINT16_MAX     0xffff        /* maximum unsigned short value */
#endif

#ifndef INT32_MIN
#define INT32_MIN     (-2147483647 - 1) /* minimum (signed) int value */
#endif

#ifndef INT32_MAX
#define INT32_MAX       2147483647    /* maximum (signed) int value */
#endif

#ifndef UINT32_MAX
#define UINT32_MAX      0xffffffff    /* maximum unsigned int value */
#endif 

/*
 * pseud float point type
 */
union pseud_float {
	struct {
		uint32_t	decimal_part;
		uint32_t	integer_part;
	} u32;
	uint64_t		u64;
};

#if defined(_MSC_VER) 
	/* microsoft complier */
	#define	CONSTANT_4G					4294967296I64
	#define APPROXIMATELY_CARRY_VAL		0xFFFFFFFFFD000000I64 // if decimal_part is not less than (>=) 0.99, than think it has carry. 
	#define APPROXIMATELY_EQUAL_VAL		0xFFFFFFFFFF000000I64 // if MSB 8 bit is equal between tow value, than think deciaml part of these is equal.
#else 
	/* else, GCC */
	#define	CONSTANT_4G					4294967296ull
	#define APPROXIMATELY_CARRY_VAL		0xFFFFFFFFFD000000ull // if decimal_part is not less than (>=) 0.99, than think it has carry. 
	#define APPROXIMATELY_EQUAL_VAL		0xFFFFFFFFFF000000ull // if MSB 8 bit is equal between tow value, than think deciaml part of these is equal.
#endif

extern void pseud_float_evaluate(union pseud_float *p, uint64_t dividend, uint64_t divisor); 

extern int pseud_float_cmp(union pseud_float *p1, union pseud_float *p2);

#define pseud_float_add(p1, p2) \
    (p1)->u64 = (p1)->u64 + (p2)->u64

#define pseud_float_sub(p1, p2) \
    (p1)->u64 = (p1)->u64 - (p2)->u64

#define CONSTANT_1G			1073741824
#define CONSTANT_768M		805306368
#define CONSTANT_512M		536870912
#define CONSTANT_384M		402653184
#define CONSTANT_256M		268435456
#define CONSTANT_128M		134217728
#define CONSTANT_64M		67108864
#define CONSTANT_32M		33554432
#define CONSTANT_16M		16777216
#define CONSTANT_8M			8388608
#define CONSTANT_4M			4194304
#define CONSTANT_3M			3145728
#define CONSTANT_2M			2097152
#define CONSTANT_1M			1048576
#define CONSTANT_768K		786432
#define CONSTANT_512K		524288
#define CONSTANT_384K		393216
#define CONSTANT_256K		262144
#define CONSTANT_192K		196608
#define CONSTANT_128K		131072
#define CONSTANT_64K		65536
#define CONSTANT_32K		32768
#define CONSTANT_16K		16384
#define CONSTANT_8K			8192
#define CONSTANT_4K			4096
#define CONSTANT_2K			2048
#define CONSTANT_1K			1024

#ifndef _PUT_16
#define _PUT_16
#define PUT_16(p,v) ((p)[0]=((v)>>8)&0xff,(p)[1]=(v)&0xff)
#define PUT_32(p,v) ((p)[0]=((v)>>24)&0xff,(p)[1]=((v)>>16)&0xff,(p)[2]=((v)>>8)&0xff,(p)[3]=(v)&0xff)
// #define PUT_64(p,v) ((p)[0]=((v)>>56)&0xff,(p)[1]=((v)>>48)&0xff,(p)[2]=((v)>>40)&0xff,(p)[3]=((v)>>32)&0xff,(p)[4]=((v)>>24)&0xff,(p)[5]=((v)>>16)&0xff,(p)[6]=((v)>>8)&0xff,(p)[7]=(v)&0xff)
#define PUT_64(p,v) ((p)[0]=(char)((v)>>56)&0xff,(p)[1]=(char)((v)>>48)&0xff,(p)[2]=(char)((v)>>40)&0xff,(p)[3]=(char)((v)>>32)&0xff,(p)[4]=(char)((v)>>24)&0xff,(p)[5]=(char)((v)>>16)&0xff,(p)[6]=(char)((v)>>8)&0xff,(p)[7]=(char)(v)&0xff)
#define GET_16(p) (((p)[0]<<8)|(p)[1])
#define GET_32(p) (((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3])
// #define GET_64(p) (((p)[0]<<56)|((p)[1]<<48)|((p)[2]<<40)|((p)[3]<<32)|((p)[4]<<24)|((p)[5]<<16)|((p)[6]<<8)|(p)[7])
#endif

#define posix_swap32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define posix_swap16(a) ((((a) << 8) & 0xff00) | (((a) >> 8) & 0xff))

// @w: bmp宽度, 像79 x 100 x 24, 则就是79
// @bpp: bmp每像素位数, 像79 x 100 x 24, 则就是24
// 返回值: 符合bmp要求的每行4字节对齐,即实际存储时每行占用字节数, 像79 x 100 x 24, 则就是240
#define bmp_wbpp2bpl(w, bpp)		(((((w)*(bpp)) + 31) >> 5) << 2)

#endif // __POSIX_H_
