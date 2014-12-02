#include "win32x.h"
#include <shlobj.h> // SHBrowseForFolder

#include "xfunc.h"
//
// ----------------treeview(树形视图)------------------------
//

//
// 向树形列表增加一张叶子
//
// 向父节点添加子节点。它不设置子节点任何字段，子节点字段，例如图标，字符串，调用SetItem设定。
// 返回子节点句子柄
HTREEITEM TreeView_AddLeaf(HWND hwndTV, HTREEITEM hTreeParent)
{
    TV_INSERTSTRUCT tvins;
    HTREEITEM       hti;

    memset(&tvins, 0, sizeof(tvins));

    // Set the parent item
    tvins.hParent = hTreeParent;

    tvins.hInsertAfter = TVI_LAST;

    // no members are valid
    tvins.item.mask = 0;

    // Add the item to the tree-view control.
    hti = TreeView_InsertItem(hwndTV, &tvins);
    return hti;
}


// 更新某一ITEM的TV_ITEM结构信息
// @mask： 更改掩码。
// @lParam：mask有TVIF_PARAM标志时有效。
// @lImage：mask有TVIF_IMAGE标志时有效。
// @lSelectedImage：mask有TVIF_SELECTEDIMAGE标志时有效。
// @lpszText：mask有TVIF_TEXT标志时有效。
void TreeView_SetItem1(HWND hwndTV, HTREEITEM hTreeItem, UINT mask, LPARAM lParam, int iImage, int iSelectedImage, int cChildren, LPCSTR lpszText, ...)
{
	TCHAR           szBuffer[1024];
    va_list         list;

	if (lpszText) {
		// 要求lpszText不能为NULL
		va_start(list, lpszText);
		vsprintf(szBuffer, lpszText, list);
		va_end(list);
	} else {
		szBuffer[0] = 0;
	}

	TreeView_SetItem2(hwndTV, hTreeItem, mask, lParam, iImage, iSelectedImage, cChildren, szBuffer);
}

void TreeView_SetItem2(HWND hwndTV, HTREEITEM hTreeItem, UINT mask, LPARAM lParam, int iImage, int iSelectedImage, int cChildren, LPSTR pszText)
{
	TVITEMEX        tvi;

	// 得到一个基本的tvi
	tvi.mask = TVIF_HANDLE;
    tvi.hItem = hTreeItem;

    TreeView_GetItem(hwndTV, &tvi);

	tvi.mask = mask;
	tvi.lParam = lParam;
	tvi.iImage = iImage;
	tvi.iSelectedImage = iSelectedImage;
	tvi.cChildren = cChildren;
	tvi.pszText =  pszText;

	TreeView_SetItem(hwndTV, &tvi);
}

// 更新某一ITEM的TV_ITEM结构信息
// @mask： 更改掩码。
// @lParam：mask有TVIF_PARAM标志时有效。
// @lImage：mask有TVIF_IMAGE标志时有效。
// @lSelectedImage：mask有TVIF_SELECTEDIMAGE标志时有效。
// @lpszText：mask有TVIF_TEXT标志时有效。
HTREEITEM TreeView_AddLeaf1(HWND hwndTV, HTREEITEM hTreeParent, UINT mask, LPARAM lParam, int iImage, int iSelectedImage, int cChildren, LPSTR lpszText, ...)
{
    HTREEITEM       hti;

	TCHAR           szBuffer[1024];
    va_list         list;

	if (lpszText) {
		// 要求lpszText不能为NULL
		va_start(list, lpszText);
		vsprintf(szBuffer, lpszText, list);
		va_end(list);
	} else {
		szBuffer[0] = 0;
	}


	hti = TreeView_AddLeaf(hwndTV, hTreeParent);
	TreeView_SetItem1(hwndTV, hti, mask, lParam, iImage, iSelectedImage, cChildren, szBuffer);
    return hti;
}

void TreeView_ReleaseItem(HWND hctl, HTREEITEM hti, BOOL fDel)
{
	LPARAM		lparam = NULL;
	TreeView_GetItemParam(hctl, hti, lparam, LPARAM);
	if (lparam) {
		free((void*)lparam);
	}
	if (fDel) {
		TreeView_DeleteItem(hctl, hti);
	} else {
		TreeView_SetItemParam(hctl, hti, 0);
	}
	return;
}

