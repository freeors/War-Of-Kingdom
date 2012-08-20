/*
	config架构视形视图中每个结点的lParam值有特珠意义
	1)结点是子config: 该子config在config中索引值。要由它个值在game_config_中定位中该config。
	2)结点是attribute：该attribute在attribute位置值。（当前没有使用，只有是样填了）
*/

#include "global.hpp"
#include "game_config.hpp"
#include "config.hpp"
#include "foreach.hpp"
#include "loadscreen.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include <shlobj.h> // SHBrowseForFolder

#include "resource.h"

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

void cfg_refresh(HWND hctl);
void cfg_2_tv(HWND hctl, HTREEITEM htvroot, const config& cfg, uint32_t maxdeep);

class visual_cfg
{
public:
	visual_cfg();

// attribute member
	HTREEITEM htvroot_;
	config game_config_;
};

visual_cfg::visual_cfg()
{}

visual_cfg vcfg;

void cfg_enter_ui(void)
{
	HWND hctl = GetDlgItem(gdmgr._hdlg_cfg, IDC_TV_CFG_EXPLORER);

	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	cfg_refresh(hctl);
	return;
}

BOOL cfg_hide_ui(void)
{
	return TRUE;
}

// 对话框消息处理函数
BOOL On_DlgCfgInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_CFG_EXPLORER);

	gdmgr._hdlg_cfg = hdlgP;

	TreeView_SetImageList(hctl, gdmgr._himl, TVSIL_NORMAL);

	return FALSE;
}

void On_DlgCfgCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
/*	switch(id) {
	case IDC_BT_ABOUT_HELP:
		break;
	default:
		break;
	}
*/
	return;
}

BOOL On_DlgCfgNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;
	
	if (lpNMHdr->idFrom != IDC_TV_CFG_EXPLORER) {
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
	
	} else if (lpNMHdr->code == NM_CLICK) {
		//
		// 左键单击: 如果底下被击的是目录,且没有生成过叶子,成生叶子
		//
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_CHILDREN, NULL);
		if (tvi.cChildren && !TreeView_GetChild(lpNMHdr->hwndFrom, htvi)) {
			std::vector<std::pair<LPARAM, std::string> > vec;
			LPARAM cfg_index;
			const config* cfg = &vcfg.game_config_;

			TreeView_FormVector(lpNMHdr->hwndFrom, htvi, vec);
			for (std::vector<std::pair<LPARAM, std::string> >::reverse_iterator ritor = vec.rbegin(); ritor != vec.rend(); ++ ritor) {
				if (ritor == vec.rbegin()) {
					continue;
				}
				cfg_index = 0;
				foreach (const config::any_child& value, cfg->all_children_range()) {
					if (cfg_index ++ == ritor->first) {
						cfg = &value.cfg;
						break;
					}
				}
			}
			cfg_2_tv(lpNMHdr->hwndFrom, htvi, *cfg, 0);
		}

	} else if (lpNMHdr->code == NM_DBLCLK) {
		//
		// 子config: 展开/折叠叶子(系统自动)
		// attribute: 无动作
		//
		
	} else if (lpNMHdr->code == TVN_ITEMEXPANDED) {
		//
		// 子叶子已被展开或折叠, 折叠时不删除所子有叶子, (认为要被解析config在此次是不会变的)
		//
		if (lpnmitem->action & TVE_COLLAPSE) {
			// TreeView_Walk(lpNMHdr->hwndFrom, lpnmitem->itemNew.hItem, FALSE, NULL, NULL, TRUE);
			// TreeView_SetChilerenByPath(lpNMHdr->hwndFrom, htvi, TreeView_FormPath(lpNMHdr->hwndFrom, htvi, dirname(game_config::path.c_str())));
		}
	}

    return FALSE;
}

BOOL CALLBACK DlgCfgProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgCfgInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgCfgCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgCfgNotify);
	}
	
	return FALSE;
}

typedef struct {
	HWND		hctl;
	uint32_t	maxdeep;	// 最大扩展maxdeep级, 0: 只扩展根目录, 1: 扩展到根目录下的目录为止, -1: 因为是无符号数,意味着全扩展	
} tv_walk_cfg_param_t;

void walk_cfg_recursion(const config& cfg, HTREEITEM htvroot, char* text, uint16_t deep, tv_walk_cfg_param_t* wcp)
{
	HTREEITEM htvi, htvi_attribute;
	size_t attribute_index, cfg_index = 0;
	
	// attribute
	attribute_index = 0;
	foreach (const config::attribute &istrmap, cfg.attribute_range()) {
		htvi_attribute = TreeView_AddLeaf(wcp->hctl, htvroot);
		sprintf(text, "%s=%s", istrmap.first.c_str(), istrmap.second.str().c_str());
		TreeView_SetItem2(wcp->hctl, htvi_attribute, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, attribute_index ++, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	}
	// recursively resolve children
	foreach (const config::any_child& value, cfg.all_children_range()) {
		// key
		htvi = TreeView_AddLeaf(wcp->hctl, htvroot);
		strcpy(text, value.key.c_str());
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		TreeView_SetItem2(wcp->hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, cfg_index ++, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		if (wcp->maxdeep >= deep) {
			walk_cfg_recursion(value.cfg, htvi, text, deep + 1, wcp);
		}
	}
}

void cfg_2_tv(HWND hctl, HTREEITEM htvroot, const config& cfg, uint32_t maxdeep)
{
	char* text;
	tv_walk_cfg_param_t wcp;

	text = (char*)malloc(CONSTANT_1M);

	wcp.hctl = hctl;
	wcp.maxdeep = maxdeep;
	// 此处的subfolders不能设为1. 旦设为1,所有枚举出的子目录/文件都会被添加到IVI_ROOT,而在回调函数中有排序,
	// 排序这操作可能会把系统累死
	walk_cfg_recursion(cfg, htvroot, text, 0, &wcp);

	free(text);
	
	return;
}

// 根据指示config刷新树形视图控件
void cfg_refresh(HWND hctl)
{
	char text[_MAX_PATH];

	wml_config_from_file(std::string(gdmgr.cfg_fname_), vcfg.game_config_);

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);

	// 2. 向TreeView添加一级内容
	vcfg.htvroot_ = TreeView_AddLeaf(hctl, TVI_ROOT);
	strcpy(text, basename(gdmgr.cfg_fname_));
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, vcfg.htvroot_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		vcfg.game_config_.empty()? 0: 1, text);
	cfg_2_tv(hctl, vcfg.htvroot_, vcfg.game_config_, 0);
}
