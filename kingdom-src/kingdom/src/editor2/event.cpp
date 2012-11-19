#include "global.hpp"
#include "game_config.hpp"
#include "foreach.hpp"
#include "loadscreen.hpp"
#include "DlgCampaignProc.hpp"
#include <string.h>

#include "resource.h"

#include "xfunc.h"
#include "struct.h"
#include "win32x.h"
#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "map_location.hpp"

void OnEventFilterBt(HWND hdlgP);
bool OnEventFilterBt2(HWND hdlgP);
void OnSetVariableEditBt(HWND hdlgP);
void OnEventKillEditBt(HWND hdlgP);
void OnEventJoinEditBt(HWND hdlgP);
void OnEventUnitEditBt(HWND hdlgP);
void OnConditionEditBt(HWND hdlgP);
BOOL CALLBACK DlgVariableProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgHaveUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace ns {
	int action_event_item;

	tevent::tfilter_* filter;
	tevent::tcommand* clicked_command;
	std::string clicked_variable;
	LPARAM clicked_param;
	HTREEITEM clicked_htvi;

	std::map<std::string, std::string> cumulate_variables;
	
	HIMAGELIST himl_evt;
	int iico_evt_event;	
	int iico_evt_filter;
	int iico_evt_command;
	int iico_evt_if;
	int iico_evt_condition;
	int iico_evt_then;
	int iico_evt_attribute;

	HMENU hpopup_new;
	HMENU hpopup_event;
	HMENU hpopup_variable;
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

std::set<int> to_set_int(const std::string& value)
{
	std::vector<std::string> vstr = utils::split(value);
	std::set<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.insert(lexical_cast_default<int>(*it));
	}
	return ret;
}

void cumulate_variables_to_lv(HWND hdlgP)
{
	char text[_MAX_PATH];
	
	HWND hwndlv = GetDlgItem(hdlgP, IDC_LV_EVENT_VARIABLE);

	LVCOLUMN lvc;
	// variable
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 70;
	lvc.pszText = "标识";
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hwndlv, 0, &lvc);

	lvc.cx = 100;
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

std::vector<std::pair<std::string, std::string> > tevent::name_map;

void tevent::from_config_branch(const config& cfg, std::vector<tcommand*>& b)
{
	foreach (const config::any_child& tmp, cfg.all_children_range()) {
		tcommand* c = NULL;
		if (tmp.key == "set_variable") {
			c = new tset_variable();
		} else if (tmp.key == "kill") {
			c = new tkill();
		} else if (tmp.key == "join") {
			c = new tjoin();
		} else if (tmp.key == "unit") {
			c = new tunit();
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
		} else if (cmd->type_ == tcommand::JOIN) {
			new_cmd = new tjoin(*dynamic_cast<const tjoin*>(cmd));
		} else if (cmd->type_ == tcommand::UNIT) {
			new_cmd = new tunit(*dynamic_cast<const tunit*>(cmd));
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

		} else if (a->type_ == tcommand::JOIN) {
			if (*dynamic_cast<const tjoin*>(a) != *dynamic_cast<const tjoin*>(b)) return false;

		} else if (a->type_ == tcommand::UNIT) {
			if (*dynamic_cast<const tunit*>(a) != *dynamic_cast<const tunit*>(b)) return false;

		} else if (a->type_ == tcommand::IF) {
			if (*dynamic_cast<const tif*>(a) != *dynamic_cast<const tif*>(b)) return false;

		} 
	}
	return true;
}

tevent::tevent()
	: name_()
	, first_time_only_(true)
	, filter_()
	, second_filter_()
	, commands_()
{
	if (name_map.empty()) {
		name_map.push_back(std::make_pair<std::string, std::string>("new turn", "新回合"));
		name_map.push_back(std::make_pair<std::string, std::string>("last breach", "攻击时部队被击溃"));
	}
}

