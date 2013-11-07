/*
	config架构视形视图中每个结点的lParam值有特珠意义
	1)结点是子config: 该子config在config中索引值。要由它个值在game_config_中定位中该config。
	2)结点是attribute：该attribute在attribute位置值。（当前没有使用，只有是样填了）
*/

#include "global.hpp"
#include "game_config.hpp"
#include "config.hpp"
#include "editor.hpp"
#include "loadscreen.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include <string.h>
#include <shlobj.h> // SHBrowseForFolder

#include "resource.h"
#include <boost/foreach.hpp>

#include "xfunc.h"
#include "win32x.h"
#include "struct.h"

#include "builder.hpp"
#include "image.hpp"

class tvisual
{
public:
	tvisual();
	~tvisual();

	void refresh(int type, HWND hctl);
	void on_rclick(LPNMHDR lpNMHdr, HTREEITEM htvi);

	// WML section
	void wml_2_tv(HWND hctl, HTREEITEM htvroot, const config& cfg, uint32_t maxdeep);

	// building rule section
	void br_2_tv(HWND hctl, HTREEITEM htvroot, terrain_builder::building_rule* rules, uint32_t rules_size);
	void br_2_tv_fields(HWND hctl, HTREEITEM htvroot, const terrain_builder::building_rule& rule);
	void release_heap();

public:
	HTREEITEM htvroot_;
	// WML section
	config game_config_;

	// building rule section
	terrain_builder::building_rule* rules_;
	uint32_t rules_size_;

private:
	int type_;
};

tvisual::tvisual()
	: rules_(NULL)
	, rules_size_(0)
{}

tvisual::~tvisual()
{
	release_heap();
}

void tvisual::release_heap()
{
	if (rules_) {
		delete []rules_;
		rules_ = NULL;
	}
	rules_size_ = 0;
}

tvisual visual;

void visual_enter_ui(void)
{
	HWND hctl = GetDlgItem(gdmgr._hdlg_visual, IDC_TV_CFG_EXPLORER);

	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	visual.refresh(editor_config::type, hctl);
	return;
}

BOOL visual_hide_ui(void)
{
	return TRUE;
}

// 对话框消息处理函数
BOOL On_DlgCfgInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_CFG_EXPLORER);

	gdmgr._hdlg_visual = hdlgP;

	TreeView_SetImageList(hctl, gdmgr._himl, TVSIL_NORMAL);

	return FALSE;
}

void On_DlgCfgCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	return;
}

void tvisual::on_rclick(LPNMHDR lpNMHdr, HTREEITEM htvi)
{
	TVITEMEX tvi;

	if (type_ == BIN_WML) {
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_CHILDREN, NULL);
		if (tvi.cChildren && !TreeView_GetChild(lpNMHdr->hwndFrom, htvi)) {
			std::vector<std::pair<LPARAM, std::string> > vec;
			LPARAM cfg_index;
			const config* cfg = &game_config_;

			TreeView_FormVector(lpNMHdr->hwndFrom, htvi, vec);
			for (std::vector<std::pair<LPARAM, std::string> >::reverse_iterator ritor = vec.rbegin(); ritor != vec.rend(); ++ ritor) {
				if (ritor == vec.rbegin()) {
					continue;
				}
				cfg_index = 0;
				BOOST_FOREACH (const config::any_child& value, cfg->all_children_range()) {
					if (cfg_index ++ == ritor->first) {
						cfg = &value.cfg;
						break;
					}
				}
			}
			wml_2_tv(lpNMHdr->hwndFrom, htvi, *cfg, 0);
		}
	} else {
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
		if (tvi.cChildren && !TreeView_GetChild(lpNMHdr->hwndFrom, htvi) && tvi.lParam != UINT32_MAX) {
			terrain_builder::building_rule* rules = rules_;
			uint32_t rule_index;

			for (rule_index = 0; rule_index < rules_size_; rule_index ++) {
				terrain_builder::building_rule& r = rules[rule_index];
				if (rule_index == (uint32_t)(tvi.lParam)) {
					br_2_tv_fields(lpNMHdr->hwndFrom, htvi, r);
					break;
				}
			}
		}
	}
}

BOOL On_DlgCfgNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
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
		visual.on_rclick(lpNMHdr, htvi);

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
	BOOST_FOREACH (const config::attribute &istrmap, cfg.attribute_range()) {
		htvi_attribute = TreeView_AddLeaf(wcp->hctl, htvroot);
		sprintf(text, "%s=%s", istrmap.first.c_str(), utf8_2_ansi(istrmap.second.str().c_str()));
		TreeView_SetItem2(wcp->hctl, htvi_attribute, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, attribute_index ++, gdmgr._iico_txt, gdmgr._iico_txt, 0, text);
	}
	// recursively resolve children
	BOOST_FOREACH (const config::any_child& value, cfg.all_children_range()) {
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

void tvisual::wml_2_tv(HWND hctl, HTREEITEM htvroot, const config& cfg, uint32_t maxdeep)
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

// building rule section
void tvisual::br_2_tv(HWND hctl, HTREEITEM htvroot, terrain_builder::building_rule* rules, uint32_t rules_size)
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

void tvisual::br_2_tv_fields(HWND hctl, HTREEITEM htvroot, const terrain_builder::building_rule& rule)
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
void tvisual::refresh(int type, HWND hctl)
{
	type_ = type;

	char text[_MAX_PATH];

	if (type_ == BIN_WML) {
		wml_config_from_file(std::string(gdmgr.cfg_fname_), visual.game_config_);
	} else {
		// use the swap trick to clear the rules cache and get a fresh one.
		// because simple clear() seems to cause some progressive memory degradation.
		// terrain_builder::building_ruleset empty;
		// std::swap(vtb.rules_, empty);
		release_heap();

		rules_ = wml_building_rules_from_file(std::string(gdmgr.cfg_fname_), &rules_size_);
	}

	// 1. 删除Treeview中所有项
	TreeView_DeleteAllItems(hctl);

	// 2. 向TreeView添加一级内容
	visual.htvroot_ = TreeView_AddLeaf(hctl, TVI_ROOT);
	strcpy(text, basename(gdmgr.cfg_fname_));
	// 这里一定要设TVIF_CHILDREN, 否则接下折叠后将判断出其cChildren为0, 再不能展开
	if (type_ == BIN_WML) {
		TreeView_SetItem1(hctl, htvroot_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
			game_config_.empty()? 0: 1, text);
		wml_2_tv(hctl, visual.htvroot_, visual.game_config_, 0);
	} else {
		TreeView_SetItem1(hctl, htvroot_, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN, 0, 0, 0, 
			rules_? 1: 0, text);
		br_2_tv(hctl, htvroot_, rules_, rules_size_);
	}
}
