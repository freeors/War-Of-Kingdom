#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include <shlobj.h> // SHBrowseForFolder

#include "resource.h"

#include "struct.h"
#include "xfunc.h"
#include "win32x.h"
// #include "param_def.h"
// #include "utf8.h"

void OnStartupStrategyCmb(HWND hdlgP, HWND hwndCtl, UINT codeNotify);
static void do_xchg(char *siteloc, char *fname, char *bfname);

void tbox_enter_ui(void)
{
	// 让底下状态栏处于空闲状态
	StatusBar_Idle();
	StatusBar_SetText(gdmgr._hwnd_status, 0, "");

	return;
}

// TRUE: 允许隐藏
// FALSE: 不允许隐藏
BOOL tbox_hide_ui(void)
{
	return TRUE;
}

// 对话框消息处理函数
BOOL On_DlgTBoxInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND				hctl = GetDlgItem(hdlgP, IDC_LV_TBOX_DESC);
	LVCOLUMN			lvc;

	gdmgr._hdlg_tbox = hdlgP;

	//
	// 初始化显示工作目录的列表框
	//
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 220;
	lvc.pszText = "Name";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 80;
	lvc.iSubItem = 0;
	lvc.pszText = "Length";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 180;
	lvc.iSubItem = 0;
	lvc.pszText = "LastWriteTime";
	ListView_InsertColumn(hctl, 2, &lvc);

    ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);

							 
	return FALSE;
}

void On_DlgTBoxCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

BOOL CALLBACK DlgTBoxProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPPSHNOTIFY			lppsn = (LPPSHNOTIFY)lParam;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgTBoxInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTBoxCommand);
	}
	
	return FALSE;
} 

//
//
//
void OnStartupStrategyCmb(HWND hdlgP, HWND hwndCtl, UINT codeNotify)
{
	int			idx;
	LRESULT		itemdata;

    if (codeNotify == CBN_SELCHANGE) {
		idx = ComboBox_GetCurSel(hwndCtl);
		if (idx != -1) {
			itemdata = ComboBox_GetItemData(hwndCtl, idx);
		}
	}

	return;
}

typedef struct {
	HWND		hctl;
	char		bfname[64];
	int64_t		maxLastWriteTime;
	int			idxMaxLastWriteTime;
} walkdirctx_t;

// @lastWriteTime: UTC时间，即通常的unix时候，自midnight (00:00:00), January 1, 1970后经过的秒数
BOOL cb_walk_dir(char *name, uint32_t flags, uint64_t len, int64_t lastWriteTime, uint32_t* ctx)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	walkdirctx_t* wdp = (walkdirctx_t*)ctx;

	if (strcasecmp(basename(name), wdp->bfname)) {
		return TRUE;
	}
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	// name
	lvi.iItem = ListView_GetItemCount(wdp->hctl);
	lvi.iSubItem = 0;
	lvi.pszText = name;
	lvi.lParam = (LPARAM) len;
	lvi.iImage = select_iimage_according_fname(name, flags);
	ListView_InsertItem(wdp->hctl, &lvi);


	if (flags & FILE_ATTRIBUTE_DIRECTORY) {
		text[0] = 0;
	} else {
        sprintf(text, "%s KB", format_len_u64n(len));
	}
	// Length
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	lvi.pszText = text;
	ListView_SetItem(wdp->hctl, &lvi);

	strcpy(text, _ctime64((__time64_t *)&lastWriteTime));
	text[strlen(text) - 1] = 0;
	// Create Time
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	lvi.pszText = text;
	ListView_SetItem(wdp->hctl, &lvi);

	// ListView_SortItemsEx(wdp->hctl, fn_dvr_compare, wdp->hctl);

	if (lastWriteTime > wdp->maxLastWriteTime) {
		wdp->maxLastWriteTime = lastWriteTime;
		wdp->idxMaxLastWriteTime = ListView_GetItemCount(wdp->hctl) - 1; 
	}
	
	return TRUE;
}

