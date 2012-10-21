#include "xfunc.h"

char *formatstr(char *formt, ...)
{
	static char formatstrbuf[256];
	va_list va_param;
			
	va_start(va_param, formt);
	vsprintf(formatstrbuf, formt, va_param);
	va_end(va_param);

	return formatstrbuf;
}

// 如果pathname没有反划线,添加,如果有,则不再添加
// 函数会改变pathname,调用程序应该确保pathname接下一个还有空位
char* appendbackslash(const char* pathname)
{
	static char		gname[_MAX_PATH];
	size_t			len = strlen(pathname);

	strcpy(gname, pathname);
	if (gname[len - 1] != '\\') {
		gname[len] = '\\';
		gname[len + 1] = 0;
	}
	return gname;
}

char* dirname(const char* fullfilename) 
{
	static char		gname[_MAX_PATH];
	const char			*p, *p1;

	p = p1 = fullfilename;
	while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}

	if (p) {
		strncpy(gname, fullfilename, p - fullfilename);
		gname[p - fullfilename] = 0;
		if (gname[1] != ':') {
			gname[0] = 0;
		}
	} else {
		gname[0] = 0;
	}
	return gname[0]? gname: NULL;
}

// 返回路径中的文件名部分
// 给出一个包含有指向一个文件的全路径的字符串，本函数返回基本的文件名。
char* basename(const char* fullfilename) 
{
	static char		gname[_MAX_PATH];
	const char		*p, *p1;

	if ((strlen(fullfilename) == 2) && (fullfilename[1] == ':')) {
		strcpy(gname, fullfilename);
		return gname;
	}

	p = p1 = fullfilename;
	while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}

	if (p + 1) {
		strcpy(gname, p + 1);
	} else {
		gname[0] = 0;
	}
	return gname[0]? gname: NULL;
}

// 返回值:
//  不会返回NULL,像strcasecmp要是遇到参数NULL会报非法而调用程序经常就是直接拿这函数返回值去比较
char* extname(const char* filename)
{
	static char		gname[_MAX_EXTNAME + 1];
	const char		*p, *p1;

	gname[0] = 0;

	p = p1 = filename;
	while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}
	// p或者指向了短文件时的文件,长文件名时的'\'
	if (*p == '\\') {
		p = p + 1;
	}
	if (*p == 0) {
		return gname;
	}
	p1 = p;	// 短文件名中可能含有多个'.'，例如image.2510.img，此处只反返回最后一个'.'后内容，img
	while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '.');
	}
	// p或者指向了最后一个'.',或指向字符串结尾符'\0'
	if (*p != '.') {	// 这个短文件名中就没有点
		// 这是有文件名的，只是没有扩展名
		gname[0] = 0;
	} else {
		if (strlen(p + 1) > _MAX_EXTNAME) {
			return gname;	// 扩展名长度太长，认为无效	
		}
		strcpy(gname, p + 1);	// p1指向'.'
	}

	return gname;
}


// 111222,333 KB
char *format_len_u32n(uint32_t len)
{
	static char		fmtstr[20];
	uint32_t		integer_K;

	integer_K = len / CONSTANT_1K + ((len % CONSTANT_1K)? 1: 0);
	if (integer_K >= 1000) {
		sprintf(fmtstr, "%u,%03u", integer_K / 1000, integer_K % 1000);
	} else {
		sprintf(fmtstr, "%u", integer_K);
	}

	return fmtstr;
}
// 111222,333 KB
char *format_len_u64n(uint64_t len)
{
	static char		fmtstr[20];
	uint32_t		integer_K;

	integer_K = (uint32_t)(len / CONSTANT_1K + ((len % CONSTANT_1K)? 1: 0));
	if (integer_K >= 1000) {
		sprintf(fmtstr, "%u,%03u", integer_K / 1000, integer_K % 1000);
	} else {
		sprintf(fmtstr, "%u", integer_K);
	}
	
	return fmtstr;
}