tevent::tevent(const tevent& that)
	: name_(that.name_)
	, first_time_only_(that.first_time_only_)
	, filter_(that.filter_)
	, second_filter_(that.second_filter_)
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
	filter_ = that.filter_;
	second_filter_ = that.second_filter_;

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

	filter_.from_config(event_cfg.child("filter"));
	second_filter_.from_config(event_cfg.child("second_filter"));

	for (std::vector<tcommand*>::iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		delete *it;
	}
	commands_.clear();

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
	strstr << (first_time_only_? "是": "不是");
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tevent::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "只触发一次: " << (first_time_only_? "是": "不是");
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::update_to_ui_event_edit(HWND hdlgP)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER);

	HTREEITEM htvi_root, htvi_filter, htvi_second_filter;
	// name
	htvi_root = TreeView_AddLeaf(hctl, TVI_ROOT);
	strcpy(text, name_.c_str());
	TreeView_SetItem1(hctl, htvi_root, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		PARAM_EVENT, ns::iico_evt_event, ns::iico_evt_event, 1, text);

	update_to_ui_event_edit(hctl, htvi_root);

	// filter/second filter
	for (int index = 0; index < 2; index ++) {
		HTREEITEM htvi1;
		tfilter* filter;
		LPARAM param;
		strstr.str("");
		if (index == 0) {
			htvi_filter = TreeView_AddLeaf(hctl, htvi_root);
			strstr << "条件I: 第一单位筛选";
			htvi1 = htvi_filter;
			filter = &filter_;
			param = PARAM_FILTER;
		} else {
			htvi_second_filter = TreeView_AddLeaf(hctl, htvi_root);
			strstr << "条件II: 第二单位筛选";
			htvi1 = htvi_second_filter;
			filter = &second_filter_;
			param = PARAM_SECONDFILTER;
		}
		strcpy(text, strstr.str().c_str());
		TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
			param, ns::iico_evt_filter, ns::iico_evt_filter, 1, text);

		filter->update_to_ui_event_edit(hctl, htvi1);
	}
	for (std::vector<tcommand*>::const_iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		tcommand& cmd = **it;
		cmd.update_to_ui_event_edit(hctl, htvi_root);
	}

	TreeView_Walk(hctl, TVI_ROOT, TRUE, cb_treeview_walk_expand, NULL, FALSE);
}

void tevent::generate(std::stringstream& strstr, const std::string& prefix) const
{
	strstr << prefix << "[event]\n";

	strstr << prefix << "\tname = " << name_ << "\n";
	strstr << prefix << "\tfirst_time_only = " << (first_time_only_? "yes": "no") << "\n";

	if (!filter_.is_null()) {
		strstr << prefix << "\t[filter]\n";
		filter_.generate(strstr, prefix + "\t\t");
		strstr << prefix << "\t[/filter]\n";
	}
	if (!second_filter_.is_null()) {
		strstr << prefix << "\t[filter]\n";
		second_filter_.generate(strstr, prefix + "\t\t");
		strstr << prefix << "\t[/filter]\n";
	}
	strstr << prefix << "\n";

	for (std::vector<tcommand*>::const_iterator it = commands_.begin(); it != commands_.end(); ++ it) {
		const tcommand* cmd = *it;
		cmd->generate(strstr, prefix + "\t");
	}

	strstr << prefix << "[/event]\n";
}

bool tevent::operator==(const tevent& that) const
{
	if (name_ != that.name_) return false;
	if (first_time_only_ != that.first_time_only_) return false;
	if (filter_ != that.filter_) return false;
	if (second_filter_ != that.second_filter_) return false;

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
		if (cmd->type_ == tcommand::IF) {
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

	} else if (ns::clicked_param == PARAM_FILTER || ns::clicked_param == PARAM_SECONDFILTER) {
		commands_.insert(commands_.begin(), new_cmd);
	}
	TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
	update_to_ui_event_edit(hdlgP);
}

void tevent::erase_command(const tcommand* cmd, HWND hdlgP)
{
	std::vector<tcommand*>* destination = NULL;
	int pos = -1;

	locate(cmd, commands_, &destination, pos);

	delete (*destination)[pos];
	destination->erase(destination->begin() + pos);

	TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
	update_to_ui_event_edit(hdlgP);
}

bool tevent::do_edit(HWND hwndtv)
{
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
		} else if (ns::clicked_command->type_ == tcommand::JOIN) {
			OnEventJoinEditBt(hdlgP);
		} else if (ns::clicked_command->type_ == tcommand::CONDITION) {
			OnConditionEditBt(hdlgP);
		} else {
			processed = false;
		}

	} else if (ns::clicked_param == PARAM_EVENT) {
		first_time_only_ = !first_time_only_;
		TreeView_DeleteAllItems(hwndtv);
		update_to_ui_event_edit(hdlgP);

	} else if (ns::clicked_param == PARAM_FILTER) {
		ns::filter = &filter_;
		OnEventFilterBt(hdlgP);

	} else if (ns::clicked_param == PARAM_SECONDFILTER) {
		ns::filter = &second_filter_;
		OnEventFilterBt(hdlgP);
	} else {
		processed = false;
	}

	return processed;
}

void tevent::tfilter_::from_config(const config& cfg)
{
	master_hero_ = HEROS_INVALID_NUMBER;
	must_heros_.clear();

	if (!cfg) {
		return;
	}
	master_hero_ = cfg["master_hero"].to_int(HEROS_INVALID_NUMBER);

	must_heros_.clear();
	std::vector<std::string> vstr = utils::split(cfg["must_heros"].str());
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		must_heros_.insert(lexical_cast_default<int>(*it));
	}

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
}

