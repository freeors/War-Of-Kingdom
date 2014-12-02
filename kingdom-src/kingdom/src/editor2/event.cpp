#ifndef _ROSE_EDITOR

#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "DlgCampaignProc.hpp"
#include <string.h>
#include "formula_string_utils.hpp"
#include "rectangle.hpp"
#include "DlgCoreProc.hpp"

#include "resource.h"

#include "xfunc.h"
#include "struct.h"
#include "win32x.h"
#include "gettext.hpp"
#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "map_location.hpp"

#include <boost/foreach.hpp>

void OnEventFilterBt(HWND hdlgP);
bool OnEventFilterBt2(HWND hdlgP, int id);
void OnFilterHeroBt(HWND hdlgP);
void OnSetVariableEditBt(HWND hdlgP);
void OnEventKillEditBt(HWND hdlgP);
void OnEventEndlevelEditBt(HWND hdlgP);
void OnEventJoinEditBt(HWND hdlgP);
void OnEventUnitEditBt(HWND hdlgP);
void OnEventModifyUnitEditBt(HWND hdlgP);
void OnEventModifySideEditBt(HWND hdlgP);
void OnEventModifyCityEditBt(HWND hdlgP);
void OnEventMessageEditBt(HWND hdlgP);
void OnEventPrintEditBt(HWND hdlgP);
void OnEventLabelEditBt(HWND hdlgP);
void OnSideHerosEditBt(HWND hdlgP);
void OnStoreUnitEditBt(HWND hdlgP);
void OnEventRenameEditBt(HWND hdlgP);
void OnEventObjectivesEditBt(HWND hdlgP);
void OnConditionEditBt(HWND hdlgP);
BOOL CALLBACK DlgVariableProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgHaveUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace ns {
	int action_event_item;

	tevent::tfilter_* filter;
	tevent::tcommand* clicked_command;
	std::string clicked_variable;
	int clicked_judge;
	int action_judge;
	int clicked_item;

	std::map<std::string, std::string> cumulate_variables;
	
	HIMAGELIST himl_evt;
	int iico_evt_event;	
	int iico_evt_filter;
	int iico_evt_command;
	int iico_evt_if;
	int iico_evt_condition;
	int iico_evt_then;
	int iico_evt_attribute;

	HMENU hpopup_new_event;
	HMENU hpopup_new;
	HMENU hpopup_event;
	HMENU hpopup_variable;
}

namespace digit_op {

std::map<int, std::string> op_map;

void fill_op_map()
{
	if (op_map.empty()) {
		op_map.insert(std::make_pair(GREATER, ">"));
		op_map.insert(std::make_pair(LESS, "<"));
		op_map.insert(std::make_pair(GREATER_EQUAL, ">="));
		op_map.insert(std::make_pair(LESS_EQUAL, "<="));
	}
}

int find(const std::string& op) 
{
	fill_op_map();
	for (std::map<int, std::string>::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		if (it->second == op) {
			return it->first;
		}
	}
	return NONE;
}

const std::string& find(int tag) 
{
	fill_op_map();
	std::map<int, std::string>::const_iterator it = op_map.find(tag);
	if (it != op_map.end()) {
		return it->second;
	}
	return null_str;
}

void ops_2_combo(HWND hctl, int tag)
{
	fill_op_map();
	ComboBox_ResetContent(hctl);
	int cursel = -1;
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, NONE);
	for (std::map<int, std::string>::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (cursel == -1 && tag == it->first) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (cursel == -1) {
		cursel = 0;
	}
	ComboBox_SetCurSel(hctl, cursel);
}

}

namespace valuex {
bool is_variable(const std::string& value)
{
	return !value.empty() && value.at(0) == '$';
}

int to_int(const std::string& value, int def)
{
	return lexical_cast_default<int>(value, def);
}

void cumulate_variables_to_lv(HWND hdlgP)
{
	char text[_MAX_PATH];
	
	HWND hwndlv = GetDlgItem(hdlgP, IDC_LV_EVENT_VARIABLE);

	LVCOLUMN lvc;
	// variable
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 120;
	lvc.pszText = "标识";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hwndlv, 0, &lvc);

	lvc.cx = 150;
	lvc.iSubItem = 1;
	lvc.pszText = "值";
	ListView_InsertColumn(hwndlv, 1, &lvc);

	ListView_SetImageList(hwndlv, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hwndlv, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	LVITEM lvi;
	int count = ListView_GetItemCount(hwndlv);
	ListView_DeleteAllItems(hwndlv);
	int index = 0;
	for (std::map<std::string, std::string>::const_iterator it = ns::cumulate_variables.begin(); it != ns::cumulate_variables.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 标识
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strcpy(text, it->first.c_str());
		lvi.pszText = text;
		lvi.lParam = index ++;
		ListView_InsertItem(hwndlv, &lvi);

		// 值
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, it->second.c_str());
		lvi.pszText = text;
		ListView_SetItem(hwndlv, &lvi);
	}
}

void cumulate_variables_notify_handler_rclick(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	LVITEM lvi;
	char text[_MAX_PATH];

	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom != IDC_LV_EVENT_VARIABLE) {
			return;
		}
		if (lpnmitem->iItem < 0) {
			return;
		}

		POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
		MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		ns::clicked_variable = std::string("$") + text;

		// EnableMenuItem(ns::hpopup_variable, IDM_VARIABLE_ITEM2, MF_BYCOMMAND | MF_GRAYED);

		TrackPopupMenuEx(ns::hpopup_variable, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		// EnableMenuItem(ns::hpopup_variable, IDM_VARIABLE_ITEM2, MF_BYCOMMAND | MF_ENABLED);
	}
}

}

void tevent::from_config_branch(const config& cfg, std::vector<tcommand*>& b)
{
	BOOST_FOREACH (const config::any_child& tmp, cfg.all_children_range()) {
		tcommand* c = NULL;
		if (tmp.key == "set_variable") {
			c = new tset_variable();
		} else if (tmp.key == "kill") {
			c = new tkill();
		} else if (tmp.key == "endlevel") {
			c = new tendlevel();
		} else if (tmp.key == "join") {
			c = new tjoin();
		} else if (tmp.key == "unit") {
			c = new tunit();
		} else if (tmp.key == "modify_unit2") {
			c = new tmodify_unit();
		} else if (tmp.key == "modify_side") {
			c = new tmodify_side();
		} else if (tmp.key == "modify_city") {
			c = new tmodify_city();
		} else if (tmp.key == "message") {
			c = new tmessage();
		} else if (tmp.key == "print") {
			c = new tprint();
		} else if (tmp.key == "label") {
			c = new tlabel();
		} else if (tmp.key == "allow_undo") {
			c = new tallow_undo();
		} else if (tmp.key == "sideheros") {
			c = new tsideheros();
		} else if (tmp.key == "store_unit") {
			c = new tstore_unit();
		} else if (tmp.key == "rename") {
			c = new trename();
		} else if (tmp.key == "objectives") {
			c = new tobjectives();
		} else if (tmp.key == "event") {
			c = new tevent();
		} else if (tmp.key == "if") {
			c = new tif();
		}
		if (c) {
			c->from_config(tmp.cfg);
			b.push_back(c);
		}
	}
}

void tevent::copy_commands(const std::vector<tcommand*>& src, std::vector<tcommand*>& dst)
{
	dst.clear();
	for (std::vector<tcommand*>::const_iterator it = src.begin(); it != src.end(); ++ it) {
		const tcommand* cmd = *it;
		tcommand* new_cmd = NULL;
		if (cmd->type_ == tcommand::SET_VARIABLE) {
			new_cmd = new tset_variable(*dynamic_cast<const tset_variable*>(cmd));
		} else if (cmd->type_ == tcommand::KILL) {
			new_cmd = new tkill(*dynamic_cast<const tkill*>(cmd));
		} else if (cmd->type_ == tcommand::ENDLEVEL) {
			new_cmd = new tendlevel(*dynamic_cast<const tendlevel*>(cmd));
		} else if (cmd->type_ == tcommand::JOIN) {
			new_cmd = new tjoin(*dynamic_cast<const tjoin*>(cmd));
		} else if (cmd->type_ == tcommand::UNIT) {
			new_cmd = new tunit(*dynamic_cast<const tunit*>(cmd));
		} else if (cmd->type_ == tcommand::MODIFY_UNIT) {
			new_cmd = new tmodify_unit(*dynamic_cast<const tmodify_unit*>(cmd));
		} else if (cmd->type_ == tcommand::MODIFY_SIDE) {
			new_cmd = new tmodify_side(*dynamic_cast<const tmodify_side*>(cmd));
		} else if (cmd->type_ == tcommand::MODIFY_CITY) {
			new_cmd = new tmodify_city(*dynamic_cast<const tmodify_city*>(cmd));
		} else if (cmd->type_ == tcommand::MESSAGE) {
			new_cmd = new tmessage(*dynamic_cast<const tmessage*>(cmd));
		} else if (cmd->type_ == tcommand::PRINT) {
			new_cmd = new tprint(*dynamic_cast<const tprint*>(cmd));
		} else if (cmd->type_ == tcommand::LABEL) {
			new_cmd = new tlabel(*dynamic_cast<const tlabel*>(cmd));
		} else if (cmd->type_ == tcommand::ALLOW_UNDO) {
			new_cmd = new tallow_undo(*dynamic_cast<const tallow_undo*>(cmd));
		} else if (cmd->type_ == tcommand::SIDEHEROS) {
			new_cmd = new tsideheros(*dynamic_cast<const tsideheros*>(cmd));
		} else if (cmd->type_ == tcommand::STORE_UNIT) {
			new_cmd = new tstore_unit(*dynamic_cast<const tstore_unit*>(cmd));
		} else if (cmd->type_ == tcommand::RENAME) {
			new_cmd = new trename(*dynamic_cast<const trename*>(cmd));
		} else if (cmd->type_ == tcommand::OBJECTIVES) {
			new_cmd = new tobjectives(*dynamic_cast<const tobjectives*>(cmd));
		} else if (cmd->type_ == tcommand::EVENT) {
			new_cmd = new tevent(*dynamic_cast<const tevent*>(cmd));
		} else if (cmd->type_ == tcommand::FILTER) {
			new_cmd = new tfilter_(*dynamic_cast<const tfilter_*>(cmd));
		} else if (cmd->type_ == tcommand::FILTER_HERO) {
			new_cmd = new tfilter_hero_(*dynamic_cast<const tfilter_hero_*>(cmd));
		} else if (cmd->type_ == tcommand::IF) {
			new_cmd = new tif(*dynamic_cast<const tif*>(cmd));
		}
		dst.push_back(new_cmd);
	}
}

bool tevent::compare_commands(const std::vector<tevent::tcommand*>& left, const std::vector<tevent::tcommand*>& right)
{
	if (left.size() != right.size()) return false;
	for (size_t i = 0; i < left.size(); i ++) {
		const tcommand* a = left[i];
		const tcommand* b = right[i];

		if (a->type_ != b->type_) return false;

		if (a->type_ == tcommand::SET_VARIABLE) {
			if (*dynamic_cast<const tset_variable*>(a) != *dynamic_cast<const tset_variable*>(b)) return false;

		} else if (a->type_ == tcommand::KILL) {
			if (*dynamic_cast<const tkill*>(a) != *dynamic_cast<const tkill*>(b)) return false;

		} else if (a->type_ == tcommand::ENDLEVEL) {
			if (*dynamic_cast<const tendlevel*>(a) != *dynamic_cast<const tendlevel*>(b)) return false;

		} else if (a->type_ == tcommand::JOIN) {
			if (*dynamic_cast<const tjoin*>(a) != *dynamic_cast<const tjoin*>(b)) return false;

		} else if (a->type_ == tcommand::UNIT) {
			if (*dynamic_cast<const tunit*>(a) != *dynamic_cast<const tunit*>(b)) return false;

		} else if (a->type_ == tcommand::MODIFY_UNIT) {
			if (*dynamic_cast<const tmodify_unit*>(a) != *dynamic_cast<const tmodify_unit*>(b)) return false;

		} else if (a->type_ == tcommand::MODIFY_SIDE) {
			if (*dynamic_cast<const tmodify_side*>(a) != *dynamic_cast<const tmodify_side*>(b)) return false;

		} else if (a->type_ == tcommand::MODIFY_CITY) {
			if (*dynamic_cast<const tmodify_city*>(a) != *dynamic_cast<const tmodify_city*>(b)) return false;

		} else if (a->type_ == tcommand::MESSAGE) {
			if (*dynamic_cast<const tmessage*>(a) != *dynamic_cast<const tmessage*>(b)) return false;

		} else if (a->type_ == tcommand::PRINT) {
			if (*dynamic_cast<const tprint*>(a) != *dynamic_cast<const tprint*>(b)) return false;

		} else if (a->type_ == tcommand::LABEL) {
			if (*dynamic_cast<const tlabel*>(a) != *dynamic_cast<const tlabel*>(b)) return false;

		} else if (a->type_ == tcommand::ALLOW_UNDO) {
			if (*dynamic_cast<const tallow_undo*>(a) != *dynamic_cast<const tallow_undo*>(b)) return false;

		} else if (a->type_ == tcommand::STORE_UNIT) {
			if (*dynamic_cast<const tstore_unit*>(a) != *dynamic_cast<const tstore_unit*>(b)) return false;

		} else if (a->type_ == tcommand::RENAME) {
			if (*dynamic_cast<const trename*>(a) != *dynamic_cast<const trename*>(b)) return false;

		} else if (a->type_ == tcommand::OBJECTIVES) {
			if (*dynamic_cast<const tobjectives*>(a) != *dynamic_cast<const tobjectives*>(b)) return false;

		} else if (a->type_ == tcommand::SIDEHEROS) {
			if (*dynamic_cast<const tsideheros*>(a) != *dynamic_cast<const tsideheros*>(b)) return false;

		} else if (a->type_ == tcommand::EVENT) {
			if (*dynamic_cast<const tevent*>(a) != *dynamic_cast<const tevent*>(b)) return false;

		} else if (a->type_ == tcommand::FILTER) {
			if (*dynamic_cast<const tfilter_*>(a) != *dynamic_cast<const tfilter_*>(b)) return false;

		} else if (a->type_ == tcommand::FILTER_HERO) {
			if (*dynamic_cast<const tfilter_hero_*>(a) != *dynamic_cast<const tfilter_hero_*>(b)) return false;

		} else if (a->type_ == tcommand::IF) {
			if (*dynamic_cast<const tif*>(a) != *dynamic_cast<const tif*>(b)) return false;

		} 
	}
	return true;
}

std::vector<tevent::tevent_name> tevent::name_map;

std::vector<tevent::tevent_name>::const_iterator tevent::find_name(const std::string& id)
{
	for (std::vector<tevent_name>::const_iterator it = name_map.begin(); it != name_map.end(); ++ it) {
		if (it->id_ == id) {
			return it;
		}
	}
	return name_map.end();
}

void tevent::fill_name_map()
{
	if (name_map.empty()) {
		name_map.push_back(tevent_name("start", "开始关卡"));

		name_map.push_back(tevent_name("select", "选中单位"));
		name_map.back().filter_ = std::make_pair(true, _("Selector"));

		name_map.push_back(tevent_name("moveto", "移动到"));
		name_map.back().filter_ = std::make_pair(true, _("Mover at destination grid"));
		name_map.back().filter_second_ = std::make_pair(true, _("Unit at source grid"));

		name_map.push_back(tevent_name("comeinto", "进入城市"));
		name_map.back().filter_ = std::make_pair(true, _("City that come into"));
		name_map.back().filter_second_ = std::make_pair(true, _("Mover in reside list"));

		name_map.push_back(tevent_name("post_recruit", "征兵后"));
		name_map.back().filter_ = std::make_pair(true, _("City that recruit"));
		name_map.back().filter_second_ = std::make_pair(true, _("Recurited troop in reside list"));

		name_map.push_back(tevent_name("post_build", "建造后"));
		name_map.back().filter_ = std::make_pair(true, _("Build artifical"));
		name_map.back().filter_second_ = std::make_pair(true, _("Builder"));

		name_map.push_back(tevent_name("post_disband", "拆散部队后"));
		name_map.back().filter_ = std::make_pair(true, _("City that disband"));

		name_map.push_back(tevent_name("post_moveheros", "移动武将后"));
		name_map.back().filter_ = std::make_pair(true, _("Destination city"));
		name_map.back().filter_second_ = std::make_pair(true, _("Source city"));

		name_map.push_back(tevent_name("post_armory", "重编部队后"));
		name_map.back().filter_ = std::make_pair(true, _("City that armoried"));

		name_map.push_back(tevent_name("post_wander", "武将在野在新城市后"));
		name_map.back().filter_ = std::make_pair(true, _("City that wander"));
		name_map.back().filter_hero_ = std::make_pair(true, _("Wandering hero"));

		name_map.push_back(tevent_name("post_recommend", "武将自荐/仕官后"));
		name_map.back().filter_ = std::make_pair(true, _("City that recommend"));
		name_map.back().filter_hero_ = std::make_pair(true, _("Recommending hero"));

		name_map.push_back(tevent_name("post_duel", "单挑后"));
		name_map.back().filter_ = std::make_pair(true, _("Left troop"));
		name_map.back().filter_second_ = std::make_pair(true, _("Right troop"));

		name_map.push_back(tevent_name("sitdown", "驻扎"));
		name_map.back().filter_ = std::make_pair(true, _("Sit down troop"));

		name_map.push_back(tevent_name("attack_end", "一次完整攻击后"));
		name_map.back().filter_ = std::make_pair(true, _("Attacker"));
		name_map.back().filter_second_ = std::make_pair(true, _("Defender"));

		name_map.push_back(tevent_name("last breath", "单位即将死亡"));
		name_map.back().filter_ = std::make_pair(true, _("Dead"));
		name_map.back().filter_second_ = std::make_pair(true, _("Attacker"));

		name_map.push_back(tevent_name("new turn", "新回合"));
		name_map.push_back(tevent_name("time over", "超过关卡限定回合数"));
		name_map.push_back(tevent_name("victory", "胜利过关卡后"));
		name_map.push_back(tevent_name("defeat", "过关卡失败后"));
	}
}

tevent::tevent()
	: tcommand(tcommand::EVENT)
	, name_()
	, first_time_only_(true)
	, commands_()
{
	// ready "fixed" command
	tcommand* c = new tfilter_();
	commands_.push_back(c);

	c = new tfilter_();
	commands_.push_back(c);

	c = new tfilter_hero_();
	commands_.push_back(c);
}

tevent::tevent(const tevent& that)
	: tcommand(tcommand::EVENT)
	, name_(that.name_)
	, first_time_only_(that.first_time_only_)
{
	copy_commands(that.commands_, commands_);
}

tevent::~tevent()
{
	for (std::vector<tcommand*>::iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		delete *it;
	}
}

tevent& tevent::operator=(const tevent& that)
{
	name_ = that.name_;
	first_time_only_ = that.first_time_only_;

	for (std::vector<tcommand*>::iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		delete *it;
	}
	copy_commands(that.commands_, commands_);
	return *this;
}

void tevent::from_config(const config& event_cfg)
{
	name_ = event_cfg["name"].str();
	first_time_only_ = event_cfg["first_time_only"].to_bool(true);

	for (std::vector<tcommand*>::iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		delete *it;
	}
	commands_.clear();

	// first is "fixed" command. in order to use same location fucntion, put "fixed" command into commands_
	tcommand* c = new tfilter_();
	c->from_config(event_cfg.child("filter"));
	commands_.push_back(c);

	c = new tfilter_();
	c->from_config(event_cfg.child("filter_second"));
	commands_.push_back(c);

	c = new tfilter_hero_();
	c->from_config(event_cfg.child("filter_hero"));
	commands_.push_back(c);

	from_config_branch(event_cfg, commands_);
}

void tevent::update_to_ui(HWND hdlgP, int index) const
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::string str;
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_EVENT);
	int count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 时机
	if (index < 0 || index >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = index;
	}
	lvi.iSubItem = 0;
	strstr.str("");
	strstr << lvi.iItem + 1;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 时机
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	strstr.str("");
	strstr << name_;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 只一次
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	strstr.str("");
	strstr << (first_time_only_? utf8_2_ansi(_("Yes")): utf8_2_ansi(_("No")));
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void cb_treeview_update_scroll_event(HWND htv, HTREEITEM htvi, TVITEMEX& tvi, void* ctx)
{
	tevent& evt = *reinterpret_cast<tevent*>(ctx);
	TVITEMEX tvi1;
	if (!tvi.cChildren) {
		htvi = TreeView_GetParent(htv, htvi);
		TreeView_GetItem1(htv, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	}
	if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
		TreeView_GetItem1(htv, TreeView_GetParent(htv, htvi), &tvi1, TVIF_PARAM, NULL);
		scroll::first_visible_lparam = tvi1.lParam;
	} else if (tvi.lParam != tevent::PARAM_NONE) {
		scroll::first_visible_lparam = tvi.lParam;
	}

	TreeView_DeleteAllItems(htv);
	evt.update_to_ui_event_edit(htv, TVI_ROOT);
}