// 列出siteloc下的所有bfname文件，如果需要的话执行更新（以最后修改文件为范本）
// 
void do_coherence(const char* siteloc, char* bfname, int update, char* specialfname)
{
	walkdirctx_t			wdp;
	LVITEM					lvi;
	char					text[_MAX_PATH], text1[_MAX_PATH];
	int						idx, cnt;
	BOOL					fok, fLastModify;
	HWND					hdlgP = gdmgr._hdlg_tbox;

	fLastModify = is_empty_str(specialfname);

	memset(&wdp, 0, sizeof(walkdirctx_t));
	wdp.idxMaxLastWriteTime = -1;

	wdp.hctl = GetDlgItem(hdlgP, IDC_LV_TBOX_DESC);
	ListView_DeleteAllItems(wdp.hctl);

	strcpy(wdp.bfname, bfname);
	walk_dir_win32_fleeten(siteloc, 1, cb_walk_dir, (uint32_t *)&wdp);

	if (wdp.idxMaxLastWriteTime != -1) {
        lvi.iItem = wdp.idxMaxLastWriteTime;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(wdp.hctl, &lvi);
		strcpy(text, formatstr("%s\\%s", siteloc, text));
	} else {
		text[0] = 0;
		return;
	}
	if (!update) {
		return;
	}
	// text: 最近修改的全路径文件
	cnt = ListView_GetItemCount(wdp.hctl);
	for (idx = 0; idx < cnt; idx ++) {
		if (fLastModify && (idx == wdp.idxMaxLastWriteTime)) {
			continue;
		}
		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = text1;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(wdp.hctl, &lvi);
		strcpy(text1, formatstr("%s\\%s", siteloc, text1));
		if (fLastModify) {
			fok = CopyFile(text, text1, FALSE);
		} else if (strcasecmp(specialfname, text1)) {
			fok = CopyFile(specialfname, text1, FALSE);
		} else {
			continue;	// 没必要提示被copy了
		}

		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		lvi.pszText = text1;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(wdp.hctl, &lvi);
		
		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT | LVIF_IMAGE;
		lvi.iSubItem = 2;
		lvi.pszText = strcat(text1, fok? "[!SUCC]": "[!FAIL]");
		ListView_SetItem(wdp.hctl, &lvi);
	}
	
	return;
}

// 以fname为范本,改写siteloc下的所有bfname文件
void do_xchg(char *siteloc, char *fname, char *bfname)
{
	return;
}


#define TPLDIR					"templates\\default"

// 遇到{xxx <yyy> }，如果<yyy>是有效变量名，则以相应内容去替代
#define VARNAME_TITLE_STR			"title"
#define VARNAME_TITLE_LEN			5
#define VARNAME_CAPTION_STR			"caption"
#define VARNAME_CAPTION_LEN			7
#define VARNAME_CURPAGE_STR			"curpage"
#define VARNAME_CURPAGE_LEN			7
#define VARNAME_PREVPAGEURL_STR		"prevpageurl"
#define VARNAME_PREVPAGEURL_LEN		11
#define VARNAME_PREVPAGECAPTION_STR	"prevpagecaption"
#define VARNAME_PREVPAGECAPTION_LEN	15
#define VARNAME_NEXTPAGEURL_STR		"nextpageurl"
#define VARNAME_NEXTPAGEURL_LEN		11
#define VARNAME_NEXTPAGECAPTION_STR	"nextpagecaption"
#define VARNAME_NEXTPAGECAPTION_LEN	15


#define VARNAME_TXTFNAME_STR	"txtfname"
#define VARNAME_TXTFNAME_LEN	8

#define PHPVAR_PREFIX_STR		"{<"
#define PHPVAR_PREFIX_LEN		2
#define PHPVAR_POSTFIX_STR		">}"
#define PHPVAR_POSTFIX_LEN		2


#define MAXLEN_VARNAME			19
#define MAXLEN_VARNAMEPLUS1		20
#define MAXLEN_VARVAL			63
#define MAXLEN_VARVALPLUS1		64
typedef struct {
	char		_name[MAXLEN_VARNAMEPLUS1];
	uint32_t	_namelen;
	char		_val[MAXLEN_VARVALPLUS1];
	uint32_t	_vallen;
} varitem_t;

#define varitem_eval(varitem, name, namelen, val, vallen)	\
	strcpy((varitem)._name, name);	\
	(varitem)._namelen = namelen;	\
	strcpy((varitem)._val, val);	\
	(varitem)._vallen = vallen;