void cb_treeview_walk_expand(HWND hctl, HTREEITEM hti, uint32_t* ctx)
{
	TreeView_Expand(hctl, hti, TVE_EXPAND);
}

// @fSubLeaf: 是否要遍子下级以及更下级叶子
// 遍历(hctl, htvi)叶子下的所有叶子, 分别调用fn.
// 注:
// 1. 调用函数中不要去删除本叶子,要删除使用函数给的fDelete置TRUE
void TreeView_Walk(HWND hctl, HTREEITEM htvi, BOOL fSubLeaf, fn_treeview_walk fn, uint32_t *ctx, BOOL fDelete)
{
	HTREEITEM			hChildTreeItem = NULL;
	HTREEITEM			hPrevChildTreeItem;

	hChildTreeItem = TreeView_GetChild(hctl, htvi);
	while (hChildTreeItem) {
		//
        // Call the fn on the node itself.
        //
		if (fSubLeaf) {
			TreeView_Walk(hctl, hChildTreeItem, fSubLeaf, fn, ctx);
		}
		if (fn) {
			fn(hctl, hChildTreeItem, ctx);
		}
		
        hPrevChildTreeItem = hChildTreeItem;
		hChildTreeItem = TreeView_GetNextSibling(hctl, hChildTreeItem);
		if (fDelete) {
			TreeView_DeleteItem(hctl, hPrevChildTreeItem);
		}
    }
	return;
}

void TreeView_GetItem1(HWND hctl, HTREEITEM hti, TVITEMEX *tvi, UINT mask, char *text)
{
	tvi->mask = mask;	
	tvi->hItem = hti;
	tvi->pszText = text;
	tvi->cchTextMax = _MAX_PATH;
	TreeView_GetItem(hctl, tvi);
}

// @pathprefix: 如果该值为NULL/pathprefix[0]=='\0', 在形成的路径前前加这一串
char* TreeView_FormPath(HWND hctl, HTREEITEM htvi, char *pathprefix)
{
	static char		path[_MAX_PATH];
	char			text[_MAX_PATH];
	HTREEITEM		htvip;
	TVITEMEX		tvi;

	path[0] = '\0';
	TreeView_GetItem1(hctl, htvi, &tvi, TVIF_TEXT, path);
	while ((htvip = TreeView_GetParent(hctl, htvi)) && (htvip != TVI_ROOT)) {
		TreeView_GetItem1(hctl, htvip, &tvi, TVIF_TEXT, text);
		if (path[0]) {
            strcpy(path, formatstr("%s\\%s", text, path));
		} else {
			strcpy(path, text);
		}
		htvi = htvip;
	}
	if (!is_empty_str(pathprefix)) {
		if (path[0]) {
			strcpy(path, formatstr("%s\\%s", pathprefix, path));
		} else {
			strcpy(path, pathprefix);
		}
	}

	return path;
}

void TreeView_FormVector(HWND hctl, HTREEITEM htvi, std::vector<std::string>& vec)
{
	char text[_MAX_PATH];
	HTREEITEM htvip;
	TVITEMEX tvi;

	TreeView_GetItem1(hctl, htvi, &tvi, TVIF_TEXT, text);
	vec.push_back(text);
	while ((htvip = TreeView_GetParent(hctl, htvi)) && (htvip != TVI_ROOT)) {
		TreeView_GetItem1(hctl, htvip, &tvi, TVIF_TEXT, text);
		vec.push_back(text);
		htvi = htvip;
	}
}

void TreeView_FormVector(HWND hctl, HTREEITEM htvi, std::vector<std::pair<LPARAM, std::string> >& vec)
{
	char text[_MAX_PATH];
	HTREEITEM htvip;
	TVITEMEX tvi;

	TreeView_GetItem1(hctl, htvi, &tvi, TVIF_TEXT | TVIF_PARAM, text);
	vec.push_back(std::make_pair(tvi.lParam, text));
	while ((htvip = TreeView_GetParent(hctl, htvi)) && (htvip != TVI_ROOT)) {
		TreeView_GetItem1(hctl, htvip, &tvi, TVIF_TEXT | TVIF_PARAM, text);
		vec.push_back(std::make_pair(tvi.lParam, text));
		htvi = htvip;
	}
}