void tevent::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi_root, htvi_only_one_time;
	// name
	htvi_root = TreeView_AddLeaf(hctl, branch);
	strcpy(text, name_.c_str());
	TreeView_SetItem1(hctl, htvi_root, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_event, ns::iico_evt_event, 1, text);

	htvi_only_one_time = TreeView_AddLeaf(hctl, htvi_root);
	strstr.str("");
	strstr << utf8_2_ansi(_("Only one time")) << ": ";
	strstr << (first_time_only_? utf8_2_ansi(_("Yes")): utf8_2_ansi(_("No")));
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi_only_one_time, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	std::vector<tevent_name>::const_iterator find = find_name(name_);
	// filter/second filter
	for (size_t index = 0; index < commands_.size(); index ++) {
		const tcommand* cmd = commands_[index];

		if (index < fixed_commands) {
			HTREEITEM htvi1;
			strstr.str("");
			if (index == 0) {
				htvi1 = TreeView_AddLeaf(hctl, htvi_root);
				strstr << _("Cond.I: Filter first unit");
				if (find != name_map.end() && find->filter_.first) {
					strstr << "(" << find->filter_.second << ")";
				}
			} else if (index == 1) {
				htvi1 = TreeView_AddLeaf(hctl, htvi_root);
				strstr << _("Cond.II: Filter second unit");
				if (find != name_map.end() && find->filter_second_.first) {
					strstr << "(" << find->filter_second_.second << ")";
				}
			} else {
				htvi1 = TreeView_AddLeaf(hctl, htvi_root);
				strstr << _("Cond.III: Filter hero");
				if (find != name_map.end() && find->filter_hero_.first) {
					strstr << "(" << find->filter_hero_.second << ")";
				}
			}
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				(LPARAM)(tcommand*)(cmd), ns::iico_evt_filter, ns::iico_evt_filter, 1, text);
			cmd->update_to_ui_event_edit(hctl, htvi1);
		} else {
			cmd->update_to_ui_event_edit(hctl, htvi_root);
		}
	}

	TreeView_Walk(hctl, TVI_ROOT, TRUE, cb_treeview_walk_expand, NULL, FALSE);

	scroll::treeview_scroll_to(hctl);
}

void tevent::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[event]\n";

	strstr << prefix << "\tname = " << name_ << "\n";
	strstr << prefix << "\tfirst_time_only = " << (first_time_only_? "yes": "no") << "\n";

	for (size_t index = 0; index < commands_.size(); index ++) {
		const tcommand* cmd = commands_[index];
		if (index < fixed_commands) {
			if (index == 0) {
				if (!cmd->is_null()) {
					strstr << prefix << "\t[filter]\n";
					cmd->generate(strstr, prefix + "\t\t");
					strstr << prefix << "\t[/filter]\n";
				}
			} else if (index == 1) {
				if (!cmd->is_null()) {
					strstr << prefix << "\t[filter_second]\n";
					cmd->generate(strstr, prefix + "\t\t");
					strstr << prefix << "\t[/filter_second]\n";
				}
			} else if (index == 2) {
				if (!cmd->is_null()) {
					strstr << prefix << "\t[filter_hero]\n";
					cmd->generate(strstr, prefix + "\t\t");
					strstr << prefix << "\t[/filter_hero]\n";
				}
			}
		} else {
			if (index == fixed_commands) {
				strstr << prefix << "\n";
			}
			if (!cmd->is_null()) {
				cmd->generate(strstr, prefix + "\t");
			}
		}
	}

	strstr << prefix << "[/event]\n";
}

bool tevent::operator==(const tevent& that) const
{
	if (name_ != that.name_) return false;
	if (first_time_only_ != that.first_time_only_) return false;

	if (!compare_commands(commands_, that.commands_)) return false;

	return true;
}

bool tevent::cumulate_variables_internal(const tcommand* until, const std::vector<tcommand*>& commands, std::map<std::string, std::string>& ret) const
{
	for (std::vector<tcommand*>::const_iterator it = commands.begin(); it != commands.end(); ++ it) {
		const tcommand* cmd = *it;
		if (cmd == until) {
			return true;
		}

		if (cmd->type_ == tcommand::SET_VARIABLE) {
			const tset_variable* derive = dynamic_cast<const tset_variable*>(cmd);

			std::map<std::string, std::string>::iterator find_it = ret.find(derive->name_);
			if (find_it != ret.end()) {
				std::stringstream strstr;
				strstr << find_it->second << " | " << derive->value_;
				ret[derive->name_] = strstr.str();
			} else {
				ret[derive->name_] = derive->value_;
			}

		} else if (cmd->type_ == tcommand::IF) {
			const tif* derive = dynamic_cast<const tif*>(cmd);
			cumulate_variables_internal(until, derive->then_, ret);
			cumulate_variables_internal(until, derive->else_, ret);
		}
	}
	return false;
}

std::map<std::string, std::string> tevent::cumulate_variables(const tcommand* until) const
{
	std::map<std::string, std::string> ret;

	ret["unit.type"] = "第一单位兵种";
	ret["unit.heros_army"] = "第一单位武将";
	ret["unit.master_hero"] = "第一单位主将";
	ret["unit.cityno"] = "第一单位所在城市";
	ret["unit.side"] = "第一单位所在势力";
	ret["unit.x"] = "第一单位X坐标";
	ret["unit.y"] = "第一单位Y坐标";
	ret["second_unit.type"] = "第二单位兵种";
	ret["second_unit.heros_army"] = "第二单位武将";
	ret["second_unit.master_hero"] = "第二单位主将";
	ret["second_unit.cityno"] = "第二单位所在城市";
	ret["second_unit.side"] = "第二单位所在势力";
	ret["second_unit.x"] = "第二单位X坐标";
	ret["second_unit.y"] = "第二单位Y坐标";

	for (std::vector<tcommand*>::const_iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		const tcommand* cmd = *it;
		if (cmd == until) {
			return ret;
		}

		if (cmd->type_ == tcommand::SET_VARIABLE) {
			const tset_variable* derive = dynamic_cast<const tset_variable*>(cmd);
			std::map<std::string, std::string>::iterator find_it = ret.find(derive->name_);
			if (find_it != ret.end()) {
				std::stringstream strstr;
				strstr << find_it->second << " | " << derive->value_;
				ret[derive->name_] = strstr.str();
			} else {
				ret[derive->name_] = derive->value_;
			}

		} else if (cmd->type_ == tcommand::IF) {
			const tif* derive = dynamic_cast<const tif*>(cmd);
			if (cumulate_variables_internal(until, derive->then_, ret)) return ret;
			if (cumulate_variables_internal(until, derive->else_, ret)) return ret;
		}
	}
	return ret;
}

bool tevent::locate(const tcommand* until, std::vector<tcommand*>& commands, std::vector<tcommand*>** destination, int& pos)
{
	int index = 0;
	for (std::vector<tcommand*>::iterator it = commands.begin(); it != commands.end(); ++ it, index ++) {
		tcommand* cmd = *it;
		if (cmd == until) {
			*destination = &commands;
			pos = index;
			return true;;
		}
		if (cmd->type_ == tcommand::EVENT) {
			tevent* derive = dynamic_cast<tevent*>(cmd);
			if (locate(until, derive->commands_, destination, pos)) return true;

		} else if (cmd->type_ == tcommand::IF) {
			tif* derive = dynamic_cast<tif*>(cmd);
			if (locate(until, derive->then_, destination, pos)) return true;
			if (locate(until, derive->else_, destination, pos)) return true;
		}
	}
	return false;
}

void tevent::new_command(tcommand* new_cmd, HWND hdlgP)
{
	std::vector<tcommand*>* destination = NULL;
	int pos = -1;

	if (ns::clicked_command) {
		locate(ns::clicked_command, commands_, &destination, pos);
		if (ns::clicked_param == PARAM_THEN) {
			tif* derive = dynamic_cast<tif*>((*destination)[pos]);
			destination = &derive->then_;
			pos = -1;
		} else if (ns::clicked_param == PARAM_ELSE) {
			tif* derive = dynamic_cast<tif*>((*destination)[pos]); 
			destination = &derive->else_;
			pos = -1;
		}
		destination->insert(destination->begin() + (pos + 1), new_cmd);

	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &scenario.event_[ns::clicked_event]);
}

void tevent::erase_command(const tcommand* cmd, HWND hdlgP)
{
	std::vector<tcommand*>* destination = NULL;
	int pos = -1;

	locate(cmd, commands_, &destination, pos);

	delete (*destination)[pos];
	destination->erase(destination->begin() + pos);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &scenario.event_[ns::clicked_event]);
}

bool tevent::do_edit(HWND hwndtv)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	HWND hdlgP = GetParent(hwndtv);
	bool processed = true;

	if (ns::clicked_command) {
		ns::cumulate_variables = cumulate_variables(ns::clicked_command);
		if (ns::clicked_command->type_ == tcommand::SET_VARIABLE) {
			OnSetVariableEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::UNIT) {
			OnEventUnitEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::KILL) {
			OnEventKillEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::ENDLEVEL) {
			OnEventEndlevelEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::JOIN) {
			OnEventJoinEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::MODIFY_UNIT) {
			OnEventModifyUnitEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::MODIFY_SIDE) {
			OnEventModifySideEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::MODIFY_CITY) {
			OnEventModifyCityEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::MESSAGE) {
			OnEventMessageEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::PRINT) {
			OnEventPrintEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::LABEL) {
			OnEventLabelEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::ALLOW_UNDO) {
			processed = false;
		} else if (ns::clicked_command->type_ == tcommand::SIDEHEROS) {
			OnSideHerosEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::STORE_UNIT) {
			OnStoreUnitEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::RENAME) {
			OnEventRenameEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::OBJECTIVES) {
			OnEventObjectivesEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::FILTER) {
			ns::filter = dynamic_cast<tevent::tfilter_*>(ns::clicked_command);
			OnEventFilterBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::FILTER_HERO) {
			OnFilterHeroBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::EVENT) {
			tevent* derive = dynamic_cast<tevent*>(ns::clicked_command);
			derive->first_time_only_ = !derive->first_time_only_;
			scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &scenario.event_[ns::clicked_event]);

		} else if (ns::clicked_command->type_ == tcommand::CONDITION) {
			OnConditionEditBt(hdlgP);
		} else {
			processed = false;
		}

	} else {
		processed = false;
	}

	return processed;
}

void tevent::tfilter_::from_config(const config& cfg)
{
	if (!cfg) {
		return;
	}
	std::vector<std::string> vstr = utils::split(cfg["hp"].str());
	if (vstr.size() == 2) {
		int op = digit_op::find(vstr[0]);
		hp_ = std::make_pair(op, vstr[1]);
	} else {
		hp_ = std::make_pair(digit_op::NONE, null_str);
	}
	must_heros_ = cfg["must_heros"].str();

	int cityno = cfg["cityno"].to_int(-1);
	if (cityno >= 0) {
		int city_hero = HEROS_INVALID_NUMBER;
		if ((int)ns::cityno_map.size() >= cityno) {
			for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
				if (it->second == cityno) {
					city_hero = it->first;
					break;
				}
			}
		}
		city_ = city_hero;
	}

	side_ = cfg["side"].to_int(HEROS_INVALID_SIDE);
	if (side_ != HEROS_INVALID_SIDE && side_ >= 1) {
		side_ --;
	}

	controller_ = controller_tag::find(cfg["controller"].str());

	type_ = cfg["type"].str();

	family_ = cfg["family"].str();

	x_ = cfg["x"].str();
	y_ = cfg["y"].str();
	if (cfg.has_child("filter_location")) {
		filter_location_ = cfg.child("filter_location");
	}
}

void tevent::tfilter_::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_HP);
	hp_.first = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_HP), text, sizeof(text) / sizeof(text[0]));
	hp_.second = text;

	// must_heros
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS), text, _MAX_PATH);
	if (valuex::is_variable(text)) {
		must_heros_ = text;
	} else {
		strstr.str("");
		std::set<int> armied;
		const std::vector<std::string>& vsize = utils::split(text);
		for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
			int number = lexical_cast<int>(*it);
			if (armied.find(number) != armied.end()) {
				continue;
			}
			armied.insert(number);
			if (!strstr.str().empty()) {
				strstr << ", ";
			}
			strstr << number;
		}
		must_heros_ = strstr.str();
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CITY);
	city_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_SIDE);
	side_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CONTROLLER);
	controller_ = (controller_tag::CONTROLLER)ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_TYPE), text, sizeof(text) / sizeof(text[0]));
	type_ = text;

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_FAMILY), text, sizeof(text) / sizeof(text[0]));
	family_ = text;

	// X/Y
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_X), text, _MAX_PATH);
	if (text[0] == '\0') {
		int x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTFILTER_X));
		strstr.str("");
		if (x > 0) {
			strstr << x;
		}
		x_ = strstr.str();
	} else {
		x_ = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_Y), text, _MAX_PATH);
	if (text[0] == '\0') {
		int y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTFILTER_Y));
		strstr.str("");
		if (y > 0) {
			strstr << y;
		}
		y_ = strstr.str();
	} else {
		y_ = text;
	}

	// fitler location
	config cfg;
	filter_location_.clear();
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_FINDIN), text, _MAX_PATH);
	if (text[0] != '\0') {
		cfg["find_in"] = text;
	}
	if (!cfg.empty()) {
		if (Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTFILTER_NOT))) {
			filter_location_.add_child("not", cfg);
		} else {
			filter_location_ = cfg;
		}
	}
}

void tevent::tfilter_::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	description(hwndtv, branch, false);
}

std::string tevent::tfilter_::description(HWND hwndtv, HTREEITEM branch, bool newline) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HTREEITEM htvi;

	if (hp_.first != digit_op::NONE && !hp_.second.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "hp: " << digit_op::find(hp_.first) << " " << hp_.second;
		if (strstr.str()[strstr.str().size() - 1] == '%') {
			// fix below display bug.
			strstr << "%";
		}
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (!must_heros_.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "must_heros: ";
		if (valuex::is_variable(must_heros_)) {
			strstr << must_heros_;
		} else {
			std::vector<int> sstr = utils::to_vector_int(must_heros_);
			for (std::vector<int>::const_iterator it = sstr.begin(); it != sstr.end(); ++ it) {
				hero& h = gdmgr.heros_[*it];
				if (it == sstr.begin()) {
					strstr << h.name();
				} else {
					strstr << ", " << h.name();
				}
			}
		}
		if (hwndtv != NULL) {
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (city_ >= 0) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "city = ";
		
		if (city_ != HEROS_INVALID_NUMBER) {
			strstr << gdmgr.heros_[city_].name();
		} else {
			strstr << "(" << _("Roam") << ")";
		}
		if (hwndtv != NULL) {
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (side_ != HEROS_INVALID_SIDE) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "side = ";
		int leader = ns::_scenario[ns::current_scenario].side_[side_].leader_;
		strstr << gdmgr.heros_[leader].name();
		if (hwndtv != NULL) {
			strcpy(text, utf8_2_ansi(strstr.str().c_str()));
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (controller_ != controller_tag::NONE) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "controller = ";
		strstr << controller_tag::rfind(controller_);
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (!type_.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "type = " << type_;
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (family_tag::valid(family_)) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "family = " << family_;
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (!x_.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "x = " << x_;
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (!y_.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "y = " << y_;

		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	if (!filter_location_.empty()) {
		if (hwndtv != NULL) {
			htvi = TreeView_AddLeaf(hwndtv, branch);
			strstr.str("");
		}
		strstr << "[filter_location]: Existed";
		strcpy(text, strstr.str().c_str());
		if (hwndtv != NULL) {
			strcpy(text, strstr.str().c_str());
			TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
				0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		} else {
			strstr << (newline? "\r\n": "; ");
		}
	}

	return strstr.str();
}

void tevent::tfilter_::update_to_ui_special(HWND hdlgP) const
{
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HP), utf8_2_ansi(_("HP")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MUSTHEROS), utf8_2_ansi(_("Must heros")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SIDE), dgettext_2_ansi("wesnoth-lib", "Side"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CONTROLLER), dgettext_2_ansi("wesnoth-lib", "Controller"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TYPE), dgettext_2_ansi("wesnoth-lib", "troop^Type"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FAMILY), dgettext_2_ansi("wesnoth-lib", "Family"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_COOR), utf8_2_ansi(_("Coordinate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DEFINEDVARIABLES), utf8_2_ansi(_("Defined variables(Right click to popup menu)")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_HP);
	digit_op::ops_2_combo(hctl, hp_.first);
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_HP), hp_.second.c_str());

	int selected_row = 0;
	int index = 1;
	// must_heros
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS);
	Edit_SetText(hctl, must_heros_.c_str());

	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTFILTER_MUSTHEROS);
	LVITEM lvi;
	index = 0;
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it, index ++) {
		hero& h = *it;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// city
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CITY);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, -1);
	strstr.str("");
	strstr << "(" << utf8_2_ansi(_("Roam")) << ")";
	ComboBox_AddString(hctl, strstr.str().c_str());
	ComboBox_SetItemData(hctl, 1, HEROS_INVALID_NUMBER);
	if (city_ == HEROS_INVALID_NUMBER) {
		selected_row = 1;
	} else {
		selected_row = 0;
	}
	for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
		int city = it->first;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[city].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, city);
		if (city_ == city) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// side
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_SIDE);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_SIDE);
	selected_row = 0;
	for (index = 0; index < (int)ns::_scenario[ns::current_scenario].side_.size(); index ++) {
		const tside& side = ns::_scenario[ns::current_scenario].side_[index];
		hero& leader = gdmgr.heros_[side.leader_];
		ComboBox_AddString(hctl, utf8_2_ansi(leader.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, side.side_);
		if (side_ == side.side_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// controller
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CONTROLLER);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, controller_tag::NONE);
	selected_row = 0;
	for (std::set<controller_tag::CONTROLLER>::const_iterator it = controller_tag::game_running_tags.begin(); it != controller_tag::game_running_tags.end(); ++ it) {
		controller_tag::CONTROLLER controller = *it;
		ComboBox_AddString(hctl, controller_tag::rfind(controller).c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, controller);
		if (controller_ == controller) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_TYPE), type_.c_str());

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_FAMILY), family_.c_str());

	// X/Y
	bool is_variable = valuex::is_variable(x_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_X);
	if (is_variable) {
		Edit_SetText(hctl, x_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	map_location loc;
	if (!is_variable) {
		loc.x = valuex::to_int(x_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTFILTER_X);
	UpDown_SetPos(hctl, loc.x);

	is_variable = valuex::is_variable(y_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_Y);
	if (is_variable) {
		Edit_SetText(hctl, y_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	if (!is_variable) {
		loc.y = valuex::to_int(y_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTFILTER_Y);
	UpDown_SetPos(hctl, loc.y);

	// filter location
	if (const config& not = filter_location_.child("not")) {
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTFILTER_NOT), true);
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_FINDIN), not["find_in"].str().c_str());
	} else {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_FINDIN), filter_location_["find_in"].str().c_str());
	}
}

void tevent::tfilter_::generate(std::stringstream& strstr, const std::string& prefix) const
{
	if (hp_.first != digit_op::NONE && !hp_.second.empty()) {
		strstr << prefix << "hp = \"" << digit_op::find(hp_.first) << ", " << hp_.second << "\"\n";
	}
	if (!must_heros_.empty()) {
		strstr << prefix << "must_heros = " << must_heros_ << "\n";
	}
	if (city_ > 0) {
		strstr << prefix << "cityno = ";
		if (city_ != HEROS_INVALID_NUMBER) {
			std::map<int, int>::iterator find_it = ns::cityno_map.find(city_);
			if (find_it != ns::cityno_map.end()) {
				strstr << find_it->second;
			} else {
				strstr << "0";
			}
		} else {
			strstr << "0";
		}
		strstr << "\n";
	}
	if (side_ != HEROS_INVALID_SIDE) {
		strstr << prefix << "side = " << side_ + 1 << "\n";
	}
	if (controller_ != controller_tag::NONE) {
		strstr << prefix << "controller = " << controller_tag::rfind(controller_) << "\n";
	}
	if (!type_.empty()) {
		strstr << prefix << "type = " << type_ << "\n";
	}
	if (family_tag::valid(family_)) {
		strstr << prefix << "family = " << family_ << "\n";
	}
	if (!x_.empty() && !y_.empty()) {
		strstr << prefix << "x = " << x_ << "\n";
		strstr << prefix << "y = " << y_ << "\n";
	}
	if (!filter_location_.empty()) {
		config cfg;
		cfg.add_child("filter_location", filter_location_);
		::write(strstr, cfg, std::count(prefix.begin(), prefix.end(), '\t'));
	}
}

void tevent::tfilter_hero_::from_config(const config& cfg)
{
	number_ = HEROS_INVALID_NUMBER;

	if (!cfg) {
		return;
	}
	number_ = cfg["number"].to_int(HEROS_INVALID_NUMBER);
}

void tevent::tfilter_hero_::from_ui_special(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FILTERHERO_NUMBER);
	number_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tevent::tfilter_hero_::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HTREEITEM htvi;
	
	if (number_ != HEROS_INVALID_NUMBER) {
		htvi = TreeView_AddLeaf(hwndtv, branch);
		strstr.str("");
		strstr << "number: ";
		if (number_ != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[number_].name().c_str());
		}
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tfilter_hero_::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FILTERHERO_NUMBER);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	int selected_row = 0;
	int index = 1;
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it, index ++) {
		hero& h = gdmgr.heros_[it->first];
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, index, h.number_);
		if (h.number_ == number_) {
			selected_row = index;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);
}

void tevent::tfilter_hero_::generate(std::stringstream& strstr, const std::string& prefix) const
{
	if (number_ != HEROS_INVALID_NUMBER) {
		strstr << prefix << "number = " << number_ << "\n";
	}
}

ttranslatable_msgid::ttranslatable_msgid(int type)
	: tcommand(type)
	, value_("val_")
	, textdomain_()
{
}

void ttranslatable_msgid::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENT_VALUE_MSGID), text, _MAX_PATH);
	value_ = get_rid_of_return(text);
}

void ttranslatable_msgid::update_to_ui_special(HWND hdlgP) const
{
	utils::string_map symbols;
	std::stringstream strstr;

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));
	symbols["key"] = _("Domain");
	symbols["textdomain"] = ns::_main.textdomain_;
	strstr.str("");
	strstr << vgettext2("If value is translatable, set \"$key\", as if it is \"$textdomain\"", symbols);
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TIP), utf8_2_ansi(strstr.str().c_str()));


	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENT_VALUE_MSGID), value_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENT_TEXTDOMAIN), textdomain_.c_str());
	if (!value_.empty() && !textdomain_.empty()) {
		std::string str = dsgettext(textdomain_.c_str(), value_.c_str());
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENT_VALUE), utf8_2_ansi(str.c_str()));
	}
}

