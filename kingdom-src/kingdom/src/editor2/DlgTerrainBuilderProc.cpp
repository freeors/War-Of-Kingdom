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

#include "builder.hpp"
#include "image.hpp"

void tb_refresh(HWND hctl);
void tb_2_tv(HWND hctl, HTREEITEM htvroot, terrain_builder::building_rule** rules, uint32_t rules_size);
void tb_2_tv_fields(HWND hctl, HTREEITEM htvroot, const terrain_builder::building_rule& rule);

class visual_tb
{
public:
	visual_tb();
	~visual_tb();

	void release_heap();

// attribute member
	HTREEITEM htvroot_;
	terrain_builder::building_rule* rules_;
	uint32_t rules_size_;
};

visual_tb::visual_tb() : rules_(NULL),
	rules_size_(0)
{}

visual_tb::~visual_tb()
{
	release_heap();
}

void visual_tb::release_heap()
{
/*
	while (rules_size_) {
		delete rules_[-- rules_size_];
	}
	if (rules_) {
		free(rules_);
		rules_ = NULL;
	}
*/
	if (rules_) {
		delete []rules_;
		rules_ = NULL;
	}
	rules_size_ = 0;
}

visual_tb vtb;

void tb_enter_ui(void)
{
	HWND hctl = GetDlgItem(gdmgr._hdlg_tb, IDC_TV_TB_EXPLORER);

	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	tb_refresh(hctl);
	return;
}

BOOL tb_hide_ui(void)
{
	return TRUE;
}

// 对话框消息处理函数
BOOL On_DlgTbInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_TB_EXPLORER);

	gdmgr._hdlg_cfg = hdlgP;

	TreeView_SetImageList(hctl, gdmgr._himl, TVSIL_NORMAL);

	return FALSE;
}

void On_DlgTbCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

BOOL On_DlgTbNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi;
	POINT					point;
	
	if (lpNMHdr->idFrom != IDC_TV_TB_EXPLORER) {
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
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
		if (tvi.cChildren && !TreeView_GetChild(lpNMHdr->hwndFrom, htvi) && tvi.lParam != UINT32_MAX) {
			terrain_builder::building_rule* rules = vtb.rules_;
			uint32_t rule_index;

			for (rule_index = 0; rule_index < vtb.rules_size_; rule_index ++) {
				terrain_builder::building_rule& r = rules[rule_index];
				if (rule_index == (uint32_t)(tvi.lParam)) {
					tb_2_tv_fields(lpNMHdr->hwndFrom, htvi, r);
					break;
				}
			}
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

void On_DlgTbDestroy(HWND hdlgP)
{
	return;
}

BOOL CALLBACK DlgTbProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTbInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTbCommand);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgTbNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgTbDestroy);
	}
	
	return FALSE;
}

void tb_2_tv(HWND hctl, HTREEITEM htvroot, terrain_builder::building_rule* rules, uint32_t rules_size)
{
	char text[_MAX_PATH];
	uint32_t rule_index;
	HTREEITEM htvi;

	// 此处的subfolders不能设为1. 旦设为1,所有枚举出的子目录/文件都会被添加到IVI_ROOT,而在回调函数中有排序,
	// 排序这操作可能会把系统累死
	
	for (rule_index = 0; rule_index < rules_size; rule_index ++) {
		terrain_builder::building_rule& r = rules[rule_index];
		htvi = TreeView_AddLeaf(hctl, htvroot);
		sprintf(text, "#%04u.building_rule", rule_index);
		// 枚举到此为止,此个config一定有孩子,强制让出来前面+符号
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, rule_index, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
	}
	
	return;
}

#define null_2_space(c) ((c) != '\0'? (c): '$')
std::string format_t_terrain(t_translation::t_terrain t)
{
	char text[MAX_PATH];
	sprintf(text, "(%c%c%c%c, %c%c%c%c)", 
		null_2_space((t.base & 0xff000000) >> 24), null_2_space((t.base & 0xff0000) >> 16), null_2_space((t.base & 0xff00) >> 8), null_2_space(t.base & 0xff),
		null_2_space((t.overlay & 0xff000000) >> 24), null_2_space((t.overlay & 0xff0000) >> 16), null_2_space((t.overlay & 0xff00) >> 8), null_2_space(t.overlay & 0xff));

	return std::string(text);
}