// 怕一些静态变量可能冲突,由宏改为函数
// 当目录下删除最后一张叶子后, 直接设TVIF_CHILDREN为0, 不能除去前面-符号. 增加TreeView_Expand还是不行
void TreeView_SetChilerenByPath(HWND hctl, HTREEITEM htvi, char *path)
{
	// 这里要求要是path是文件的话(原则上是不应该是出现的,但调用程序可能不小心出现), is_empty_dir需要返回TRUE,
	// 否则这个文件叶子要被错误的被cChildren置为1.
	if (is_empty_dir(path)) {
		TreeView_Expand(hctl, htvi, TVE_COLLAPSE);	
		TreeView_SetItem1(hctl, htvi, TVIF_CHILDREN, 0, 0, 0, 0, NULL);	
	} else {	
		TreeView_SetItem1(hctl, htvi, TVIF_CHILDREN, 0, 0, 0, 1, NULL);	
	}	
	return;
}

int CALLBACK fn_tvi_compare_sort(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	HWND				hctl = (HWND)lParamSort;
	TVITEMEX			tvi1, tvi2;
	int					tvi1_type, tvi2_type;
	char				name1[_MAX_PATH], name2[_MAX_PATH];
	
	memset(&tvi1, 0, sizeof(TVITEMEX));
	memset(&tvi2, 0, sizeof(TVITEMEX));
	TreeView_GetItem1(hctl, (HTREEITEM)lParam1, &tvi1, TVIF_HANDLE | TVIF_TEXT | TVIF_IMAGE, name1);
	TreeView_GetItem1(hctl, (HTREEITEM)lParam2, &tvi2, TVIF_HANDLE | TVIF_TEXT | TVIF_IMAGE, name2);

	tvi1_type = tvi1.iImage? 1: 0;
	tvi2_type = tvi2.iImage? 1: 0;
	if (tvi1_type < tvi2_type) {
		// tvi1是目录, tvi2是文件
		return -1;
	} else if (tvi1_type > tvi2_type) {
		// tvi1是文件, tvi2是目录
		return 1;
	} else {
		// lvi1和lvi2都是目录或文件,只好用字符串比较
		return strcasecmp(name1, name2);
	}
}

// --------------------------------------------------------------


#define FT2UT_INTVAL_100NS		116444736000000000I64
#define FileTimeToUnixTime(ft)	\
	((posix_mku64(ft.dwLowDateTime, ft.dwHighDateTime) - 116444736000000000I64) / 10000000)

// @rootdir: 欲遍历的根目录
// @subdir: 此次要遍历的除去根目录的部分的子目录.
void walk_subdir_win32(const char* rootdir, char *subdir, int deepen, fn_walk_dir fn, uint32_t *ctx)
{
	HANDLE				hFind;
	WIN32_FIND_DATA		finddata;
	BOOL				fOk;
	int64_t				timestamp = 0;
	char				cFileNameComp[_MAX_PATH];
	
	hFind = FindFirstFile("*.*", &finddata);
	fOk = (hFind != INVALID_HANDLE_VALUE);

	while (fOk) {
		sprintf(cFileNameComp, "%s\\%s", subdir, finddata.cFileName);
		timestamp = FileTimeToUnixTime(finddata.ftLastWriteTime);
		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// 目录
			if ((strcmp(finddata.cFileName, ".") != 0) && (strcmp(finddata.cFileName, "..") != 0)) {
				if (deepen && fn) {
					// 由浅到深: 是先调用fn,再去在该目上递归
					fn(cFileNameComp, FILE_ATTRIBUTE_DIRECTORY, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx);
				}
				SetCurrentDirectory(formatstr("%s\\%s\\", rootdir, cFileNameComp));
				walk_subdir_win32(rootdir, cFileNameComp, deepen, fn, ctx);
				if (!deepen && fn) {
					// 由深到浅: 是先在该目上递归,再去调动fn
					fn(cFileNameComp, FILE_ATTRIBUTE_DIRECTORY, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx);
				}
			}
		} else {
			// 文件
			if (fn) {
				fn(cFileNameComp, 0, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx);
			}
		}
		fOk = FindNextFile(hFind, &finddata);
	}
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
	}

	return;
}