char *format_space_u64n(uint64_t space)
{
	static char		fmtstr[20];
	double			val;

	if (space > CONSTANT_1G) {
		// 4.98 GB
		val = 1.0 * space / CONSTANT_1G;
		sprintf(fmtstr, "%.02f GB", val);
	} else if (space > CONSTANT_1M) {
		// 4.98 MB
		val = 1.0 * space / CONSTANT_1M;
		sprintf(fmtstr, "%u MB", (uint32_t)(val));
	} else {
		// 4.98 KB
		val = 1.0 * space / CONSTANT_1K;
		sprintf(fmtstr, "%u KB", (uint32_t)(val));
	}
	return fmtstr;
}

// 注意以下这两个函数和socket默认是相应的
char *inet_u32toa(const uint32_t addr)
{
	static char szIpAddr[50];
	
	sprintf(szIpAddr, "%i.%i.%i.%i",
		((int)(addr>>24)&0xFF),
        ((int)(addr>>16)&0xFF),
        ((int)(addr>> 8)&0xFF),
        ((int)(addr>> 0)&0xFF));
	
	return szIpAddr;
}

uint32_t inet_atou32(const char *ipaddr)
{
	int			f1, f2, f3, f4, retval = 0;
	uint32_t	ipv4;

	if (ipaddr) {
		retval = sscanf(ipaddr, "%i.%i.%i.%i", &f1, &f2, &f3, &f4);
	}

	if (retval == 4) {
		ipv4 =  ((f1 & 0xFF) << 24) | ((f2 & 0xFF) << 16) | ((f3 & 0xFF) << 8) | (f4 & 0xFF); 

	} else {
		ipv4 = 0;
	}
	return ipv4;
}

char *updextname(char *filename, char *newextname)
{
	static char		gname[_MAX_PATH];
	char			*p, *p1;

	strcpy(gname, filename);
	p = p1 = gname;
    while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}

	// p或者指向了短文件时的文件,长文件名时的'\'
	if (*p == '\\') {
		p = p + 1;
	}
	if (*p == 0) {
		return NULL;
	}
	p1 = strchr(p, '.');
	if (!p1) {
		// 没找到扩展名
		return NULL;
	}
	p1[1] = 0;	// 将扩展名每个字符置为'\0'
	strcat(gname, newextname);

	return gname;
}

char *offextname(const char *filename)
{
	static char		gname[_MAX_PATH];
	char			*p, *p1;

	strcpy(gname, filename);
	p = p1 = gname;
    while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}

	// p或者指向了短文件时的文件,长文件名时的'\'
	if (*p == '\\') {
		p = p + 1;
	}
	if (*p == 0) {
		return NULL;
	}
	p1 = strchr(p, '.');
	if (!p1) {
		// 没找到扩展名
		return NULL;
	}
	p1[0] = 0;	// 将扩展名的'.'改为'\0'

	return gname;
}

// -1: 找不到扩展名
// 0: 两扩展名相同
int cmpextname(char *filename, char *desiredextname)
{
	char			*p, *p1;

	p = p1 = filename;
    while (p1) {
		p = p1;
		p1 = strchr(p1 + 1, '\\');
	}

	// p或者指向了短文件时的文件,长文件名时的'\'
	if (*p == '\\') {
		p = p + 1;
	}
	if (*p == 0) {
		return -1;
	}
	p1 = strchr(p, '.');
	if (!p1) {
		// 没找到扩展名
		return -1;
	}
	// p1[1]是扩展名第一个字符
	return strcmp(p1 + 1, desiredextname);
}

char *u32tostr(uint32_t u32n)
{
	static char		text[12];
	sprintf(text, "%u", u32n);
	return text;
}

uint32_t strtou32(char *str, int hex)
{
	uint32_t	u32n;
	int			retval;

	if (hex) {
		retval = sscanf(str, "%x", &u32n);
	} else {
		retval = sscanf(str, "%u", &u32n);
	}
	return (retval == 1)? u32n: 0;
}

// 剔除str结尾的空格符
// 返回值: 被修改的字符串长度
// 注:可能改变str内容
uint32_t eliminate_terminal_blank_chars(char *str)
{
	char			*ptr = str + strlen(str);

	if (!str || !str[0]) {
		return 0;
	}
	ptr = str + strlen(str) - 1;

	while(*ptr == ' ') {
		*ptr = 0;
		if (ptr == str) {
			break;
		}
		ptr = ptr - 1;
	}
	return *str? (uint32_t)(ptr - str + 1): 0;
}