bool ttranslatable_msgid::on_command(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (id != IDC_ET_EVENT_VALUE_MSGID && id != IDC_ET_EVENT_TEXTDOMAIN) {
		return false;
	}
	if (codeNotify != EN_CHANGE) {
		return true;
	}

	char text[_MAX_PATH];
	std::stringstream strstr;

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));

	if (id == IDC_ET_EVENT_VALUE_MSGID) {
		value_ = text;
	} else if (id == IDC_ET_EVENT_TEXTDOMAIN) {
		textdomain_ = text;
	} else {
		return true;
	}
	
	strstr.str("");
	if (!value_.empty() && !textdomain_.empty()) {
		strstr << dsgettext(textdomain_.c_str(), value_.c_str());
	} else {
		strstr << value_;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENT_VALUE), utf8_2_ansi(strstr.str().c_str()));

	return true;
}

const std::string tevent::tset_variable::null_value = "";
const std::pair<long, long> tevent::tset_variable::null_rand = std::make_pair(0, 0);

std::vector<std::pair<std::string, std::string> > tevent::tset_variable::op_map;

tevent::tset_variable::tset_variable()
	: ttranslatable_msgid(SET_VARIABLE)
	, name_("name_")
	, op_("value")
{
	if (op_map.empty()) {
		op_map.push_back(std::make_pair("value", _("equal value")));
		op_map.push_back(std::make_pair("add", _("add value")));
		op_map.push_back(std::make_pair("sub", _("subtract value")));
		op_map.push_back(std::make_pair("multiply", _("multiply value")));
		op_map.push_back(std::make_pair("divide", _("divide value")));
		op_map.push_back(std::make_pair("modulo", _("modulo value")));
		op_map.push_back(std::make_pair("rand", _("evaluats one amone range")));
	}
}

void tevent::tset_variable::from_config(const config& cfg)
{
	name_ = cfg["name"].str();
	BOOST_FOREACH (const config::attribute &istrmap, cfg.attribute_range()) {
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
			if (it->first == istrmap.first) {
				op_ = istrmap.first;
				// value_ = istrmap.second;

				std::vector<t_string_base::trans_str> trans = istrmap.second.t_str().valuex();
				for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
					// only support one textdomain
					value_ = ti->str;
					textdomain_ = ti->td;
					break;
				}
				break;
			}
		}
	}
/*
	rand_ = null_rand;
	std::string rand = cfg["rand"].str();
	if (!rand.empty()) {
		std::stringstream ss(std::stringstream::in | std::stringstream::out);
		std::string::size_type tmp = rand.find("..");
		if (tmp != std::string::npos) {
			const std::string first = rand.substr(0, tmp);
			const std::string second = rand.substr(tmp + 2, rand.length());

			long low, high;
			ss << first + " " + second;
			ss >> low;
			ss >> high;
			ss.clear();
			if (low > high) {
				std::swap(low, high);
			}
			rand_ = std::make_pair(low, high);
		}
	}
*/
}

void tevent::tset_variable::from_ui_special(HWND hdlgP)
{
	ttranslatable_msgid::from_ui_special(hdlgP);

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_SETVARIABLE_OP);
	op_ = op_map[ComboBox_GetCurSel(hctl)].first;
}

void tevent::tset_variable::update_to_ui_event_edit(HWND hwndtv, HTREEITEM htvi_root) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hwndtv, htvi_root);
	strstr.str("");
	strstr << "set_variable";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hwndtv, htvi1);

	utils::string_map symbols;
	symbols["name"] = name_;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		if (it->first == op_) {
			symbols["op"] = it->second;
		}
	}
	if (textdomain_.empty() || value_.empty()) {
		symbols["value"] = value_;
	} else {
		symbols["value"] = dsgettext(textdomain_.c_str(), value_.c_str());
	}
	strstr.str("");
	strstr << utf8_2_ansi(vgettext2("Variable \"$name\" $op \"$value\"", symbols).c_str());
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tset_variable::update_to_ui_special(HWND hdlgP) const
{
	int selected_row = 0;
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_SETVARIABLE_OP);
	for (int index = 0; index < ComboBox_GetCount(hctl); index ++) {
		if (op_ == op_map[index].first) {
			selected_row = index;
			break;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_NAME), name_.c_str());
	ttranslatable_msgid::update_to_ui_special(hdlgP);
}

void tevent::tset_variable::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[set_variable]\n";
	strstr << prefix << "\tname = " << name_ << "\n";
	if (!textdomain_.empty()) {
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << textdomain_ << "\n";
		}
		strstr << prefix << "\t" << op_ << " = _\"" << value_ << "\"\n";
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << ns::_main.textdomain_ << "\n";
		}
	} else {
		strstr << prefix << "\t" << op_ << " = " << value_ << "\n";
	}
	strstr << prefix << "[/set_variable]\n";
}

void tevent::tkill::from_config(const config& cfg)
{
	master_hero_ = cfg["hero"].str();
	side_ = cfg["a_side"].str();
	direct_hero_ = cfg["direct_hero"].to_bool();
}

void tevent::tkill::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	// hero
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_HERO), text, _MAX_PATH);
	if (valuex::is_variable(text)) {
		master_hero_ = text;
	} else {
		strstr.str("");
		std::set<int> armied;
		const std::vector<std::string>& vsize = utils::split(text);
		for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
			int number = lexical_cast<int>(*it);
			if (armied.find(number) != armied.end()) {
				continue;
			}
			armied.insert(number);
			if (!strstr.str().empty()) {
				strstr << ",";
			}
			strstr << number;
		}
		master_hero_ = strstr.str();
	}

	// a_side
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_SIDE), text, _MAX_PATH);
	side_ = text;

	// direct hero
	direct_hero_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTKILL_DIRECTHERO));
}

void tevent::tkill::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "kill";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "hero = ";
	if (valuex::is_variable(master_hero_)) {
		strstr << master_hero_;
	} else {
		std::set<int> sstr = utils::to_set_int(master_hero_);
		for (std::set<int>::const_iterator it = sstr.begin(); it != sstr.end(); ++ it) {
			hero& h = gdmgr.heros_[*it];
			if (it == sstr.begin()) {
				strstr << utf8_2_ansi(h.name().c_str());
			} else {
				strstr << "," << utf8_2_ansi(h.name().c_str());
			}
		}
	}

	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	// a_side
	if (!side_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "a_side = ";
		if (valuex::is_variable(side_)) {
			strstr << side_;
		} else {
			int side_index = valuex::to_int(side_, 0) - 1;
			if ((int)scenario.side_.size() > side_index) {
				int leader = scenario.side_[side_index].leader_;
				strstr << utf8_2_ansi(gdmgr.heros_[leader].name().c_str());
			}
		}
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// direct hero
	if (direct_hero_) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "direct_hero = yes";
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tkill::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;
	
	bool is_variable = valuex::is_variable(master_hero_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTKILL_HERO);
	Edit_SetText(hctl, master_hero_.c_str());

	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
	ListView_DeleteAllItems(hctl);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		if (!it->second.allocated(HEROS_INVALID_SIDE, false)) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];
		candidate_hero::fill_row(hctl, h);
	}

	// side
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_SIDE), side_.c_str());

	LVITEM lvi;
	char text[_MAX_PATH];
	int index = 0;
	const std::vector<tside>& side = ns::_scenario[ns::current_scenario].side_;
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTKILL_SIDE);
	for (std::vector<tside>::const_iterator it = side.begin(); it != side.end(); ++ it, index ++) {
		int column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem = index;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << (it->side_ + 1);
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)(it->side_ + 1);
		ListView_InsertItem(hctl, &lvi);

		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column;
		strstr.str("");
		strstr << gdmgr.heros_[it->leader_].name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// direct hero
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTKILL_DIRECTHERO), direct_hero_);
}

void tevent::tkill::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[kill]\n";
	strstr << prefix << "\thero = " << master_hero_ << "\n";
	if (!side_.empty()) {
		strstr << prefix << "\ta_side = " << side_ << "\n";
	}
	if (direct_hero_) {
		strstr << prefix << "\tdirect_hero = yes\n";
	}
	strstr << prefix << "[/kill]\n";
}


void tevent::tendlevel::from_config(const config& cfg)
{
	result_ = cfg["result"];
}

void tevent::tendlevel::from_ui_special(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTENDLEVEL_RESULT);
	// master_hero
	int sel = ComboBox_GetCurSel(hctl);
	if (sel == 0) {
		result_ = "victory";
	} else {
		result_ = "defeat";
	}
}

void tevent::tendlevel::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "endlevel";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "result = ";
	strstr << result_;
	
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tendlevel::update_to_ui_special(HWND hdlgP) const
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTENDLEVEL_RESULT);
	ComboBox_ResetContent(hctl);
	std::map<int, std::string> result_map;
	result_map.insert(std::make_pair(0, "victory"));
	result_map.insert(std::make_pair(1, "defeat"));
	for (std::map<int, std::string>::const_iterator it = result_map.begin(); it != result_map.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (result_ == it->second) {
			ComboBox_SetCurSel(hctl, ComboBox_GetCount(hctl) - 1);
		}
	}
}

void tevent::tjoin::from_config(const config& cfg)
{
	master_hero_ = cfg["to"].str();
	join_ = cfg["join"].str();
}

void tevent::tjoin::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl;

	// master_hero
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_MASTERHERO), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_MASTERHERO);
		strstr.str("");
		strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		master_hero_ = strstr.str();
	} else {
		master_hero_ = text;
	}
	// join
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_JOIN), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_JOIN);
		strstr.str("");
		strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		join_ = strstr.str();
	} else {
		join_ = text;
	}
}

void tevent::tjoin::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "join";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "to = ";
	if (valuex::is_variable(master_hero_)) {
		strstr << master_hero_;
	} else {
		int master_hero = valuex::to_int(master_hero_, HEROS_INVALID_NUMBER);
		if (master_hero != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[master_hero].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "join = ";
	if (valuex::is_variable(join_)) {
		strstr << join_;
	} else {
		int join = valuex::to_int(join_, HEROS_INVALID_NUMBER);
		if (join != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[join].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tjoin::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;

	// master_hero
	bool is_variable = valuex::is_variable(master_hero_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_MASTERHERO);
	if (is_variable) {
		Edit_SetText(hctl, master_hero_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	int master_hero;
	if (!is_variable) {
		master_hero = valuex::to_int(master_hero_, -1);
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_MASTERHERO);
	ComboBox_ResetContent(hctl);
	int selected_row = 0;
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
		if (!is_variable && master_hero == h.number_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// join
	is_variable = valuex::is_variable(join_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_JOIN);
	if (is_variable) {
		Edit_SetText(hctl, join_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	int join;
	if (!is_variable) {
		join = valuex::to_int(join_, -1);
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_JOIN);
	ComboBox_ResetContent(hctl);
	selected_row = 0;
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
		if (!is_variable && join == h.number_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);
}

tevent::tunit::tunit()
	: tcommand(UNIT)
	, type_(editor_config::troop_utypes[0].first)
	, heros_army_()
	, side_("0")
	, city_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
	, x_("1")
	, y_("1")
	, state_()
{}

void tevent::tunit::from_config(const config& cfg)
{
	type_ = cfg["type"].str();
	if (!valuex::is_variable(type_)) {
		if (!unit_types.find(type_)) {
			type_ = editor_config::troop_utypes[0].first;
		}
	}
	heros_army_ = cfg["heros_army"].str();
	side_ = cfg["side"].str();
	if (!valuex::is_variable(side_)) {
		int side = valuex::to_int(side_, 1);
		side --;
		if ((int)ns::_scenario[ns::current_scenario].side_.size() <= side) {
			side = 0;
		}
		side_ = lexical_cast_default<std::string>(side);
	}
	city_ = cfg["cityno"].str();
	if (!valuex::is_variable(city_)) {
		int cityno = valuex::to_int(city_, 0);
		int city_hero = HEROS_INVALID_NUMBER;
		if ((int)ns::cityno_map.size() >= cityno) {
			for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
				if (it->second == cityno) {
					city_hero = it->first;
					break;
				}
			}
		}
		city_ = lexical_cast_default<std::string>(city_hero);
	}
	x_ = cfg["x"].str();
	y_ = cfg["y"].str();
	// traits
	std::vector<std::string> vstr = utils::split(cfg["traits"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		traits_.insert(*i);
	}

	// state
	vstr = utils::split(cfg["states"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		state_.insert(*i);
	}
}

void tevent::tunit::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl;

	// type
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_TYPE), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_TYPE);
		type_ = editor_config::troop_utypes[ComboBox_GetCurSel(hctl)].first;
	} else {
		type_ = text;
	}

	// heros_army
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY), text, _MAX_PATH);
	if (valuex::is_variable(text)) {
		heros_army_ = text;
	} else {
		strstr.str("");
		std::set<int> armied;
		const std::vector<std::string>& vsize = utils::split(text);
		for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
			int number = lexical_cast<int>(*it);
			if (armied.find(number) != armied.end()) {
				continue;
			}
			armied.insert(number);
			if (!strstr.str().empty()) {
				strstr << ",";
			}
			strstr << number;
		}
		heros_army_ = strstr.str();
	}

	// city/side
	int city_hero = HEROS_INVALID_NUMBER;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_CITY), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_CITY);
		city_hero = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		strstr.str("");
		strstr << city_hero;
		city_ = strstr.str();
	} else {
		city_ = text;
	}
	int side_index = -1;
	if (city_hero != HEROS_INVALID_NUMBER) {
		// find city hero from side
		tscenario& scenario = ns::_scenario[ns::current_scenario];
		for (std::vector<tside>::const_iterator it = scenario.side_.begin(); it != scenario.side_.end(); ++ it) {
			for (std::vector<tside::tcity>::const_iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
				if (it2->heros_army_[0] == city_hero) {
					side_index = it->side_;
					break;
				}
			}
			if (side_index != -1) {
				break;
			}
		}
	}
	strstr.str("");
	if (side_index != -1) {
		strstr << side_index;
		side_ = strstr.str();
	} else {
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_SIDE), text, _MAX_PATH);
		if (text[0] == '\0') {
			hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_SIDE);
			strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
			side_ = strstr.str();
		} else {
			side_ = text;
		}
	}

	// X/Y
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARX), text, _MAX_PATH);
	if (text[0] == '\0') {
		int x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_X));
		strstr.str("");
		strstr << x;
		x_ = strstr.str();
	} else {
		x_ = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARY), text, _MAX_PATH);
	if (text[0] == '\0') {
		int y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_Y));
		strstr.str("");
		strstr << y;
		y_ = strstr.str();
	} else {
		y_ = text;
	}

	// traits
	traits_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_TRAIT);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			traits_.insert(editor_config::troop_traits[idx].first);
		}
	}
	// state
	state_.clear();
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_STATE), text, _MAX_PATH);
	if (!valuex::is_variable(text)) {
		std::vector<std::string> vstr = utils::split(text);
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			if (ustate_tag::find(*it) != ustate_tag::NONE) {
				state_.insert(*it);
			}
		}
	} else {
		state_.insert(text);
	}
}

void tevent::tunit::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "unit";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "type = ";
	if (valuex::is_variable(type_)) {
		strstr << type_;
	} else {
		strstr << utf8_2_ansi(unit_types.find(type_)->type_name().c_str());
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "heros_army = ";
	if (valuex::is_variable(heros_army_)) {
		strstr << heros_army_;
	} else {
		std::vector<int> sstr = utils::to_vector_int(heros_army_);
		for (std::vector<int>::const_iterator it = sstr.begin(); it != sstr.end(); ++ it) {
			hero& h = gdmgr.heros_[*it];
			if (it == sstr.begin()) {
				strstr << utf8_2_ansi(h.name().c_str());
			} else {
				strstr << "," << utf8_2_ansi(h.name().c_str());
			}
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "side = ";
	if (valuex::is_variable(side_)) {
		strstr << side_;
	} else {
		int side_index = valuex::to_int(side_, 0);
		if ((int)scenario.side_.size() > side_index) {
			int leader = scenario.side_[side_index].leader_;
			strstr << utf8_2_ansi(gdmgr.heros_[leader].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "city = ";
	if (valuex::is_variable(city_)) {
		strstr << city_;
	} else {
		int city_hero = valuex::to_int(city_, HEROS_INVALID_NUMBER);
		if (city_hero != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[city_hero].name().c_str());
		} else {
			strstr << "(" << utf8_2_ansi(_("Roam")) << ")";
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "x = " << x_;
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "y = " << y_;
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	if (!traits_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "traits = ";
		for (std::set<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
			if (it != traits_.begin()) {
				strstr << ",";
			}
			strstr << *it;
		}
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	if (!state_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "states = ";
		if (!valuex::is_variable(*state_.begin())) {
			int n = 0;
			for (std::set<std::string>::const_iterator it = state_.begin(); it != state_.end(); ++ it) {
				if (ustate_tag::find(*it) == ustate_tag::NONE) {
					continue;
				}
				if (n ++) {
					strstr << ",";
				}
				strstr << *it;
			}
		} else {
			strstr << (*state_.begin());
		}
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
	
}

void tevent::tunit::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;
	char text[_MAX_PATH];

	// type
	bool is_variable = valuex::is_variable(type_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_TYPE);
	if (is_variable) {
		Edit_SetText(hctl, type_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_TYPE);
	ComboBox_ResetContent(hctl);
	int index = 0;
	int selected_row = 0;
	for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = editor_config::troop_utypes.begin(); it != editor_config::troop_utypes.end(); ++ it) {
		const unit_type* ut = it->second;
		strstr.str("");
		strstr << utf8_2_ansi(ut->type_name().c_str());
		strstr << "(" << ut->level() << "级" << utf8_2_ansi(hero::arms_str(ut->arms()).c_str()) << ")";
		ComboBox_AddString(hctl, strstr.str().c_str());
		if (!is_variable && type_ == it->first) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// heros_army
	is_variable = valuex::is_variable(heros_army_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY);
	Edit_SetText(hctl, heros_army_.c_str());

	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	LVITEM lvi;
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)it->first;
		ListView_InsertItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// trait
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_TRAIT);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::vector<std::pair<std::string, const config*> >::const_iterator it = editor_config::troop_traits.begin(); it != editor_config::troop_traits.end(); ++ it) {
		const config& cfg = *(it->second);
		
		std::string name = cfg["male_name"];
		if (name.empty()) {
			name = cfg["female_name"];
		}
		if (name.empty()) {
			name = cfg["name"];
		}
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 名称
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << name << "(" << cfg["id"].str() << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)0;
		ListView_InsertItem(hctl, &lvi);

		// 描述
		std::string description = cfg["description"];

		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(description.c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		if (std::find(traits_.begin(), traits_.end(), it->first) != traits_.end()) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		} else {
			ListView_SetCheckState(hctl, lvi.iItem, FALSE);
		}
	}
	strstr.str("");
	strstr << dsgettext("wesnoth-lib", "Traits") << "(" << traits_.size() << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TRAITS), utf8_2_ansi(strstr.str().c_str()));

	// city/side
	is_variable = valuex::is_variable(city_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_CITY);
	if (is_variable) {
		Edit_SetText(hctl, city_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	int city_hero;
	if (!is_variable) {
		city_hero = valuex::to_int(city_, -1);
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_CITY);
	ComboBox_ResetContent(hctl);
	strstr.str("");
	strstr << "(" << utf8_2_ansi(_("Roam")) << ")";
	ComboBox_AddString(hctl, strstr.str().c_str());
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	selected_row = 0;
	for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
		int city = it->first;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[city].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, city);
		if (!is_variable && city_hero == city) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	is_variable = valuex::is_variable(side_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_SIDE);
	if (is_variable) {
		Edit_SetText(hctl, side_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	int side_index;
	if (!is_variable) {
		side_index = valuex::to_int(side_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_SIDE);
	ComboBox_ResetContent(hctl);
	selected_row = 0;
	const std::vector<tside>& side = ns::_scenario[ns::current_scenario].side_;
	for (std::vector<tside>::const_iterator it = side.begin(); it != side.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[it->leader_].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->side_);
		if (!is_variable && side_index == it->side_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// X/Y
	is_variable = valuex::is_variable(x_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARX);
	if (is_variable) {
		Edit_SetText(hctl, x_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	map_location loc;
	if (!is_variable) {
		loc.x = valuex::to_int(x_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_X);
	UpDown_SetPos(hctl, loc.x);

	is_variable = valuex::is_variable(y_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARY);
	if (is_variable) {
		Edit_SetText(hctl, y_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	if (!is_variable) {
		loc.y = valuex::to_int(y_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_Y);
	UpDown_SetPos(hctl, loc.y);

	// state
	strstr.str("");
	if (!state_.empty()) {
		if (!valuex::is_variable(*state_.begin())) {
			for (std::set<std::string>::const_iterator it = state_.begin(); it != state_.end(); ++ it) {
				if (ustate_tag::find(*it) == ustate_tag::NONE) {
					continue;
				}
				if (!strstr.str().empty()) {
					strstr << ",";
				}
				strstr << *it;
			}
		} else {
			strstr << *state_.begin();
		}
	}
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_STATE);
	Edit_SetText(hctl, strstr.str().c_str());
}

void tevent::tunit::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[unit]\n";

	strstr << prefix << "\ttype = " << type_ << "\n";
	strstr << prefix << "\theros_army = " << heros_army_ << "\n";

	strstr << prefix << "\tside = ";
	if (valuex::is_variable(side_)) {
		strstr << side_;
	} else {
		strstr << valuex::to_int(side_, 0) + 1;
	}
	strstr << "\n";

	strstr << prefix << "\tcityno = ";
	if (valuex::is_variable(city_)) {
		strstr << city_;
	} else {
		int number = valuex::to_int(city_, HEROS_INVALID_NUMBER);
		if (number != HEROS_INVALID_NUMBER) {
			const std::map<int, int>::const_iterator find_it = ns::cityno_map.find(number);
			if (find_it != ns::cityno_map.end()) {
				strstr << find_it->second;
			} else {
				strstr << "0";
			}
		} else {
			strstr << "0";
		}
	}
	strstr << "\n";

	// strstr << prefix << "\tattacks_left = 0\n";

	strstr << prefix << "\tx,y = ";
	strstr << x_ << ", " << y_ << "\n";

	if (!traits_.empty()) {
		strstr << prefix << "\ttraits = ";
		for (std::set<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
			if (it != traits_.begin()) {
				strstr << ",";
			}
			strstr << *it;
		}
		strstr << "\n";
	}

	if (!state_.empty()) {
		strstr << prefix << "\tstates = ";
		if (!valuex::is_variable(*state_.begin())) {
			for (std::set<std::string>::const_iterator it = state_.begin(); it != state_.end(); ++ it) {
				if (it != state_.begin()) {
					strstr << ",";
				}
				strstr << *it;
			}
		} else {
			strstr << *state_.begin();
		}
		strstr << "\n";
	}

	strstr << prefix << "[/unit]\n";
}

void tevent::tmodify_unit::from_config(const config& cfg)
{
	master_hero_ = cfg["hero"];
	effect_.clear();
	BOOST_FOREACH (const config &c, cfg.child_range("effect")) {
		effect_.add_child("effect", c);
	}
}

config edit_ctrl_2_cfg(const std::string& str)
{
	config cfg;
	try {
		::read(cfg, str);
	} catch(config::error& e) {
		posix_print_mb(e.message.c_str());
		cfg = config();
	}
	return cfg;
}

void tevent::tmodify_unit::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl;

	// master_hero
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_MASTERHERO), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYUNIT_MASTERHERO);
		strstr.str("");
		strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		master_hero_ = strstr.str();
	} else {
		master_hero_ = text;
	}
	// effect
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_EFFECT), text, _MAX_PATH);
	effect_ = edit_ctrl_2_cfg(text);
	int index = 0;
	BOOST_FOREACH (const config &c, effect_.child_range("effect")) {
		index ++;
		const std::string& apply_to = c["apply_to"].str();
		if (apply_to_tag::find(apply_to) == apply_to_tag::NONE) {
			strstr.str("");
			strstr << "#" << index << "[effect], " << "Unsupport apply to: " << apply_to;
			posix_print_mb(strstr.str().c_str());
		}
	}
}

void tevent::tmodify_unit::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "modify_unit2";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "hero = ";
	if (valuex::is_variable(master_hero_)) {
		strstr << master_hero_;
	} else {
		int number = valuex::to_int(master_hero_, HEROS_INVALID_NUMBER);
		if ((int)gdmgr.heros_.size() > number) {
			strstr << utf8_2_ansi(gdmgr.heros_[number].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	// effect
	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "effect = ";
	int index = 0;
	BOOST_FOREACH (const config &c, effect_.child_range("effect")) {
		if (index ++) {
			strstr << ", ";
		}
		strstr << c["apply_to"].str();
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

std::string cfg_2_edit_ctrl(const config& cfg)
{
	std::stringstream strstr;
	::write(strstr, cfg);

	const std::vector<std::string> vstr = utils::split(strstr.str(), '\n', 0);
	strstr.str("");
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		if (it != vstr.begin()) {
			if (it->empty() || it->at(it->size() - 1) != '\r') {
				strstr << '\r';
			}
			strstr << '\n';
		}
		strstr << *it;
	}
	return strstr.str();
}

void tevent::tmodify_unit::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;
	
	bool is_variable = valuex::is_variable(master_hero_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_MASTERHERO);
	if (is_variable) {
		Edit_SetText(hctl, master_hero_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYUNIT_MASTERHERO);
	ComboBox_ResetContent(hctl);
	int selected_row = 0;
	int master_hero = HEROS_INVALID_NUMBER;
	if (!is_variable) {
		master_hero = valuex::to_int(master_hero_, HEROS_INVALID_NUMBER);
	}
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		hero& h = *it;
		if (h.number_ < hero::number_city_min) {
			continue;
		}
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
		if (!is_variable && h.number_ == master_hero) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// [effect]
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_EFFECT), cfg_2_edit_ctrl(effect_).c_str());
}

void tevent::tmodify_unit::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[modify_unit2]\n";
	strstr << prefix << "\thero = " << master_hero_ << "\n";
	::write(strstr, effect_, std::count(prefix.begin(), prefix.end(), '\t') + 1);
	strstr << prefix << "[/modify_unit2]\n";
}

void tevent::tmodify_side::from_config(const config& cfg)
{
	side_ = cfg["side"].to_int(1) - 1;
	leader_ = cfg["leader"].to_int(HEROS_INVALID_NUMBER);
	controller_ = controller_tag::find(cfg["controller"].str());
	gold_ = cfg["gold"].to_int(-1);
	income_ = cfg["income"].to_int(-1);

	agree_.clear();
	std::vector<std::string> vstr = utils::split(cfg["agree"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		int side = lexical_cast_default<int>(*i);
		if (side <= 0) {
			// program error
			continue;
		}
		agree_.insert(side - 1);
	}
	terminate_.clear();
	vstr = utils::split(cfg["terminate"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		int side = lexical_cast_default<int>(*i);
		if (side <= 0) {
			// program error
			continue;
		}
		terminate_.insert(side - 1);
	}

	exclude_human_ = cfg["exclude_human"].to_bool();

	technology_.clear();
	vstr = utils::split(cfg["technology"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		technology_.insert(*it);
	}
}

void tevent::tmodify_side::from_ui_special(HWND hdlgP)
{
	// side
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_SIDE);
	side_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_LEADER);
	leader_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_CONTROLLER);
	controller_ = (controller_tag::CONTROLLER)ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	// gold
	gold_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_GOLD));
	// income
	income_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_INCOME));

	// agree
	agree_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_AGREE);
	LVITEM lvi;
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_PARAM;
		lvi.iSubItem = 0;
		ListView_GetItem(hctl, &lvi);
		agree_.insert(lvi.lParam);
	}

	// terminate
	terminate_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_TERMINATE);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_PARAM;
		lvi.iSubItem = 0;
		ListView_GetItem(hctl, &lvi);
		terminate_.insert(lvi.lParam);
	}

	exclude_human_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTMODIFYSIDE_EXCLUDEHUMAN))? true: false;
}

