#ifndef __XFUNC_H_
#define __XFUNC_H_

#undef _UNICODE
#undef UNICODE

#include "posix.h"

// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>
// for memory lead detect end

#define is_empty_str(str)		(!(str) || !(str)[0])

//
// function section
//
#ifdef  __cplusplus
extern "C" {
#endif

char *appendbackslash(const char* pathname);
char *formatstr(char *format, ...);

char* dirname(const char *fullfilename);
char* basename(const char *fullfilename);
char* extname(const char *filename);

char *format_len_u64n(uint64_t len);
char *format_len_u32n(uint32_t len);
char *format_space_u64n(uint64_t space);

char *inet_u32toa(const uint32_t addr);
uint32_t inet_atou32(const char *ipaddr);

char *offextname(const char *filename);
char *updextname(char *filename, char *newextname);
int cmpextname(char *filename, char *desiredextname);
char *u32tostr(uint32_t u32n);
uint32_t strtou32(char *str, int hex);
uint32_t eliminate_terminal_blank_chars(char *str);

void trim_blank_characters(char* string);
void trim_comments(char* string);

bool file_compare(char* fname1, char*fname2);

#ifdef  __cplusplus
}
#endif

#endif // __XFUNC_H_