// @rootdir: 欲遍历的根目录
// @subfolders: 是否要遍历子目录. 0: 不遍历
// @deepen: 遍历, 调用fn时给的参数是, 1: 浅到深, 0: 由深到浅
// 注:
//  1.靠这种FindFirstFile方法删除目录是不行的.当检测到是目时,调用RemoveDirectory将返回总是失败(错误码:32,指示另有进行在使用)
//    FindClose后才可以,而这时close时不可能的事.要删目录改调用SHFileOperation.
BOOL walk_dir_win32(const char* rootdir, int subfolders, int deepen, fn_walk_dir fn, uint32_t *ctx, int del)
{
	char				szCurrDir[_MAX_PATH], text[_MAX_PATH];
	HANDLE				hFind;
	WIN32_FIND_DATA		finddata;
	BOOL				fOk, fRet = TRUE;
	int64_t				timestamp = 0;
		
	GetCurrentDirectory(_MAX_PATH, szCurrDir);
	SetCurrentDirectory(appendbackslash(rootdir));
	hFind = FindFirstFile("*.*", &finddata);
	fOk = (hFind != INVALID_HANDLE_VALUE);

	while (fOk) {
		timestamp = FileTimeToUnixTime(finddata.ftLastWriteTime);
		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// 目录
			if ((strcmp(finddata.cFileName, ".") != 0) && (strcmp(finddata.cFileName, "..") != 0)) {
				if (deepen && fn) {
					// 由浅到深: 是先调用fn,再去在该目上递归
					if (!fn(finddata.cFileName, FILE_ATTRIBUTE_DIRECTORY, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx)) {
						fRet = FALSE;
						break;
					}
				}
				sprintf(text, "%s\\%s\\", rootdir, finddata.cFileName);
				if (subfolders) {
					SetCurrentDirectory(text);
					walk_subdir_win32(rootdir, finddata.cFileName, deepen, fn, ctx);
				}
				if (!deepen && fn) {
					// 由深到浅: 是先在该目上递归,再去调动fn
					if (!fn(finddata.cFileName, FILE_ATTRIBUTE_DIRECTORY, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx)) {
						fRet = FALSE;
						break;
					}
				}
				if (del) {
					RemoveDirectory(text);
					DWORD err = GetLastError();
				}
			}
		} else {
			// 文件
			if (fn) {
				if (!fn(finddata.cFileName, 0, posix_mku64(finddata.nFileSizeLow, finddata.nFileSizeHigh), timestamp, ctx)) {
					fRet = FALSE;
					break;
				}
			}
			if (del) {
				sprintf(text, "%s\\%s", rootdir, finddata.cFileName);
				delfile(text);
			}
		}
		fOk = FindNextFile(hFind, &finddata);
	}
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
	}

	SetCurrentDirectory(szCurrDir);
	return fRet;
}

BOOL is_empty_dir(const char* dir)
{
	HANDLE				hFind;
	WIN32_FIND_DATA		finddata;
	char				szCurrDir[_MAX_PATH];
	BOOL				fOk, fEmpty = TRUE;;

	if (!is_directory(dir)) {
		// 不是目录,返回TRUE
		return TRUE;
	}
	GetCurrentDirectory(_MAX_PATH, szCurrDir);
	SetCurrentDirectory(appendbackslash(dir));

	hFind = FindFirstFile("*.*", &finddata);
	fOk = (hFind != INVALID_HANDLE_VALUE);
	
	while (fOk) {
		if ((strcmp(finddata.cFileName, ".") != 0) && (strcmp(finddata.cFileName, "..") != 0)) {
			fEmpty = FALSE;
			break;
		}
		fOk = FindNextFile(hFind, &finddata);
	}
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
	}

	

	SetCurrentDirectory(szCurrDir);
	return fEmpty;
}