void tpl_substitute_var(char *dir, char *tplbfname, char *rltbfname, varitem_t *varitems, uint32_t itemcnt)
{
	char			tplfname[_MAX_PATH], rltfname[_MAX_PATH];
	posix_file_t	fhtpl = INVALID_FILE, fhrlt = INVALID_FILE;
	uint32_t		tplflen_high, tplflen_low;
	uint32_t		resibyte, bytertd;
	uint8_t			*tpldata = NULL, *ptr, *ptr1;
	uint32_t		idxvar;
	
	sprintf(tplfname, "%s\\%s", dir, tplbfname);
	sprintf(rltfname, "%s\\%s", dir, rltbfname);

	posix_fopen(tplfname, GENERIC_READ, OPEN_EXISTING, fhtpl);
	if (fhtpl == INVALID_FILE) { 
		posix_print_mb("tpl_substitute_var, cannot open template file: %s", tplfname);
		goto exit; 
	}
	posix_fopen(rltfname, GENERIC_WRITE, CREATE_ALWAYS, fhrlt);
	if (fhrlt == INVALID_FILE) { 
		posix_print_mb("tpl_substitute_var, cannot create file: %s", rltfname);
		goto exit; 
	}
	posix_fsize(fhtpl, tplflen_low, tplflen_high);

	tpldata = (uint8_t *)malloc(tplflen_low);
	if (tpldata == NULL) { 
		goto exit; 
	}

	posix_fread(fhtpl, tpldata, tplflen_low, bytertd);
	resibyte = tplflen_low;
	ptr = tpldata;
	while (ptr1 = (uint8_t *)strstr((char *)ptr, PHPVAR_PREFIX_STR)) {
		for (idxvar = 0; idxvar < itemcnt; idxvar ++) {
			if (!strncasecmp((char *)ptr1 + PHPVAR_PREFIX_LEN, varitems[idxvar]._name, varitems[idxvar]._namelen)) {
				break;
			}
		}
		if (idxvar == itemcnt) {
			ptr = ptr1 + PHPVAR_PREFIX_LEN;
			continue;
		}
		if (strncasecmp((char *)ptr1 + PHPVAR_PREFIX_LEN + varitems[idxvar]._namelen, PHPVAR_POSTFIX_STR, PHPVAR_POSTFIX_LEN)) {
			ptr = ptr1 + PHPVAR_PREFIX_LEN;
			continue;
		}
		posix_fwrite(fhrlt, ptr, (uint32_t)(ptr1 - ptr), bytertd);
		posix_fwrite(fhrlt, varitems[idxvar]._val, varitems[idxvar]._vallen, bytertd);
		resibyte -= (uint32_t)(ptr1 - ptr) + PHPVAR_PREFIX_LEN + varitems[idxvar]._namelen + PHPVAR_POSTFIX_LEN;
		ptr = ptr1 + PHPVAR_PREFIX_LEN + varitems[idxvar]._namelen + PHPVAR_POSTFIX_LEN;
	}
	posix_fwrite(fhrlt, tpldata + tplflen_low - resibyte, resibyte, bytertd);
	
exit:
	if (tpldata) {
		free(tpldata);
	}
	if (fhtpl != INVALID_FILE) {
		posix_fclose(fhtpl);
	}
	if (fhrlt != INVALID_FILE) {
		posix_fclose(fhrlt);
	}
    
	return;
}

typedef struct {
	char	_phptplbfname[MAXLEN_VARVALPLUS1];
	char	_htmtplbfname[MAXLEN_VARVALPLUS1];

	// php tpl variables
	char	_phprltbfname[MAXLEN_VARVALPLUS1];
	char	_title[MAXLEN_VARVALPLUS1];
	char	_caption[MAXLEN_VARVALPLUS1];
	char	_curpage[MAXLEN_VARVALPLUS1];
	char	_prevpageurl[MAXLEN_VARVALPLUS1];
	char	_prevpagecaption[MAXLEN_VARVALPLUS1];
	char	_nextpageurl[MAXLEN_VARVALPLUS1];
	char	_nextpagecaption[MAXLEN_VARVALPLUS1];

	// htm tpl variables
	char	_htmrltbfname[MAXLEN_VARVALPLUS1];
	char	_txtfname[MAXLEN_VARVALPLUS1];
} xbook_page_var_t;

void xbook_genpage(char *siteloc, xbook_page_var_t *pagevar)
{
	return;
}

#define XBOOKCFG_SEPCHAR		'\t'
int xbook_parse_tplline(char *line, char *phptplbfname, char *htmtplbfname)
{
	char	*ptr, *ptr1;

	phptplbfname[0] = htmtplbfname[0] = '\0';
	ptr = line;

	// ptr1: phptplbfname
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(phptplbfname, ptr);
	ptr = ptr1 + 1;

	// ptr1: htmtplbfname(最后一项)
	strcpy(htmtplbfname, ptr);

exit:
	return 0;
}