void tevent::tmodify_side::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "modify_side";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "side = ";
	if (side_ != HEROS_INVALID_SIDE) {
		strstr << gdmgr.heros_[sides[side_].leader_].name();
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	// leader
	if (leader_ != HEROS_INVALID_NUMBER) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "leader = ";
		strstr << gdmgr.heros_[leader_].name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	if (controller_ != controller_tag::NONE) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "controller = ";
		strstr << controller_tag::rfind(controller_);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// gold
	if (gold_ != -1) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "gold = " << gold_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// income
	if (income_ != -1) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "income = " << income_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// agree
	if (!agree_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "agree = ";
		for (std::set<int>::const_iterator it = agree_.begin(); it != agree_.end(); ++ it) {
			if (it != agree_.begin()) {
				strstr << ", ";
			}
			strstr << gdmgr.heros_[sides[*it].leader_].name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// terminate
	if (!terminate_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "terminate = ";
		for (std::set<int>::const_iterator it = terminate_.begin(); it != terminate_.end(); ++ it) {
			if (it != terminate_.begin()) {
				strstr << ", ";
			}
			strstr << gdmgr.heros_[sides[*it].leader_].name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	if (!agree_.empty() || !terminate_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "exclude_human = " << (exclude_human_? "yes": "no");
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// technology
	if (!technology_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "technology = ";
		for (std::set<std::string>::const_iterator it = technology_.begin(); it != technology_.end(); ++ it) {
			if (it != technology_.begin()) {
				strstr << ", ";
			}
			strstr << unit_types.technology(*it).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tmodify_side::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_SIDE);
	int cursel = -1;
	for (std::vector<tside>::const_iterator it = sides.begin(); it != sides.end(); ++ it) {
		const tside& side = *it;
		strstr.str("");
		strstr << gdmgr.heros_[side.leader_].name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, side.side_);
		if (cursel == -1 && side_ == side.side_) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (cursel == -1) {
		cursel = 0;
	}
	ComboBox_SetCurSel(hctl, cursel);

	// leader
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_LEADER);
	cursel = -1;
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		hero& h = *it;
		if (h.number_ < hero::number_normal_min) {
			continue;
		}
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, number);
		if (cursel == -1 && leader_ == number) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (cursel == -1) {
		cursel = 0;
	}
	ComboBox_SetCurSel(hctl, cursel);

	// controller
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYSIDE_CONTROLLER);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, controller_tag::NONE);
	cursel = 0;
	for (std::set<controller_tag::CONTROLLER>::const_iterator it = controller_tag::game_running_tags.begin(); it != controller_tag::game_running_tags.end(); ++ it) {
		controller_tag::CONTROLLER controller = *it;
		ComboBox_AddString(hctl, controller_tag::rfind(controller).c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, controller);
		if (controller_ == controller) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, cursel);

	// gold
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_GOLD);
	UpDown_SetPos(hctl, gold_);

	// income
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_INCOME);
	UpDown_SetPos(hctl, income_);

	update_to_ui_ally(hdlgP);

	// exclude human
	hctl = GetDlgItem(hdlgP, IDC_CHK_EVENTMODIFYSIDE_EXCLUDEHUMAN);
	Button_SetCheck(hctl, exclude_human_);

	// technology
	explorer_technology::update_to_ui_special(hdlgP, explorer_technology::MODIFY_SIDE);
}

void tevent::tmodify_side::update_to_ui_ally(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	char text[_MAX_PATH];

	LVITEM lvi;
	int column;
	int index = 0;
	// agree
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_AGREE);
	ListView_DeleteAllItems(hctl);
	for (std::set<int>::const_iterator it = agree_.begin(); it != agree_.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		column = 0;

		// name
		lvi.iItem = index ++;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (sides[*it].leader_ != HEROS_INVALID_NUMBER) {
			strstr << gdmgr.heros_[sides[*it].leader_].name();
		}
		strstr << "(" << *it + 1 << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)*it;
		ListView_InsertItem(hctl, &lvi);
	}

	// terminate
	index = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_TERMINATE);
	ListView_DeleteAllItems(hctl);
	for (std::set<int>::const_iterator it = terminate_.begin(); it != terminate_.end(); ++ it) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		column = 0;

		// name
		lvi.iItem = index ++;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (sides[*it].leader_ != HEROS_INVALID_NUMBER) {
			strstr << gdmgr.heros_[sides[*it].leader_].name();
		}
		strstr << "(" << *it + 1 << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)*it;
		ListView_InsertItem(hctl, &lvi);
	}

	// ally
	index = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_SIDE);
	ListView_DeleteAllItems(hctl);
	for (size_t i = 0; i < sides.size(); i ++) {
		if (i == side_) {
			continue;
		}
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		column = 0;

		// name
		lvi.iItem = index ++;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (sides[i].leader_ != HEROS_INVALID_NUMBER) {
			strstr << gdmgr.heros_[sides[i].leader_].name();
		}
		strstr << "(" << i + 1 << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)i;
		ListView_InsertItem(hctl, &lvi);
	}
}

void tevent::tmodify_side::update_ally(HWND hdlgP, int side, bool agree, bool insert)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, agree? IDC_LV_EVENTMODIFYSIDE_AGREE: IDC_LV_EVENTMODIFYSIDE_TERMINATE);
	std::set<int>* list = agree? &agree_: &terminate_;
	if (insert) {
		list->insert(side);
	} else {
		list->erase(side);
	}

	LVITEM lvi;
	ListView_DeleteAllItems(hctl);
	int index = 0;
	for (std::set<int>::const_iterator it = list->begin(); it != list->end(); ++ it) {
		int side = *it;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// name
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << gdmgr.heros_[sides[side].leader_].name();
		strstr << "(" << side + 1 << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)side;
		ListView_InsertItem(hctl, &lvi);
	}
}

void tevent::tmodify_side::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[modify_side]\n";
	strstr << prefix << "\tside = " << (side_ + 1) << "\n";
	if (leader_ != HEROS_INVALID_NUMBER) {
		strstr << prefix << "\tleader = " << leader_ << "\n";
	}
	if (controller_ != controller_tag::NONE) {
		strstr << prefix << "\tcontroller = " << controller_tag::rfind(controller_) << "\n";
	}
	if (gold_ != -1) {
		strstr << prefix << "\tgold = " << gold_ << "\n";
	}
	if (income_ != -1) {
		strstr << prefix << "\tincome = " << income_ << "\n";
	}
	strstr << prefix << "\tagree = ";
	for (std::set<int>::const_iterator it = agree_.begin(); it != agree_.end(); ++ it) {
		if (it != agree_.begin()) {
			strstr << ", ";
		}
		strstr << (*it + 1);
	}
	strstr << "\n";
	strstr << prefix << "\tterminate = ";
	for (std::set<int>::const_iterator it = terminate_.begin(); it != terminate_.end(); ++ it) {
		if (it != terminate_.begin()) {
			strstr << ", ";
		}
		strstr << (*it + 1);
	}
	strstr << "\n";

	if ((!agree_.empty() || !terminate_.empty()) && exclude_human_) {
		strstr << prefix << "\texclude_human = yes\n";
	}

	if (!technology_.empty()) {
		strstr << prefix << "\ttechnology = ";
		for (std::set<std::string>::const_iterator it = technology_.begin(); it != technology_.end(); ++ it) {
			if (it != technology_.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		strstr << "\n";
	}

	strstr << prefix << "[/modify_side]\n";
}

void tevent::tmodify_city::from_config(const config& cfg)
{
	std::string str = cfg["city"].str();
	if (valuex::is_variable(str)) {
		city_ = std::make_pair(str, HEROS_INVALID_NUMBER);
	} else {
		int number = valuex::to_int(str, 0);
		std::map<int, int>::const_iterator it = ns::cityno_map.find(number);
		if (it == ns::cityno_map.end()) {
			number = HEROS_INVALID_NUMBER;	
		}
		city_ = std::make_pair(null_str, number);
	}
	
	soldiers_ = cfg["soldiers"].to_int(-1);

	std::vector<std::string> vstr = utils::split(cfg["service"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		int number = lexical_cast_default<int>(*i);
		service_.insert(number);
	}
}

void tevent::tmodify_city::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	HWND hctl;

	// city
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYCITY_CITY), text, _MAX_PATH);
	if (valuex::is_variable(text)) {
		city_ = std::make_pair(text, HEROS_INVALID_NUMBER);
	} else {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYCITY_CITY);
		city_ = std::make_pair(null_str, ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl)));
	}
	

	// soldiers
	soldiers_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYCITY_SOLDIERS));

	// service
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYCITY_SERVICE);
	LVITEM lvi;
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_PARAM;
		lvi.iSubItem = 0;
		ListView_GetItem(hctl, &lvi);
		service_.insert(lvi.lParam);
	}
}

void tevent::tmodify_city::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "modify_city";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "city = ";
	if (!city_.first.empty()) {
		strstr << city_.first;
	} else {
		strstr << gdmgr.heros_[city_.second].name();
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	// soldiers
	if (soldiers_ != -1) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "soldiers = " << soldiers_;
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	// service
	if (!service_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "service = ";
		for (std::set<int>::const_iterator it = service_.begin(); it != service_.end(); ++ it) {
			if (it != service_.begin()) {
				strstr << ", ";
			}
			strstr << gdmgr.heros_[*it].name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tmodify_city::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::stringstream strstr;
	char text[_MAX_PATH];
	
	if (!city_.first.empty()) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYCITY_CITY), city_.first.c_str());
	}
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYCITY_CITY);
	int cursel = -1;
	ComboBox_ResetContent(hctl);
	for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
		strstr.str("");
		strstr << gdmgr.heros_[it->first].name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (cursel == -1 && city_.first.empty() && city_.second == it->first) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	if (cursel == -1) {
		cursel = 0;
	}
	ComboBox_SetCurSel(hctl, cursel);

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYCITY_SOLDIERS);
	UpDown_SetPos(hctl, soldiers_);

	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
	ListView_DeleteAllItems(hctl);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		if (!it->second.allocated(HEROS_INVALID_SIDE, false)) {
			continue;
		}
		if (service_.find(it->first) != service_.end()) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];
		candidate_hero::fill_row(hctl, h);
	}

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYCITY_SERVICE);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	for (std::set<int>::const_iterator it = service_.begin(); it != service_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// 相性
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		int value = h.base_catalog_;
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tevent::tmodify_city::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[modify_city]\n";
	strstr << prefix << "\tcity = ";
	if (!city_.first.empty()) {
		strstr << city_.first;
	} else {
		strstr << city_.second;
	}
	strstr << "\n";
	if (soldiers_ != -1) {
		strstr << prefix << "\tsoldiers = " << soldiers_ << "\n";
	}
	if (!service_.empty()) {
		strstr << prefix << "\tservice = ";
		for (std::set<int>::const_iterator it = service_.begin(); it != service_.end(); ++ it) {
			if (it != service_.begin()) {
				strstr << ", ";
			}
			strstr << *it;
		}
		strstr << "\n";
	}
	strstr << prefix << "[/modify_city]\n";
}

void tevent::tmessage::from_config(const config& cfg)
{
	hero_ = cfg["hero"].str();

	incident_ = cfg["incident"].to_int(-1);
	yesno_ = cfg["yesno"].to_bool();

	std::vector<t_string_base::trans_str> trans = cfg["caption"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		caption_ = ti->str;
		break;
	}

	trans = cfg["message"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		message_ = ti->str;
		break;
	}
}

void tevent::tmessage::from_ui_special(HWND hdlgP)
{
	char text[4096];
	std::stringstream strstr;
	HWND hctl;

	// hero
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_HERO), text, sizeof(text) / sizeof(text[0]));
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMESSAGE_HERO);
		strstr.str("");
		strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		hero_ = strstr.str();
	} else {
		hero_ = text;
	}
	// caption
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID), text, sizeof(text) / sizeof(text[0]));
	caption_ = get_rid_of_return(text);

	// incident
	incident_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTMESSAGE_INDICENT));

	// yes/no style
	yesno_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTMESSAGE_YESNO))? true: false;

	// message
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE_MSGID), text, sizeof(text) / sizeof(text[0]));
	message_ = get_rid_of_return(text);
}

void tevent::tmessage::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[4096];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "message";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "hero = ";
	if (valuex::is_variable(hero_)) {
		strstr << hero_;
	} else {
		int hero = valuex::to_int(hero_, HEROS_INVALID_NUMBER);
		if (hero != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[hero].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "incident = ";
	if (incident_ >= 0) {
		strstr << incident_;
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	if (yesno_) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "yesno = " << (yesno_? "yes": "no");
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "caption = ";
	if (!caption_.empty()) {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), caption_.c_str());
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "message = ";
	if (!message_.empty()) {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), message_.c_str());
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tmessage::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;

	// hero
	bool is_variable = valuex::is_variable(hero_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_HERO);
	if (is_variable) {
		Edit_SetText(hctl, hero_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	int master_hero;
	if (!is_variable) {
		master_hero = valuex::to_int(hero_, -1);
	}
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTMESSAGE_HERO);
	ComboBox_ResetContent(hctl);
	int selected_row = 0;
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];
		int number = h.number_;
		strstr.str("");
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		ComboBox_AddString(hctl, utf8_2_ansi(strstr.str().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, h.number_);
		if (!is_variable && master_hero == h.number_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	// title
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID);
	Edit_SetText(hctl, insert_return(caption_).c_str());

	strstr.str("");
	if (!caption_.empty()) {
		strstr << insert_return(dsgettext(ns::_main.textdomain_.c_str(), caption_.c_str()));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION), utf8_2_ansi(strstr.str().c_str()));

	// incident
	UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_EVENTMESSAGE_INDICENT), incident_);

	// yes/no style
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_EVENTMESSAGE_YESNO), yesno_);

	// message
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE_MSGID);
	Edit_SetText(hctl, insert_return(message_).c_str());
	strstr.str("");
	if (!message_.empty()) {
		strstr << insert_return(dsgettext(ns::_main.textdomain_.c_str(), message_.c_str()));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE), utf8_2_ansi(strstr.str().c_str()));
}

void tevent::tmessage::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[message]\n";
	strstr << prefix << "\thero = " << hero_ << "\n";
	if (incident_ >= 0) {
		strstr << prefix << "\tincident = " << incident_ << "\n";
	}
	if (yesno_) {
		strstr << prefix << "\tyesno = yes\n";
	}
	if (!caption_.empty()) {
		strstr << prefix << "\tcaption = _\"" << caption_ << "\"\n";
	}
	if (!textdomain_.empty()) {
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << textdomain_ << "\n";
		}
		strstr << prefix << "\tmessage = _\"" << message_ << "\"\n";
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << ns::_main.textdomain_ << "\n";
		}
	} else {
		strstr << prefix << "\tmessage = _\"" << message_ << "\"\n";
	}
	strstr << prefix << "[/message]\n";
}

void tevent::tprint::from_config(const config& cfg)
{
	size_ = cfg["size"].to_int();
	duration_ = cfg["duration"].to_int();

	std::vector<t_string_base::trans_str> trans = cfg["text"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		text_ = ti->str;
		break;
	}
	color_[0] = cfg["red"].to_int();
	color_[1] = cfg["green"].to_int();
	color_[2] = cfg["blue"].to_int();
}

void tevent::tprint::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	// text
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID), text, _MAX_PATH);
	text_ = get_rid_of_return(text);

	if (!text_.empty()) {
		size_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_FONTSIZE));
		duration_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_DURATION));

		color_[0] = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_RED));
		color_[1] = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_GREEN));
		color_[2] = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_BLUE));
	}
}

void tevent::tprint::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "print";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "text = ";
	if (!text_.empty()) {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), text_.c_str());
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	if (!text_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "size = " << size_;
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "duration = " << duration_;
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "color = (" << color_[0] << ", " << color_[1] << ", " << color_[2] << ")";
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tprint::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_FONTSIZE);
	UpDown_SetRange(hctl, 8, 18);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_FONTSIZE));
	UpDown_SetPos(hctl, size_);

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_DURATION);
	UpDown_SetRange(hctl, 50, 10000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_DURATION));
	UpDown_SetPos(hctl, duration_);

	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID);
	Edit_SetText(hctl, insert_return(text_).c_str());

	strstr.str("");
	if (!text_.empty()) {
		strstr << insert_return(dsgettext(ns::_main.textdomain_.c_str(), text_.c_str()));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT), utf8_2_ansi(strstr.str().c_str()));

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_RED);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_RED));
	UpDown_SetPos(hctl, color_[0]);

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_GREEN);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_GREEN));
	UpDown_SetPos(hctl, color_[1]);

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTPRINT_BLUE);
	UpDown_SetRange(hctl, 0, 255);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_BLUE));
	UpDown_SetPos(hctl, color_[2]);
}