void tevent::tfilter_::from_ui_special(HWND hdlgP)
{
	tfilter_& filter = *ns::filter;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_MASTERHERO);
	filter.master_hero_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	filter.must_heros_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTFILTER_MUSTHEROS);
	LVITEM lvi;
	for (int index = 0; index < ListView_GetItemCount(hctl); index ++) {
		if (ListView_GetCheckState(hctl, index)) {
			lvi.iItem = index;
			lvi.mask = LVIF_PARAM;
			lvi.iSubItem = 0;
			lvi.pszText = NULL;
			lvi.cchTextMax = 0;
			ListView_GetItem(hctl, &lvi);

			filter.must_heros_.insert(lvi.lParam);
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CITY);
	city_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tevent::tfilter_::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HTREEITEM htvi = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "master_hero: ";
	if (master_hero_ != HEROS_INVALID_NUMBER) {
		strstr << utf8_2_ansi(gdmgr.heros_[master_hero_].name().c_str());
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "must_heros: ";
	for (std::set<int>::const_iterator it = must_heros_.begin(); it != must_heros_.end(); ++ it) {
		if (it == must_heros_.begin()) {
			strstr << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);

	htvi = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "city = ";
	if (city_ >= 0) {
		if (city_ != HEROS_INVALID_NUMBER) {
			strstr << utf8_2_ansi(gdmgr.heros_[city_].name().c_str());
		} else {
			strstr << "(流浪)";
		}
	}
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tfilter_::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	const tfilter_& filter = *ns::filter;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_MASTERHERO);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	int selected_row = 0;
	int index = 1;
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it, index ++) {
		hero& h = gdmgr.heros_[it->first];
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, index, h.number_);
		if (h.number_ == filter.master_hero_) {
			selected_row = index;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTFILTER_MUSTHEROS);
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	LVITEM lvi;
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
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

		if (filter.must_heros_.find(h.number_) != filter.must_heros_.end()) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		} else {
			ListView_SetCheckState(hctl, lvi.iItem, FALSE);
		}
	}
	strstr.str("");
	strstr << "必须存在的武将(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EVENTFILTER_MUSTHEROS), strstr.str().c_str());

	// city/side
	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTFILTER_CITY);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, -1);
	ComboBox_AddString(hctl, "(流浪)");
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
}

void tevent::tfilter_::generate(std::stringstream& strstr, const std::string& prefix) const
{
	if (master_hero_ != HEROS_INVALID_NUMBER) {
		strstr << prefix << "master_hero = " << master_hero_ << "\n";
	}
	if (!must_heros_.empty()) {
		strstr << prefix << "must_heros_ = ";
		for (std::set<int>::const_iterator it = must_heros_.begin(); it != must_heros_.end(); ++ it) {
			if (it == must_heros_.begin()) {
				strstr << *it;
			} else {
				strstr << ", " << *it;
			}
		}
		strstr << "\n";
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
}

const std::string tevent::tset_variable::null_value = "";
const std::pair<long, long> tevent::tset_variable::null_rand = std::make_pair<long, long>(0, 0);

std::vector<std::pair<std::string, std::string> > tevent::tset_variable::op_map;

tevent::tset_variable::tset_variable()
	: tcommand(SET_VARIABLE)
	, name_("name_")
	, op_("value")
	, value_("val_")
{
	if (op_map.empty()) {
		op_map.push_back(std::make_pair<std::string, std::string>("value", "等于值"));
		op_map.push_back(std::make_pair<std::string, std::string>("rand", "范围中取一个值"));
	}
}

void tevent::tset_variable::from_config(const config& cfg)
{
	name_ = cfg["name"].str();
	foreach (const config::attribute &istrmap, cfg.attribute_range()) {
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
			if (it->first == istrmap.first) {
				op_ = istrmap.first;
				value_ = istrmap.second;
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
			rand_ = std::make_pair<long, long>(low, high);
		}
	}
*/
}

void tevent::tset_variable::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_VALUE), text, _MAX_PATH);
	value_ = text;

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
	strstr.str("");
	strstr << "变量“" << name_ << "”";
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		if (it->first == op_) {
			strstr << it->second;
		}
	}
	strstr << "“" << value_ << "”";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tset_variable::update_to_ui_special(HWND hdlgP) const
{
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SETVARIABLE_ENV);
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	std::vector<std::pair<std::string, std::string> > env;
	env.push_back(std::make_pair<std::string, std::string>("unit.type", "第一单位兵种"));
	env.push_back(std::make_pair<std::string, std::string>("unit.heros_army", "第一单位武将"));
	env.push_back(std::make_pair<std::string, std::string>("unit.master_hero", "第一单位主将"));
	env.push_back(std::make_pair<std::string, std::string>("unit.cityno", "第一单位所在城市"));
	env.push_back(std::make_pair<std::string, std::string>("unit.side", "第一单位所在势力"));
	env.push_back(std::make_pair<std::string, std::string>("unit.x", "第一单位X坐标"));
	env.push_back(std::make_pair<std::string, std::string>("unit.y", "第一单位Y坐标"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.type", "第二单位兵种"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.heros_army", "第二单位武将"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.master_hero", "第二单位主将"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.cityno", "第二单位所在城市"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.side", "第二单位所在势力"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.x", "第二单位X坐标"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.y", "第二单位Y坐标"));
		
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
	hctl = GetDlgItem(hdlgP, IDC_CMB_SETVARIABLE_OP);
	for (index = 0; index < ComboBox_GetCount(hctl); index ++) {
		if (op_ == op_map[index].first) {
			selected_row = index;
			break;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_NAME), name_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_VALUE), value_.c_str());
}

