#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "editor.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include "resource.h"
#include "struct.h"

#include "xfunc.h"
#include "win32x.h"

#include <string>
#include <vector>

extern editor editor_;

static void OnDeleteBt(HWND hdlgP, char *fname);
bool extra_kingdom_ins_disk(char* kingdom_src, char* kingdom_ins, char* kingdon_ins_android);
bool extract_rose(tcopier& copier);
BOOL generate_kingdom_mod_res(const std::string& kingdom_res, const std::string& kingdom_star_patch, const std::string& kingdom_star);
BOOL extract_kingdom_star_patch(const std::string& kingdom_star, const std::string& kingdom_star_patch);

void create_ddesc_toolinfo(HWND hwndP)
{
	TOOLINFO	ti;
    RECT		rect;

	// CREATE A TOOLTIP WINDOW for OK
	if (gdmgr._tt_refresh) {
		DestroyWindow(gdmgr._tt_refresh);
	}
	gdmgr._tt_refresh = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,		
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GetDlgItem(hwndP, IDC_ST_MEDIA),
        NULL,
        gdmgr._hinst,
        NULL
        );
	// SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // GET COORDINATES OF THE MAIN CLIENT AREA
    GetClientRect(GetDlgItem(hwndP, IDC_ST_MEDIA), &rect);
    // INITIALIZE MEMBERS OF THE TOOLINFO STRUCTURE
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = GetDlgItem(hwndP, IDC_ST_MEDIA);
    ti.hinst = gdmgr._hinst;
    ti.uId = 0;
    ti.lpszText = "Refresh";
    // ToolTip control will cover the whole window
    ti.rect.left = rect.left;    
    ti.rect.top = rect.top;
    ti.rect.right = rect.right;
    ti.rect.bottom = rect.bottom;

    // SEND AN ADDTOOL MESSAGE TO THE TOOLTIP CONTROL WINDOW 
    SendMessage(gdmgr._tt_refresh, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

	return;
}

typedef struct {
	HWND		hctl;
	HTREEITEM	htvi;
	char		curdir[_MAX_PATH];
	uint32_t	maxdeep;	// 最大扩展maxdeep级, 0: 只扩展根目录, 1: 扩展到根目录下的目录为止, -1: 因为是无符号数,意味着全扩展	
	uint32_t	deep;		// 0, 1, 2, ... 
} tv_walk_dir_param_t;

BOOL cb_walk_dir_explorer(char *name, uint32_t flags, uint64_t len, int64_t lastWriteTime, uint32_t* ctx)
{
	int			iimg = select_iimage_according_fname(name, flags);
	HTREEITEM	htvi;
	TVSORTCB	tvscb;
	tv_walk_dir_param_t* wdp = (tv_walk_dir_param_t*)ctx;
	
	htvi = TreeView_AddLeaf(wdp->hctl, wdp->htvi);
	TreeView_SetItem1(wdp->hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, (LPARAM)htvi, iimg, iimg, 0, name);
	
	tvscb.hParent = wdp->htvi;
	tvscb.lpfnCompare = fn_tvi_compare_sort;
	tvscb.lParam = (LPARAM)wdp->hctl;

	TreeView_SortChildrenCB(wdp->hctl, &tvscb, 0);

	if (flags & FILE_ATTRIBUTE_DIRECTORY ) {
		tv_walk_dir_param_t	wdp2;

		sprintf(wdp2.curdir, "%s\\%s", wdp->curdir, name);

		if (wdp->maxdeep > wdp->deep) {
			tv_walk_dir_param_t	wdp2;

			wdp2.hctl = wdp->hctl;
			wdp2.htvi = htvi;
			wdp2.maxdeep = wdp->maxdeep;
			wdp2.deep = wdp->deep + 1;
			walk_dir_win32_deepen(wdp2.curdir, 0, cb_walk_dir_explorer, (uint32_t *)&wdp2);
		} else if (!is_empty_dir(wdp2.curdir)) {
			// 枚举到此为止,但因为是目录,强制让出来前面+符号
			TreeView_SetItem1(wdp->hctl, htvi, TVIF_CHILDREN, 0, 0, 0, 1, NULL);
		}
	}
	return TRUE;
}