void tevent::tprint::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[print]\n";
	if (!text_.empty()) {
		strstr << prefix << "\ttext = _\"" << text_ << "\"\n";
		strstr << prefix << "\tsize = " << size_ << "\n";
		strstr << prefix << "\tduration = " << duration_ << "\n";
		strstr << prefix << "\tred = " << color_[0] << "\n";
		strstr << prefix << "\tgreen = " << color_[1] << "\n";
		strstr << prefix << "\tblue = " << color_[2] << "\n";
	} else {
		strstr << prefix << "\ttext = \n";
	}
	strstr << prefix << "[/print]\n";
}

void tevent::tlabel::from_config(const config& cfg)
{
	std::vector<t_string_base::trans_str> trans = cfg["text"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		text_ = ti->str;
		break;
	}
	x_ = cfg["x"].str();
	y_ = cfg["y"].str();
}

void tevent::tlabel::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	// text
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_TEXT_MSGID), text, _MAX_PATH);
	text_ = get_rid_of_return(text);

	// X/Y
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_X), text, _MAX_PATH);
	if (text[0] == '\0') {
		int x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTLABEL_X));
		strstr.str("");
		strstr << x;
		x_ = strstr.str();
	} else {
		x_ = text;
	}
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_Y), text, _MAX_PATH);
	if (text[0] == '\0') {
		int y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_EVENTLABEL_Y));
		strstr.str("");
		strstr << y;
		y_ = strstr.str();
	} else {
		y_ = text;
	}
}

void tevent::tlabel::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "label";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "text = ";
	if (!text_.empty()) {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), text_.c_str());
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "x = " << x_;
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "y = " << y_;
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tlabel::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_TEXT_MSGID);
	Edit_SetText(hctl, insert_return(text_).c_str());

	strstr.str("");
	if (!text_.empty()) {
		strstr << insert_return(dsgettext(ns::_main.textdomain_.c_str(), text_.c_str()));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_TEXT), utf8_2_ansi(strstr.str().c_str()));

	// X/Y
	bool is_variable = valuex::is_variable(x_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_X);
	if (is_variable) {
		Edit_SetText(hctl, x_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	map_location loc;
	if (!is_variable) {
		loc.x = valuex::to_int(x_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTLABEL_X);
	UpDown_SetPos(hctl, loc.x);

	is_variable = valuex::is_variable(y_);
	hctl = GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_Y);
	if (is_variable) {
		Edit_SetText(hctl, y_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}
	if (!is_variable) {
		loc.y = valuex::to_int(y_, 0);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTLABEL_Y);
	UpDown_SetPos(hctl, loc.y);
}

void tevent::tlabel::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[label]\n";
	if (!text_.empty()) {
		strstr << prefix << "\ttext = _\"" << text_ << "\"\n";
	} else {
		strstr << prefix << "\ttext = \n";
	}
	strstr << prefix << "\tx,y = ";
	strstr << x_ << ", " << y_ << "\n";
	strstr << prefix << "[/label]\n";
}

void tevent::tallow_undo::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	char text[MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strcpy(text, "allow_undo");
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);
}

void tevent::tsideheros::from_config(const config& cfg)
{
	side_ = cfg["side"].str();

	const std::vector<std::string> vsize = utils::split(cfg["heros"].str());
	for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
		heros_.insert(lexical_cast_default<int>(*it));
	}
}

void tevent::tsideheros::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	// side
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_SIDEHEROS_SIDE), text, _MAX_PATH);
	side_ = text;
}

void tevent::tsideheros::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "sideheros";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	// side
	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "side = ";
	if (valuex::is_variable(side_)) {
		strstr << side_;
	} else if (!side_.empty()) {
		int side_index = valuex::to_int(side_, 0) - 1;
		if ((int)scenario.side_.size() > side_index) {
			int leader = scenario.side_[side_index].leader_;
			strstr << utf8_2_ansi(gdmgr.heros_[leader].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	// heros
	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "heros = ";
	for (std::set<int>::const_iterator it = heros_.begin(); it != heros_.end(); ++ it) {
		if (it != heros_.begin()) {
			strstr << ", ";
		}
		strstr << *it;
	}
	
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

}

void tevent::tsideheros::update_to_ui_special(HWND hdlgP) const
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::stringstream strstr;
	
	// side
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEHEROS_SIDE), side_.c_str());

	LVITEM lvi;
	char text[_MAX_PATH];
	int index = 0;
	const std::vector<tside>& side = ns::_scenario[ns::current_scenario].side_;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_SIDE);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tside>::const_iterator it = side.begin(); it != side.end(); ++ it, index ++) {
		int column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem = index;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << (it->side_ + 1);
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)(it->side_ + 1);
		ListView_InsertItem(hctl, &lvi);

		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = column;
		strstr.str("");
		strstr << gdmgr.heros_[it->leader_].name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	// hero
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_CANDIDATE);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (hero_map::iterator it = gdmgr.heros_.begin(); it != gdmgr.heros_.end(); ++ it) {
		hero& h = *it;

		if (hero::is_system(h.number_) || hero::is_soldier(h.number_) || hero::is_commoner(h.number_)) {
			continue;
		}
		if (h.gender_ == hero_gender_neutral) {
			continue;
		}
		if (heros_.find(h.number_) != heros_.end()) {
			continue;
		}

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// name
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		int number = h.number_;
		strstr.str("");
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// catalog
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		int value = h.base_catalog_;
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// feature
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// side feature
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.side_feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// tactic
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// leadership
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 5;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
	
	// heros
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_HEROS);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<int>::const_iterator it = heros_.begin(); it != heros_.end(); ++ it) {
		hero& h = gdmgr.heros_[*it];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// name
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strstr.str("");
		int number = h.number_;
		strstr << std::setfill('0') << std::setw(3) << number << ": " << h.name();
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)h.number_;
		ListView_InsertItem(hctl, &lvi);

		// catalog
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		int value = h.base_catalog_;
		strstr << value;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// feature
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// tactic
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
	strstr.str("");
	strstr << _("Service") << "(" << ListView_GetItemCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SERVICE), utf8_2_ansi(strstr.str().c_str()));
}

void tevent::tsideheros::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[sideheros]\n";
	strstr << prefix << "\tside = " << side_ << "\n";
	strstr << prefix << "\theros = ";
	for (std::set<int>::const_iterator it = heros_.begin(); it != heros_.end(); ++ it) {
		if (it != heros_.begin()) {
			strstr << ", ";
		}
		strstr << *it;
	}
	strstr << "\n";
	strstr << prefix << "[/sideheros]\n";
}

std::map<int, std::pair<std::string, std::string> > tevent::tstore_unit::mode_map;

tevent::tstore_unit::tstore_unit()
	: tcommand(STORE_UNIT)
	, filter_()
	, variable_()
	, mode_(ALWAYS_CLEAR)
	, kill_(false)
{
	if (mode_map.empty()) {
		mode_map[ALWAYS_CLEAR] = std::make_pair("always_clear", _("Always clear"));
		mode_map[REPLACE] = std::make_pair("replace", _("Replace"));
		mode_map[APPEND] = std::make_pair("append", _("Append"));
	}
}

void tevent::tstore_unit::from_config(const config& cfg)
{
	filter_.from_config(cfg.child("filter"));
	variable_ = cfg["variable"].str();
	std::string str = cfg["mode"].str();
	for (std::map<int, std::pair<std::string, std::string> >::const_iterator it = mode_map.begin(); it != mode_map.end(); ++ it) {
		if (str == it->second.first) {
			mode_ = it->first;
			break;
		}
	}
	kill_ = cfg["kill"].to_bool();
}

void tevent::tstore_unit::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	// variable
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_STOREUNIT_VARIABLE), text, sizeof(text) / sizeof(text[0]));
	variable_ = text;

	// mode
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_STOREUNIT_MODE);
	mode_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tevent::tstore_unit::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "store_unit";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "[filter]";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);
	filter_.update_to_ui_event_edit(hctl, htvi);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "variable = " << variable_;
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "mode = " << mode_map.find(mode_)->second.first;
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tstore_unit::update_to_ui_special(HWND hdlgP) const
{
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_STOREUNIT_VARIABLE);
	Edit_SetText(hctl, variable_.c_str());

	hctl = GetDlgItem(hdlgP, IDC_CMB_STOREUNIT_MODE);
	int cursel = 0;
	for (std::map<int, std::pair<std::string, std::string> >::const_iterator it = mode_map.begin(); it != mode_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.second.c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (mode_ == it->first) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, cursel);
}

void tevent::tstore_unit::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[store_unit]\n";
	strstr << prefix << "\t[filter]\n";
	filter_.generate(strstr, prefix + "\t\t");
	strstr << prefix << "\t[/filter]\n";
	strstr << prefix << "\tvariable = " << variable_ << "\n";
	if (mode_ != ALWAYS_CLEAR) {
		strstr << prefix << "\tmode = " << mode_map.find(mode_)->second.first << "\n";
	}
	strstr << prefix << "[/store_unit]\n";
}

tevent::trename::trename()
	: ttranslatable_msgid(RENAME)
	, number_(HEROS_INVALID_NUMBER)
{
}

void tevent::trename::from_config(const config& cfg)
{
	number_ = cfg["number"].to_int();

	std::vector<t_string_base::trans_str> trans = cfg["name"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		textdomain_ = ti->td;
		value_ = ti->str;
		break;
	}
}

void tevent::trename::from_ui_special(HWND hdlgP)
{
	// number
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTRENAME_NUMBER);
	number_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	// name
	ttranslatable_msgid::from_ui_special(hdlgP);
}

void tevent::trename::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "rename";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "number = " << gdmgr.heros_[number_].name();
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "name = ";
	if (textdomain_.empty() || value_.empty()) {
		strstr << value_;
	} else {
		strstr << dsgettext(textdomain_.c_str(), value_.c_str());
	}
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::trename::update_to_ui_special(HWND hdlgP) const
{
	int cursel = -1;
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTRENAME_NUMBER);
	for (std::map<int, int>::const_iterator it = ns::cityno_map.begin(); it != ns::cityno_map.end(); ++ it) {
		int city = it->first;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[city].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, city);
		if (number_ == city) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, cursel);

	ttranslatable_msgid::update_to_ui_special(hdlgP);
}

void tevent::trename::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[rename]\n";
	strstr << prefix << "\tnumber = " << number_ << "\n";
	if (!textdomain_.empty()) {
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << textdomain_ << "\n";
		}
		strstr << prefix << "\tname = _\"" << value_ << "\"\n";
		if (textdomain_ != ns::_main.textdomain_) {
			strstr << "#textdomain " << ns::_main.textdomain_ << "\n";
		}
	} else {
		strstr << prefix << "\tname = " << value_ << "\n";
	}
	strstr << prefix << "[/rename]\n";
}

void tevent::tobjectives::from_config(const config& cfg)
{
	std::string str;
	std::vector<t_string_base::trans_str> trans = cfg["summary"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		str = ti->str;
		break;
	}
	summary_ = str;
	
	BOOST_FOREACH (const config &subcfg, cfg.child_range("objective")) {
		const std::string condition = subcfg["condition"].str();
		if (condition != "win" && condition != "lose") {
			continue;
		}
		str.clear();
		trans = subcfg["description"].t_str().valuex();
		for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
			// only support one textdomain
			str = ti->str;
			break;
		}
		if (condition == "win") {
			win_.push_back(str);
		} else {
			lose_.push_back(str);
		}
	}
}

void tevent::tobjectives::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_OBJECTIVES_SUMMARY_MSGID), text, sizeof(text) / sizeof(text[0]));
	summary_ = get_rid_of_return(text);
}

void tevent::tobjectives::from_ui_special_msgid(HWND hdlgP)
{
	char text[_MAX_PATH];
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_MSGID_MSGID), text, sizeof(text) / sizeof(text[0]));
	std::vector<std::string>::iterator it;
	if (ns::type == IDC_LV_OBJECTIVES_WIN) {
		it = win_.begin();
	} else if (ns::type == IDC_LV_OBJECTIVES_LOSE) {
		it = lose_.begin();
	} else {
		return;
	}
	std::advance(it, ns::clicked_item);
	*it = text;
}

void tevent::tobjectives::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "objectives";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi;
	
	if (!summary_.empty()) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "summary = " << dgettext(ns::_main.textdomain_.c_str(), summary_.c_str());
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}

	for (std::vector<std::string>::const_iterator it = win_.begin(); it != win_.end(); ++ it) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "(win)";
		if (ns::_main.textdomain_.empty() || it->empty()) {
			strstr << *it;
		} else {
			strstr << dgettext(ns::_main.textdomain_.c_str(), it->c_str());
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
	for (std::vector<std::string>::const_iterator it = lose_.begin(); it != lose_.end(); ++ it) {
		htvi = TreeView_AddLeaf(hctl, htvi1);
		strstr.str("");
		strstr << "(lose)";
		if (ns::_main.textdomain_.empty() || it->empty()) {
			strstr << *it;
		} else {
			strstr << dgettext(ns::_main.textdomain_.c_str(), it->c_str());
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hctl, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
	}
}

void tevent::tobjectives::update_to_ui_special(HWND hdlgP) const
{
	// win/lose
	std::string str;
	if (!summary_.empty()) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_OBJECTIVES_SUMMARY_MSGID), insert_return(summary_).c_str());

		str = insert_return(dgettext(ns::_main.textdomain_.c_str(), summary_.c_str()));
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_OBJECTIVES_SUMMARY), utf8_2_ansi(str.c_str()));
	}

	char text[_MAX_PATH];
	LVITEM lvi;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_OBJECTIVES_WIN);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	for (std::vector<std::string>::const_iterator it = win_.begin(); it != win_.end(); ++ it) {
		str = *it;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// msgid
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strcpy(text, str.c_str());
		lvi.pszText = text;
		lvi.lParam = index ++;
		ListView_InsertItem(hctl, &lvi);

		// translated
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), str.c_str())));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}

	hctl = GetDlgItem(hdlgP, IDC_LV_OBJECTIVES_LOSE);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::vector<std::string>::const_iterator it = lose_.begin(); it != lose_.end(); ++ it) {
		str = *it;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// msgid
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strcpy(text, str.c_str());
		lvi.pszText = text;
		lvi.lParam = index ++;
		ListView_InsertItem(hctl, &lvi);

		// translated
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), str.c_str())));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tevent::tobjectives::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[objectives]\n";
	if (!summary_.empty()) {
		strstr << prefix << "\tsummary = _\"" << summary_ << "\"\n";
	}
	for (std::vector<std::string>::const_iterator it = win_.begin(); it != win_.end(); ++ it) {
		strstr << prefix << "\t[objective]\n";
		strstr << prefix << "\t\tdescription = _\"" << *it << "\"\n";
		strstr << prefix << "\t\tcondition = win\n";
		strstr << prefix << "\t[/objective]\n";
	}
	for (std::vector<std::string>::const_iterator it = lose_.begin(); it != lose_.end(); ++ it) {
		strstr << prefix << "\t[objective]\n";
		strstr << prefix << "\t\tdescription = _\"" << *it << "\"\n";
		strstr << prefix << "\t\tcondition = lose\n";
		strstr << prefix << "\t[/objective]\n";
	}
	strstr << prefix << "[/objectives]\n";
}

std::map<int, std::pair<std::string, std::string> > tevent::tcondition::logic_map;

std::vector<std::pair<std::string, std::string> > tevent::tcondition::tjudge::op_map;

tevent::tcondition::tjudge::tjudge(int type, int logic)
	: tfilter_()
	, type_(type)
	, logic_(logic)
{
	if (op_map.empty()) {
		op_map.push_back(std::make_pair("equals", _("equals string")));
		op_map.push_back(std::make_pair("not_equals", _("not equals string")));
		op_map.push_back(std::make_pair("contains", _("contains string")));
		op_map.push_back(std::make_pair("numerical_equals", _("equals numerical")));
		op_map.push_back(std::make_pair("numerical_not_equals", _("not equals numerical")));
		op_map.push_back(std::make_pair("greater_than", _("greater than numerical")));
		op_map.push_back(std::make_pair("less_than", _("less than numerical")));
		op_map.push_back(std::make_pair("greater_than_equal_to", _("greater than equal to numerical")));
		op_map.push_back(std::make_pair("less_than_equal_to", _("less_than_equal_to numerical")));
		op_map.push_back(std::make_pair("boolean_equals", _("equals boolean")));
		op_map.push_back(std::make_pair("boolean_not_equals", _("not equals boolean")));
	}
}

void tevent::tcondition::tjudge::from_config(const config& cfg)
{
	if (type_ == VARIABLE) {
		left_ = cfg["name"].str();
		BOOST_FOREACH (const config::attribute &istrmap, cfg.attribute_range()) {
			for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
				if (it->first == istrmap.first) {
					op_ = istrmap.first;
					right_ = istrmap.second;
					break;
				}
			}
		}
	} else {
		tfilter_::from_config(cfg);
		count_ = cfg["count"].str();
	}
}

void tevent::tcondition::tjudge::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	if (type_ == VARIABLE) {
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_LEFT), text, _MAX_PATH);
		left_ = text;
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_RIGHT), text, _MAX_PATH);
		right_ = text;

		HWND hctl = GetDlgItem(hdlgP, IDC_CMB_VARIABLE_OP);
		op_ = op_map[ComboBox_GetCurSel(hctl)].first;
	} else {
		// tfilter_::from_ui_special(hdlgP);
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_HAVEUNIT_COUNT), text, _MAX_PATH);
		count_ = text;
	}

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CONDITION_LOGIC);
	int index = ComboBox_GetCurSel(hctl);
	if (index >= 0) {
		std::map<int, std::pair<std::string, std::string> >::iterator find = logic_map.begin();
		std::advance(find, index);
		logic_ = find->first;
	} else {
		logic_ = LOGIC_NONE;
	}
}

std::string tevent::tcondition::tjudge::description(bool to_tree) const
{
	std::stringstream strstr;
	utils::string_map symbols;

	if (type_ == VARIABLE) {
		symbols["left"] = left_;
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
			if (it->first == op_) {
				symbols["op"] = it->second;
			}
		}
		symbols["right"] = right_;

		strstr.str("");
		strstr << vgettext2("\"$left\" $op \"$right\"", symbols);
	} else if (type_ == HAVE_UNIT && !to_tree) {
		strstr << tfilter_::description(NULL, NULL, false);
		
		if (!filter_location_.empty()) {
			strstr << "[filter_location]: Existed";
		}
		strstr << " count: " << count_;
	}
	return strstr.str();
}


void tevent::tcondition::tjudge::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (type_ == VARIABLE) {
		HTREEITEM htvi = TreeView_AddLeaf(hwndtv, branch);
		strcpy(text, utf8_2_ansi(description(true).c_str()));
		TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	} else {
		tfilter_::update_to_ui_event_edit(hwndtv, branch);

		HTREEITEM htvi = TreeView_AddLeaf(hwndtv, branch);
		strstr.str("");
		strstr << "count: " << count_;
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
		
	}
}

void tevent::tcondition::tjudge::update_to_ui_special(HWND hdlgP) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl;

	if (type_ == VARIABLE) {
		hctl = GetDlgItem(hdlgP, IDC_LV_VARIABLE_ENV);
		int count = ListView_GetItemCount(hctl);
		ListView_DeleteAllItems(hctl);
		int index = 0;
		LVITEM lvi;
		std::vector<std::pair<std::string, std::string> > env;
		env.push_back(std::make_pair("choice", "是/否风格消框选择结果(布尔)"));
		env.push_back(std::make_pair("random", "随机数(数字)"));
		env.push_back(std::make_pair("turn_number", "当前回合(数字)"));
		env.push_back(std::make_pair("unit.heros_army", "第一单位武将"));
		env.push_back(std::make_pair("unit.side", "第一单位势力"));
		env.push_back(std::make_pair("second_unit.heros_army", "第二单位武将"));
		env.push_back(std::make_pair("second_unit.side", "第二单位势力"));
			
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = env.begin(); it != env.end(); ++ it) {
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 标识
			lvi.iItem = index;
			lvi.iSubItem = 0;
			strcpy(text, it->first.c_str());
			lvi.pszText = text;
			lvi.lParam = (LPARAM)index ++;
			ListView_InsertItem(hctl, &lvi);

			// 描述
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 1;
			strcpy(text, it->second.c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);
		}

		int selected_row = 0;
		hctl = GetDlgItem(hdlgP, IDC_CMB_VARIABLE_OP);
		for (index = 0; index < ComboBox_GetCount(hctl); index ++) {
			if (op_ == op_map[index].first) {
				selected_row = index;
				break;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		if (!left_.empty()) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_LEFT), left_.c_str());
		} else {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_LEFT), env[0].first.c_str());
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_RIGHT), right_.c_str());

	} else if (type_ == HAVE_UNIT) {
		strstr.str("");
		strstr << tfilter_::description(NULL, NULL, true);
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HAVEUNIT_FILTER), utf8_2_ansi(strstr.str().c_str()));

		strstr.str("");
		if (count_.empty()) {
			strstr << "1-99999";
		} else {
			strstr << count_;
		}
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HAVEUNIT_COUNT), strstr.str().c_str());

	}
}

void tevent::tcondition::fill_logic_cmb(HWND hctl) const
{
	for (std::map<int, std::pair<std::string, std::string> >::const_iterator it = logic_map.begin(); it != logic_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.second.c_str()));
	}
	const tjudge& judge = judges_[ns::clicked_judge];
	std::map<int, std::pair<std::string, std::string> >::iterator find = logic_map.find(judge.logic_);
	if (find != logic_map.end()) {
		ComboBox_SetCurSel(hctl, std::distance(logic_map.begin(), find));
	}
	ComboBox_Enable(hctl, ns::clicked_judge != 0);
}