BOOL is_directory(const char* fname)
{
	WIN32_FILE_ATTRIBUTE_DATA		fattrdata;
	BOOL fok = GetFileAttributesEx(fname, GetFileExInfoStandard, &fattrdata);
	if (!fok) return FALSE;
	return (fattrdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? TRUE: FALSE;
}

BOOL is_file(const char *fname)
{
	WIN32_FILE_ATTRIBUTE_DATA		fattrdata;
	BOOL fok = GetFileAttributesEx(fname, GetFileExInfoStandard, &fattrdata);
	if (!fok) return FALSE;
	return (fattrdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)? FALSE: TRUE;
}

typedef struct {
	char			path[_MAX_PATH];
	fn_walk_dir		fn;
	uint32_t		*ctx;
} walkdirempty_t;

static void cb_walk_dir_empty(char *name, uint32_t flags, uint64_t len, int64_t lastWriteTime, walkdirempty_t *wdepy)
{
	char			path[_MAX_PATH];
	
	if (wdepy->fn) {
		wdepy->fn(name, flags, len, lastWriteTime, wdepy->ctx);
	}
	sprintf(path, "%s\\%s", wdepy->path, name);
	if (flags & FILE_ATTRIBUTE_DIRECTORY) {
		RemoveDirectory(appendbackslash(path));
		DWORD err = GetLastError();
	} else {
        delfile(path);
	}
	return;
}

BOOL empty_directory(const char *path, fn_walk_dir fn, uint32_t *ctx)
{
	walk_dir_win32(path, 1, 0, fn, ctx, 1);
	return TRUE;
}

// @fname: 须是文件
BOOL delfile(char *fname)
{
	WIN32_FILE_ATTRIBUTE_DATA		fattrdata;

	// 去掉只读属性
	GetFileAttributesEx(fname, GetFileExInfoStandard, &fattrdata);
	SetFileAttributes(fname, fattrdata.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
	return DeleteFile(fname);
}

// @fname: 可能是文件也可能是目录
BOOL delfile1(const char *fname)
{
	WIN32_FILE_ATTRIBUTE_DATA		fattrdata;
	BOOL							fok = TRUE;
	char							text[_MAX_PATH];

	fok = GetFileAttributesEx(fname, GetFileExInfoStandard, &fattrdata);
	if (!fok) {
		DWORD err = GetLastError();
		return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
	}
	if (fattrdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		// 要删除是目录
		// 1.清空目录
		SHFILEOPSTRUCT		fopstruct;

		memset(&fopstruct, 0, sizeof(SHFILEOPSTRUCT));
		fopstruct.hwnd = NULL;
		fopstruct.wFunc = FO_DELETE;
		strcpy(text, fname);
		text[strlen(text) + 1] = '\0';
		fopstruct.pFrom = text;
		fopstruct.fFlags = FOF_NOERRORUI | FOF_NOCONFIRMATION; // 不要显示不能删除, 不要显示确认要不要删除对话框
		fok = SHFileOperation(&fopstruct)? FALSE: TRUE;
		// empty_directory(fname, NULL, NULL);
	} else {
		// 要删除是文件
		// 去掉只读属性
		SetFileAttributes(fname, fattrdata.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
		fok = DeleteFile(fname);
	}

	return fok;
}


// GetCurrentExePath,返回当前正在执行的应用程序的所有目录.
// 返回的szCurExeDir不包括最后目录符
void GetCurrentExePath(char *szCurExeDir)
{
	char szCurDir[MAX_PATH];
	char szCmdLine[MAX_PATH];
	char *pszTmp1, *pszTmp2;
	BOOL fFound = FALSE;
	// 在命令行情况下,GetCurrentDirectory返回控制台启动
	// 的路径,GetCommandLine是命令行,如debug\tvinstall.exe
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);

    // GetCommandLine是加了" "符号的,因为后面程序会去掉后面的"号
	strcpy(szCmdLine, GetCommandLine());

	// 1.szCmdLine可能含有两部分,应用程序和参数.应用程序肯定存在,参数可能不存在.
	// 2.对应用程序,如果中间含有空间,必定会用" "包围,如果没有空格,不会用" "包围.
	// 3.应用程序和参数之间用空格分隔.

	if (szCmdLine[0] == '\"') {
		// 应用程序中间含有空隔,只复制" "之间数据
		strcpy(szCmdLine, GetCommandLine() + 1);
		pszTmp1 = strchr(szCmdLine, '\"');
		if (pszTmp1) {
            *pszTmp1 = 0;
		}
	} else {
		// 应用程序中间不含空隔,如果有可能就找空格,去掉参数部分
        pszTmp1 = strchr(szCmdLine, ' ');
		if (pszTmp1) {
			*pszTmp1 = 0;
		}
	}

	// 1.去掉命令行最后一节,最后一节就是可执行文件名,.exe可能会没有
	pszTmp1 = pszTmp2 = szCmdLine;
	while (*pszTmp1) {
		if (*pszTmp1 == '\\') {
			pszTmp2 = pszTmp1;
		}
		pszTmp1 ++;
	}
	*pszTmp2 = 0;
	// 2.检查命令行是否有驱动器名,如果有就把它认为时全路径,命令行
	// 一定包含了应用程序名,只不过可能没有.exe扩展名
	// Window文件名不可能包含':'
	pszTmp1 = szCmdLine;
	while (*pszTmp1) {
		if (*pszTmp1 == ':') {
			fFound = TRUE;
			break;
		}
		pszTmp1 ++;
	}
    if (fFound) { 
		// 一个全路径的命令行,不必再与CurrentDirectory进行相加
		strcpy(szCurExeDir, szCmdLine);
	} else {
		wsprintf(szCurExeDir, "%s\\%s", szCurDir, szCmdLine);
	}
}

struct tcompare_dir_param
{
	tcompare_dir_param(const std::string& dir1, const std::string& dir2, int& recursion_count)
		: current_path1(dir1)
		, current_path2(dir2)
		, recursion_count(recursion_count)
	{
		std::replace(current_path1.begin(), current_path1.end(), '/', '\\');
		if (current_path1.at(current_path1.size() - 1) == '\\') {
			current_path1.erase(current_path1.size() - 1);
		}

		if (!dir2.empty()) {
			std::replace(current_path2.begin(), current_path2.end(), '/', '\\');
			if (current_path2.at(current_path2.size() - 1) == '\\') {
				current_path2.erase(current_path2.size() - 1);
			}
		}
	}

	std::string current_path1;
	std::string current_path2;
	int& recursion_count;
};

BOOL cb_compare_dir_explorer(char *name, uint32_t flags, uint64_t len, int64_t lastWriteTime, uint32_t* ctx)
{
	tcompare_dir_param& cdp = *(tcompare_dir_param*)ctx;
	bool compair = !cdp.current_path2.empty();
	cdp.recursion_count ++;
	std::string path2 = cdp.current_path2;
	if (compair) {
		path2.append("\\");
		path2.append(name);
	}
	
	// compair
	if (flags & FILE_ATTRIBUTE_DIRECTORY) {
		if (compair) {
			if (!is_directory(path2.c_str())) {
				return FALSE;
			}
		}

		tcompare_dir_param cdp2(cdp.current_path1 + "\\" + name, path2, cdp.recursion_count);
		if (!walk_dir_win32_deepen(cdp2.current_path1.c_str(), 0, cb_compare_dir_explorer, (uint32_t *)&cdp2)) {
			return FALSE;
		}

	} else if (compair) {
		if (!is_file(path2.c_str())) {
			return FALSE;
		}

	}
	return TRUE;
}

BOOL compare_dir(const std::string& dir1, const std::string& dir2)
{
	int recursion1_count = 0;
	tcompare_dir_param cdp1(dir1, dir2, recursion1_count);
	BOOL fok = walk_dir_win32_deepen(dir1.c_str(), 0, cb_compare_dir_explorer, (uint32_t *)&cdp1);
	if (!fok) {
		return FALSE;
	}

	int recursion2_count = 0;
	tcompare_dir_param cdp2(dir2, "", recursion2_count);
	walk_dir_win32_deepen(dir2.c_str(), 0, cb_compare_dir_explorer, (uint32_t *)&cdp2);

	if (cdp1.recursion_count != cdp2.recursion_count) {
		return FALSE;
	}
	return TRUE;
}

// @src: 可能是文件也可能是目录
// @dst: 可能是文件也可能是目录
// if copy directory, dst must be not exist.
bool copyfile(const char *src, const char *dst)
{
	WIN32_FILE_ATTRIBUTE_DATA		fattrdata;
	BOOL							fok = TRUE;
	char							text1[_MAX_PATH], text2[_MAX_PATH];

	fok = GetFileAttributesEx(src, GetFileExInfoStandard, &fattrdata);
	if (!fok) return false;
	if (fattrdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		// 要复制的是目录
		SHFILEOPSTRUCT		fopstruct;

		memset(&fopstruct, 0, sizeof(SHFILEOPSTRUCT));
		fopstruct.hwnd = NULL;
		fopstruct.wFunc = FO_COPY;

		strcpy(text1, src);
		text1[strlen(text1) + 1] = '\0';
		fopstruct.pFrom = text1;

		strcpy(text2, dst);
		text2[strlen(text2) + 1] = '\0';
		fopstruct.pTo = text2;

		fopstruct.fFlags = FOF_NOERRORUI | FOF_NOCONFIRMATION; // 不要显示不能删除, 不要显示确认要不要删除对话框
		fok = SHFileOperation(&fopstruct)? FALSE: TRUE;
		if (fok) {
			fok = compare_dir(src, dst);
		}
	} else {
		// 要复制的是文件
		fok = CopyFile(src, dst, FALSE);
	}

	return fok? true: false;
}

bool copy_root_files(const char* src, const char* dst)
{
	char				szCurrDir[_MAX_PATH], text1[_MAX_PATH], text2[_MAX_PATH];
	HANDLE				hFind;
	WIN32_FIND_DATA		finddata;
	BOOL				fok, fret = TRUE;
	
	if (!src || !src[0] || !dst || !dst[0]) {
		return false;
	}
		
	GetCurrentDirectory(_MAX_PATH, szCurrDir);
	SetCurrentDirectory(appendbackslash(src));
	hFind = FindFirstFile("*.*", &finddata);
	fok = (hFind != INVALID_HANDLE_VALUE);

	while (fok) {
		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// 目录
		} else {
			// 文件
			sprintf(text1, "%s\\%s", src, finddata.cFileName);
			sprintf(text2, "%s\\%s", dst, finddata.cFileName);
			fret = CopyFile(text1, text2, FALSE);
			if (!fret) {
				posix_print_mb("copy file from %s to %s fail", text1, text2);
				break;
			}
		}
		fok = FindNextFile(hFind, &finddata);
	}
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
	}

	SetCurrentDirectory(szCurrDir);
	return fret? true: false;
}