void dir_2_tv(HWND hctl, HTREEITEM htvi, const char* rootdir, uint32_t maxdeep)
{
	tv_walk_dir_param_t		wdp;

	wdp.hctl = hctl;
	wdp.maxdeep = maxdeep;
	wdp.deep = 0;
	wdp.htvi = htvi;
	strcpy(wdp.curdir, rootdir);
	// 此处的subfolders不能设为1. 旦设为1,所有枚举出的子目录/文件都会被添加到IVI_ROOT,而在回调函数中有排序,
	// 排序这操作可能会把系统累死
	walk_dir_win32_deepen(rootdir, 0, cb_walk_dir_explorer, (uint32_t *)&wdp);
	
	return;
}

BOOL On_DlgDDescInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_DDESC_EXPLORER);
			
	gdmgr._hdlg_ddesc = hdlgP;

	// 显示welcome.bmp
	SendMessage(GetDlgItem(hdlgP, IDC_ST_WELCOME), STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)gdmgr._welcomebmp);
	
	// ListView_SetImageList(hwndCtl, gdmgr._himl, LVSIL_SMALL);
	TreeView_SetImageList(hctl, gdmgr._himl, TVSIL_NORMAL);

	OnLSBt(TRUE);

	return TRUE;
}

bool is_app_res(const std::string& folder)
{
	const std::string file = folder + "/data/campaigns.cfg";
	return file_exists(file);
}

bool check_wok_root_folder(const std::string& folder)
{
	std::stringstream strstr;
	
	// <wok>\data\_main.cfg
	strstr << folder << "\\data\\_main.cfg";
	if (!is_file(strstr.str().c_str())) {
		return false;
	}

	bool app_res = is_app_res(folder);
#ifndef _ROSE_EDITOR
	return app_res; 
#else
	return !app_res;
#endif
}

void On_DlgDDescCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char *ptr = NULL;
	int retval;
	char text[_MAX_PATH];
	BOOL fok;
	std::string str;
	std::stringstream strstr;
	utils::string_map symbols;
	
	switch (id) {
	case IDC_BT_DDESC_BROWSE:
		ptr = GetBrowseFilePath(hdlgP);
		if (!ptr) {
			break;
		}
		strcpy(text, appendbackslash(ptr));
		if (text[strlen(text) - 1] == '\\') {
			text[strlen(text) - 1] = 0;
		}
		if (!check_wok_root_folder(text)) {
			symbols["directory"] = appendbackslash(text);
			strstr.str("");
			strstr << utf8_2_ansi(vgettext2("\"$directory\" isn't valid resource directory, please select again", symbols).c_str());
			posix_print_mb(strstr.str().c_str());
			break;
		}
		if (strcasecmp(ptr, game_config::path.c_str())) {
			game_config::path = ptr;
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_DDESC_WWWROOT), appendbackslash(ptr));
			OnLSBt(TRUE);

			update_locale_dir();

			if (gdmgr._da != da_sync) {
				title_select(da_sync);
			} else {
				sync_enter_ui();
			}
		}
		break;

	case IDM_NEW_EXTRAINSDIST:
		strcpy(text, utf8_2_ansi(_("Do you want to extract release package to \"C:\\kingdom-ins\"?"))); 
		strstr.str("");
		strstr << utf8_2_ansi(_("Confirm generate"));
		retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			fok = extra_kingdom_ins_disk(gdmgr._menu_text, "c:\\kingdom-ins", "c:\\kingdom-ins-android\\com.freeors.kingdom");
			symbols["src"] = gdmgr._menu_text;
			symbols["result"] = fok? _("Success"): _("Fail");
			strcpy(text, utf8_2_ansi(vgettext2("Extract release package from \"$src\" to \"C:\\kingdom-ins\", $result!", symbols).c_str())); 
			posix_print_mb(text);
		}
		break;

	case IDM_NEW_EXTRAROSE:
		{
			tcopier copier(generate_cfg(editor_config::data_cfg, "rose"));

			symbols["dst"] = copier.get_path("rose_src");
			strcpy(text, utf8_2_ansi(vgettext2("Do you want to extract Rose package to $dst?", symbols).c_str())); 
			strstr.str("");
			strstr << utf8_2_ansi(_("Confirm generate"));
			retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
			if (retval == IDYES) {
				fok = extract_rose(copier);
				symbols["src"] = copier.get_path("src");
				symbols["result"] = fok? "Success": "Fail";
				strcpy(text, utf8_2_ansi(vgettext2("Extract Rose package from \"$src\" to \"$dst\", $result!", symbols).c_str())); 
				posix_print_mb(text);
			}
		}
		break;