std::string hex_t_terrain(t_translation::t_terrain t)
{
	char text[MAX_PATH];
	sprintf(text, "(0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x)", 
		(t.base & 0xff000000) >> 24, (t.base & 0xff0000) >> 16, (t.base & 0xff00) >> 8, t.base & 0xff,
		(t.overlay & 0xff000000) >> 24, (t.overlay & 0xff0000) >> 16, (t.overlay & 0xff00) >> 8, t.overlay & 0xff);

	return std::string(text);
}

void tb_2_tv_fields(HWND hctl, HTREEITEM htvroot, const terrain_builder::building_rule& rule)
{
	char text[_MAX_PATH];
	size_t index;
	HTREEITEM htvi, htvi2, htvi3, htvi4, htvi5, htvi6;
	std::stringstream str;
	
	// constraint_set constraints;
	index = 0;
	for (terrain_builder::constraint_set::const_iterator constraint = rule.constraints.begin(); constraint != rule.constraints.end(); ++constraint, index ++) {
		htvi2 = TreeView_AddLeaf(hctl, htvroot);
		sprintf(text, "#%04u.constraint", index);
		TreeView_SetItem2(hctl, htvi2, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);

		//
		// terrain_constraint
		//
		// [t_translation::t_match terrain_types_match;]
		htvi3 = TreeView_AddLeaf(hctl, htvi2);
		strcpy(text, "t_match.terrain_types_match");
		TreeView_SetItem2(hctl, htvi3, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		const t_translation::t_match& match = constraint->terrain_types_match;
		// t_list terrain;
		str.str("");
		str << "terrain=";
		for (t_translation::t_list::const_iterator itor = match.terrain.begin(); itor != match.terrain.end(); ++ itor) {
			if (itor != match.terrain.begin()) {
				str << ",";
			}
			str << format_t_terrain(*itor);
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi3);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// t_list mask;
		str.str("");
		str << "mask=";
		for (t_translation::t_list::const_iterator itor = match.mask.begin(); itor != match.mask.end(); ++ itor) {
			if (itor != match.mask.begin()) {
				str << ",";
			}
			str << hex_t_terrain(*itor);
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi3);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// t_list masked_terrain;
		str.str("");
		str << "masked_terrain=";
		for (t_translation::t_list::const_iterator itor = match.masked_terrain.begin(); itor != match.masked_terrain.end(); ++ itor) {
			if (itor != match.masked_terrain.begin()) {
				str << ",";
			}
			str << format_t_terrain(*itor);
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi3);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// bool has_wildcard;
		sprintf(text, "has_wildcard=%s", match.has_wildcard? "true": "false");
		htvi = TreeView_AddLeaf(hctl, htvi3);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// bool is_empty;
		sprintf(text, "is_empty=%s", match.has_wildcard? "true": "false");
		htvi = TreeView_AddLeaf(hctl, htvi3);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);

		// [rule_imagelist images;]
		htvi3 = TreeView_AddLeaf(hctl, htvi2);
		strcpy(text, "rule_imagelist.images");
		TreeView_SetItem2(hctl, htvi3, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
		const terrain_builder::rule_imagelist& images = constraint->images;
		size_t image_index = 0;
		for (terrain_builder::rule_imagelist::const_iterator itor = images.begin(); itor != images.end(); ++ itor, image_index ++) {
			htvi4 = TreeView_AddLeaf(hctl, htvi3);
			sprintf(text, "#%02u.image", image_index);
			TreeView_SetItem2(hctl, htvi4, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
			// [std::vector<rule_image_variant> variants;]
			size_t variant_index = 0;
			const std::vector<terrain_builder::rule_image_variant>& variants = itor->variants;
			for (std::vector<terrain_builder::rule_image_variant>::const_iterator variant = variants.begin(); variant != variants.end(); ++ variant, variant_index ++) {
				htvi5 = TreeView_AddLeaf(hctl, htvi4);
				sprintf(text, "#%02u.variant", variant_index);
				TreeView_SetItem2(hctl, htvi5, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 1, text);
				// std::vector< animated<image::locator> > images;
				htvi6 = TreeView_AddLeaf(hctl, htvi5);
				strcpy(text, "animated.images");
				TreeView_SetItem2(hctl, htvi6, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, UINT32_MAX, gdmgr._iico_dir, gdmgr._iico_dir, 0, text);

				// std::string image_string;
				sprintf(text, "image_string=%s", variant->image_string.c_str());
				htvi = TreeView_AddLeaf(hctl, htvi5);
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
				// std::string variations;
				sprintf(text, "variations=%s", variant->variations.c_str());
				htvi = TreeView_AddLeaf(hctl, htvi5);
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
				// std::set<std::string> tods;
				str.str("");
				str << "tods=";
				for (std::set<std::string>::const_iterator tod = variant->tods.begin(); tod != variant->tods.end(); ++ tod) {
					if (tod != variant->tods.begin()) {
						str << ",";
					}
					str << *tod;
				}
				strcpy(text, str.str().c_str());
				htvi = TreeView_AddLeaf(hctl, htvi5);
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
				// bool random_start;
				sprintf(text, "random_start=%s", variant->random_start? "true": "false");
				htvi = TreeView_AddLeaf(hctl, htvi5);
				TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
			}

			// int layer;
			sprintf(text, "layer=%i", itor->layer);
			htvi = TreeView_AddLeaf(hctl, htvi4);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
			// int basex, basey;
			sprintf(text, "base=(%i, %i)", itor->basex, itor->basey);
			htvi = TreeView_AddLeaf(hctl, htvi4);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
			// int center_x, center_y;
			sprintf(text, "center=(%i, %i)", itor->center_x, itor->center_y);
			htvi = TreeView_AddLeaf(hctl, htvi4);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
			// bool global_image;
			sprintf(text, "global_image=%s", itor->global_image? "true": "false");
			htvi = TreeView_AddLeaf(hctl, htvi4);
			TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		}

		// map_location loc;
		htvi = TreeView_AddLeaf(hctl, htvi2);
		sprintf(text, "loc=(%i, %i)", constraint->loc.x, constraint->loc.y);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// std::vector<std::string> set_flag;
		str.str("");
		str << "set_flag=";
		for (std::vector<std::string>::const_iterator itor = constraint->set_flag.begin(); itor != constraint->set_flag.end(); ++ itor) {
			if (itor != constraint->set_flag.begin()) {
				str << ",";
			}
			str << *itor;
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi2);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// std::vector<std::string> no_flag;
		str.str("");
		str << "no_flag=";
		for (std::vector<std::string>::const_iterator itor = constraint->no_flag.begin(); itor != constraint->no_flag.end(); ++ itor) {
			if (itor != constraint->no_flag.begin()) {
				str << ",";
			}
			str << *itor;
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi2);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
		// std::vector<std::string> has_flag;
		str.str("");
		str << "has_flag=";
		for (std::vector<std::string>::const_iterator itor = constraint->has_flag.begin(); itor != constraint->has_flag.end(); ++ itor) {
			if (itor != constraint->set_flag.begin()) {
				str << ",";
			}
			str << *itor;
		}
		strcpy(text, str.str().c_str());
		htvi = TreeView_AddLeaf(hctl, htvi2);
		TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	}

	// map_location location_constraints;
	htvi = TreeView_AddLeaf(hctl, htvroot);
	sprintf(text, "location_constraints=(%i, %i)", rule.location_constraints.x, rule.location_constraints.y);
	TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	// int probability;
	htvi = TreeView_AddLeaf(hctl, htvroot);
	sprintf(text, "probability=%i", rule.probability);
	TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	// int precedence;
	htvi = TreeView_AddLeaf(hctl, htvroot);
	sprintf(text, "precedence=%i", rule.precedence);
	TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	// bool local;
	htvi = TreeView_AddLeaf(hctl, htvroot);
	sprintf(text, "local=%s", rule.local? "true": "false");
	TreeView_SetItem2(hctl, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 0, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
}

extern terrain_builder::building_rule* wml_building_rules_from_file(const std::string& fname, uint32_t* rules_size_ptr);

// 根据指示config刷新树形视图控件
void tb_refresh(HWND hctl)
{
	char text[_MAX_PATH];

	// use the swap trick to clear the rules cache and get a fresh one.
	// because simple clear() seems to cause some progressive memory degradation.
	// terrain_builder::building_ruleset empty;
	// std::swap(vtb.rules_, empty);
	vtb.release_heap();

	vtb.rules_ = wml_building_rules_from_file(std::string(gdmgr.cfg_fname_), &vtb.rules_size_);

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);

	// 2. 向TreeView添加一级内容
	vtb.htvroot_ = TreeView_AddLeaf(hctl, TVI_ROOT);
	strcpy(text, basename(gdmgr.cfg_fname_));
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	TreeView_SetItem1(hctl, vtb.htvroot_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
		vtb.rules_? 1: 0, text);
	tb_2_tv(hctl, vtb.htvroot_, vtb.rules_, vtb.rules_size_);
}