tevent::tcondition::tcondition()
	: tcommand(CONDITION)
{
	if (logic_map.empty()) {
		logic_map[LOGIC_AND] = std::make_pair("and", _("&&(and)"));
		logic_map[LOGIC_OR] = std::make_pair("or", _("||(or)"));
		logic_map[LOGIC_NOT] = std::make_pair("not", _("&& !(not)"));
	}
}

void tevent::tcondition::from_config_internal(const config& cfg, int logic)
{
	BOOST_FOREACH (const config::any_child& tmp, cfg.all_children_range()) {
		int type = NONE;

		if (tmp.key == "variable") {
			type = VARIABLE;

		} else if (tmp.key == "have_unit") {
			type = HAVE_UNIT;

		}
		if (type != NONE) {
			judges_.push_back(tjudge(type, logic));
			judges_.back().from_config(tmp.cfg);
		}
	}
}

void tevent::tcondition::from_config(const config& cfg)
{
	BOOST_FOREACH (const config::any_child& tmp, cfg.all_children_range()) {
		int type = NONE;
		if (tmp.key == "variable") {
			type = VARIABLE;

		} else if (tmp.key == "have_unit") {
			type = HAVE_UNIT;

		}
		if (type != NONE) {
			judges_.push_back(tjudge(type, LOGIC_NONE));
			judges_.back().from_config(tmp.cfg);
			break;
		}
	}
	
	BOOST_FOREACH (const config::any_child& tmp, cfg.all_children_range()) {
		if (tmp.key == "not") {
			from_config_internal(tmp.cfg, LOGIC_NOT);

		} else if (tmp.key == "and") {
			from_config_internal(tmp.cfg, LOGIC_AND);

		} else if (tmp.key == "or") {
			from_config_internal(tmp.cfg, LOGIC_OR);

		}
	}
}

void tevent::tcondition::from_ui_special(HWND hdlgP)
{
}

void tevent::tcondition::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi_condition = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "condition";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi_condition, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_condition, ns::iico_evt_condition, 1, text);

	for (std::vector<tjudge>::const_iterator it = judges_.begin(); it != judges_.end(); ++ it) {
		HTREEITEM htvi = TreeView_AddLeaf(hwndtv, htvi_condition);
		strstr.str("");
		strstr << "[";
		
		const tjudge& judge = *it;
		if (judge.logic_ == LOGIC_NOT) {
			strstr << "! ";
		} else if (judge.logic_ == LOGIC_AND) {
			strstr << "&& ";
		} else if (judge.logic_ == LOGIC_OR) {
			strstr << "|| ";
		}

		if (judge.type_ == VARIABLE) {
			strstr << "variable";
			judge.update_to_ui_event_edit(hwndtv, htvi);
		} else if (judge.type_ == HAVE_UNIT) {
			strstr << "have_unit";
			judge.update_to_ui_event_edit(hwndtv, htvi);
		}
		strstr << "]";
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			(LPARAM)(tcommand*)(this), ns::iico_evt_condition, ns::iico_evt_condition, 1, text);
	}
}

void tevent::tcondition::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	// explorer
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CONDITION_EXPLORER);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	for (std::vector<tjudge>::const_iterator it = judges_.begin(); it != judges_.end(); ++ it, index ++) {
		const tjudge& judge = *it;
		int column = 0;

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem = index;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (judge.logic_ == LOGIC_NOT) {
			strstr << "!";
		} else if (judge.logic_ == LOGIC_AND) {
			strstr << "&&";
		} else if (judge.logic_ == LOGIC_OR) {
			strstr << "||";
		}
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)index;
		ListView_InsertItem(hctl, &lvi);
	
		lvi.mask = LVIF_TEXT;
		lvi.iItem = index;
		lvi.iSubItem = column ++;
		strstr.str("");
		if (judge.type_ == VARIABLE) {
			strstr << "variable";
		} else if (judge.type_ == HAVE_UNIT) {
			strstr << "have_unit";
		}
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// description
		lvi.mask = LVIF_TEXT;
		lvi.iItem = index;
		lvi.iSubItem = column ++;
		strstr.str("");
		strstr << judge.description(false);
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

tevent::tif::tif(const tevent::tif& that)
	: tcommand(that)
	, condition_(that.condition_)
	, then_()
	, else_()
{
	tevent::copy_commands(that.then_, then_);
	tevent::copy_commands(that.else_, else_);
}

tevent::tif& tevent::tif::operator=(const tevent::tif& that)
{
	// super::operator=(that);
	type_ = tcommand::SET_VARIABLE;
	tcommand::operator=(that);

	condition_ = that.condition_;
	for (std::vector<tcommand*>::iterator it = then_.begin(); it != then_.end(); ++ it) {
		delete *it;
	}
	tevent::copy_commands(that.then_, then_);
	for (std::vector<tcommand*>::iterator it = else_.begin(); it != else_.end(); ++ it) {
		delete *it;
	}
	tevent::copy_commands(that.else_, else_);
	return *this;
}

void tevent::tif::from_config(const config& cfg)
{
	for (std::vector<tcommand*>::iterator it = then_.begin(); it != then_.end(); ++ it) {
		delete *it;
	}
	then_.clear();

	for (std::vector<tcommand*>::iterator it = else_.begin(); it != else_.end(); ++ it) {
		delete *it;
	}
	else_.clear();

	condition_.from_config(cfg);
	BOOST_FOREACH (const config::any_child& tmp, cfg.all_children_range()) {
		if (tmp.key == "then") {
			tevent::from_config_branch(tmp.cfg, then_);
		} else if (tmp.key == "else") {
			tevent::from_config_branch(tmp.cfg, else_);
		}
	}
}

void tevent::tif::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi_if = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "if";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi_if, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_if, ns::iico_evt_if, 1, text);

	condition_.update_to_ui_event_edit(hctl, htvi_if);

	HTREEITEM htvi_then = TreeView_AddLeaf(hctl, htvi_if);
	strstr.str("");
	strstr << "then";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi_then, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_THEN, ns::iico_evt_then, ns::iico_evt_then, 1, text);
	for (std::vector<tcommand*>::const_iterator it = then_.begin(); it != then_.end(); ++ it) {
		tcommand* cmd = *it;
		cmd->update_to_ui_event_edit(hctl, htvi_then);
	}

	HTREEITEM htvi_else = TreeView_AddLeaf(hctl, htvi_if);
	strstr.str("");
	strstr << "else";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi_else, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_ELSE, ns::iico_evt_then, ns::iico_evt_then, 1, text);
	for (std::vector<tcommand*>::const_iterator it = else_.begin(); it != else_.end(); ++ it) {
		tcommand* cmd = *it;
		cmd->update_to_ui_event_edit(hctl, htvi_else);
	}
}


BOOL On_DlgVariableInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_judge == ma_edit) {
		SetWindowText(hdlgP, "编辑判断条件: variable");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加判断条件: variable");
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tcondition::tjudge& judge = cond->judges_[ns::clicked_judge];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_VARIABLE_OP);
	const std::vector<std::pair<std::string, std::string> >& op_map = tevent::tcondition::tjudge::op_map;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.c_str()));
	}

	hctl = GetDlgItem(hdlgP, IDC_LV_VARIABLE_ENV);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 120;
	lvc.pszText = "标识";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 160;
	lvc.iSubItem = 1;
	lvc.pszText = "描述";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	cond->tcondition::fill_logic_cmb(GetDlgItem(hdlgP, IDC_CMB_CONDITION_LOGIC));
	judge.update_to_ui_special(hdlgP);

	return FALSE;
}

void On_DlgVariableCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tcondition::tjudge& judge = cond->judges_[ns::clicked_judge];

	switch (id) {
	case IDOK:
		changed = TRUE;
		judge.from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgVariableNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	LVITEM lvi;

	if (lpNMHdr->code == NM_DBLCLK) {
		if (lpNMHdr->idFrom != IDC_LV_VARIABLE_ENV) {
			return FALSE;
		}
		if (lpnmitem->iItem < 0) {
			return FALSE;
		}

		POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
		MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_LEFT), text);
	}
	return FALSE;
}

BOOL CALLBACK DlgVariableProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgVariableInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgVariableCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgVariableNotify);
	}
	
	return FALSE;
}

BOOL On_DlgHaveUnitInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;

	if (ns::action_judge == ma_edit) {
		strstr << utf8_2_ansi(_("Edit judge condition"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Append judge condition"));
	}
	strstr << " ---- " << utf8_2_ansi(_("have_unit"));
	SetWindowText(hdlgP, strstr.str().c_str());

	strstr.str("");
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOGIC), utf8_2_ansi(_("Logic")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_COUNT), utf8_2_ansi(_("Count")));
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_HAVEUNIT_FILTER), utf8_2_ansi(_("Set filter condition")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tcondition::tjudge& judge = cond->judges_[ns::clicked_judge];

	cond->tcondition::fill_logic_cmb(GetDlgItem(hdlgP, IDC_CMB_CONDITION_LOGIC));
	judge.update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgHaveUnitCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tcondition::tjudge& judge = cond->judges_[ns::clicked_judge];

	switch (id) {
	case IDC_BT_HAVEUNIT_FILTER:
		ns::filter = &judge;
		if (OnEventFilterBt2(hdlgP, id)) {
			judge.update_to_ui_special(hdlgP);
		}
		break;

	case IDOK:
		changed = TRUE;
		judge.from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgHaveUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgHaveUnitInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgHaveUnitCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnJudgeAddBt(HWND hdlgP, int type)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CONDITION_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);

	int logic = tevent::tcondition::LOGIC_NONE;
	if (!cond->judges_.empty()) {
		logic = tevent::tcondition::LOGIC_AND;
	}
	cond->judges_.push_back(tevent::tcondition::tjudge(type, logic));
	ns::clicked_judge = cond->judges_.size() - 1;
	
	ns::action_judge = ma_new;
	if (type == tevent::tcondition::VARIABLE) {
		DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_VARIABLE), hdlgP, DlgVariableProc, lParam);
	} else if (type == tevent::tcondition::HAVE_UNIT) {
		DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_HAVEUNIT), hdlgP, DlgHaveUnitProc, lParam);
	}
	cond->update_to_ui_special(hdlgP);

	return;
}

void OnJudgeEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CONDITION_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tcondition::tjudge& judge = cond->judges_[ns::clicked_judge];

	ns::action_judge = ma_edit;
	bool updating = false;
	if (judge.type_ == tevent::tcondition::VARIABLE) {
		if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_VARIABLE), hdlgP, DlgVariableProc, lParam)) {
			updating = true;
		}
	} else if (judge.type_ == tevent::tcondition::HAVE_UNIT) {
		if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_HAVEUNIT), hdlgP, DlgHaveUnitProc, lParam)) {
			updating = true;
		}
	}
	if (updating) {
		cond->update_to_ui_special(hdlgP);
	}

	return;
}

void OnJudgeDelBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CONDITION_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);

	std::vector<tevent::tcondition::tjudge>::iterator it = cond->judges_.begin();
	std::advance(it, ns::clicked_judge);
	cond->judges_.erase(it);
	if (cond->judges_.size() == 1) {
		cond->judges_[0].logic_ = tevent::tcondition::LOGIC_NONE;
	}

	cond->update_to_ui_special(hdlgP);
	return;
}

BOOL On_DlgConditionInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑判断条件");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加判断条件");
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);

	std::stringstream strstr;
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CONDITION_EXPLORER);
	LVCOLUMN lvc;
	int column = 0;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strstr.str("");
	strstr << _("Logic");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Type");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 300;
	lvc.iSubItem = column;
	strstr.str("");
	strstr << _("Description");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgConditionCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	int type = tevent::tcondition::NONE;

	switch (id) {
	case IDM_NEW_ITEM0:
		type = tevent::tcondition::VARIABLE;
	case IDM_NEW_ITEM1:
		if (type == tevent::tcondition::NONE) {
			type = tevent::tcondition::HAVE_UNIT;
		}
		if (ns::type == IDC_LV_CONDITION_EXPLORER) {
			OnJudgeAddBt(hdlgP, type);
		}
		break;
	case IDM_EDIT:
		if (ns::type == IDC_LV_CONDITION_EXPLORER) {
			OnJudgeEditBt(hdlgP);
		}
		break;
	case IDM_DELETE:
		if (ns::type == IDC_LV_CONDITION_EXPLORER) {
			OnJudgeDelBt(hdlgP);
		}
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void condition_notify_handler_rclick(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;
	int						icount;

	if (lpNMHdr->idFrom == IDC_LV_CONDITION_EXPLORER) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		
		POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
		MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

		HMENU hpopup_new = CreatePopupMenu();

		AppendMenu(hpopup_new, MF_STRING, IDM_NEW_ITEM0, utf8_2_ansi(_("variable")));
		AppendMenu(hpopup_new, MF_STRING, IDM_NEW_ITEM1, utf8_2_ansi(_("have_unit")));

		HMENU hpopup_judge = CreatePopupMenu();
		AppendMenu(hpopup_judge, MF_POPUP, (UINT_PTR)(hpopup_new), utf8_2_ansi(_("Add")));
		AppendMenu(hpopup_judge, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_judge, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup_judge, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_judge, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}		

		TrackPopupMenuEx(hpopup_judge, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_new);
		DestroyMenu(hpopup_judge);
	}

	if (lpNMHdr->idFrom == IDC_LV_CONDITION_EXPLORER) {
		ns::clicked_judge = lpnmitem->iItem;

	}
	ns::type = lpNMHdr->idFrom;
    return;
}

void condition_notify_handler_dblclk(HWND hdlgP, LPNMHDR lpNMHdr)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_DBLCLK) {
		return;
	}
	if (lpNMHdr->idFrom != IDC_LV_CONDITION_EXPLORER) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		ns::clicked_judge = lpnmitem->iItem;
		OnJudgeEditBt(hdlgP);
	}
    return;
}

BOOL On_DlgConditionNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		condition_notify_handler_rclick(hdlgP, lpNMHdr);

	} else if (lpNMHdr->code == NM_DBLCLK) {
		condition_notify_handler_dblclk(hdlgP, lpNMHdr);

	}
	return FALSE;
}

void On_DlgConditionDestroy(HWND hdlgP)
{
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (pHdr) {
		delete pHdr->apRes;
		delete pHdr;
	}
}

BOOL CALLBACK DlgConditionProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgConditionInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgConditionCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgConditionNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgConditionDestroy);
	}
	
	return FALSE;
}

void OnConditionEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CONDITION), hdlgP, DlgConditionProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventKillInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_event_item == ma_edit) {
		strstr << utf8_2_ansi(_("Edit operation"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Append operation"));
	}
	strstr << " ---- " << utf8_2_ansi(_("Kill troop/hero"));
	SetWindowText(hdlgP, strstr.str().c_str());

	strstr.str("");
	strstr << dgettext_2_ansi("wesnoth-lib", "Hero");
	strstr << "(" << utf8_2_ansi(_("Support variable")) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HERO), strstr.str().c_str());
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_EVENTKILL_DIRECTHERO), utf8_2_ansi(_("Direct hero")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SIDE), utf8_2_ansi(_("Belong to side(If be killed is city, must set it)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DEFINEDVARIABLES), utf8_2_ansi(_("Defined variables(Right click to popup menu)")));

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, utf8_2_ansi(_("To hero")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// candidate
	candidate_hero::fill_header(hdlgP);

	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTKILL_SIDE);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strstr.str("");
	strstr << _("NO.");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 120;
	lvc.iSubItem = 1;
	strstr.str("");
	strstr << dsgettext("wesnoth-lib", "Side");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void strcat_heros_str(HWND hdlgP, int id)
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, id), text, sizeof(text) / sizeof(text[0]));
	if (valuex::is_variable(text)) {
		text[0] = '\0';
	}
	strstr.str("");
	strstr << text;
	if (!strstr.str().empty()) {
		std::set<int> sstr = utils::to_set_int(strstr.str());
		if (sstr.find(ns::clicked_hero) != sstr.end()) {
			return;
		}
		strstr << ", ";
	}
	strstr << ns::clicked_hero;
	Edit_SetText(GetDlgItem(hdlgP, id), strstr.str().c_str());
}

void On_DlgEventKillCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}

	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	std::stringstream strstr;

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_HERO), ns::clicked_variable.c_str());
		break;
	case IDM_TOSERVICE:
		strstr.str("");
		if (ns::type == IDC_LV_CANDIDATEHERO) {
			strcat_heros_str(hdlgP, IDC_ET_EVENTKILL_HERO);

		} else if (ns::type == IDC_LV_EVENTKILL_SIDE) {
			strstr << ns::clicked_side;
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_SIDE), strstr.str().c_str());
		}
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void eventkill_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_EVENTKILL_SIDE) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		HMENU hpopup = CreatePopupMenu();
		AppendMenu(hpopup, MF_STRING, IDM_TOSERVICE, utf8_2_ansi(_("To side")));
		
		TrackPopupMenuEx(hpopup, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup);

		ns::clicked_side = lvi.lParam;
	}

    return;
}

BOOL On_DlgEventKillNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_TOSERVICE, _("To hero")));

		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr)) {
			ns::type = DlgItem;
			ns::clicked_hero = candidate_hero::lParam;

		} else if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);

		} else {
			ns::type = DlgItem;
			eventkill_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgEventKillDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventKillProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventKillInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventKillCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventKillNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventKillDestroy);
	}
	
	return FALSE;
}

void OnEventKillEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTKILL), hdlgP, DlgEventKillProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventEndlevelInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：结束关卡");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：结束关卡");
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgEventEndlevelCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgEventEndlevelProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventEndlevelInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventEndlevelCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnEventEndlevelEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTENDLEVEL), hdlgP, DlgEventEndlevelProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventJoinInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：加入部队");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：加入部队");
	}

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到主将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到加入武将");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventJoinEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl1;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTJOIN_MASTERHERO) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_MASTERHERO);

	} else if (id == IDC_ET_EVENTJOIN_JOIN) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTJOIN_JOIN);

	} else {
		return;
	}
	ShowWindow(hctl1, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	if (hctl2) {
		ShowWindow(hctl2, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	}

	return;
}

void On_DlgEventJoinCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // master_hero
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_MASTERHERO), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM1: // join
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTJOIN_JOIN), ns::clicked_variable.c_str());
		break;

	case IDC_ET_EVENTJOIN_MASTERHERO:
	case IDC_ET_EVENTJOIN_JOIN:
		OnEventJoinEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventJoinNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgEventJoinDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventJoinProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventJoinInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventJoinCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventJoinNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventJoinDestroy);
	}
	
	return FALSE;
}

void OnEventJoinEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTJOIN), hdlgP, DlgEventJoinProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventModifyUnitInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：修改单位");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：修改单位");
	}

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到主将");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventModifyUnitEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl1;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTMODIFYUNIT_MASTERHERO) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTMODIFYUNIT_MASTERHERO);

	} else {
		return;
	}
	ShowWindow(hctl1, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	if (hctl2) {
		ShowWindow(hctl2, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	}

	return;
}

void On_DlgEventModifyUnitCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_MASTERHERO), ns::clicked_variable.c_str());
		break;

	case IDC_ET_EVENTMODIFYUNIT_MASTERHERO:
		OnEventModifyUnitEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventModifyUnitNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgEventModifyUnitDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventModifyUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventModifyUnitInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventModifyUnitCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventModifyUnitNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventModifyUnitDestroy);
	}
	
	return FALSE;
}

void OnEventModifyUnitEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTMODIFYUNIT), hdlgP, DlgEventModifyUnitProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventModifySideInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	char text[_MAX_PATH];

	strstr << _("Editing") << ": ";
	strstr << _("Modify side");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	strstr.str("");
	strstr << _("Base gold");
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_GOLD), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << _("Base income");
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_INCOME), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << _("Set") << "...";
	Static_SetText(GetDlgItem(hdlgP, IDC_BT_TECHNOLOGY), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到主将");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_GOLD);
	UpDown_SetRange(hctl, -1, 2000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYSIDE_GOLD));

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYSIDE_INCOME);
	UpDown_SetRange(hctl, -1, 1000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYSIDE_INCOME));

	LVCOLUMN lvc;
	int column = 0;
	// agree
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_AGREE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// terminate
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_TERMINATE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// side
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_SIDE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// hero join/leave
	column = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_HERO);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, utf8_2_ansi(_("Action")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx =60;
	strcpy(text, utf8_2_ansi(_("City")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 300;
	strcpy(text, utf8_2_ansi(_("Hero")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	ns::clicked_command->update_to_ui_special(hdlgP);

	tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);
	if (modify_side->side_ == HEROS_INVALID_SIDE) {
		hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYSIDE_SIDE);
		modify_side->side_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	}

	return FALSE;
}

void OnEventModifySideCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);

	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	modify_side->side_ = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	modify_side->agree_.clear();
	modify_side->terminate_.clear();
	modify_side->update_to_ui_ally(hdlgP);
	return;
}

extern void OnSideEditTechnologyBt(HWND hdlgP, int id, int explorer);