#ifndef _ROSE_EDITOR
	case IDM_NEW_CAMPAIGN:
		if (campaign_new()) {
			// title_select(da_sync);
		}
		break;
#endif

	case IDM_STAR_RESOURCE:
		symbols["mod_res_path"] = mod_config.get_path(tmod_config::res_tag);
		strcpy(text, utf8_2_ansi(vgettext2("Do you want to generate star resource package to $mod_res_path?", symbols).c_str()));
		strstr.str("");
		strstr << utf8_2_ansi(_("Confirm generate"));
		retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			fok = generate_kingdom_mod_res(gdmgr._menu_text, mod_config.get_path(tmod_config::patch_tag), mod_config.get_path(tmod_config::res_tag));
			symbols["src1"] = gdmgr._menu_text;
			symbols["src2"] = mod_config.get_path(tmod_config::patch_tag);
			symbols["dst"] = mod_config.get_path(tmod_config::res_tag);
			symbols["result"] = fok? "Success": "Fail";
			strcpy(text, utf8_2_ansi(vgettext2("Generate star resource package from \"$src1\" and \"$src2\" to \"$dst\", $result!", symbols).c_str())); 
			posix_print_mb(text);
		}
		break;

	case IDM_STAR_PATCH:
		symbols["mod_patch_path"] = mod_config.get_path(tmod_config::patch_tag);
		strcpy(text, utf8_2_ansi(vgettext2("Do you want to extract star different files to $mod_patch_path?", symbols).c_str()));
		strstr.str("");
		strstr << utf8_2_ansi(_("Confirm generate"));
		retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			fok = extract_kingdom_star_patch(gdmgr._menu_text, mod_config.get_path(tmod_config::patch_tag));
			symbols["src"] = gdmgr._menu_text;
			symbols["dst"] = mod_config.get_path(tmod_config::patch_tag);
			symbols["result"] = fok? "Success": "Fail";
			strcpy(text, utf8_2_ansi(vgettext2("Extract star different files from \"$src\" to \"$dst\", $result!", symbols).c_str())); 
			posix_print_mb(text);
		}
		break;

	case IDM_EXPLORER_WML:
		if (!_stricmp(gdmgr._menu_text, "hero.dat")) {
			if (gdmgr._da != da_wgen) {
				title_select(da_wgen);
			} else {
#ifndef _ROSE_EDITOR
				wgen_enter_ui();
#endif
			}
		} else if (!_stricmp(basename(gdmgr._menu_text), "tb.dat")) {
			editor_config::type = BIN_BUILDINGRULE;
			if (gdmgr._da != da_visual) {
				title_select(da_visual);
			} else {
				visual_enter_ui();
			}
		} else if (!_stricmp(extname(gdmgr._menu_text), "bin")) {
			if (wml_checksum_from_file(std::string(gdmgr._menu_text))) {
				editor_config::type = BIN_WML;
				if (gdmgr._da != da_visual) {
					title_select(da_visual);
				} else {
					visual_enter_ui();
				}
			}
		}
		break;

	case IDM_INTEGRATE:
		if (gdmgr._da != da_integrate) {
			title_select(da_integrate);
		} else {
			integrate_enter_ui();
		}
		break;

	case IDM_DELETE_ITEM0:
		// 册除指定的文件/目录
		strstr.str("");
		str = gdmgr._menu_text;
		strstr << str.substr(0, str.rfind("\\xwml"));
		strstr << "\\data\\campaigns\\" << offextname(basename(gdmgr._menu_text));
		str = strstr.str();

		symbols["file"] = gdmgr._menu_text;
		symbols["directory"] = str;
		strstr.str("");
		strstr << utf8_2_ansi(vgettext2("Do you want to delete file: \"$file\" and directory: \"$directory\"?", symbols).c_str());

		strcpy(text, utf8_2_ansi(_("Confirm delete")));
		// Confirm Multiple File Delete
		retval = MessageBox(hdlgP, strstr.str().c_str(), text, MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			if (delfile1(gdmgr._menu_text)) {
                TreeView_DeleteItem(GetDlgItem(hdlgP, IDC_TV_DDESC_EXPLORER), (HTREEITEM)(UINT_PTR)gdmgr._menu_lparam);
				TreeView_SetChilerenByPath(GetDlgItem(hdlgP, IDC_TV_DDESC_EXPLORER), (HTREEITEM)(UINT_PTR)gdmgr._menu_lparam, dirname(gdmgr._menu_text)); 
			} else {
				posix_print_mb("Failed delete %s !", gdmgr._menu_text); 
			}

			if (!delfile1(str.c_str())) {
                posix_print_mb("Failed delete %s !", text); 
			}
			sync_refresh_sync();
		}

		break;

	case IDM_DELETE_ITEM1:
		symbols["file"] = gdmgr._menu_text;
		strstr.str("");
		strstr << utf8_2_ansi(vgettext2("Will generate non-sync, do you want to only delete file: \"$file\"?", symbols).c_str());

		strcpy(text, utf8_2_ansi(_("Confirm delete")));
		// Confirm Multiple File Delete 
		retval = MessageBox(hdlgP, strstr.str().c_str(), text, MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			if (delfile1(gdmgr._menu_text)) {
                TreeView_DeleteItem(GetDlgItem(hdlgP, IDC_TV_DDESC_EXPLORER), (HTREEITEM)(UINT_PTR)gdmgr._menu_lparam);
				TreeView_SetChilerenByPath(GetDlgItem(hdlgP, IDC_TV_DDESC_EXPLORER), (HTREEITEM)(UINT_PTR)gdmgr._menu_lparam, dirname(gdmgr._menu_text)); 
			} else {
				posix_print_mb("Failed delete %s !", gdmgr._menu_text); 
			}
			// inform user, there is non sync.
			if (gdmgr._da != da_sync) {
				title_select(da_sync);
			} else {
				// forbid refresh
				sync_enter_ui();
			}
		}
		break;

	default:
		break;
	}
	return;
}