void tevent::tkill::from_config(const config& cfg)
{
	master_hero_ = cfg["master_hero"];
}

void tevent::tkill::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl;

	// master_hero
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_MASTERHERO), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTKILL_MASTERHERO);
		strstr.str("");
		strstr << ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
		master_hero_ = strstr.str();
	} else {
		master_hero_ = text;
	}
}

void tevent::tkill::update_to_ui_event_edit(HWND hctl, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi1 = TreeView_AddLeaf(hctl, branch);
	strstr.str("");
	strstr << "kill";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hctl, htvi1, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_command, ns::iico_evt_command, 1, text);

	HTREEITEM htvi = TreeView_AddLeaf(hctl, htvi1);
	strstr.str("");
	strstr << "master_hero = ";
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
}

void tevent::tkill::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;
	
	bool is_variable = valuex::is_variable(master_hero_);
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_EVENTKILL_MASTERHERO);
	if (is_variable) {
		Edit_SetText(hctl, master_hero_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTKILL_MASTERHERO);
	ComboBox_ResetContent(hctl);
	int selected_row = 0;
	int master_hero = HEROS_INVALID_NUMBER;
	if (!is_variable) {
		master_hero = valuex::to_int(master_hero_, HEROS_INVALID_NUMBER);
	}
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	int index = 1;
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it, index ++) {
		hero& h = gdmgr.heros_[it->first];
		ComboBox_AddString(hctl, utf8_2_ansi(h.name().c_str()));
		ComboBox_SetItemData(hctl, index, h.number_);
		if (!is_variable && h.number_ == master_hero) {
			selected_row = index;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);
}

void tevent::tjoin::from_config(const config& cfg)
{
	master_hero_ = cfg["master_hero"].str();
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
	strstr << "master_hero = ";
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
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
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
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
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
	, type_(ns::campaign.troop_utypes_[0].first)
	, heros_army_()
	, side_("0")
	, city_(lexical_cast_default<std::string>(HEROS_INVALID_NUMBER))
	, x_("1")
	, y_("1")
{}

void tevent::tunit::from_config(const config& cfg)
{
	type_ = cfg["type"].str();
	if (!valuex::is_variable(type_)) {
		if (!unit_types.find(type_)) {
			type_ = ns::campaign.troop_utypes_[0].first;
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
}

void tevent::tunit::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl;
	LVITEM lvi;
	int index;

	// type
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_TYPE), text, _MAX_PATH);
	if (text[0] == '\0') {
		hctl = GetDlgItem(hdlgP, IDC_CMB_EVENTUNIT_TYPE);
		type_ = ns::campaign.troop_utypes_[ComboBox_GetCurSel(hctl)].first;
	} else {
		type_ = text;
	}

	// heros_army
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_EVENTUNIT_HEROSARMY), text, _MAX_PATH);
	if (text[0] == '\0') {
		strstr.str("");
		hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);
		for (index = 0; index < ListView_GetItemCount(hctl); index ++) {
			if (ListView_GetCheckState(hctl, index)) {
				lvi.iItem = index;
				lvi.mask = LVIF_PARAM;
				lvi.iSubItem = 0;
				lvi.pszText = NULL;
				lvi.cchTextMax = 0;
				ListView_GetItem(hctl, &lvi);

				if (strstr.str().empty()) {
					strstr << lvi.lParam;
				} else {
					strstr << ", " << lvi.lParam;
				}
			}
		}
		heros_army_ = strstr.str();
	} else {
		heros_army_ = text;
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
		std::set<int> sstr = valuex::to_set_int(heros_army_);
		for (std::set<int>::const_iterator it = sstr.begin(); it != sstr.end(); ++ it) {
			hero& h = gdmgr.heros_[*it];
			if (it == sstr.begin()) {
				strstr << utf8_2_ansi(h.name().c_str());
			} else {
				strstr << ", " << utf8_2_ansi(h.name().c_str());
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
			strstr << "(流浪)";
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
}

void tevent::tunit::update_to_ui_special(HWND hdlgP) const
{
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
	for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = ns::campaign.troop_utypes_.begin(); it != ns::campaign.troop_utypes_.end(); ++ it) {
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
	if (is_variable) {
		Edit_SetText(hctl, heros_army_.c_str());
	} else {
		Edit_SetText(hctl, "");
	}

	hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	LVITEM lvi;
	std::set<int> heros_army;
	if (!is_variable) {
		heros_army = valuex::to_set_int(heros_army_);
	}
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
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

		if (heros_army.find(h.number_) != heros_army.end()) {
			ListView_SetCheckState(hctl, lvi.iItem, TRUE);
		} else {
			ListView_SetCheckState(hctl, lvi.iItem, FALSE);
		}
	}
	strstr.str("");
	strstr << "部队中武将(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EVENTUNIT_HEROSARMY), strstr.str().c_str());

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
	ComboBox_AddString(hctl, "(流浪)");
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

	strstr << prefix << "\tattacks_left = 0\n";

	strstr << prefix << "\tx,y = ";
	strstr << x_ << ", " << y_ << "\n";

	strstr << prefix << "[/unit]\n";
}

std::vector<std::pair<std::string, std::string> > tevent::tvariable::op_map;

tevent::tvariable::tvariable()
	: left_("name_")
	, op_("equals")
	, right_()
{
	if (op_map.empty()) {
		op_map.push_back(std::make_pair<std::string, std::string>("equals", "等于(字符串)"));
		op_map.push_back(std::make_pair<std::string, std::string>("not_equals", "不等于(字符串)"));
		op_map.push_back(std::make_pair<std::string, std::string>("contains", "包含(字符串)"));
		op_map.push_back(std::make_pair<std::string, std::string>("numerical_equals", "等于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("numerical_not_equals", "不等于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("greater_than", "大于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("less_than", "小于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("greater_than_equal_to", "大于等于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("less_than_equal_to", "小于等于(数字)"));
		op_map.push_back(std::make_pair<std::string, std::string>("boolean_equals", "等于(布尔)"));
		op_map.push_back(std::make_pair<std::string, std::string>("boolean_not_equals", "不等于(布尔)"));
	}
}

void tevent::tvariable::from_config(const config& cfg)
{
	left_ = cfg["name"].str();
	foreach (const config::attribute &istrmap, cfg.attribute_range()) {
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
			if (it->first == istrmap.first) {
				op_ = istrmap.first;
				right_ = istrmap.second;
				break;
			}
		}
	}
}

void tevent::tvariable::from_ui_special(HWND hdlgP)
{
	char text[_MAX_PATH];

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_LEFT), text, _MAX_PATH);
	left_ = text;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_VARIABLE_RIGHT), text, _MAX_PATH);
	right_ = text;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_VARIABLE_OP);
	op_ = op_map[ComboBox_GetCurSel(hctl)].first;
}