char* GetBrowseFilePath(HWND hdlgP)
{
	static char			gname[_MAX_PATH];
	LPITEMIDLIST		pidl;
	BROWSEINFO			bsif;

	bsif.hwndOwner = hdlgP;
    bsif.pidlRoot = NULL;
    bsif.pszDisplayName = gname;
    bsif.lpszTitle = NULL;
    bsif.ulFlags = 0;
    bsif.lpfn = NULL;
    bsif.lParam = NULL;
    bsif.iImage = 0;

	pidl = SHBrowseForFolder(&bsif);

	if (pidl) {
		SHGetPathFromIDList(pidl, gname);
		CoTaskMemFree(pidl);

		// 如果可能,去掉结尾的'\'字符
		if (gname[0] && (gname[strlen(gname) - 1] == '\\')) {
			gname[strlen(gname) - 1] = 0;
		}
		return gname;
	} else {
		return NULL;
	}
}

char* GetBrowseFileName(const char *strInintialDir, char *strFilter, BOOL fFileMustExist)
{
	static char		gname[_MAX_PATH];
    OPENFILENAME	ofn={0};

    // Reset filename
    *gname = 0;

    // Fill in standard structure fields
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize       = sizeof(OPENFILENAME);
    ofn.hwndOwner         = NULL;
    ofn.lpstrFilter       = strFilter;
//		"Video Files (*.asf; *.avi; *.qt; *.mov)\0*.asf; *.avi; *.qt; *.mov\0\0";
		
    ofn.lpstrCustomFilter = NULL;
    ofn.nFilterIndex      = 1;
    ofn.lpstrFile         = gname;
    ofn.nMaxFile          = _MAX_PATH;
    ofn.lpstrTitle        = TEXT("Open File...\0");
    ofn.lpstrFileTitle    = NULL;
    ofn.lpstrDefExt       = TEXT("*\0");
	ofn.Flags             = OFN_PATHMUSTEXIST;
	if (fFileMustExist) {
		ofn.Flags		 |= OFN_FILEMUSTEXIST;
	}
    
    // ofn.lpstrInitialDir = "\\\0";
	ofn.lpstrInitialDir = strInintialDir;
    
    // Create the standard file open dialog and return its result
	if (GetOpenFileName((LPOPENFILENAME)&ofn)) {
		return gname;
	} else {
		return NULL;
	}
}