void On_DlgEventModifySideCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_MASTERHERO), ns::clicked_variable.c_str());
		break;

	case IDM_TOSERVICE:
		modify_side->update_ally(hdlgP, ns::clicked_side, true, true);
		break;
	case IDM_TOWANDER:
		modify_side->update_ally(hdlgP, ns::clicked_side, false, true);
		break;
	case IDM_DELETE_ITEM0:
		modify_side->update_ally(hdlgP, ns::clicked_side, ns::type == IDC_LV_EVENTMODIFYSIDE_AGREE, false);
		break;

	case IDC_CMB_EVENTMODIFYSIDE_SIDE:
		OnEventModifySideCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_BT_TECHNOLOGY:
		ns::clicked_side = modify_side->side_;
		OnSideEditTechnologyBt(hdlgP, id, explorer_technology::MODIFY_SIDE);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void eventmodifyside_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM lvi;
	LPNMITEMACTIVATE lpnmitem;
	std::stringstream strstr;

	if (lpNMHdr->idFrom != IDC_LV_EVENTMODIFYSIDE_AGREE &&
		lpNMHdr->idFrom != IDC_LV_EVENTMODIFYSIDE_TERMINATE &&
		lpNMHdr->idFrom != IDC_LV_EVENTMODIFYSIDE_SIDE &&
		lpNMHdr->idFrom != IDC_LV_EVENTMODIFYSIDE_HERO) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	HMENU hpopup = CreatePopupMenu();

	if (lpNMHdr->idFrom == IDC_LV_EVENTMODIFYSIDE_AGREE) {
		AppendMenu(hpopup, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
		}
		
	} else if (lpNMHdr->idFrom == IDC_LV_EVENTMODIFYSIDE_TERMINATE) {
		AppendMenu(hpopup, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
		}

	} else if (lpNMHdr->idFrom == IDC_LV_EVENTMODIFYSIDE_SIDE) {
		tevent::tmodify_side* modify_side = dynamic_cast<tevent::tmodify_side*>(ns::clicked_command);

		AppendMenu(hpopup, MF_STRING, IDM_TOSERVICE, utf8_2_ansi(_("To agree")));
		AppendMenu(hpopup, MF_STRING, IDM_TOWANDER, utf8_2_ansi(_("To terminate")));
		if (lpnmitem->iItem < 0 || modify_side->agree_.find(lvi.lParam) != modify_side->agree_.end() ||
			modify_side->terminate_.find(lvi.lParam) != modify_side->terminate_.end()) {
			EnableMenuItem(hpopup, IDM_TOSERVICE, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup, IDM_TOWANDER, MF_BYCOMMAND | MF_GRAYED);
		}
	} else if (lpNMHdr->idFrom == IDC_LV_EVENTMODIFYSIDE_HERO) {
		strstr.str("");
		strstr << _("Add") << "...";
		AppendMenu(hpopup, MF_STRING, IDM_ADD, utf8_2_ansi(strstr.str().c_str()));
		strstr.str("");
		strstr << _("Edit") << "...";
		AppendMenu(hpopup, MF_STRING, IDM_EDIT, utf8_2_ansi(strstr.str().c_str()));
		strstr.str("");
		strstr << _("Delete") << "...";
		AppendMenu(hpopup, MF_STRING, IDM_DELETE, utf8_2_ansi(strstr.str().c_str()));

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
	}

	ns::type = lpNMHdr->idFrom;
	ns::clicked_side = lvi.lParam;
	
	TrackPopupMenuEx(hpopup, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup);

    return;
}

BOOL On_DlgEventModifySideNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			eventmodifyside_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgEventModifySideDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventModifySideProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventModifySideInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventModifySideCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventModifySideNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventModifySideDestroy);
	}
	
	return FALSE;
}

void OnEventModifySideEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTMODIFYSIDE), hdlgP, DlgEventModifySideProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventModifyCityInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	char text[_MAX_PATH];

	strstr << _("Editing") << ": ";
	strstr << _("Modify city");
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到主将");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMODIFYCITY_SOLDIERS);
	UpDown_SetRange(hctl, -1, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYCITY_SOLDIERS));

	// candidate
	candidate_hero::fill_header(hdlgP);

	LVCOLUMN lvc;
	int column = 0;
	// service
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTMODIFYCITY_SERVICE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgEventModifyCityCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}

	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent::tmodify_city* modify_city = dynamic_cast<tevent::tmodify_city*>(ns::clicked_command);

	switch (id) {
	case IDM_TOSERVICE:
		modify_city->service_.insert(ns::clicked_hero);
		modify_city->update_to_ui_special(hdlgP);
		break;
	case IDM_DELETE_ITEM0:
		modify_city->service_.erase(ns::clicked_hero);
		modify_city->update_to_ui_special(hdlgP);
		break;

	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTMODIFYUNIT_MASTERHERO), ns::clicked_variable.c_str());
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void eventmodifycity_notify_handler_rclick(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LVITEM lvi;
	LPNMITEMACTIVATE lpnmitem;
	std::stringstream strstr;

	if (lpNMHdr->idFrom != IDC_LV_EVENTMODIFYCITY_SERVICE) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	HMENU hpopup = CreatePopupMenu();

	if (lpNMHdr->idFrom == IDC_LV_EVENTMODIFYCITY_SERVICE) {
		AppendMenu(hpopup, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
		}
		ns::clicked_hero = lvi.lParam;
	}

	ns::type = lpNMHdr->idFrom;
	
	TrackPopupMenuEx(hpopup, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup);

    return;
}

BOOL On_DlgEventModifyCityNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_TOSERVICE, _("To service")));

		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr)) {
			ns::type = DlgItem;
			ns::clicked_hero = candidate_hero::lParam;

		} else if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			eventmodifycity_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgEventModifyCityDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventModifyCityProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventModifyCityInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventModifyCityCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventModifyCityNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventModifyCityDestroy);
	}
	
	return FALSE;
}

void OnEventModifyCityEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTMODIFYCITY), hdlgP, DlgEventModifyCityProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventMessageInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：提示框");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：提示框");
	}

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HERO), dgettext_2_ansi("wesnoth-lib", "Hero"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_INCIDENT), utf8_2_ansi(_("Incident(-1 indicate no incident)")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_EVENTMESSAGE_YESNO), utf8_2_ansi(_("Yes/No style")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TITLE), utf8_2_ansi(_("Title")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MESSAGE), utf8_2_ansi(_("Message")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_VARIABLE), utf8_2_ansi(_("Defined variable(Right click popup menu)")));

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到武将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到标题");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到内容");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_EVENTMESSAGE_INDICENT);
	UpDown_SetRange(hctl, -1, 1000);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_INDICENT));

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventMessageEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl1;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTMESSAGE_HERO) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTMESSAGE_HERO);

	} else {
		return;
	}
	ShowWindow(hctl1, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	if (hctl2) {
		ShowWindow(hctl2, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	}

	return;
}

void OnEventMessageEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND boddy;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTMESSAGE_CAPTION_MSGID) {
		boddy = GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION);

	} else if (id == IDC_ET_EVENTMESSAGE_MESSAGE_MSGID) {
		boddy = GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE);

	} else {
		return;
	}
	std::stringstream strstr;
	if (text[0] != '\0') {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), get_rid_of_return(text).c_str());
	}
	Edit_SetText(boddy, utf8_2_ansi(strstr.str().c_str()));

	return;
}

void insert_str_2_edit(HWND hctl, const std::string& span)
{
	char text[_MAX_PATH];
	Edit_GetText(hctl, text, sizeof(text) / sizeof(text[0]));

	int pos = Edit_GetSel(hctl);

	std::string str = text;
	str = str + span;
	Edit_SetText(hctl, str.c_str());
}

void On_DlgEventMessageCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // hero
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_HERO), ns::clicked_variable);
		break;
	case IDM_VARIABLE_ITEM1: // caption
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID), ns::clicked_variable);
		// OnEventMessageEt2(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID, GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID), EN_CHANGE);
		break;
	case IDM_VARIABLE_ITEM2: // message
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE_MSGID), ns::clicked_variable);
		// OnEventMessageEt2(hdlgP, IDC_ET_EVENTMESSAGE_CAPTION_MSGID, GetDlgItem(hdlgP, IDC_ET_EVENTMESSAGE_MESSAGE_MSGID), EN_CHANGE);
		break;

	case IDC_ET_EVENTMESSAGE_CAPTION_MSGID:
	case IDC_ET_EVENTMESSAGE_MESSAGE_MSGID:
		OnEventMessageEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_ET_EVENTMESSAGE_HERO:
		OnEventMessageEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventMessageNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgEventMessageDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventMessageProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventMessageInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventMessageCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventMessageNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventMessageDestroy);
	}
	
	return FALSE;
}

void OnEventMessageEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTMESSAGE), hdlgP, DlgEventMessageProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventPrintInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	SetWindowText(hdlgP, "编辑操作：屏幕打印");
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	
	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到内容");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventPrintEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND boddy;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTPRINT_TEXT_MSGID) {
		boddy = GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT);

	} else {
		return;
	}
	std::stringstream strstr;
	if (text[0] != '\0') {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), get_rid_of_return(text).c_str());
	}
	Edit_SetText(boddy, utf8_2_ansi(strstr.str().c_str()));

	return;
}

void On_DlgEventPrintCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // hero
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID), ns::clicked_variable);
		OnEventPrintEt(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID), EN_CHANGE);
		break;

	case IDC_ET_EVENTPRINT_TEXT_MSGID:
		OnEventPrintEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventPrintNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgEventPrintDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventPrintProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventPrintInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventPrintCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventPrintNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventPrintDestroy);
	}
	
	return FALSE;
}

void OnEventPrintEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTPRINT), hdlgP, DlgEventPrintProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventLabelInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	SetWindowText(hdlgP, "编辑操作：放置/擦除标签");
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	
	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到内容");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到X");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到Y");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventLabelEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND boddy;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTLABEL_TEXT_MSGID) {
		boddy = GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_TEXT);

	} else {
		return;
	}
	std::stringstream strstr;
	if (text[0] != '\0') {
		strstr << dsgettext(ns::_main.textdomain_.c_str(), get_rid_of_return(text).c_str());
	}
	Edit_SetText(boddy, utf8_2_ansi(strstr.str().c_str()));

	return;
}

void On_DlgEventLabelCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // hero
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID), ns::clicked_variable);
		OnEventPrintEt(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID, GetDlgItem(hdlgP, IDC_ET_EVENTPRINT_TEXT_MSGID), EN_CHANGE);
		break;
	case IDM_VARIABLE_ITEM1: // x
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_X), ns::clicked_variable);
		break;
	case IDM_VARIABLE_ITEM2: // y
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENTLABEL_Y), ns::clicked_variable);
		break;

	case IDC_ET_EVENTLABEL_TEXT_MSGID:
		OnEventLabelEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventLabelNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgEventLabelDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventLabelProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventLabelInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventLabelCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventLabelNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventLabelDestroy);
	}
	
	return FALSE;
}

void OnEventLabelEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTLABEL), hdlgP, DlgEventLabelProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgSetVariableInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：设置变量");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：设置变量");
	}

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到变量值");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_SETVARIABLE_OP);
	const std::vector<std::pair<std::string, std::string> >& op_map = tevent::tset_variable::op_map;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.c_str()));
	}

	hctl = GetDlgItem(hdlgP, IDC_LV_SETVARIABLE_ENV);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 120;
	lvc.pszText = "标识";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 160;
	lvc.iSubItem = 1;
	lvc.pszText = "描述";
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::clicked_command->update_to_ui_special(hdlgP);

	return FALSE;
}

void OnSetVariableEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_SETVARIABLE_NAME) {
		return;
	}

	tevent::tset_variable* set_variable = dynamic_cast<tevent::tset_variable*>(ns::clicked_command);

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_variable(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_NAMESTATUS), utf8_2_ansi(_("Invalid string")));
		Button_Enable(GetDlgItem(hdlgP, IDOK), FALSE);
		return;
	}
	Button_Enable(GetDlgItem(hdlgP, IDOK), TRUE);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_NAMESTATUS), "");
	if (set_variable->name_ == str) {
		return;
	}
		
	// update relative control.
	set_variable->name_ = str;
	return;
}

void On_DlgSetVariableCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::tset_variable* set_variable = dynamic_cast<tevent::tset_variable*>(ns::clicked_command);

	switch (id) {
	case IDC_ET_SETVARIABLE_NAME:
		OnSetVariableEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_VARIABLE_ITEM0:
		insert_str_2_edit(GetDlgItem(hdlgP, IDC_ET_EVENT_VALUE_MSGID), ns::clicked_variable);
		break;

	case IDC_ET_EVENT_VALUE_MSGID:
	case IDC_ET_EVENT_TEXTDOMAIN:
		set_variable->on_command(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		set_variable->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgSetVariableNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
	}
	return FALSE;
}

void On_DlgSetVariableDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgSetVariableProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgSetVariableInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgSetVariableCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgSetVariableNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgSetVariableDestroy);
	}
	
	return FALSE;
}

void OnSetVariableEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SETVARIABLE), hdlgP, DlgSetVariableProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventUnitInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：生成部队");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：生成部队");
	}

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TRAITS), dgettext_2_ansi("wesnoth-lib", "Traits"));

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到兵种");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到武将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到城市");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM3, "到势力");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM4, "到“X”坐标");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM5, "到“Y”坐标");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	lvc.pszText = "姓名";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 1;
	lvc.pszText = "特技";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 2;
	lvc.pszText = "统帅";
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// traits
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_TRAIT);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 120;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 120;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// X/Y
	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_X);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_X));

	hctl = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_Y);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_Y));

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnEventUnitEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl1;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTUNIT_TYPE) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_TYPE);

	} else if (id == IDC_ET_EVENTUNIT_HEROSARMY) {
		hctl1 = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);

	} else if (id == IDC_ET_EVENTUNIT_CITY) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_CITY);

	} else if (id == IDC_ET_EVENTUNIT_SIDE) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_SIDE);

	} else if (id == IDC_ET_EVENTUNIT_VARX) {
		hctl1 = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_X);
		hctl2 = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_X);

	} else if (id == IDC_ET_EVENTUNIT_VARY) {
		hctl1 = GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_Y);
		hctl2 = GetDlgItem(hdlgP, IDC_UD_EVENTUNIT_Y);

	} else {
		return;
	}

	ShowWindow(hctl1, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	if (hctl2) {
		ShowWindow(hctl2, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	}

	return;
}

void On_DlgEventUnitCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_TYPE), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM1: // heros_army
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM2: // city
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_CITY), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM3: // side
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_SIDE), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM4: // x
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARX), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM5: // y
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_VARY), ns::clicked_variable.c_str());
		break;

	case IDC_ET_EVENTUNIT_TYPE:
	// case IDC_ET_EVENTUNIT_HEROSARMY:
	case IDC_ET_EVENTUNIT_CITY:
	case IDC_ET_EVENTUNIT_SIDE:
	case IDC_ET_EVENTUNIT_VARX:
	case IDC_ET_EVENTUNIT_VARY:
		OnEventUnitEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_TOSERVICE:
		strcat_heros_str(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void eventunit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_EVENTUNIT_HEROSARMY) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	char text[_MAX_PATH];
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		HMENU hpopup = CreatePopupMenu();
		AppendMenu(hpopup, MF_STRING, IDM_TOSERVICE, "到在职");
		
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY), text, sizeof(text) / sizeof(text[0]));
		if (utils::to_vector_int(text).size() >= 3) {
			EnableMenuItem(hpopup, IDM_TOSERVICE, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(hpopup, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup);

		ns::clicked_hero = lvi.lParam;
	}

    return;
}

BOOL On_DlgEventUnitNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	if (lpNMHdr->code == NM_CLICK) {
		if ((lpNMHdr->idFrom == IDC_LV_EVENTUNIT_TRAIT) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 3) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
			strstr.str("");
			if (lpNMHdr->idFrom == IDC_LV_EVENTUNIT_TRAIT) {
				strstr << dsgettext("wesnoth-lib", "Traits") << "(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TRAITS), utf8_2_ansi(strstr.str().c_str()));
			}
		}
	} else if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			eventunit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgEventUnitDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventUnitInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventUnitCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventUnitNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventUnitDestroy);
	}
	
	return FALSE;
}

void OnEventUnitEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTUNIT), hdlgP, DlgEventUnitProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgSideHerosInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：调整势力武将");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：调整势力武将");
	}

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, utf8_2_ansi(_("To side")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_SIDE);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strstr.str("");
	strstr << _("NO.");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 120;
	lvc.iSubItem = 1;
	strstr.str("");
	strstr << dsgettext("wesnoth-lib", "Side");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	// candidate
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_CANDIDATE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 90;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "side feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 5;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 5, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// heros
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEHEROS_HEROS);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 90;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 35;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 45;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "tactic"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgSideHerosCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tsideheros& sideheros = *dynamic_cast<tevent::tsideheros*>(ns::clicked_command);;
	std::stringstream strstr;

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEHEROS_SIDE), ns::clicked_variable.c_str());
		break;
	case IDM_TOSIDE:
		strstr.str("");
		strstr << ns::clicked_side;
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEHEROS_SIDE), strstr.str().c_str());
		break;

	case IDM_TOSERVICE:
		sideheros.heros_.insert(ns::clicked_hero);
		sideheros.update_to_ui_special(hdlgP);
		break;
	case IDM_DELETE_ITEM0:
		sideheros.heros_.erase(ns::clicked_hero);
		sideheros.update_to_ui_special(hdlgP);
		break;
	case IDM_DELETE_ITEM1:
		sideheros.heros_.clear();
		sideheros.update_to_ui_special(hdlgP);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void sideheros_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_SIDEHEROS_SIDE &&
		lpNMHdr->idFrom != IDC_LV_SIDEHEROS_CANDIDATE &&
		lpNMHdr->idFrom != IDC_LV_SIDEHEROS_HEROS) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		HMENU hpopup = CreatePopupMenu();

		if (lpNMHdr->idFrom == IDC_LV_SIDEHEROS_SIDE) {
			AppendMenu(hpopup, MF_STRING, IDM_TOSIDE, utf8_2_ansi(_("To side")));
			ns::clicked_side = lvi.lParam;

		} else if (lpNMHdr->idFrom == IDC_LV_SIDEHEROS_CANDIDATE) {
			AppendMenu(hpopup, MF_STRING, IDM_TOSERVICE, utf8_2_ansi(_("To service")));
			ns::clicked_hero = lvi.lParam;

		} else if (lpNMHdr->idFrom == IDC_LV_SIDEHEROS_HEROS) {
			AppendMenu(hpopup, MF_STRING, IDM_DELETE_ITEM0, utf8_2_ansi(_("Delete")));
			AppendMenu(hpopup, MF_SEPARATOR, 0, NULL);
			AppendMenu(hpopup, MF_STRING, IDM_DELETE_ITEM1, utf8_2_ansi(_("Delete all")));

			ns::clicked_hero = lvi.lParam;
		}
		
		TrackPopupMenuEx(hpopup, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup);
	}

    return;
}

BOOL On_DlgSideHerosNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			sideheros_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgSideHerosDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgSideHerosProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgSideHerosInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgSideHerosCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgSideHerosNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgSideHerosDestroy);
	}
	
	return FALSE;
}

void OnSideHerosEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SIDEHEROS), hdlgP, DlgSideHerosProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgStoreUnitInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作: store_unit");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作: store_unit");
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgStoreUnitCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::tstore_unit* store_unit = dynamic_cast<tevent::tstore_unit*>(ns::clicked_command);

	switch (id) {
	case IDC_BT_STOREUNIT_FILTER:
		ns::filter = &store_unit->filter_;
		if (OnEventFilterBt2(hdlgP, id)) {
			store_unit->update_to_ui_special(hdlgP);
		}
		break;

	case IDOK:
		changed = TRUE;
		store_unit->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgStoreUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgStoreUnitInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgStoreUnitCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnStoreUnitEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_STOREUNIT), hdlgP, DlgStoreUnitProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventRenameInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作: rename");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作: rename");
	}

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NUMBER), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Renamed to")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgEventRenameCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::trename* rename = dynamic_cast<tevent::trename*>(ns::clicked_command);

	switch (id) {
	case IDC_ET_EVENT_VALUE_MSGID:
	case IDC_ET_EVENT_TEXTDOMAIN:
		rename->on_command(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		rename->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgEventRenameProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventRenameInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventRenameCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnEventRenameEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTRENAME), hdlgP, DlgEventRenameProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgMsgidInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_event_item == ma_new) {
		strstr << _("Add");
	} else {
		strstr << _("Edit");
	}
	strstr << " ";
	if (ns::type == IDC_LV_OBJECTIVES_WIN) {
		strstr << _("Win condition");
	} else if (ns::type == IDC_LV_OBJECTIVES_LOSE) {
		strstr << _("Lose condition");
	}
	SetWindowText(hdlgP, utf8_2_ansi(strstr.str().c_str()));
	if (ns::action_event_item != ma_new) {
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	}

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SUMMARY), utf8_2_ansi(_("Summary")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WIN), utf8_2_ansi(_("Win condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOSE), utf8_2_ansi(_("Lose condition")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	const tevent::tobjectives* objectives = dynamic_cast<const tevent::tobjectives*>(ns::clicked_command);

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_MSGID_MSGID);
	std::vector<std::string>::const_iterator it;
	if (ns::type == IDC_LV_OBJECTIVES_WIN) {
		it = objectives->win_.begin();
	} else if (ns::type == IDC_LV_OBJECTIVES_LOSE) {
		it = objectives->lose_.begin();
	} else {
		return FALSE;
	}
	std::advance(it, ns::clicked_item);

	Edit_SetText(hctl, it->c_str());
	hctl = GetDlgItem(hdlgP, IDC_ET_MSGID_MSGSTR);
	Edit_SetText(hctl, utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), it->c_str())));

	return FALSE;
}

void OnMsgidEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_MSGID_MSGID) {
		hctl = GetDlgItem(hdlgP, IDC_ET_MSGID_MSGSTR);
	} else {
		return;
	}
	strstr.str("");
	if (text[0]) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), text));
	}
	Edit_SetText(hctl, strstr.str().c_str());

	Button_Enable(GetDlgItem(hdlgP, IDOK), text[0] != '\0');
	return;
}