void tevent::tvariable::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "“" << left_ << "”";
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		if (it->first == op_) {
			strstr << it->second;
		}
	}
	strstr << "“" << right_ << "”";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi, TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		0, ns::iico_evt_attribute, ns::iico_evt_attribute, 0, text);
}

void tevent::tvariable::update_to_ui_special(HWND hdlgP) const
{
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_VARIABLE_ENV);
	int count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	int index = 0;
	LVITEM lvi;
	std::vector<std::pair<std::string, std::string> > env;
	env.push_back(std::make_pair<std::string, std::string>("random", "概率"));
	env.push_back(std::make_pair<std::string, std::string>("unit.heros_army", "第一单位武将"));
	env.push_back(std::make_pair<std::string, std::string>("unit.side", "第一单位势力"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.heros_army", "第二单位武将"));
	env.push_back(std::make_pair<std::string, std::string>("second_unit.side", "第二单位势力"));
		
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
}

void tevent::thave_unit::from_config(const config& cfg)
{
	tfilter_::from_config(cfg);
}

void tevent::thave_unit::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	tfilter_::update_to_ui_event_edit(hwndtv, branch);
}

void tevent::thave_unit::update_to_ui_special(HWND hdlgP) const
{
	std::stringstream strstr;

	strstr << "主将: ";
	if (master_hero_ != HEROS_INVALID_NUMBER) {
		strstr << utf8_2_ansi(gdmgr.heros_[master_hero_].name().c_str());
	}
	strstr << "\r\n";

	strstr << "必须存的武将: ";
	for (std::set<int>::const_iterator it = must_heros_.begin(); it != must_heros_.end(); ++ it) {
		if (it == must_heros_.begin()) {
			strstr << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_HAVEUNIT_FILTER), strstr.str().c_str());
}

std::map<int, std::string> tevent::tcondition::name_map;
std::map<int, int> tevent::tcondition::idd_map;
std::map<int, DLGPROC> tevent::tcondition::dlgproc_map;

tevent::tcondition::tcondition()
	: tcommand(CONDITION)
	, type_(VARIABLE)
	, variable_()
	, have_unit_()
{
	if (name_map.empty()) {
		name_map[VARIABLE] = "判断变量";
		name_map[HAVE_UNIT] = "判断单位是否存在";
	}
	if (idd_map.empty()) {
		idd_map[VARIABLE] = IDD_VARIABLE;
		idd_map[HAVE_UNIT] = IDD_HAVEUNIT;
	}
	if (dlgproc_map.empty()) {
		dlgproc_map[VARIABLE] = DlgVariableProc;
		dlgproc_map[HAVE_UNIT] = DlgHaveUnitProc;
	}
}

void tevent::tcondition::from_config(const config& cfg)
{
	type_ = tcondition::NONE;

	foreach (const config::any_child& tmp, cfg.all_children_range()) {
		if (tmp.key == "variable") {
			if (type_ == NONE) {
				type_ = VARIABLE;
				variable_.from_config(tmp.cfg);
			}
		} else if (tmp.key == "have_unit") {
			if (type_ == NONE) {
				type_ = HAVE_UNIT;
				have_unit_.from_config(tmp.cfg);
			}
		}
	}
}