int xbook_parse_cfgline(char *line, xbook_page_var_t *pagevar)
{
	char	*ptr, *ptr1;

	pagevar->_phprltbfname[0] = pagevar->_title[0] = pagevar->_caption[0] = pagevar->_curpage[0] = pagevar->_prevpageurl[0] = pagevar->_prevpagecaption[0] = pagevar->_nextpageurl[0] = pagevar->_nextpagecaption[0] = '\0';
	pagevar->_htmrltbfname[0] = pagevar->_txtfname[0] = '\0';
	ptr = line;
	
	// ptr1: phprltbfname
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_phprltbfname, ptr);
	ptr = ptr1 + 1;

	// ptr1: title
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_title, ptr);
	ptr = ptr1 + 1;

	// ptr1: caption
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_caption, ptr);
	ptr = ptr1 + 1;

	// ptr1: curpage
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_curpage, ptr);
	ptr = ptr1 + 1;

	// ptr1: prevpageurl
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_prevpageurl, ptr);
	ptr = ptr1 + 1;

	// ptr1: prevpagecaption
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_prevpagecaption, ptr);
	ptr = ptr1 + 1;

	// ptr1: nextpageurl
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_nextpageurl, ptr);
	ptr = ptr1 + 1;

	// ptr1: nextpagecaption
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_nextpagecaption, ptr);
	ptr = ptr1 + 1;

	// ptr1: htmrltbfname
	ptr1 = strchr(ptr, XBOOKCFG_SEPCHAR);
	if (!ptr1) goto exit;
	*ptr1 = 0;
	strcpy(pagevar->_htmrltbfname, ptr);
	ptr = ptr1 + 1;

	// ptr1: txtfname(最后一项)
	strcpy(pagevar->_txtfname, ptr);
	
exit:
		
	return 0;
}

extern void trim_comments(char* string);
#define MAX_LINE_BYTES		1024
// 返回值:
//   0: 成功
//  -1: (fname参数不合法)|(不能打开文件)|(不能为line分配内存)
int read_pagecfg_from_file(char *siteloc, char *cfgbfname)
{
	FILE				*fp = NULL;
	char				*line = NULL;
	int					line_no, retval;
	size_t				length, pos;
	xbook_page_var_t	pagevar;
	
	memset(&pagevar, 0, sizeof(xbook_page_var_t));
	
	if (!cfgbfname || !cfgbfname[0]) {
		return -1;
	}
	fp = fopen(formatstr("%s\\%s", siteloc, cfgbfname), "r");
	if (!fp) {
		retval = -1;
		posix_print_mb("cann't open page configuration file: %s\\%s\n", siteloc, cfgbfname);
		goto exit;
	}
			
	line = (char*)malloc(MAX_LINE_BYTES);
	if (!line) {
		retval = -1;
		posix_print("malloc %i BYTE for line buffer fail\n", MAX_LINE_BYTES);
		goto exit;
	}
	
	line_no = 0;
	length = 0;
	while (fgets(line, MAX_LINE_BYTES - 1, fp) != NULL) {
		// First trim off any whitespace 
		size_t len = strlen(line);

		// trim trailing whitespace
		while (len > 0 && isspace(line[len - 1])) line[--len] = '\0';
		// trim leading whitespace 
		pos = strspn(line, " \n\r\t\v");
		memmove(line, &line[pos], len - pos + 1); // 1 is 'NULL'

		// trim all blank characters
		// trim_blank_characters(line);

		// trim "//" and subsequence characters 
		trim_comments(line);

		len = strlen(line);
		
		/* If this is NOT a comment line, try to interpret it */
		if (len && *line != '#') {
			posix_print("%03i:  %s\n", line_no, line);
			xbook_parse_cfgline(line, &pagevar);
			xbook_genpage(siteloc, &pagevar);
		} else if (len && *line == '#') {
			posix_print("%03i:  %s\n", line_no, line);
			xbook_parse_tplline(line + 1, pagevar._phptplbfname, pagevar._htmtplbfname);
		} else {
			posix_print("%03i:  %s\n", line_no, line);
		}
		
		line_no ++;
	}

	retval = 0;
exit:
	if (line) {
		free(line);
	}
	if (fp) {
		fclose(fp);
	}
	return retval;
}