void On_DlgMsgidCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent::tobjectives* objectives = dynamic_cast<tevent::tobjectives*>(ns::clicked_command);

	BOOL changed = FALSE;

	switch (id) {
	case IDC_ET_MSGID_MSGID:
		OnMsgidEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		objectives->from_ui_special_msgid(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgMsgidProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgMsgidInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgMsgidCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnMsgidAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	HWND hctl = GetDlgItem(hdlgP, ns::type);
	GetWindowRect(hctl, &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tevent::tobjectives* objectives = dynamic_cast<tevent::tobjectives*>(ns::clicked_command);
	std::vector<std::string>* mess = NULL;
	if (ns::type == IDC_LV_OBJECTIVES_WIN) {
		mess = &objectives->win_;
	} else if (ns::type == IDC_LV_OBJECTIVES_LOSE) {
		mess = &objectives->lose_;
	} else {
		return;
	}
	mess->push_back("msgid...");
	ns::clicked_item = mess->size() - 1;

	ns::action_event_item = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_MSGID), hdlgP, DlgMsgidProc, lParam)) {
		ns::clicked_command->update_to_ui_special(hdlgP);
	} else {
		std::vector<std::string>::iterator it = mess->begin();
		std::advance(it, ns::clicked_item);
		mess->erase(it);
	}

	return;
}

void OnMsgidEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	HWND hctl = GetDlgItem(hdlgP, ns::type);
	GetWindowRect(hctl, &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_MSGID), hdlgP, DlgMsgidProc, lParam)) {
		ns::clicked_command->update_to_ui_special(hdlgP);
	}

	return;
}

void OnMsgidDeleteBt(HWND hdlgP)
{
	tevent::tobjectives* objectives = dynamic_cast<tevent::tobjectives*>(ns::clicked_command);
	std::vector<std::string>* mess = NULL;
	if (ns::type == IDC_LV_OBJECTIVES_WIN) {
		mess = &objectives->win_;
	} else if (ns::type == IDC_LV_OBJECTIVES_LOSE) {
		mess = &objectives->lose_;
	} else {
		return;
	}

	std::vector<std::string>::iterator it = mess->begin();
	std::advance(it, ns::clicked_item);
	mess->erase(it);
	objectives->update_to_ui_special(hdlgP);
}

BOOL On_DlgObjectivesInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	char text[_MAX_PATH];
	strstr << utf8_2_ansi(_("Edit objectives"));
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SUMMARY), utf8_2_ansi(_("Summary")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WIN), utf8_2_ansi(_("Win condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOSE), utf8_2_ansi(_("Lose condition")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_OBJECTIVES_WIN);

	LVCOLUMN lvc;
	// msgid
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 220;
	strstr.str("");
	strstr << _("msgid");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 250;
	lvc.iSubItem = 1;
	strstr.str("");
	strstr << _("translated");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);


	hctl = GetDlgItem(hdlgP, IDC_LV_OBJECTIVES_LOSE);
	// msgid
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 220;
	strstr.str("");
	strstr << _("msgid");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 250;
	lvc.iSubItem = 1;
	strstr.str("");
	strstr << _("translated");
	strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::clicked_command->update_to_ui_special(hdlgP);

	return FALSE;
}

void OnObjectivesEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_OBJECTIVES_SUMMARY_MSGID) {
		hctl = GetDlgItem(hdlgP, IDC_ET_OBJECTIVES_SUMMARY);
		strcpy(text, get_rid_of_return(text).c_str());
	} else {
		return;
	}
	strstr.str("");
	if (text[0]) {
		if (id == IDC_ET_OBJECTIVES_SUMMARY_MSGID) {
			strstr << utf8_2_ansi(insert_return(dgettext(ns::_main.textdomain_.c_str(), text)).c_str());
		} else {
			strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), text));
		}
	}
	Edit_SetText(hctl, strstr.str().c_str());

	return;
}

void On_DlgObjectivesCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_ADD:
		OnMsgidAddBt(hdlgP);
		break;
	case IDM_EDIT:
		OnMsgidEditBt(hdlgP);
		break;
	case IDM_DELETE:
		OnMsgidDeleteBt(hdlgP);
		break;

	case IDC_ET_OBJECTIVES_SUMMARY_MSGID:
		OnObjectivesEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void objectives_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM lvi;
	LPNMITEMACTIVATE lpnmitem;
	tevent::tobjectives* objectives = dynamic_cast<tevent::tobjectives*>(ns::clicked_command);
	const std::vector<std::string>* mess = NULL;

	if (lpNMHdr->idFrom == IDC_LV_OBJECTIVES_WIN) {
		mess = &objectives->win_;
	} else if (lpNMHdr->idFrom == IDC_LV_OBJECTIVES_LOSE) {
		mess = &objectives->lose_;
	} else {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	std::stringstream strstr;
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	HMENU hpopup = CreatePopupMenu();
	strstr.str("");
	strstr << _("Add") << "...";
	AppendMenu(hpopup, MF_STRING, IDM_ADD, utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << _("Edit") << "...";
	AppendMenu(hpopup, MF_STRING, IDM_EDIT, utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << _("Delete");
	AppendMenu(hpopup, MF_STRING, IDM_DELETE, utf8_2_ansi(strstr.str().c_str()));
	
	if (lpnmitem->iItem < 0) {
		EnableMenuItem(hpopup, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hpopup, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
	} else if (mess->size() <= 1) {
		EnableMenuItem(hpopup, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
	}

	TrackPopupMenuEx(hpopup, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	DestroyMenu(hpopup);

	ns::clicked_item = lvi.lParam;
	ns::type = id;

    return;
}

void objectives_notify_handler_dblclk(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_OBJECTIVES_WIN &&
		lpNMHdr->idFrom != IDC_LV_OBJECTIVES_LOSE) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	std::stringstream strstr;
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem < 0) {
		return;
	}
	ns::clicked_item = lvi.lParam;
	ns::type = id;

	OnMsgidEditBt(hdlgP);

    return;
}

BOOL On_DlgObjectivesNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			objectives_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	} else if (lpNMHdr->code == NM_DBLCLK) {
		objectives_notify_handler_dblclk(hdlgP, DlgItem, lpNMHdr);
	}
	return FALSE;
}

BOOL CALLBACK DlgObjectivesProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgObjectivesInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgObjectivesCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgObjectivesNotify);
	}
	
	return FALSE;
}

void OnEventObjectivesEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_OBJECTIVES), hdlgP, DlgObjectivesProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventFilterInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑单位筛选");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加单位筛选");
	}

	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_EVENTFILTER_HP), utf8_2_ansi(_("HP > 0(if not check, HP is free)")));

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到必须存在的武将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到X");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到Y");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTFILTER_MUSTHEROS);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	lvc.pszText = "姓名";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 1;
	lvc.pszText = "特技";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 2;
	lvc.pszText = "统帅";
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::filter->tevent::tfilter_::update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgEventFilterCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // must_heros
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM1: // X
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_X), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM2: // Y
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_Y), ns::clicked_variable.c_str());
		break;

	case IDM_TOSERVICE:
		strcat_heros_str(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS);
		break;

	case IDOK:
		changed = TRUE;
		ns::filter->tevent::tfilter_::from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void eventfilter_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->idFrom != IDC_LV_EVENTFILTER_MUSTHEROS) {
		return;
	}

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	
	char text[_MAX_PATH];
	POINT point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem >= 0) {
		HMENU hpopup = CreatePopupMenu();
		AppendMenu(hpopup, MF_STRING, IDM_TOSERVICE, "到必须");
		
		Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS), text, sizeof(text) / sizeof(text[0]));
		if (utils::to_vector_int(text).size() >= 3) {
			EnableMenuItem(hpopup, IDM_TOSERVICE, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(hpopup, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup);

		ns::clicked_hero = lvi.lParam;
	}

    return;
}

BOOL On_DlgEventFilterNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		} else {
			eventfilter_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgEventFilterDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgEventFilterProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventFilterInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventFilterCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventFilterNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgEventFilterDestroy);
	}
	
	return FALSE;
}

void OnEventFilterBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTFILTER), hdlgP, DlgEventFilterProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

bool OnEventFilterBt2(HWND hdlgP, int id)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, id), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTFILTER), hdlgP, DlgEventFilterProc, lParam)) {
		return true;
	}
	return false;
}

BOOL On_DlgFilterHeroInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑武将筛选");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加武将筛选");
	}

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到必须存在的武将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到X");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到Y");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	// variable
	valuex::cumulate_variables_to_lv(hdlgP);

	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgFilterHeroCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // must_heros
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_MUSTHEROS), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM1: // X
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_X), ns::clicked_variable.c_str());
		break;
	case IDM_VARIABLE_ITEM2: // Y
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTFILTER_Y), ns::clicked_variable.c_str());
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgFilterHeroNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	if (lpNMHdr->code == NM_RCLICK) {
		if (lpNMHdr->idFrom == IDC_LV_EVENT_VARIABLE) {
			valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
		}
	}
	return FALSE;
}

void On_DlgFilterHeroDestroy(HWND hdlgP)
{
	DestroyMenu(ns::hpopup_variable);
}

BOOL CALLBACK DlgFilterHeroProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgFilterHeroInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgFilterHeroCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgFilterHeroNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgFilterHeroDestroy);
	}
	
	return FALSE;
}

void OnFilterHeroBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event_item = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FILTERHERO), hdlgP, DlgFilterHeroProc, lParam)) {
		scroll::treeview_update_scroll(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), cb_treeview_update_scroll_event, &evt);
	}

	return;
}

BOOL On_DlgEventEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	utils::string_map symbols;

	strstr.str("");
	if (ns::action_event == ma_edit) {
		symbols["number"] = lexical_cast_default<std::string>(ns::clicked_event + 1);
		strstr << utf8_2_ansi(vgettext2("Edit $number|th event", symbols).c_str());
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add event"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	ns::hpopup_new_event = CreatePopupMenu();
	int index = 0;
	for (std::vector<tevent::tevent_name>::const_iterator it = tevent::name_map.begin(); it != tevent::name_map.end(); ++ it, index ++) {
		strstr.str("");
		strstr << it->id_ << "(" << it->name_ << ")";
		AppendMenu(ns::hpopup_new_event, MF_STRING, IDM_NEW_EVENT0 + index, strstr.str().c_str());
	}

	ns::hpopup_new = CreatePopupMenu();
	AppendMenu(ns::hpopup_new, MF_POPUP, (UINT_PTR)(ns::hpopup_new_event), utf8_2_ansi(_("Event branch")));
	AppendMenu(ns::hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM1, utf8_2_ansi(_("Judge branch")));
	AppendMenu(ns::hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM2, utf8_2_ansi(_("Set variable")));
	AppendMenu(ns::hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM3, utf8_2_ansi(_("Generate troop")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM4, utf8_2_ansi(_("Kill troop/hero")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM5, utf8_2_ansi(_("End level")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM6, utf8_2_ansi(_("Join in troop")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM7, utf8_2_ansi(_("Modify unit(troop, city, artifical)")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM8, utf8_2_ansi(_("Modify side")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM9, utf8_2_ansi(_("Modify city")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM10, utf8_2_ansi(_("Pop message")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM11, utf8_2_ansi(_("Print/Clear on-screen")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM12, utf8_2_ansi(_("Set/Clear label")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM13, utf8_2_ansi(_("Allow undo")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM14, utf8_2_ansi(_("Adjust heros in side")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM15, utf8_2_ansi(_("Store unit")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM16, utf8_2_ansi(_("Rename")));
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM17, utf8_2_ansi(_("Change scenario objectives")));

	ns::hpopup_event = CreatePopupMenu();
	AppendMenu(ns::hpopup_event, MF_POPUP, (UINT_PTR)(ns::hpopup_new), utf8_2_ansi(_("Append after it")));
	AppendMenu(ns::hpopup_event, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
	AppendMenu(ns::hpopup_event, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::himl_evt = ImageList_Create(15, 15, ILC_COLOR24, 7, 0);
	ImageList_SetBkColor(ns::himl_evt, RGB(236, 233, 216));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_EVENT));
	ns::iico_evt_event = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_FILTER));
	ns::iico_evt_filter = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_COMMAND));
	ns::iico_evt_command = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_IF));
	ns::iico_evt_if = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_CONDITION));
	ns::iico_evt_condition = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_THEN));
	ns::iico_evt_then = ImageList_AddIcon(ns::himl_evt, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_EVT_ATTRIBUTE));
	ns::iico_evt_attribute = ImageList_AddIcon(ns::himl_evt, hicon);

	HWND hctl = GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER);

	TreeView_SetImageList(hctl, ns::himl_evt, TVSIL_NORMAL);

	evt.update_to_ui_event_edit(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), TVI_ROOT);
	return FALSE;
}

void On_DlgEventEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcommand* new_cmd = NULL;

	switch (id) {
	case IDM_NEW_ITEM1: // if
		if (!new_cmd) {
			new_cmd = new tevent::tif();
		}
	case IDM_NEW_ITEM2: // set_variable
		if (!new_cmd) {
			new_cmd = new tevent::tset_variable();
		}
	case IDM_NEW_ITEM3: // unit
		if (!new_cmd) {
			new_cmd = new tevent::tunit();
		}
	case IDM_NEW_ITEM4: // kill
		if (!new_cmd) {
			new_cmd = new tevent::tkill();
		}
	case IDM_NEW_ITEM5: // endlevel
		if (!new_cmd) {
			new_cmd = new tevent::tendlevel();
		}
	case IDM_NEW_ITEM6: // join
		if (!new_cmd) {
			new_cmd = new tevent::tjoin();
		}
	case IDM_NEW_ITEM7: // modify unit
		if (!new_cmd) {
			new_cmd = new tevent::tmodify_unit();
		}
	case IDM_NEW_ITEM8: // modify side
		if (!new_cmd) {
			new_cmd = new tevent::tmodify_side();
		}
	case IDM_NEW_ITEM9: // modify city
		if (!new_cmd) {
			new_cmd = new tevent::tmodify_city();
		}
	case IDM_NEW_ITEM10: // message
		if (!new_cmd) {
			new_cmd = new tevent::tmessage();
		}
	case IDM_NEW_ITEM11: // print
		if (!new_cmd) {
			new_cmd = new tevent::tprint();
		}
	case IDM_NEW_ITEM12: // label
		if (!new_cmd) {
			new_cmd = new tevent::tlabel();
		}
	case IDM_NEW_ITEM13: // allow undo
		if (!new_cmd) {
			new_cmd = new tevent::tallow_undo();
		}
	case IDM_NEW_ITEM14: // side heros
		if (!new_cmd) {
			new_cmd = new tevent::tsideheros();
		}
	case IDM_NEW_ITEM15: // store unit
		if (!new_cmd) {
			new_cmd = new tevent::tstore_unit();
		}
	case IDM_NEW_ITEM16: // rename
		if (!new_cmd) {
			new_cmd = new tevent::trename();
		}
	case IDM_NEW_ITEM17: // objectives
		if (!new_cmd) {
			new_cmd = new tevent::tobjectives();
		}
		evt.new_command(new_cmd, hdlgP);
		break;

	case IDM_EDIT:
		TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (!evt.do_edit(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER))) {
			TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
		}

		break;
	case IDM_DELETE:
		TreeView_SetItemState (GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (MessageBox(hdlgP, "您想删除该条操作吗？", "确认删除", MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			TreeView_SetItemState(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
			return;
		}
		evt.erase_command(ns::clicked_command, hdlgP);
		break;

	case IDOK:
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;

	default:
		if (id >= IDM_NEW_EVENT0 && id < IDM_NEW_EVENT0 + (int)tevent::name_map.size()) {
			tevent* new_event = new tevent();
			new_event->name_ = tevent::name_map[id - IDM_NEW_EVENT0].id_;
			evt.new_command(new_event, hdlgP);
		}
	}
}

BOOL On_DlgEventEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	LPNMTREEVIEW			lpnmitem;
	HTREEITEM				htvi;
	TVITEMEX				tvi, tvi1;
	POINT					point;
	
	if (lpNMHdr->idFrom != IDC_TV_EVENTEDIT_EXPLORER) {
		return FALSE;
	}
	if (lpNMHdr->code != NM_RCLICK && lpNMHdr->code != NM_DBLCLK) {
		return FALSE;
	}

	lpnmitem = (LPNMTREEVIEW)lpNMHdr;

	// NM_RCLICK/NM_CLICK/NM_DBLCLK这些通知被发来后,其附代参数没法指定是哪个叶子句柄,
	// 需通过判断鼠标坐标来判断是哪个叶子被按下?
	// 1. GetCursorPos, 得到屏幕坐标系下的鼠标坐标
	// 2. TreeView_HitTest1(自写宏),由屏幕坐标系下的鼠标坐标返回指向的叶子句柄
	GetCursorPos(&point);	// 得到的是屏幕坐标
	TreeView_HitTest1(lpNMHdr->hwndFrom, point, htvi);
	
	TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	if (!htvi) {
		return FALSE;
	}
	if (lpNMHdr->code == NM_DBLCLK) {
		if (tvi.cChildren) {
			return FALSE;
		}
		htvi = TreeView_GetParent(lpNMHdr->hwndFrom, htvi);
		TreeView_GetItem1(lpNMHdr->hwndFrom, htvi, &tvi, TVIF_PARAM | TVIF_CHILDREN, NULL);
	}
	ns::clicked_param = tvi.lParam;
	ns::clicked_command = NULL;

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
		TreeView_GetItem1(lpNMHdr->hwndFrom, TreeView_GetParent(lpNMHdr->hwndFrom, htvi), &tvi1, TVIF_PARAM, NULL);
		ns::clicked_command = reinterpret_cast<tevent::tcommand*>(tvi1.lParam);

	} else if (tvi.lParam != tevent::PARAM_NONE) {
		ns::clicked_command = reinterpret_cast<tevent::tcommand*>(tvi.lParam);
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->code == NM_RCLICK) {
		if (!tvi.cChildren) {
			return FALSE;
		}

		bool disable = false;
		// new
		if (ns::clicked_command) {
			if (ns::clicked_command->type_ == tcommand::FILTER || ns::clicked_command->type_ == tevent::tcommand::CONDITION) {
				disable = true;
			}
		}
		if (disable) {
			EnableMenuItem(ns::hpopup_event, (UINT_PTR)(ns::hpopup_new), MF_BYCOMMAND | MF_GRAYED);
		}

		// edit
		disable = false;
		if (ns::clicked_command) {
			if (ns::clicked_command->type_ == tevent::tcommand::IF) {
				disable = true;
			} else {
				size_t index = std::distance(evt.commands_.begin(), std::find(evt.commands_.begin(), evt.commands_.end(), ns::clicked_command));
				std::vector<tevent::tevent_name>::const_iterator it = tevent::find_name(evt.name_);
				if (index < tevent::fixed_commands && it != tevent::name_map.end()) {
					if (index == 0 && !it->filter_.first) {
						disable = true;
					} else if (index == 1 && !it->filter_second_.first) {
						disable = true;
					} else if (index == 2 && !it->filter_hero_.first) {
						disable = true;
					}
				}
			}
		} else if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
			disable = true;
		}
		if (disable) {
			EnableMenuItem(ns::hpopup_event, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
		}

		// delete
		disable = false;
		if (ns::clicked_command) {
			if (ns::clicked_command->type_ == tcommand::FILTER || ns::clicked_command->type_ == tcommand::FILTER_HERO) {
				disable = true;
			} else if ((ns::clicked_command->type_ == tcommand::EVENT && ns::clicked_command == &evt) || 
				ns::clicked_command->type_ == tcommand::CONDITION) {
				disable = true;
			}
		} else if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
			disable = true;
		}
		if (disable) {
			EnableMenuItem(ns::hpopup_event, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}

		
		TrackPopupMenuEx(ns::hpopup_event, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		EnableMenuItem(ns::hpopup_event, (UINT_PTR)(ns::hpopup_new), MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(ns::hpopup_event, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(ns::hpopup_event, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);

	} else if (lpNMHdr->code == NM_DBLCLK) {
		// edit
		bool disable = false;
		if (ns::clicked_command && ns::clicked_command->type_ == tevent::tcommand::IF) {
			disable = true;
		} else if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
			disable = true;
		}
		if (!disable) {
			PostMessage(hdlgP, WM_COMMAND, IDM_EDIT, 0);
		}
	}
	ns::clicked_htvi = htvi;

    return FALSE;
}

void On_DlgEventEditDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_evt);

	DestroyMenu(ns::hpopup_new_event);
	DestroyMenu(ns::hpopup_new);
	DestroyMenu(ns::hpopup_event);
}

BOOL CALLBACK DlgEventEditProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventEditInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventEditCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY,  On_DlgEventEditNotify);
	HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgEventEditDestroy);
	}
	
	return FALSE;
}

void OnEventAddBt(HWND hdlgP, const std::string& name)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_EVENT), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::cityno_map = ns::_scenario[ns::current_scenario].generate_cityno_map();
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	if (!scenario.new_event(name)) {
		return;
	}
	// set current city to new added city.
	ns::clicked_event = scenario.event_.size() - 1;
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTEDIT), hdlgP, DlgEventEditProc, lParam)) {
		evt.update_to_ui(hdlgP, ns::clicked_event);
		scenario.set_dirty(tscenario::BIT_EVENT, !scenario.event_equal());
	} else {
		scenario.erase_event(ns::clicked_event, NULL);
	}

	return;
}

void OnEventEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_EVENT), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	ns::cityno_map = ns::_scenario[ns::current_scenario].generate_cityno_map();
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	ns::action_event = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTEDIT), hdlgP, DlgEventEditProc, lParam)) {
		evt.update_to_ui(hdlgP, ns::clicked_event);
		scenario.set_dirty(tscenario::BIT_EVENT, !scenario.event_equal()); 
	}

	return;
}

void OnEventDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	char text[_MAX_PATH];
	std::stringstream strstr;
	utils::string_map symbols;

	symbols["number"] = lexical_cast_default<std::string>(ns::clicked_event + 1);
	strstr << utf8_2_ansi(vgettext2("Do you want to delete $number|th event?", symbols).c_str());

	strcpy(text, utf8_2_ansi(_("Confirm delete")));
	int retval = MessageBox(gdmgr._hwnd_main, strstr.str().c_str(), text, MB_YESNO);
	if (retval == IDNO) {
		return;
	}

	scenario.erase_event(ns::clicked_event, hdlgP);

	scenario.set_dirty(tscenario::BIT_EVENT, !scenario.event_equal()); 
	return;
}

#endif