void tevent::tcondition::from_ui_special(HWND hdlgP)
{
	if (type_ == VARIABLE) {
		variable_.from_ui_special(hdlgP);
	}
}

void tevent::tcondition::update_to_ui_event_edit(HWND hwndtv, HTREEITEM branch) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];

	HTREEITEM htvi_condition = TreeView_AddLeaf(hwndtv, branch);
	strstr.str("");
	strstr << "condition.[";
	if (type_ == VARIABLE) {
		strstr << "variable";
		variable_.update_to_ui_event_edit(hwndtv, htvi_condition);
	} else if (type_ == HAVE_UNIT) {
		strstr << "have_unit";
		have_unit_.update_to_ui_event_edit(hwndtv, htvi_condition);
	}
	strstr << "]";
	strcpy(text, strstr.str().c_str());
	TreeView_SetItem1(hwndtv, htvi_condition, TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE, 
		(LPARAM)(tcommand*)(this), ns::iico_evt_condition, ns::iico_evt_condition, 1, text);
}

void tevent::tcondition::update_to_ui_special(HWND hdlgP) const
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CONDITION_TYPE);
	int index;
	for (index = 0; index < ComboBox_GetCount(hctl); index ++) {
		if (type_ == ComboBox_GetItemData(hctl, index)) {
			break;
		}
	}
	if (index < ComboBox_GetCount(hctl)) {
		ComboBox_SetCurSel(hctl, index);
	}
}

void tevent::tcondition::switch_type(HWND hdlgP, int to)
{
	char text[_MAX_PATH];
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);

	if (pHdr && to == type_) {
		return;
	}
	type_ = to;

	if (!pHdr) {
		pHdr = (DLGHDR*)malloc(sizeof(DLGHDR));
		memset(pHdr, 0, sizeof(DLGHDR));
		// Save a pointer to the DLGHDR structure. 
		SetWindowLong(hdlgP, GWL_USERDATA, (LONG) pHdr); 

		pHdr->hwndTab = GetDlgItem(hdlgP, IDC_TAB_CONDITION_CONDITION);

		pHdr->reserved_pages = 1;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->valid_pages = 1;

	} else if (pHdr->hwndDisplay != NULL) {
		DestroyWindow(pHdr->hwndDisplay);
		TabCtrl_DeleteItem(pHdr->hwndTab, 0);
	}

	TCITEM tie;
	// Add a tab for each of the three child dialog boxes. 
	tie.mask = TCIF_TEXT | TCIF_IMAGE; 
	tie.iImage = -1; 
	strcpy(text,name_map.find(to)->second.c_str());
	tie.pszText = text;
	TabCtrl_InsertItem(pHdr->hwndTab, 0, &tie);

	// Lock the resources for the three child dialog boxes. 
	pHdr->apRes[0] = ns::DoLockDlgRes(MAKEINTRESOURCE(idd_map.find(type_)->second));

	RECT rc;
	// Calculate how large to make the tab control, so 
	// the display area can accommodate all the child dialog boxes.
	GetWindowRect(hdlgP, &rc);
	GetWindowRect(pHdr->hwndTab, &pHdr->rcDisplay);
	OffsetRect(&pHdr->rcDisplay, -1 * rc.left, -1 * rc.top);
	TabCtrl_GetItemRect(pHdr->hwndTab, 0, &rc);
	// OffsetRect(&pHdr->rcDisplay, 0, rc.bottom);

	// Create the new child dialog box.
	pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[0], hdlgP, dlgproc_map.find(type_)->second);
	ShowWindow(pHdr->hwndDisplay, SW_RESTORE);
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
	foreach (const config::any_child& tmp, cfg.all_children_range()) {
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
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::tvariable& variable = cond->variable_;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_VARIABLE_OP);
	const std::vector<std::pair<std::string, std::string> >& op_map = tevent::tvariable::op_map;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
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


	variable.update_to_ui_special(hdlgP);

	return FALSE;
}

void On_DlgVariableCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDOK:
		changed = TRUE;
		// ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		// EndDialog(hdlgP, changed? 1: 0);
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
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgVariableNotify);
	}
	
	return FALSE;
}

BOOL On_DlgHaveUnitInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::thave_unit& have_unit = cond->have_unit_;

	have_unit.update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgHaveUnitCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	tevent::thave_unit& have_unit = cond->have_unit_;

	switch (id) {
	case IDC_BT_HAVEUNIT_FILTER:
		ns::filter = &have_unit;
		if (OnEventFilterBt2(hdlgP)) {
			have_unit.update_to_ui_special(hdlgP);
		}
		break;

	case IDOK:
		changed = TRUE;
		// ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		// EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgHaveUnitProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgHaveUnitInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgHaveUnitCommand);
	}
	
	return FALSE;
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

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CONDITION_TYPE);
	for (std::map<int, std::string>::const_iterator it = tevent::tcondition::name_map.begin(); it != tevent::tcondition::name_map.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
	}

	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);
	cond->switch_type(hdlgP, cond->type_);
	ns::clicked_command->update_to_ui_special(hdlgP);
	return FALSE;
}

void OnConditionCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcondition* cond = dynamic_cast<tevent::tcondition*>(ns::clicked_command);

	int type = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	cond->switch_type(hdlgP, type);
	
	return;
}

void On_DlgConditionCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);

	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDC_CMB_CONDITION_TYPE:
		OnConditionCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(pHdr->hwndDisplay);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgConditionNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
	}

	return;
}

BOOL On_DlgEventKillInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	if (ns::action_event_item == ma_edit) {
		SetWindowText(hdlgP, "编辑操作：杀死单位");
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加操作：杀死部队");
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

void OnEventKillEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl1;
	HWND hctl2 = NULL;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_EVENTKILL_MASTERHERO) {
		hctl1 = GetDlgItem(hdlgP, IDC_CMB_EVENTKILL_MASTERHERO);

	} else {
		return;
	}
	ShowWindow(hctl1, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	if (hctl2) {
		ShowWindow(hctl2, (text[0] == '\0')? SW_RESTORE: SW_HIDE);
	}

	return;
}

void On_DlgEventKillCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDM_VARIABLE_ITEM0: // type
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_EVENTKILL_MASTERHERO), ns::clicked_variable.c_str());
		break;

	case IDC_ET_EVENTKILL_MASTERHERO:
		OnEventKillEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventKillNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
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

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_SETVARIABLE_OP);
	const std::vector<std::pair<std::string, std::string> >& op_map = tevent::tset_variable::op_map;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = op_map.begin(); it != op_map.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
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

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
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
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_NAMESTATUS), "无效字符串");
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

	switch (id) {
	case IDC_ET_SETVARIABLE_NAME:
		OnSetVariableEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgSetVariableNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	LVITEM lvi;

	if (lpNMHdr->code == NM_DBLCLK) {
		if (lpNMHdr->idFrom != IDC_LV_SETVARIABLE_ENV) {
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

		std::string value = std::string("$") + text;
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SETVARIABLE_VALUE), value.c_str());
	}
	return FALSE;
}

BOOL CALLBACK DlgSetVariableProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgSetVariableInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgSetVariableCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgSetVariableNotify);
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
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

	ns::hpopup_variable = CreatePopupMenu();
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM0, "到兵种");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM1, "到武将");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM2, "到城市");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM3, "到势力");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM4, "到“X”坐标");
	AppendMenu(ns::hpopup_variable, MF_STRING, IDM_VARIABLE_ITEM5, "到“Y”坐标");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTUNIT_HEROSARMY);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
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

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

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
	case IDC_ET_EVENTUNIT_HEROSARMY:
	case IDC_ET_EVENTUNIT_CITY:
	case IDC_ET_EVENTUNIT_SIDE:
	case IDC_ET_EVENTUNIT_VARX:
	case IDC_ET_EVENTUNIT_VARY:
		OnEventUnitEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		ns::clicked_command->from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventUnitNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;

	if (lpNMHdr->code == NM_CLICK) {
		if ((lpNMHdr->idFrom == IDC_LV_EVENTUNIT_HEROSARMY) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 3) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
			strstr << "部队中武将(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
			Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EVENTUNIT_HEROSARMY), strstr.str().c_str());
		}
	} else if (lpNMHdr->code == NM_RCLICK) {
		valuex::cumulate_variables_notify_handler_rclick(hdlgP, lpNMHdr);
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
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

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_EVENTFILTER_MUSTHEROS);
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
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

	ListView_SetImageList(hctl, gdmgr._himl_checkbox, LVSIL_STATE);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	ns::filter->tfilter_::update_to_ui_special(hdlgP);
	return FALSE;
}

void On_DlgEventFilterCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	switch (id) {
	case IDOK:
		changed = TRUE;
		ns::filter->tfilter_::from_ui_special(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgEventFilterNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_EVENTFILTER_MUSTHEROS) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 3) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
			strstr << "必须存在的武将(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
			Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EVENTFILTER_MUSTHEROS), strstr.str().c_str());
		}
	}
	return FALSE;
}

BOOL CALLBACK DlgEventFilterProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgEventFilterInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgEventFilterCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgEventFilterNotify);
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
		TreeView_DeleteAllItems(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER));
		evt.update_to_ui_event_edit(hdlgP);
	}

	return;
}

bool OnEventFilterBt2(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_BT_HAVEUNIT_FILTER), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_EVENTFILTER), hdlgP, DlgEventFilterProc, lParam)) {
		return true;
	}
	return false;
}