char* GetBrowseFileName_title(const char* strInintialDir, char *strFilter, BOOL fFileMustExist, char *title)
{
	static char		gname[_MAX_PATH];
    OPENFILENAME	ofn={0};

    // Reset filename
    *gname = 0;

    // Fill in standard structure fields
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize       = sizeof(OPENFILENAME);
    ofn.hwndOwner         = NULL;
    ofn.lpstrFilter       = strFilter;
//		"Video Files (*.asf; *.avi; *.qt; *.mov)\0*.asf; *.avi; *.qt; *.mov\0\0";
		
    ofn.lpstrCustomFilter = NULL;
    ofn.nFilterIndex      = 1;
    ofn.lpstrFile         = gname;
    ofn.nMaxFile          = _MAX_PATH;
    ofn.lpstrTitle        = title;
    ofn.lpstrFileTitle    = NULL;
    ofn.lpstrDefExt       = TEXT("*\0");
	ofn.Flags             = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	if (fFileMustExist) {
		ofn.Flags		 |= OFN_FILEMUSTEXIST;
	}
    
    // ofn.lpstrInitialDir = "\\\0";
	ofn.lpstrInitialDir = strInintialDir;
    
    // Create the standard file open dialog and return its result
	if (GetOpenFileName((LPOPENFILENAME)&ofn)) {
		return gname;
	} else {
		return NULL;
	}
}


