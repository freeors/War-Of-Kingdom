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
BOOL extra_kingdom_ins_disk(char* kingdom_src, char* kingdom_ins, char* kingdon_ins_android);
BOOL generate_kingdom_star(const std::string& kingdom_res, const std::string& kingdom_star, const std::string& kingdom_star_patch);
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

BOOL check_wok_root_folder(const std::string& folder)
{
	std::stringstream strstr;
	
	// <wok>\data\_main.cfg
	strstr << folder << "\\data\\_main.cfg";
	return is_file(strstr.str().c_str());
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
			symbols["result"] = fok? "Success": "Fail";
			strcpy(text, utf8_2_ansi(vgettext2("Extract release package from \"$src\" to \"C:\\kingdom-ins\", $result!", symbols).c_str())); 
			posix_print_mb(text);
		}
		break;

	case IDM_NEW_CAMPAIGN:
		if (campaign_new()) {
			// title_select(da_sync);
		}
		break;

	case IDM_STAR_RESOURCE:
		strcpy(text, utf8_2_ansi(_("Do you want to generate star resource package to \"C:\\kingdom-star\"?"))); 
		strstr.str("");
		strstr << utf8_2_ansi(_("Confirm generate"));
		retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			fok = generate_kingdom_star(gdmgr._menu_text, "c:\\kingdom-star", "c:\\wesnoth\\kingdom-star-patch");
			symbols["src1"] = gdmgr._menu_text;
			symbols["src2"] = "c:\\wesnoth\\kingdom-star";
			symbols["result"] = fok? "Success": "Fail";
			strcpy(text, utf8_2_ansi(vgettext2("Generate star resource package from \"$src1\" and \"$src2\" to \"C:\\kingdom-star\", $result!", symbols).c_str())); 
			posix_print_mb(text);
		}
		break;

	case IDM_STAR_PATCH:
		strcpy(text, utf8_2_ansi(_("Do you want to extract star different files to \"C:\\wesnoth\\kingdom-star-patch\"?"))); 
		strstr.str("");
		strstr << utf8_2_ansi(_("Confirm generate"));
		retval = MessageBox(hdlgP, text, strstr.str().c_str(), MB_YESNO | MB_DEFBUTTON2);
		if (retval == IDYES) {
			fok = extract_kingdom_star_patch(gdmgr._menu_text, "c:\\wesnoth\\kingdom-star-patch");
			symbols["src"] = gdmgr._menu_text;
			symbols["dst"] = "c:\\wesnoth\\kingdom-star-patch";
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
				wgen_enter_ui();
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

	case IDM_DELETE_ITEM0:
		// 册除指定的文件/目录
		strstr.str("");
		str = gdmgr._menu_text;
		strstr << str.substr(0, str.rfind("\\xwml"));
		strstr << "\\data\\campaigns\\" << offextname(basename(gdmgr._menu_text));
		strcpy(text, strstr.str().c_str());

		symbols["file"] = gdmgr._menu_text;
		symbols["directory"] = text;
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
			if (!delfile1(text)) {
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
			if (game_config::path.find("kingdom-star") == std::string::npos) {
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
				wgen_enter_ui();
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
				if (strstr(gdmgr._menu_text, "\\campaigns\\") && 
					!strstr(gdmgr._menu_text, "duel") &&
					!strstr(gdmgr._menu_text, "legend_of_bei_liu") &&
					!strstr(gdmgr._menu_text, "tutorial")) {
					if (gdmgr._da != da_campaign) {
						title_select(da_campaign);
					} else {
						campaign_enter_ui();
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

BOOL copy_root_files(const char* src, const char* dst)
{
	char				szCurrDir[_MAX_PATH], text1[_MAX_PATH], text2[_MAX_PATH];
	HANDLE				hFind;
	WIN32_FIND_DATA		finddata;
	BOOL				fok, fret = TRUE;
	
	if (!src || !src[0] || !dst || !dst[0]) {
		return FALSE;
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
	return fret;
}

typedef struct {
	std::string src_campaign_dir;
	std::string ins_campaign_dir;
} walk_campaign_param_t;

static BOOL cb_walk_campaign(char* name, uint32_t flags, uint64_t len, int64_t lastWriteTime, uint32_t* ctx)
{
	char text1[_MAX_PATH], text2[_MAX_PATH];
	BOOL fok;
	walk_campaign_param_t* wcp = (walk_campaign_param_t*)ctx;

	if (flags & FILE_ATTRIBUTE_DIRECTORY ) {
		sprintf(text2, "%s\\%s", wcp->ins_campaign_dir.c_str(), name);
		MakeDirectory(std::string(text2)); // !!不要在<data>/campaigns/{campaign}下建images目录
		sprintf(text1, "%s\\%s\\images", wcp->src_campaign_dir.c_str(), name);
		sprintf(text2, "%s\\%s\\images", wcp->ins_campaign_dir.c_str(), name);
		if (is_directory(text1)) {
			posix_print("<data>, copy %s to %s ......\n", text1, text2);
			fok = copyfile(text1, text2);
			if (!fok) {
				posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
				return FALSE;
			}
		}
	}
	return TRUE;
}

BOOL extra_kingdom_ins_disk(char* kingdom_src, char* kingdom_ins, char* kingdom_ins_android)
{
	char text1[_MAX_PATH], text2[_MAX_PATH];
	BOOL fok;
	walk_campaign_param_t wcp;

	MakeDirectory(std::string(kingdom_ins));

	// 清空目录
	fok = delfile1(kingdom_ins);
	if (!fok) {
		posix_print_mb("删除目录: %s，失败", kingdom_ins);
		return fok;
	}
	fok = delfile1(kingdom_ins_android);
	if (!fok) {
		// posix_print_mb("删除目录: %s，失败", kingdom_ins_android);
		// return fok;
	}

	//
	// <kingdom-src>\data
	//

	// 1. images in all campaigns
	wcp.src_campaign_dir = std::string(kingdom_src) + "\\data\\campaigns";
	wcp.ins_campaign_dir = std::string(kingdom_ins) + "\\data\\campaigns";
	fok = walk_dir_win32_deepen(wcp.src_campaign_dir.c_str(), 0, cb_walk_campaign, (uint32_t *)&wcp);
	if (!fok) {
		return fok;
	}
	wcp.ins_campaign_dir = std::string(kingdom_ins_android) + "\\data\\campaigns";
	fok = walk_dir_win32_deepen(wcp.src_campaign_dir.c_str(), 0, cb_walk_campaign, (uint32_t *)&wcp);
	if (!fok) {
		return fok;
	}

	// 2. images in <data>\core root
	MakeDirectory(std::string("") + kingdom_ins + "\\data\\core");
	sprintf(text1, "%s\\data\\core\\images", kingdom_src);
	sprintf(text2, "%s\\data\\core\\images", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	MakeDirectory(std::string("") + kingdom_ins_android + "\\data\\core");
	sprintf(text2, "%s\\data\\core\\images", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// 3. music in <data>\core root
	sprintf(text1, "%s\\data\\core\\music", kingdom_src);
	sprintf(text2, "%s\\data\\core\\music", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\core\\music", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// 4. sounds in <data>\core root
	sprintf(text1, "%s\\data\\core\\sounds", kingdom_src);
	sprintf(text2, "%s\\data\\core\\sounds", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\core\\sounds", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	sprintf(text1, "%s\\data\\hardwired", kingdom_src);
	sprintf(text2, "%s\\data\\hardwired", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\hardwired", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	sprintf(text1, "%s\\data\\lua", kingdom_src);
	sprintf(text2, "%s\\data\\lua", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\lua", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	sprintf(text1, "%s\\data\\tools", kingdom_src);
	sprintf(text2, "%s\\data\\tools", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\tools", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// <kingdom-src>\data\_main_cfg
	sprintf(text1, "%s\\data\\_main.cfg", kingdom_src);
	sprintf(text2, "%s\\data\\_main.cfg", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制文件，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\data\\_main.cfg", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制文件，从%s到%s，失败", text1, text2);
		goto exit;
	}
	//
	// <kingdom-src>\fonts
	//
	sprintf(text1, "%s\\fonts", kingdom_src);
	sprintf(text2, "%s\\fonts", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\fonts", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	//
	// <kingdom-src>\images
	//
	sprintf(text1, "%s\\images", kingdom_src);
	sprintf(text2, "%s\\images", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\images", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	//
	// <kingdom-src>\manual
	//
	sprintf(text1, "%s\\manual", kingdom_src);
	sprintf(text2, "%s\\manual", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// andorid don't copy manual

	//
	// <kingdom-src>\sounds
	//
	sprintf(text1, "%s\\sounds", kingdom_src);
	sprintf(text2, "%s\\sounds", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\sounds", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	//
	// <kingdom-src>\translations
	//
	sprintf(text1, "%s\\translations", kingdom_src);
	sprintf(text2, "%s\\translations", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\translations", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	//
	// <kingdom-src>\xwml
	//
	sprintf(text1, "%s\\xwml", kingdom_src);
	sprintf(text2, "%s\\xwml", kingdom_ins);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// android
	sprintf(text2, "%s\\xwml", kingdom_ins_android);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	fok = copy_root_files(kingdom_src, kingdom_ins);
exit:
	if (!fok) {
		delfile1(kingdom_ins);
	}

	return fok;
}

BOOL generate_kingdom_star(const std::string& kingdom_res, const std::string& kingdom_star, const std::string& kingdom_star_patch)
{
	char text1[_MAX_PATH], text2[_MAX_PATH];
	BOOL fok;
	std::stringstream err;
	utils::string_map symbols;
	walk_campaign_param_t wcp;

	MakeDirectory(std::string(kingdom_star));

	// 清空目录
	fok = delfile1(kingdom_star.c_str());
	if (!fok) {
		posix_print_mb("删除目录: %s，失败", kingdom_star.c_str());
		return fok;
	}
	fok = is_directory(kingdom_star_patch.c_str());
	if (!fok) {
		symbols["kingdom_star_patch"] = kingdom_star_patch;
		err << utf8_2_ansi(vgettext2("Cannot find <kingdom-star-patch> directory on $kingdom_star_patch!", symbols).c_str()); 
		posix_print_mb(err.str().c_str());
		return fok;
	}

	//
	// <kingdom-src>\data
	//

	// all files from kingdom-res to kingdom-star
	MakeDirectory(kingdom_star);
	// 3. data in <data>
	sprintf(text1, "%s\\data", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\data", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		posix_print("<data>, copy %s to %s ......\n", text1, text2);
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\fonts
	//
	sprintf(text1, "%s\\fonts", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\fonts", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		posix_print("<data>, copy %s to %s ......\n", text1, text2);
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\images
	//
	sprintf(text1, "%s\\images", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\images", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\manual
	//
	sprintf(text1, "%s\\manual", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\manual", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\po
	//
	sprintf(text1, "%s\\po", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\po", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\sounds
	//
	sprintf(text1, "%s\\sounds", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\sounds", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\translations
	//
	sprintf(text1, "%s\\translations", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\translations", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	//
	// <kingdom-src>\xwml
	//
	sprintf(text1, "%s\\xwml", kingdom_res.c_str());
	sprintf(text2, "%s", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// kingdom-star-patch
	sprintf(text1, "%s\\xwml", kingdom_star_patch.c_str());
	if (is_directory(text1)) {
		fok = copyfile(text1, text2);
		if (!fok) {
			posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
			goto exit;
		}
	}

	fok = copy_root_files(kingdom_res.c_str(), kingdom_star.c_str());
	if (!fok) {
		posix_print_mb("复制根目录下文件，从%s到%s，失败", text1, text2);
		goto exit;
	}

exit:
	if (!fok) {
		delfile1(kingdom_star.c_str());
	}

	return fok;
}

BOOL extract_kingdom_star_patch(const std::string& kingdom_star, const std::string& kingdom_star_patch)
{
	char text1[_MAX_PATH], text2[_MAX_PATH];
	BOOL fok;
	std::stringstream err;
	utils::string_map symbols;
	walk_campaign_param_t wcp;

	fok = file_exists(kingdom_star + "/data/core/_main.cfg")? TRUE: FALSE;
	if (!fok) {
		symbols["kingdom_star"] = kingdom_star;
		err << utf8_2_ansi(vgettext2("Cannot find <kingdom-star> directory on $kingdom_star!", symbols).c_str()); 
		posix_print_mb(err.str().c_str());
		return fok;
	}
	MakeDirectory(kingdom_star_patch);

	// 清空目录
	fok = delfile1(kingdom_star_patch.c_str());
	if (!fok) {
		posix_print_mb("删除目录: %s，失败", kingdom_star_patch.c_str());
		return fok;
	}
	
	//
	// <kingdom-src>\data
	//
	// hero-64
	sprintf(text1, "%s\\data\\core\\images\\hero-64", kingdom_star.c_str());
	sprintf(text2, "%s\\data\\core\\images", kingdom_star_patch.c_str());
	MakeDirectory(text2);
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	// hero-256
	sprintf(text1, "%s\\data\\core\\images\\hero-256", kingdom_star.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	//
	// <kingdom-src>\fonts
	//
	sprintf(text1, "%s\\fonts\\wqy-zenhei.ttc", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\fonts");
	sprintf(text2, "%s\\fonts\\wqy-zenhei.ttc", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制文件，从%s到%s，失败", text1, text2);
		goto exit;
	}

	//
	// <kingdom-src>\po
	//
	sprintf(text1, "%s\\po\\wesnoth-card\\zh_CN.po", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\po\\wesnoth-card");
	sprintf(text2, "%s\\po\\wesnoth-card\\zh_CN.po", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	sprintf(text1, "%s\\po\\wesnoth-hero\\zh_CN.po", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\po\\wesnoth-hero");
	sprintf(text2, "%s\\po\\wesnoth-hero\\zh_CN.po", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制文件，从%s到%s，失败", text1, text2);
		goto exit;
	}
	
	//
	// <kingdom-src>\translations
	//
	sprintf(text1, "%s\\translations\\zh_CN\\LC_MESSAGES\\wesnoth-card.mo", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\translations\\zh_CN\\LC_MESSAGES");
	sprintf(text2, "%s\\translations\\zh_CN\\LC_MESSAGES\\wesnoth-card.mo", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}
	sprintf(text1, "%s\\translations\\zh_CN\\LC_MESSAGES\\wesnoth-hero.mo", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\translations\\zh_CN\\LC_MESSAGES");
	sprintf(text2, "%s\\translations\\zh_CN\\LC_MESSAGES\\wesnoth-hero.mo", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制目录，从%s到%s，失败", text1, text2);
		goto exit;
	}

	//
	// <kingdom-src>\xwml
	//
	sprintf(text1, "%s\\xwml\\hero.dat", kingdom_star.c_str());
	MakeDirectory(kingdom_star_patch + "\\xwml");
	sprintf(text2, "%s\\xwml\\hero.dat", kingdom_star_patch.c_str());
	posix_print("<data>, copy %s to %s ......\n", text1, text2);
	fok = copyfile(text1, text2);
	if (!fok) {
		posix_print_mb("复制文件，从%s到%s，失败", text1, text2);
		goto exit;
	}

exit:
	if (!fok) {
		delfile1(kingdom_star_patch.c_str());
	}

	return fok;
}