//
// remove all the blank characters except those in the double quotes
// 空字符(blank characters): 空格符 制表符 注: 不计双引号内的
// 注:
//  如果有须要被删除空字符,string中内存会被就地改掉
void trim_blank_characters(char* string)
{
	uint32_t	keyval_begin = 0, inside_quote = 0;
	uint32_t	length = (uint32_t)strlen(string);
	uint32_t	i, j;

	for (i = 0 , j = 0 ; i < length ; i ++) {
		switch ( string[i] ) {
			case ' ':
			case '\t':
				if ( inside_quote == 0 ) break;
				// fall through
			default:
				if ( keyval_begin ) {
					inside_quote = 1;
				}
				if ( string[i] == '=' ) {
					keyval_begin = 1;
				}
				if ( i != j ) {
					string[j] = string[i];
				}
				j ++;
				break;
		}
	}
	string[j] = 0;
}

//
// 删除注释符及注释符后的所有字符(在第一个/字符那里字符串值被置0)
// 注释符(blank characters): //  注: 不计双引号内的
// 
void trim_comments(char* string)
{
	uint32_t	inside_quote = 0;
	uint32_t	backslash_count = 0;
	uint32_t	length = (uint32_t)strlen(string);
	uint32_t	i;

	for (  i = 0 ; i < length ; i ++ ) {
		switch (string[i]) {
			case '/':
				if (inside_quote == 0) backslash_count ++;
				if (backslash_count == 2) {
					string[i-1] = 0;
					return;
				}
				break;				
			case '"':
				inside_quote = 1 - inside_quote;
				break;
			default:
				backslash_count = 0;
				break;
		}
	}
}

#define COMPARE_BLOCK_SIZE	CONSTANT_64M

bool file_compare(char* fname1, char*fname2)
{
	posix_file_t fp1 = INVALID_FILE, fp2 = INVALID_FILE;
	uint32_t bytertd1, bytertd2, fsizelow1, fsizehigh1, fsizelow2, fsizehigh2;
	uint64_t pos, fsize1, fsize2;
	uint8_t *data1 = NULL, *data2 = NULL;
	bool equal = false;

	posix_fopen(fname1, GENERIC_READ, OPEN_EXISTING, fp1);
	if (fp1 == INVALID_FILE) {
		posix_print_mb("cann't open file: %s", fname1);
		goto exit;
	}

	posix_fopen(fname2, GENERIC_READ, OPEN_EXISTING, fp2);
	if (fp2 == INVALID_FILE) {
		posix_print_mb("cann't open file: %s", fname2);
		goto exit;
	}

	posix_fsize(fp1, fsizelow1, fsizehigh1);
	fsize1 = posix_mku64(fsizelow1, fsizehigh1);
	posix_fsize(fp2, fsizelow2, fsizehigh2);
	fsize2 = posix_mku64(fsizelow2, fsizehigh2);

	if (fsize1 != fsize2) {
		posix_print_mb("file size is different");
		goto exit;
	}

	data1 = (uint8_t*)malloc(COMPARE_BLOCK_SIZE);
	if (!data1) {
		goto exit;
	}
	data2 = (uint8_t*)malloc(COMPARE_BLOCK_SIZE);
	if (!data2) {
		goto exit;
	}

	pos = 0;
	posix_print("file1: %s, file2: %s, compare size: %08xh\n", fname1, fname2, fsizelow1);
	while (pos < fsize1) {
		posix_print("compare address: %08xh\n", pos);
		posix_fread(fp1, data1, COMPARE_BLOCK_SIZE, bytertd1);
		posix_fread(fp2, data2, COMPARE_BLOCK_SIZE, bytertd2);
		if (bytertd1 != bytertd2) {
			goto exit;
		}
		if (memcmp(data1, data2, bytertd1)) {
			posix_print_mb("S: %08xh, detect different data", pos);
			goto exit;
		}
		pos += bytertd1;
	}
	equal = true;
	
exit:
	if (data1) {
		free(data1);
	}
	if (data2) {
		free(data2);
	}
	if (fp1 != INVALID_FILE) {
		posix_fclose(fp1);
	}
	if (fp2 != INVALID_FILE) {
		posix_fclose(fp2);
	}

	posix_print_mb("file1: %s, file2: %s, compare result: %s\n", fname1, fname2, equal? "true": "false");

	return equal;
}