void menu_delete_refresh(HMENU hmenu, char *path, BOOL fDir) 
{
	uint32_t		idx;
	uint32_t		mf_dir = fDir? MF_ENABLED: MF_GRAYED;

	// delete this
	if (strcasecmp(path, game_config::path.c_str())) {
		EnableMenuItem(hmenu, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_ENABLED);
	} else {
		EnableMenuItem(hmenu, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
	}
	// directory
	for (idx = 1; idx < 10; idx ++) {
        EnableMenuItem(hmenu, IDM_DELETE_ITEM0 + idx, MF_BYCOMMAND | mf_dir);
	}

	return;
}

//
// On_DlgDDescNotify()
//
BOOL On_DlgDDescNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	char					text[_MAX_PATH];
	POINT					point;
	
	if (gdmgr._syncing || (lpNMHdr->idFrom != IDC_TV_DDESC_EXPLORER)) {
		return FALSE;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	// NM_RCLICK/NM_CLICK/NM_DBLCLK这些通知被发来后,其附代参数没法指定是哪个叶子句柄,
	// 需通过判断鼠标坐标来判断是哪个叶子被按下?
	// 1. GetCursorPos, 得到屏幕坐标系下的鼠标坐标
	// 2. TreeView_HitTest1(自写宏),由屏幕坐标系下的鼠标坐标返回指向的叶子句柄
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	
	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->code == NM_RCLICK) {
		//
		// 右键单击: 弹出菜单
		//
		// 准备两个菜单上下文变量
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_IMAGE | TVIF_PARAM | TVIF_TEXT, text);
		strcpy(gdmgr._menu_text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
		gdmgr._menu_lparam = (uint32_t)tvi.lParam;

		// new
		if (!can_execute_tack(TASK_NEW) || strcasecmp(gdmgr._menu_text, game_config::path.c_str())) {
			EnableMenuItem(gdmgr._hpopup_new, IDM_NEW_EXTRAINSDIST, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_new, IDM_NEW_EXTRAROSE, MF_BYCOMMAND | MF_GRAYED);
		}
		if (!can_execute_tack(TASK_NEW)) {
			EnableMenuItem(gdmgr._hpopup_new, IDM_NEW_CAMPAIGN, MF_BYCOMMAND | MF_GRAYED);
		}
		// star
		if (!can_execute_tack(TASK_NEW) || strcasecmp(gdmgr._menu_text, game_config::path.c_str())) {
			EnableMenuItem(gdmgr._hpopup_new, (UINT_PTR)(gdmgr._hpopup_star), MF_BYCOMMAND | MF_GRAYED);
		} else {
			EnableMenuItem(gdmgr._hpopup_new, (UINT_PTR)(gdmgr._hpopup_star), MF_BYCOMMAND | MF_ENABLED);
			if (game_config::path.find("kingdom-res") == std::string::npos) {
				EnableMenuItem(gdmgr._hpopup_star, IDM_STAR_RESOURCE, MF_BYCOMMAND | MF_GRAYED);
			} else {
				EnableMenuItem(gdmgr._hpopup_star, IDM_STAR_RESOURCE, MF_BYCOMMAND | MF_ENABLED);
			}
			if (game_config::path.find(mod_config.res_short_path) == std::string::npos) {
				EnableMenuItem(gdmgr._hpopup_star, IDM_STAR_PATCH, MF_BYCOMMAND | MF_GRAYED);
			} else {
				EnableMenuItem(gdmgr._hpopup_star, IDM_STAR_PATCH, MF_BYCOMMAND | MF_ENABLED);
			}
		}
			

		// explorer
		if (!can_execute_tack(TASK_EXPLORER) || (_stricmp(extname(gdmgr._menu_text), "bin") && _stricmp(extname(gdmgr._menu_text), "dat"))) {
			EnableMenuItem(gdmgr._hpopup_explorer, IDM_EXPLORER_WML, MF_BYCOMMAND | MF_GRAYED);
		}
		// delete
		if (!can_execute_tack(TASK_DELETE) || _stricmp(extname(gdmgr._menu_text), "bin") || !strstr(gdmgr._menu_text, "\\campaigns\\")) {
			EnableMenuItem(gdmgr._hpopup_delete, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_delete, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(gdmgr._hpopup_ddesc, 0,
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		EnableMenuItem(gdmgr._hpopup_new, IDM_NEW_EXTRAINSDIST, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_new, IDM_NEW_EXTRAROSE, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_explorer, IDM_EXPLORER_WML, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_delete, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_delete, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_ENABLED);
	
	} else if (lpNMHdr->code == NM_CLICK) {
		//
		// 左键单击: 如果底下被击的是目录,且没有生成过叶子,成生叶子
		//
		strcpy(text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_CHILDREN, NULL);
		if (tvi.cChildren && !TreeView_GetChild(lpNMHdr->hwndFrom, htvi)) {
			dir_2_tv(lpNMHdr->hwndFrom, htvi, text, 0);
		}
		// 址址栏中显示选中目录
		strcpy(text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
		if (!is_directory(text)) {
			strcpy(text, dirname(text));
		}
		// Edit_SetText(GetDlgItem(hdlgP, IDC_ET_DDESC_WWWROOT), text + strlen(dirname(game_config::path.c_str())) + 1);

	} else if (lpNMHdr->code == NM_DBLCLK) {
		//
		// 目录: 展开/折叠叶子(系统自动)
		// 文件: 用特定应程序打开
		//
		// 切换到编辑窗口
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_IMAGE | TVIF_PARAM | TVIF_TEXT, text);

		if (!_stricmp(text, "hero.dat")) {
			strcpy(gdmgr._menu_text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
			gdmgr._menu_lparam = (uint32_t)tvi.lParam;
			if (gdmgr._da != da_wgen) {
				title_select(da_wgen);
			} else {
#ifndef _ROSE_EDITOR
				wgen_enter_ui();
#endif
			}
		} else if (!_stricmp(text, "tb.dat")) {
			strcpy(gdmgr._menu_text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
			gdmgr._menu_lparam = (uint32_t)tvi.lParam;
			editor_config::type = BIN_BUILDINGRULE;
			if (gdmgr._da != da_visual) {
				title_select(da_visual);
			} else {
				visual_enter_ui();
			}
		} else if (!_stricmp(extname(text), "bin")) {
			strcpy(gdmgr._menu_text, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
			if (wml_checksum_from_file(std::string(gdmgr._menu_text))) {
				gdmgr._menu_lparam = (uint32_t)tvi.lParam;
				if (strstr(gdmgr._menu_text, "\\campaigns\\")) {
					if (gdmgr._da != da_campaign) {
						title_select(da_campaign);
					} else {
#ifndef _ROSE_EDITOR
						campaign_enter_ui();
#endif
					}
				} else if (strstr(gdmgr._menu_text, "data.bin")) {
					if (gdmgr._da != da_core) {
						title_select(da_core);
					} else {
						// core_enter_ui();
					}
				} else {
					editor_config::type = BIN_WML;
					if (gdmgr._da != da_visual) {
						title_select(da_visual);
					} else {
						visual_enter_ui();
					}
				}
			}
		}
		
	} else if (lpNMHdr->code == TVN_ITEMEXPANDED) {
		//
		// 子叶子已被展开或折叠, 是折叠时删除所子有叶子, 以便下次展开时通反映最近文件系统
		//

		if (lpnmitem->action & TVE_COLLAPSE) {
			TreeView_Walk(lpNMHdr->hwndFrom, lpnmitem->itemNew.hItem, FALSE, NULL, NULL, TRUE);
			TreeView_SetChilerenByPath(lpNMHdr->hwndFrom, htvi, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
		}
	}

    return FALSE;
}

void On_DlgDDescDrawItem(HWND hdlgP, const DRAWITEMSTRUCT *lpdis)
{
	HDC					hdcMem; 
    hdcMem = CreateCompatibleDC(lpdis->hDC); 

	if (lpdis->itemState & ODS_SELECTED) { // if selected 
		SelectObject(hdcMem, gdmgr._hbm_refresh_select); 
	} else {
		SelectObject(hdcMem, gdmgr._hbm_refresh_unselect); 
	}

    // Destination 
    StretchBlt( 
                lpdis->hDC,         // destination DC 
                lpdis->rcItem.left, // x upper left 
                lpdis->rcItem.top,  // y upper left 
 
                // The next two lines specify the width and 
                // height. 
                lpdis->rcItem.right - lpdis->rcItem.left, 
                lpdis->rcItem.bottom - lpdis->rcItem.top, 
                hdcMem,    // source device context 
                0, 0,      // x and y upper left 
                24,        // source bitmap width 
                24,        // source bitmap height 
                SRCCOPY);  // raster operation 

	DeleteDC(hdcMem); 

	return;
}

void On_DlgDDescDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgDDescProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgDDescInitDialog(hdlgP, (HWND)wParam, lParam);		
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgDDescCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgDDescNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgDDescDestroy);
	// HANDLE_MSG(hdlgP, WM_DRAWITEM, On_DlgDDescDrawItem);
	}
	return FALSE;
}

// 
// 功能函数
//

// 为了比较上方便,此函数要求iSubItem=0处必须填iImage,且目录必须是0,文件的话则都是比0大的值
int CALLBACK fn_dvr_compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	HWND				hctl = (HWND)lParamSort;
	LVITEM				lvi1, lvi2;
	int					lvi1_type, lvi2_type;
	char				name1[_MAX_PATH], name2[_MAX_PATH];

	lvi1.iItem = (int)lParam1;
	lvi1.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi1.iSubItem = 0;
	lvi1.pszText = name1;
    lvi1.cchTextMax = _MAX_PATH;
    ListView_GetItem(hctl, &lvi1);

	lvi2.iItem = (int)lParam2;
	lvi2.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi2.iSubItem = 0;
	lvi2.pszText = name2;
    lvi2.cchTextMax = _MAX_PATH;
    ListView_GetItem(hctl, &lvi2);

	lvi1_type = lvi1.iImage? 1: 0;
	lvi2_type = lvi2.iImage? 1: 0;
	if (lvi1_type < lvi2_type) {
		// lvi1是目录, lvi2是文件
		return -1;
	} else if (lvi1_type > lvi2_type) {
		// lvi1是文件, lvi2是目录
		return 1;
	} else {
		// lvi1和lvi2都是目录或文件,只好用字符串比较
		return strcasecmp(name1, name2);
	}
}

int select_iimage_according_fname(char *name, uint32_t flags)
{
	int		iImage;
	char	*ext;

	if (flags & FILE_ATTRIBUTE_DIRECTORY) {
		iImage = gdmgr._iico_dir;
	} else {
		ext = extname(name);
		if (!strcasecmp(ext, "csf")) {
			iImage = gdmgr._iico_csf;
		} else if (!strcasecmp(ext, "txt")) {
			iImage = gdmgr._iico_txt;
		} else if (!strcasecmp(ext, "nma")) {
			iImage = gdmgr._iico_nma;
		} else if (!strcasecmp(ext, "ini")) {
			iImage = gdmgr._iico_ini;
		} else {
			iImage = gdmgr._iico_unknown;
		}
	}
	return iImage;
}

void OnLSBt(BOOL checkrunning)
{
	ULARGE_INTEGER ullFreeBytesAvailable, ullTotalNumberOfBytes;
	char text[_MAX_PATH];
	HRESULT hr = S_OK;
	HWND hctl = GetDlgItem(gdmgr._hdlg_ddesc, IDC_TV_DDESC_EXPLORER);

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);

	// 2. 地址栏编辑框
	Edit_SetText(GetDlgItem(gdmgr._hdlg_ddesc, IDC_ET_DDESC_WWWROOT), appendbackslash(game_config::path.c_str()));

	// 3. 向TreeView添加一级内容
	gdmgr._htvroot = TreeView_AddLeaf(hctl, TVI_ROOT);
	strcpy(text, basename(game_config::path.c_str()));
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, gdmgr._htvroot, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, (LPARAM)gdmgr._htvroot, 0, 0, 
		is_empty_dir(game_config::path.c_str())? 0: 1, text);
	dir_2_tv(hctl, gdmgr._htvroot, game_config::path.c_str(), 0);

	// 4. 底下状态
	strncpy(text, game_config::path.c_str(), 2);
	text[2] = '\\';
	text[3] = 0;
	GetDiskFreeSpaceEx(text, &ullFreeBytesAvailable, &ullTotalNumberOfBytes, NULL);
	strcpy(text, format_space_u64n(ullTotalNumberOfBytes.QuadPart));
	Edit_SetText(GetDlgItem(gdmgr._hdlg_ddesc, IDC_ET_DDESC_SUBAREA), formatstr("%s (Avail Space %s)", text, format_space_u64n(ullFreeBytesAvailable.QuadPart)));

	return;
}

void OnDeleteBt(HWND hdlgP, char *fname)
{
	HWND			hwndCtl = GetDlgItem(hdlgP, IDC_LV_DDESC_EXPLORER);

	// 自动刷新
	OnLSBt(TRUE);
	return;
}

bool extra_kingdom_ins_disk(char* kingdom_src, char* kingdom_ins, char* kingdom_ins_android)
{
	tcopier copier(generate_cfg(editor_config::data_cfg, "release"));
	tcallback_lock lock(false, boost::bind(&tcopier::do_delete_path, &copier, _1));

	if (!copier.make_path("ins") || !copier.make_path("ins_android")) {
		return false;
	}
	if (!copier.do_copy("res", "ins") || !copier.do_copy("res", "ins_android")) {
		return false;
	}

	lock.set_result(true);
	return true;
}

bool extract_rose(tcopier& copier)
{
	tcallback_lock lock(false, boost::bind(&tcopier::do_delete_path, &copier, _1));
	// if (!copier.make_path("rose_res")) {
	if (!copier.make_path("rose_src") || !copier.make_path("rose_res")) {
		return false;
	}
	// if (!copier.do_copy("res", "rose_res")) {
	if (!copier.do_copy("src", "rose_src") || !copier.do_copy("res", "rose_res") || !copier.do_copy("rose_res_patch", "rose_res")) {
		return false;
	}
	if (!copier.do_remove("rose_res")) {
		return false;
	}

	lock.set_result(true);
	return true;
}

BOOL generate_kingdom_mod_res(const std::string& kingdom_res, const std::string& kingdom_star_patch, const std::string& kingdom_star)
{
	tcopier copier(generate_cfg(editor_config::data_cfg, "copy"));
	tcallback_lock lock(false, boost::bind(&tcopier::do_delete_path, &copier, _1));

	if (!copier.make_path("res_mod")) {
		return false;
	}
	if (!copier.do_copy("res", "res_mod")) {
		return false;
	}
	
	if (!mod_config.opeate_file(true)) {
		return false;
	}

	lock.set_result(true);
	return true;
}

BOOL extract_kingdom_star_patch(const std::string& kingdom_star, const std::string& kingdom_star_patch)
{
	return mod_config.opeate_file(false);
}