BOOL On_DlgEventEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;

	if (ns::action_event == ma_edit) {
		strstr.str("");
		strstr << "编辑第" << ns::clicked_event + 1 << "条事件";
		SetWindowText(hdlgP, strstr.str().c_str());
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		SetWindowText(hdlgP, "添加事件");
	}

	ns::hpopup_new = CreatePopupMenu();
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM0, "判断分枝");
	AppendMenu(ns::hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM1, "设置变量");
	AppendMenu(ns::hpopup_new, MF_SEPARATOR, 0, NULL);
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM2, "生成部队");
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM3, "杀死部队");
	AppendMenu(ns::hpopup_new, MF_STRING, IDM_NEW_ITEM4, "加入部队");

	ns::hpopup_event = CreatePopupMenu();
	AppendMenu(ns::hpopup_event, MF_POPUP, (UINT_PTR)(ns::hpopup_new), "在之后添加");
	AppendMenu(ns::hpopup_event, MF_STRING, IDM_EDIT, "编辑...");
	AppendMenu(ns::hpopup_event, MF_STRING, IDM_DELETE, "删除...");

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

	evt.update_to_ui_event_edit(hdlgP);
	return FALSE;
}

void On_DlgEventEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tevent& evt = scenario.event_[ns::clicked_event];
	tevent::tcommand* new_cmd = NULL;

	switch (id) {
	case IDM_NEW_ITEM0: // if
		if (!new_cmd) {
			new_cmd = new tevent::tif();
		}
	case IDM_NEW_ITEM1: // set_variable
		if (!new_cmd) {
			new_cmd = new tevent::tset_variable();
		}
	case IDM_NEW_ITEM2: // unit
		if (!new_cmd) {
			new_cmd = new tevent::tunit();
		}
	case IDM_NEW_ITEM3: // kill
		if (!new_cmd) {
			new_cmd = new tevent::tkill();
		}
	case IDM_NEW_ITEM4: // join
		if (!new_cmd) {
			new_cmd = new tevent::tjoin();
		}
		evt.new_command(new_cmd, hdlgP);
		break;

	case IDM_EDIT:
		TreeView_SetItemState (GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (!evt.do_edit(GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER))) {
			TreeView_SetItemState (GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
		}

		break;
	case IDM_DELETE:
		TreeView_SetItemState (GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, TVIS_BOLD, TVIS_BOLD);
		if (MessageBox(hdlgP, "您想删除该条操作吗？", "确认删除", MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			TreeView_SetItemState (GetDlgItem(hdlgP, IDC_TV_EVENTEDIT_EXPLORER), ns::clicked_htvi, 0, TVIS_BOLD);
			return;
		}
		evt.erase_command(ns::clicked_command, hdlgP);
		break;

	case IDOK:
		changed = TRUE;
		// side.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
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

	} else if (tvi.lParam >= tevent::PARAM_COMMAND) {
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
		if (tvi.lParam == tevent::PARAM_EVENT || tvi.lParam == tevent::PARAM_FILTER) {
			disable = true;
		} else if (ns::clicked_command && ns::clicked_command->type_ == tevent::tcommand::CONDITION) {
			disable = true;
		}
		if (disable) {
			EnableMenuItem(ns::hpopup_event, (UINT_PTR)(ns::hpopup_new), MF_BYCOMMAND | MF_GRAYED);
		} else {
			// new---if(cannot support nest)
			disable = false;
			if (ns::clicked_command && std::find(evt.commands_.begin(), evt.commands_.end(), ns::clicked_command) == evt.commands_.end()) {
				disable = true;
			} else if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
				disable = true;
			}
			if (disable) {
				EnableMenuItem(ns::hpopup_new, IDM_NEW_ITEM0, MF_BYCOMMAND | MF_GRAYED);
			}
		}

		// edit
		disable = false;
		if (ns::clicked_command && ns::clicked_command->type_ == tevent::tcommand::IF) {
			disable = true;
		} else if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
			disable = true;
		}
		if (disable) {
			EnableMenuItem(ns::hpopup_event, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
		}

		// delete
		disable = false;
		if (tvi.lParam == tevent::PARAM_EVENT || 
			tvi.lParam == tevent::PARAM_FILTER || 
			tvi.lParam == tevent::PARAM_SECONDFILTER) {
			disable = true;
		}
		if (ns::clicked_command && ns::clicked_command->type_ == tevent::tcommand::CONDITION) {
			disable = true;
		}
		if (tvi.lParam == tevent::PARAM_THEN || tvi.lParam == tevent::PARAM_ELSE) {
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

		// 恢复回去
		EnableMenuItem(ns::hpopup_new, IDM_NEW_ITEM0, MF_BYCOMMAND | MF_ENABLED);

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

	std::stringstream message;
	message << "您想删除第" << ns::clicked_event + 1 << "条事件吗？";
	if (MessageBox(hdlgP, message.str().c_str(), "确认删除", MB_YESNO | MB_DEFBUTTON2) != IDYES) {
		return;
	}

	scenario.erase_event(ns::clicked_event, hdlgP);

	scenario.set_dirty(tscenario::BIT_EVENT, !scenario.event_equal()); 
	return;
}