// Used to retrieve a value give the section and key
int CfgQueryValueWin(char *szFileName, char* szSection, char* szKeyName, char* szKeyValue)
{
	DWORD dwRetVal;

	// Get the info from the .ini file	
	dwRetVal = GetPrivateProfileString(szSection, szKeyName, "", szKeyValue, _MAX_PATH, szFileName);	

	//
	// 返回值:
	// 0: 失败,或该key就没值,相应的szKeyValue[0]会被置为'\0'
	// 非零: key值的长度,不包括结束符,例始key = sec, dwRetVal就是3
	//

	// szKeyValue:是一些已去掉空格的字符
	return dwRetVal? 0: -1;
}

// Used to add or set a key value pair to a section
int CfgSetValueWin(char *szFileName, char* szSection, char* szKeyName, const char* szKeyValue)
{
	DWORD dwRetVal;

	dwRetVal = WritePrivateProfileString (szSection, szKeyName, szKeyValue, szFileName);
	//
	// 返回值:
	// nonzero:		成功
	// zero:		失败
	//
	return dwRetVal? 0: -1;
}

BOOL MakeDirectory(const std::string& dd)
{
	HANDLE fFile; // File Handle
	WIN32_FIND_DATA fileinfo; // File Information Structure
	std::vector<std::string> m_arr; // CString Array to  hold Directory Structures
	BOOL tt; // BOOL used to test if Create Directory was successful
	size_t x1 = 0; // Counter
	std::string tem = ""; // Temporary std::string Object
    
	if (dd.empty()) {
		return FALSE;
	}

	fFile = FindFirstFile(dd.c_str(), &fileinfo);
    
	// if the file exists and it is a directory
	if ((fFile != INVALID_HANDLE_VALUE) && (fileinfo.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)) {
		// Directory Exists close file and return
		FindClose(fFile);
		return FALSE;
	}
	// Close the file
	FindClose(fFile);

	m_arr.clear();
    
	for (x1 = 0; x1 < dd.size(); x1++ ) { // Parse the supplied CString Directory String
		if (dd.at(x1) != '\\') { // if the Charachter is not a \     
			tem += dd.at(x1); // add the character to the Temp String   
		} else {
			m_arr.push_back(tem); // if the Character is a \     
			tem += "\\"; // Now add the \ to the temp string
		}   
		if (x1 == dd.size()-1) { //   If   we   reached   the   end   of   the   String   
			m_arr.push_back(tem);
		}
	}   
    
	// Now lets cycle through the String Array and create each directory in turn   
	for (std::vector<std::string>::const_iterator itor = m_arr.begin() + 1; itor != m_arr.end(); ++ itor) {
		tt = CreateDirectory((*itor).c_str(), NULL);
    
		// If the Directory exists it will return a false
		if (tt) {
			// If we were successful we set the attributes to normal
			SetFileAttributes((*itor).c_str(), FILE_ATTRIBUTE_NORMAL);
		}
	}
	// Now lets see if the directory was successfully created
	fFile = FindFirstFile(dd.c_str(), &fileinfo);
    
	m_arr.clear();
	if (fileinfo.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {   
		// Directory Exists close file and return
		FindClose(fFile);
		return TRUE;
	} else {   
		// For Some reason the Function Failed Return FALSE
		FindClose(fFile);
		return FALSE;
	}
}