void do_publisher(char *siteloc, char *pagecfgbfname)
{
	read_pagecfg_from_file(siteloc, pagecfgbfname);
	return;
}

// 此个函数只支持匹配auto_*.htm
typedef enum {
	mp_allpass			= 0,
	mp_xgenfile			= 1,
} match_preg_t;

typedef struct {
	char		str[32];
	uint32_t	len;
} str_len_pair_t;

#define XGEN_PREFIX_STR		"x_gen_"
#define XGEN_PREFIX_LEN		6
BOOL match_preg(match_preg_t mp, char *str, char *preg)
{
	char			*ptr;
	str_len_pair_t	slp[2];
	int				idx;
	BOOL			fok = TRUE;

	if (mp == mp_xgenfile) {
		_strlwr(str);
		strcpy(slp[0].str, ".php");
		slp[0].len = 4;
		strcpy(slp[1].str, ".htm");
		slp[1].len = 4;

		if (strncmp(str, XGEN_PREFIX_STR, XGEN_PREFIX_LEN)) {
			return FALSE;
		}

		for (idx = 0; idx < 2; idx ++) {
            if (!(ptr = strstr(str, slp[idx].str))) {
				continue;
			}
			if (!ptr[slp[idx].len]) {
				break;
			}
		}
		fok = (idx == 2)? FALSE: TRUE;

	}
	return fok;
}

typedef struct {
	HWND			hctl;
	match_preg_t	mp;
	char			preg[64];
} walkdirparam_t;

BOOL cb_walk_dir_2lv(char *name, uint32_t flags, uint64_t len, int64_t lastWriteTime, uint32_t* ctx)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	walkdirparam_t* wdp = (walkdirparam_t*)ctx;

	if (!match_preg(wdp->mp, name, wdp->preg)) {
		return TRUE;
	}

	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	// name
	lvi.iItem = ListView_GetItemCount(wdp->hctl);
	lvi.iSubItem = 0;
	lvi.pszText = name;
	lvi.lParam = (LPARAM) len;
	lvi.iImage = select_iimage_according_fname(name, flags);
	ListView_InsertItem(wdp->hctl, &lvi);

	if (flags & FILE_ATTRIBUTE_DIRECTORY) {
		text[0] = 0;
	} else {
        sprintf(text, "%s KB", format_len_u64n(len));
	}
	// Length
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	lvi.pszText = text;
	ListView_SetItem(wdp->hctl, &lvi);

	strcpy(text, _ctime64((__time64_t *)&lastWriteTime));
	text[strlen(text) - 1] = 0;
	// Create Time
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	lvi.pszText = text;
	ListView_SetItem(wdp->hctl, &lvi);

	return TRUE;
}

void TreeView_GenericWalk(char *path, match_preg_t mp, char *preg, int subfolders, int update = 0)
{
	HWND				hctl = GetDlgItem(gdmgr._hdlg_tbox, IDC_LV_TBOX_DESC);
	int					idx, cnt;
	char				text[_MAX_PATH];
	LVITEM				lvi;
	BOOL				fok;
	walkdirparam_t		wdp;

	memset(&wdp, 0, sizeof(walkdirparam_t));
	wdp.hctl = hctl;
	wdp.mp = mp;
	if (preg) {
		strcpy(wdp.preg, preg);
	}

	ListView_DeleteAllItems(hctl);
	walk_dir_win32_fleeten(path, subfolders, cb_walk_dir_2lv, (uint32_t *)&wdp);

	if (!update) {
		return;
	}

	cnt = ListView_GetItemCount(hctl);
	for (idx = 0; idx < cnt; idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(hctl, &lvi);
		strcpy(text, formatstr("%s\\%s", path, text));
		fok = delfile1(text);
		fok = TRUE;
		

		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(hctl, &lvi);
		
		lvi.iItem = idx;
		lvi.mask = LVIF_TEXT | LVIF_IMAGE;
		lvi.iSubItem = 2;
		lvi.pszText = strcat(text, fok? "[!SUCC]": "[!FAIL]");
		ListView_SetItem(hctl, &lvi);
	}
	return;	
}

// @path: 目录(内部不检查) 
void do_empty_directory(char *path)
{
	TreeView_GenericWalk(path, mp_allpass, NULL, 0, 1);
	return;
}

void do_xgenfile(char *path, int del)
{
	TreeView_GenericWalk(path, mp_xgenfile, NULL, 0, del);
	return;
}
