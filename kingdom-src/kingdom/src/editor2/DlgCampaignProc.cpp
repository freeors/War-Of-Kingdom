#define GETTEXT_DOMAIN "wesnoth-maker"

#include "global.hpp"
#include "game_config.hpp"
#include "loadscreen.hpp"
#include "xfunc.h"
#include "win32x.h"
#include "gettext.hpp"
#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "map_location.hpp"

#ifndef _ROSE_EDITOR
#include "struct.h"
#include "DlgCampaignProc.hpp"
#include <string.h>
#include "sdl_utils.hpp"
#include "dlgcoreproc.hpp"
#include "rectangle.hpp"

#include "resource.h"

#include <boost/foreach.hpp>

void campaign_refresh(HWND hdlgP);

BOOL CALLBACK DlgCampaignMainProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgCampaignScenarioProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgReportHeroStateProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

namespace scenario_selector {
bool multiplayer;

void switch_to(bool _multiplayer)
{
	multiplayer = _multiplayer;
}

}

namespace ns {
	tcampaign campaign;
	tmain _main;
	std::vector<tscenario> _scenario;
	const config* campaign_cfg_ptr;

	std::map<int, int> cityno_map;

	HIMAGELIST himl_checkbox_side;

	int clicked_side;
	int clicked_event;
	int clicked_treasure;
	int clicked_road;
	int clicked_feature;
	int clicked_city;
	int clicked_troop;
	int clicked_hero;

	int type;
	int current_scenario;
	int action_side;
	int action_event;
	int action_treasure;
	int action_road;
	int action_feature;
	int action_city;
	int action_troop;

	void set_dirty() 
	{
		if (!scenario_selector::multiplayer) {
			if (_main.dirty_) {
				campaign_enable_save_btn(TRUE);
				return;
			}
			for (std::vector<tscenario>::const_iterator it = _scenario.begin(); it != _scenario.end(); ++ it) {
				if (it->dirty_) {
					campaign_enable_save_btn(TRUE);
					return;
				}
			}
			campaign_enable_save_btn(FALSE);
		} else {
			ns::core.set_dirty(tcore::BIT_MULTIPLAYER, ns::core.multiplayer_dirty());
		}
	}

	void new_campaign(const std::string& id, const std::string& firstscenario_id);
	bool new_scenario();
	void delete_scenario(HWND hdlgP);
}

void campaign_enable_save_btn(bool enable)
{
	ToolBar_EnableButton(gdmgr._htb_campaign, IDM_SAVE, enable);
	campaign_enable_delete_btn(!enable && (ns::current_scenario > 0 || (!ns::current_scenario && ns::_scenario.size() >= 2)));
}

tcampaign::tcampaign()
{}

HWND tcampaign::init_toolbar(HINSTANCE hinst, HWND hdlgP)
{
	// Create a toolbar
	gdmgr._htb_campaign = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR)NULL, 
		WS_CHILD | /*CCS_ADJUSTABLE |*/ TBSTYLE_TOOLTIPS | TBSTYLE_FLAT, 0, 0, 0, 0, hdlgP, 
		(HMENU)IDR_WGENMENU, gdmgr._hinst, NULL);

	//Enable multiple image lists
    SendMessage(gdmgr._htb_campaign, CCM_SETVERSION, (WPARAM) 5, 0); 

	// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility
	ToolBar_ButtonStructSize(gdmgr._htb_campaign, sizeof(TBBUTTON));
	
	gdmgr._tbBtns_campaign[0].iBitmap = MAKELONG(gdmgr._iico_save, 0);
	gdmgr._tbBtns_campaign[0].idCommand = IDM_SAVE;	
	gdmgr._tbBtns_campaign[0].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_campaign[0].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_campaign[0].dwData = 0L;
	gdmgr._tbBtns_campaign[0].iString = -1;

	gdmgr._tbBtns_campaign[1].iBitmap = 100;
	gdmgr._tbBtns_campaign[1].idCommand = 0;	
	gdmgr._tbBtns_campaign[1].fsState = 0;
	gdmgr._tbBtns_campaign[1].fsStyle = TBSTYLE_SEP;
	gdmgr._tbBtns_campaign[1].dwData = 0L;
	gdmgr._tbBtns_campaign[1].iString = 0;

	gdmgr._tbBtns_campaign[2].iBitmap = MAKELONG(gdmgr._iico_new, 0);
	gdmgr._tbBtns_campaign[2].idCommand = IDM_NEW;	
	gdmgr._tbBtns_campaign[2].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_campaign[2].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_campaign[2].dwData = 0L;
	gdmgr._tbBtns_campaign[2].iString = -1;


	gdmgr._tbBtns_campaign[3].iBitmap = MAKELONG(gdmgr._iico_del, 0);
	gdmgr._tbBtns_campaign[3].idCommand = IDM_DELETE;	
	gdmgr._tbBtns_campaign[3].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_campaign[3].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_campaign[3].dwData = 0L;
	gdmgr._tbBtns_campaign[3].iString = -1;

	gdmgr._tbBtns_campaign[4].iBitmap = MAKELONG(gdmgr._iico_xchg, 0);
	gdmgr._tbBtns_campaign[4].idCommand = IDM_HEROSTATE;	
	gdmgr._tbBtns_campaign[4].fsState = TBSTATE_ENABLED;
	gdmgr._tbBtns_campaign[4].fsStyle = BTNS_BUTTON;
	gdmgr._tbBtns_campaign[4].dwData = 0L;
	gdmgr._tbBtns_campaign[4].iString = -1;

	ToolBar_AddButtons(gdmgr._htb_campaign, 5, &gdmgr._tbBtns_campaign);

	ToolBar_AutoSize(gdmgr._htb_campaign);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_campaign, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_campaign, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_campaign, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_campaign, SW_SHOW);

	return gdmgr._htb_campaign;
}

const std::string& tcampaign::arms(const std::string& id)
{
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = editor_config::arms.begin(); it != editor_config::arms.end(); ++ it) {
		if (id == it->first) {
			return it->second;
		}
	}
	return null_str;
}

int tcampaign::arms_int(const std::string& id)
{
	int index = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = editor_config::arms.begin(); it != editor_config::arms.end(); ++ it, index ++) {
		if (id == it->first) {
			return index;
		}
	}
	return index;
}

std::string tcampaign::city_trait(const std::string& id)
{
	for (std::vector<std::pair<std::string, const config*> >::const_iterator it = editor_config::city_traits.begin(); it != editor_config::city_traits.end(); ++ it) {
		if (id == it->first) {
			const config& cfg = *(it->second);
			std::string name = cfg["male_name"];
			if (name.empty()) {
				name = cfg["female_name"];
			}
			if (name.empty()) {
				name = cfg["name"];
			}
			return name;
		}
	}
	return "unknown trait";
}

std::string tcampaign::troop_trait(const std::string& id)
{
	for (std::vector<std::pair<std::string, const config*> >::const_iterator it = editor_config::troop_traits.begin(); it != editor_config::troop_traits.end(); ++ it) {
		if (id == it->first) {
			const config& cfg = *(it->second);
			std::string name = cfg["male_name"];
			if (name.empty()) {
				name = cfg["female_name"];
			}
			if (name.empty()) {
				name = cfg["name"];
			}
			return name;
		}
	}
	return "unknown trait";
}

std::vector<std::pair<std::string, std::string> > tmain::catalog_map;

int tmain::find_catalog(const std::string& id)
{
	for (std::vector<std::pair<std::string, std::string> >::iterator it = catalog_map.begin(); it != catalog_map.end(); ++ it) {
		if (it->first == id) {
			return std::distance(catalog_map.begin(), it);
		}
	}
	return NONE_CATALOG;
}

tmain::tmain(const std::string& id, const std::string& firstscenario_id)
	: tmain_(id, firstscenario_id)
	, file_()
	, dirty_(0)
{
	if (!id.empty()) {
		file_ = game_config::path + "\\data\\campaigns\\" + id_ + "\\_main.cfg";
	}
}

std::string tmain::description() const
{
	return id_ + " description";
}

std::string tmain::icon(bool absolute) const
{ 
	if (absolute) {
		return game_config::path + "\\data\\campaigns\\" + id_ + "\\images\\icon.png"; 
	} else {
		return std::string("data/campaigns/") + id_ + "/images/icon.png"; 
	}
}

std::string tmain::image(bool absolute) const 
{ 
	if (absolute) {
		return game_config::path + "\\data\\campaigns\\" + id_ + "\\images\\image.png"; 
	} else {
		return std::string("data/campaigns/") + id_ + "/images/image.png"; 
	}
}

void tmain::from_config(const config& campaign_cfg)
{
	id_ = campaign_cfg["id"].str();
	std::vector<t_string_base::trans_str> trans = campaign_cfg["name"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		textdomain_ = ti->td;
		break;
	}
	trans = campaign_cfg["abbrev"].t_str().valuex();
	for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
		// only support one textdomain
		abbrev_ = ti->str;
		break;
	}
	first_scenario_ = campaign_cfg["first_scenario"].str();
	hero_data_ = campaign_cfg["hero_data"].str();

	subcontinent_ = campaign_cfg["subcontinent"].to_bool();
	mode_ = mode_tag::find(campaign_cfg["mode"].str());
	catalog_ = find_catalog(campaign_cfg["catalog"].str());
	
	file_ = game_config::path + "\\data\\campaigns\\" + id_ + "\\_main.cfg";

	dirty_ = 0;
	// remember side, will use to compare in future.
	main_from_cfg_ = *this;
}

void tmain::from_ui(HWND hdlgP)
{
	char text[_MAX_PATH];
	HWND hctl;

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ID), text, sizeof(text) / sizeof(text[0]));
	id_ = text;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV_MSGID), text, sizeof(text) / sizeof(text[0]));
	abbrev_ = text;

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_FIRSTSCENARIO);
	ComboBox_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	first_scenario_ = text;

	subcontinent_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_SUBCONTINENT));
	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_MODE);
	if (ComboBox_GetCurSel(hctl) >= 0) {
		mode_ =	(mode_tag::tmode)ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_CATALOG);
	catalog_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tmain::update_to_ui(HWND hdlgP)
{
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_FIRSTSCENARIO);
	ComboBox_ResetContent(hctl);
	for (std::vector<tscenario>::const_iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		ComboBox_AddString(hctl, it->id_.c_str());
	}
	// int selected_row = ComboBox_GetCount(hctl) - 1;
	int selected_row = -1;
	for (int index = 0; index < ComboBox_GetCount(hctl); index ++) {
		ComboBox_GetLBText(hctl, index, text);
		if (!stricmp(text, first_scenario_.c_str())) {
			selected_row = index;
			break;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_SUBCONTINENT), subcontinent_);
}

void tmain::generate()
{
	std::string define;
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_fopen(file_.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}

	define = std::string("CAMPAIGN_") + id_;
	std::transform(define.begin(), define.end(), define.begin(), std::toupper);

	strstr << "#textdomain " << textdomain_ << "\n";
	strstr << "[textdomain]\n";
	strstr << "\tname = \"" << textdomain_ << "\"\n";
	strstr << "[/textdomain]\n";

	strstr << "\n";
	strstr << "[campaign]\n";
	strstr << "\tid = " << id_ << "\n";
	strstr << "\tname = _ \"" << id_ << "\"\n";
	strstr << "\tabbrev = _ \"" << abbrev_ << "\"\n";
	strstr << "\tdefine = " << define << "\n";
	strstr << "\tfirst_scenario = " << first_scenario_ << "\n";
	strstr << "\ticon = \"" << icon() << "\"\n";
	strstr << "\timage = \"" << image() << "\"\n";
	strstr << "\tdescription = _ \"" << id_ << " description\"\n";
	strstr << "\thero_data = \"" << hero_data_ << "\"\n";
	if (subcontinent_) {
		strstr << "\tsubcontinent = yes\n";
	}
	strstr << "\tmode = \"" << mode_tag::rfind(mode_) << "\"\n";
	if (mode_ == mode_tag::TOWER || mode_ == mode_tag::SIEGE) {
		strstr << "\trank = 0\n";
	} else if (mode_ == mode_tag::RPG) {
		strstr << "\trank = 100\n";
	} else {
		strstr << "\trank = 200\n";
	}
	if (catalog_ == TUTORIAL_CATALOG) {
		strstr << "\tcatalog = tutorial\n";
	}

	strstr << "[/campaign]\n";

	strstr << "\n";
	strstr << "#ifdef " << define << "\n";
	strstr << "[campaign_addon]\n";
	strstr << "\t[binary_path]\n";
	strstr << "\t\tpath = data/campaigns/" << id_ << "\n";
	strstr << "\t[/binary_path]\n";
	strstr << "\t{campaigns/" << id_ << "/scenarios}\n";
	strstr << "[/campaign_addon]\n";
	strstr << "#endif";

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);

	main_from_cfg_ = *this;
	dirty_ = 0;
}

void tmain::set_dirty(int bit, bool set)
{
	if (set) {
		dirty_ |= 1 << bit;
	} else {
		dirty_ &= ~(1 << bit);
	}
	ns::set_dirty();
}

tside::tside(tscenario* scenario)
	: scenario_(scenario) 
	, side_(-1)
	, name_()
	, leader_(HEROS_INVALID_NUMBER)
	, controller_(controller_tag::HUMAN_AI)
	, candidate_cards_()
	, holded_cards_()
	, navigation_(0)
	, gold_(0)
	, income_(0)
	, build_()
	, technologies_()
	, ally_()
	, reserve_heros_()
	, allow_player_(true)
	, except_(false)
{
}

void tside::from_config(const config& direct_side_cfg)
{
	std::string str;
	// cityno-->hero
	std::map<int, int> cityno_map;
	int cityno;

	// expand [if] block
	config side_cfg = direct_side_cfg;
	// if --> human, else --> ai
	controller_ = controller_tag::EMPTY;
	BOOST_FOREACH (const config &cfg, direct_side_cfg.child_range("if")) {
		const config& else_cfg = cfg.child("else");
		side_cfg.merge_attributes(else_cfg);
		controller_ = controller_tag::HUMAN_AI;
	}

	side_ = side_cfg["side"].to_int() - 1;
	if (!side_cfg["name"].empty()) {
		std::vector<t_string_base::trans_str> trans = side_cfg["name"].t_str().valuex();
		for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
			// only support one textdomain
			name_ = ti->str;
			break;
		}
	}
	leader_ = side_cfg["leader"].to_int(HEROS_INVALID_NUMBER);
	if (controller_ != controller_tag::HUMAN_AI) {
		const std::string& controller = side_cfg["controller"].str();
		if (controller == "human") {
			controller_ = controller_tag::HUMAN;
		} else if (controller == "ai") {
			controller_ = controller_tag::AI;
		} else {
			// null
			controller_ = controller_tag::EMPTY;
		}
	}

	std::vector<std::string> vstr;
	candidate_cards_ = side_cfg["candidate_cards"].str();

	str = side_cfg["holded_cards"].str();
	if (!str.empty() && str.at(0) != '$') {
		vstr = utils::split(str);
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			holded_cards_.push_back(atoi(i->c_str()));
		}
	} else {
		holded_cards_.clear();
	}

	navigation_ = side_cfg["navigation"].to_int();
	gold_ = side_cfg["gold"].to_int();
	income_ = side_cfg["income"].to_int();

	str = side_cfg["build"].str();
	vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		const std::string& id = *i;
		const unit_type* ut = unit_types.find(id);
		if (!ut) {
			std::stringstream strstr;
			strstr << utf8_2_ansi(gdmgr.heros_[leader_].name().c_str()) << "势力原可造建筑物包括“" << id;
			strstr << "”，但查不到该建筑物兵种，强制取消";
			posix_print_mb(strstr.str().c_str());
			except_ = true;
			continue;
		}
		build_.insert(*i);
	}

	str = side_cfg["technologies"].str();
	vstr = utils::split(str);
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		const std::string& id = *i;
		std::map<std::string, ttechnology>::const_iterator find = technologies.find(id);
		if (find == technologies.end()) {
			std::stringstream strstr;
			strstr << utf8_2_ansi(gdmgr.heros_[leader_].name().c_str()) << "势力原拥有科技包括“" << id;
			strstr << "”，但查不到该科技，强制取消";
			posix_print_mb(strstr.str().c_str());
			except_ = true;
			continue;
		}
		technologies_.insert(*i);
	}

	vstr = utils::parenthetical_split(side_cfg["feature"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> vstr1 = utils::split(*it);
		arms_feature feature;
		if (vstr1.size() != 3) {
			if (vstr1.empty() || (vstr1.size() == 1 && vstr1[0] == ",")) {
				continue;
			} else {
				continue;
			}
		}
		feature.arms_ = ns::campaign.arms_int(vstr1[0]);
		if (feature.arms_ < 0 || feature.arms_ >= HEROS_MAX_ARMS) {
			continue;
		}
		feature.level_ = lexical_cast<int>(vstr1[1].c_str());
		if (feature.level_ < 0) {
			continue;
		}
		feature.feature_ = lexical_cast<int>(vstr1[2].c_str());
		if (feature.feature_ < 0 || feature.feature_ >= HEROS_MAX_FEATURE) {
			continue;
		}
		features_.push_back(feature);
	}

	// ally
	vstr = utils::split(side_cfg["team_name"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		int side = lexical_cast_default<int>(*i);
		if (side <= 0) {
			// program error
			continue;
		}
		ally_.insert(side - 1);
	}

	// reserve heros
	vstr = utils::split(side_cfg["reserve_heros"].str());
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		int number = lexical_cast_default<int>(*i);
		reserve_heros_.insert(number);
		scenario_->do_state(number, side_, tscenario::STATE_RESERVE);
	}

	// multiplayer dependent
	allow_player_ = side_cfg["allow_player"].to_bool(true);

	// city
	BOOST_FOREACH (const config &cfg, side_cfg.child_range("artifical")) {
		// str = cfg["type"].str();
		cities_.push_back(tcity());
		tcity& c = cities_.back();

		// c.type_ = str;
		vstr = utils::split(cfg["heros_army"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.heros_army_.push_back(number);
			scenario_->do_state(number, side_, tscenario::STATE_ARMY, number);
		}
		c.mayor_ = cfg["mayor"].to_int(HEROS_INVALID_NUMBER);
		c.soldiers_ = cfg["soldiers"].to_int();
		cityno = cfg["cityno"].to_int();
		cityno_map[cityno] = c.heros_army_[0];
		c.loc_.x = cfg["x"].to_int();
		c.loc_.y = cfg["y"].to_int();
		vstr = utils::split(cfg["traits"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			c.traits_.insert(*i);
		}
		str = cfg["especial"].str();
		c.character_ = unit_types.especial_from_id(str);
		vstr = utils::split(cfg["not_recruit"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			c.not_recruit_.push_back(*i);
		}
		// alias
		std::string textdomain;
		split_t_string(cfg["alias"].t_str(), textdomain, c.alias_);

		// service_heros
		vstr = utils::split(cfg["service_heros"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.service_heros_.insert(number);
			scenario_->do_state(number, side_, tscenario::STATE_SERVICE, c.heros_army_[0]);
		}
		// wander_heros
		vstr = utils::split(cfg["wander_heros"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.wander_heros_.insert(number);
			scenario_->do_state(number, side_, tscenario::STATE_WANDER, c.heros_army_[0]);
		}
		// economy_area
		const std::vector<std::string> economy_area = utils::parenthetical_split(cfg["economy_area"]);
		for (std::vector<std::string>::const_iterator it2 = economy_area.begin(); it2 != economy_area.end(); ++ it2) {
			vstr = utils::split(*it2);
			if (vstr.size() == 2) {
				const map_location loc(lexical_cast_default<int>(vstr[0]), lexical_cast_default<int>(vstr[1]));
				c.economy_area_.insert(loc);
			}
		}

		str = cfg["type"].str();
		const unit_type* ut = unit_types.find(str);
		if (!ut) {
			std::stringstream strstr;
			strstr << utf8_2_ansi(gdmgr.heros_[c.heros_army_[0]].name().c_str()) << "城市原兵种标识是“" << str;
			c.type_ = editor_config::city_utypes[0].first;
			strstr << "”，但查不到该兵种，强制设为“" << utf8_2_ansi(unit_types.find(c.type_)->type_name().c_str()) << "”";
			posix_print_mb(strstr.str().c_str());
			c.except_ = true;
		} else {
			c.type_ = str;
		}
	}

	// troop
	BOOST_FOREACH (const config &cfg, side_cfg.child_range("unit")) {
		troops_.push_back(tunit());
		tunit& u = troops_.back();

		cityno = cfg["cityno"].to_int();
		if (cityno == HEROS_ROAM_CITY) {
			u.city_ = HEROS_INVALID_NUMBER;
		} else {
			u.city_ = cityno_map.find(cityno)->second;
		}
		vstr = utils::split(cfg["heros_army"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			u.heros_army_.push_back(number);
			scenario_->do_state(number, side_, tscenario::STATE_ARMY, u.city_);
		}
		u.loc_.x = cfg["x"].to_int();
		u.loc_.y = cfg["y"].to_int();
		vstr = utils::split(cfg["traits"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			u.traits_.insert(*i);
		}
		str = cfg["especial"].str();
		u.character_ = unit_types.especial_from_id(str);

		str = cfg["type"].str();
		const unit_type* ut = unit_types.find(str);
		if (!ut) {
			std::stringstream strstr;
			strstr << utf8_2_ansi(gdmgr.heros_[u.heros_army_[0]].name().c_str()) << "部队原兵种标识是“" << str;
			u.type_ = editor_config::troop_utypes[0].first;
			strstr << "”，但查不到该兵种，强制设为“" << utf8_2_ansi(unit_types.find(u.type_)->type_name().c_str()) << "”";
			posix_print_mb(strstr.str().c_str());
			u.except_ = true;
		} else {
			u.type_ = str;
		}
	}
}

void tside::from_ui(HWND hdlgP) 
{
	HWND hctl;

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_LEADER);
	int cursel = ComboBox_GetCurSel(hctl);
	if (cursel >= 0) {
		leader_ = ComboBox_GetItemData(hctl, cursel);
	} else {
		leader_ = HEROS_INVALID_NUMBER;
	}
	if (leader_ == hero::number_empty_leader && ns::_main.mode_ == mode_tag::RPG) {
		controller_ = controller_tag::EMPTY;
	} else {
		hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_CONTROLLER);
		controller_ = (controller_tag::CONTROLLER)ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	}

	gold_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_GOLD));
	income_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_INCOME));

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_NAVIGATION);
	navigation_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl)) * 10000;
	navigation_ += UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_NAVIGATIONXP));

	build_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_BUILD);
	LVITEM lvi;
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			lvi.iItem = idx;
			lvi.mask = LVIF_PARAM;
			lvi.iSubItem = 0;
			ListView_GetItem(hctl, &lvi);
			build_.insert(editor_config::artifical_utype[lvi.lParam].first);
		}
	}

	ally_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_ALLY);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		lvi.iItem = idx;
		lvi.mask = LVIF_PARAM;
		lvi.iSubItem = 0;
		ListView_GetItem(hctl, &lvi);
		if (ListView_GetCheckState(hctl, idx)) {
			ally_.insert(lvi.lParam);
		}
	}

	allow_player_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_ALLOWPLAYER));
}


void cb_treeview_walk_form_technology(HWND hctl, HTREEITEM htvi, uint32_t* ctx)
{
	TVITEMEX	tvi;
	std::set<std::string>& technologies = *reinterpret_cast<std::set<std::string>*>(ctx);
	
	if (TreeView_GetItemState(hctl, htvi, TVIS_STATEIMAGEMASK) & INDEXTOSTATEIMAGEMASK(1)) {
		TreeView_GetItem1(hctl, htvi, &tvi, TVIF_PARAM, NULL);
		technologies.insert(ns::core.technology_tv_[tvi.lParam].first);
	}
}

void from_ui_technology(HWND hdlgP, std::set<std::string>& technologies)
{
	HWND htv = GetDlgItem(hdlgP, IDC_TV_TECHNOLOGY_EXPLORER);
	technologies.clear();
	TreeView_Walk(htv, TVI_ROOT, TRUE, cb_treeview_walk_form_technology, (uint32_t*)&technologies, FALSE);
}

void tside::update_to_ui(HWND hdlgP) const
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::string str;
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE);
	int count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 编号
	if (side_ >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = side_;
	}
	lvi.iSubItem = 0;
	sprintf(text, "%i", side_ + 1);
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 名称
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	strstr.str("");
	if (!name_.empty()) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), name_.c_str()));
	} else {
		strstr << "(" << utf8_2_ansi(gdmgr.heros_[leader_].name().c_str()) << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 君主
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	strcpy(text, utf8_2_ansi(gdmgr.heros_[leader_].name().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// controller
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	if (controller_ == controller_tag::HUMAN_AI) {
		strcpy(text, utf8_2_ansi(_("Human_AI")));
	} else if (controller_ == controller_tag::HUMAN) {
		strcpy(text, utf8_2_ansi(_("Human only")));
	} else if (controller_ == controller_tag::AI) {
		if (!scenario_selector::multiplayer) {
			strcpy(text, utf8_2_ansi(_("AI only")));
		} else {
			strcpy(text, utf8_2_ansi(_("Set in lobby")));
		}
	} else {
		strcpy(text, utf8_2_ansi(_("Void")));
	}
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 迷雾
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 4;
	lvi.pszText = NULL;
	ListView_SetItem(hctl, &lvi);

	// 卡牌
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
	strstr.str("");
	for (std::vector<size_t>::const_iterator it = holded_cards_.begin(); it != holded_cards_.end(); ++ it) {
		if (it == holded_cards_.begin()) {
			strstr << *it;
		} else {
			strstr << ", " << *it;
		}
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 初始金
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 6;
	sprintf(text, "%i", gold_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 回合金
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 7;
	sprintf(text, "%i", income_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 航海文明
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 8;
	sprintf(text, "%i", navigation_);
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特色
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 9;
	strstr.str("");
	strstr << features_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 建筑物
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 10;
	strstr.str("");
	strstr << "(" << build_.size() << ")";
	for (std::set<std::string>::const_iterator it = build_.begin(); it != build_.end(); ++ it) {
		if (it == build_.begin()) {
			strstr << utf8_2_ansi(unit_types.find(*it)->type_name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(unit_types.find(*it)->type_name().c_str());
		}
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 城市
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 11;
	strstr.str("");
	strstr << cities_.size() << "(";
	for (std::vector<tcity>::const_iterator it = cities_.begin(); it != cities_.end(); ++ it) {
		if (it == cities_.begin()) {
			strstr << utf8_2_ansi(gdmgr.heros_[it->heros_army_[0]].name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(gdmgr.heros_[it->heros_army_[0]].name().c_str());
		}
	}
	strstr << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 部队
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 12;
	strstr.str("");
	strstr << troops_.size() << "(";
	for (std::vector<tunit>::const_iterator it = troops_.begin(); it != troops_.end(); ++ it) {
		if (it == troops_.begin()) {
			strstr << utf8_2_ansi(gdmgr.heros_[it->heros_army_[0]].name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(gdmgr.heros_[it->heros_army_[0]].name().c_str());
		}
	}
	strstr << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tside::update_to_ui_side_edit(HWND hdlgP, bool partial)
{
	std::stringstream strstr;
	HWND hctl;
	char text[_MAX_PATH];

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;

	if (!partial) {
		strstr << side_ + 1;
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_SIDE), strstr.str().c_str());

		UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_GOLD), gold_);
		UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_INCOME), income_);

		// navigation
		ComboBox_SetCurSel(GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_NAVIGATION), navigation_ / 10000);
		UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_NAVIGATIONXP), navigation_ % 10000);
	}

	int selected_row = -1;
	std::map<int, int> mayor_map;
	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_LEADER);
	ComboBox_ResetContent(hctl);
	if (controller_ == controller_tag::EMPTY || ns::_scenario[ns::current_scenario].null_side() == -1) {
		int number = hero::number_empty_leader;
		strstr.str("");
		strstr << "(" << utf8_2_ansi(gdmgr.heros_[number].name().c_str()) << ")";
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, 0, number);
		if (leader_ == number) {
			selected_row = 0;
		}
	}

	for (std::set<int>::const_iterator it = reserve_heros_.begin(); it != reserve_heros_.end(); ++ it) {
		int number = *it;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[number].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, number);
		if (leader_ == number) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}

	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_ALLOWPLAYER), allow_player_);

	for (std::vector<tcity>::const_iterator it = cities_.begin(); it != cities_.end(); ++ it) {
		mayor_map[it->heros_army_[0]] = it->mayor_;
		for (std::set<int>::const_iterator it2 = it->service_heros_.begin(); it2 != it->service_heros_.end(); ++ it2) {
			int number = *it2;
			if (number == it->mayor_) continue;
			ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[number].name().c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, number);
			if (leader_ == number) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
	}
	for (std::vector<tunit>::const_iterator it = troops_.begin(); it != troops_.end(); ++ it) {
		int mayor;
		if (it->city_ == HEROS_INVALID_NUMBER) {
			mayor = HEROS_INVALID_NUMBER;
		} else {
			mayor = mayor_map.find(it->city_)->second;
		}
		for (std::vector<int>::const_iterator it2 = it->heros_army_.begin(); it2 != it->heros_army_.end(); ++ it2) {
			int number = *it2;
			if (number == mayor) continue;
			ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[number].name().c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, number);
			if (leader_ == number) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
	}
	if (selected_row == -1) {
		selected_row = 0;
		leader_ = ComboBox_GetItemData(hctl, selected_row);
		if (name_.empty()) {
			strstr.str("");
			strstr << "(" << utf8_2_ansi(gdmgr.heros_[leader_].name().c_str()) << ")";
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAME), strstr.str().c_str());
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_CONTROLLER);
	ComboBox_ResetContent(hctl);
	std::map<int, std::string> controller_map;
	if (!scenario_selector::multiplayer) {
		controller_map.insert(std::make_pair(controller_tag::HUMAN, _("Human only"))); 
		controller_map.insert(std::make_pair(controller_tag::HUMAN_AI, _("Human_AI")));
		controller_map.insert(std::make_pair(controller_tag::AI, _("AI only")));
	} else {
		controller_map.insert(std::make_pair(controller_tag::AI, _("Set in lobby")));
	}
	controller_map.insert(std::make_pair(controller_tag::EMPTY, _("Void")));
	int cursel = 0;
	for (std::map<int, std::string>::const_iterator it = controller_map.begin(); it != controller_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (controller_ == it->first) {
			cursel = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, cursel);

	if (!partial) {
		// build
		hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_BUILD);
		ListView_DeleteAllItems(hctl);
		int index = 0;
		LVITEM lvi;
		for (std::vector<std::pair<std::string, const unit_type*> >::iterator it = editor_config::artifical_utype.begin(); it != editor_config::artifical_utype.end(); ++ it) {
			if (it->second->fort()) {
				continue;
			}
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 姓名
			lvi.iItem = index ++;
			lvi.iSubItem = 0;
			strcpy(text, utf8_2_ansi(it->second->type_name().c_str()));
			lvi.pszText = text;
			lvi.lParam = (LPARAM)std::distance(editor_config::artifical_utype.begin(), it);
			ListView_InsertItem(hctl, &lvi);

			if (build_.find(it->first) != build_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}
		strstr.str("");
		strstr << utf8_2_ansi(_("Buildable")) << "(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
		Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_BUILD), strstr.str().c_str());

		// technology
		explorer_technology::update_to_ui_special(hdlgP, explorer_technology::SCENARIO);

		// ally
		int column;
		hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_ALLY);
		ListView_DeleteAllItems(hctl);
		index = 0;
		for (size_t i = 0; i < sides.size(); i ++) {
			if (i == ns::clicked_side) {
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

			if (ally_.find(i) != ally_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}
		strstr.str("");
		strstr << dgettext_2_ansi("wesnoth-lib", "Ally") << "(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
		Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALLY), strstr.str().c_str());

		// candidate hero
		hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
		ListView_DeleteAllItems(hctl);
		for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
			if (it->second.allocated(side_)) {
				continue;
			}
			hero& h = gdmgr.heros_[it->first];
			candidate_hero::fill_row(hctl, h);
		}

		// reserve hero
		hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_RESERVE);
		ListView_DeleteAllItems(hctl);
		index = 0;
		for (std::set<int>::const_iterator it = reserve_heros_.begin(); it != reserve_heros_.end(); ++ it) {
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

	// feature
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE);
	ListView_DeleteAllItems(hctl);
	for (std::vector<arms_feature>::const_iterator it = features_.begin(); it != features_.end(); ++ it) {
		it->update_to_ui_side_edit(hdlgP);
	}

	// city
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tcity>::const_iterator it = cities_.begin(); it != cities_.end(); ++ it) {
		it->update_to_ui_side_edit(hdlgP);
	}

	// troop
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tunit>::const_iterator it = troops_.begin(); it != troops_.end(); ++ it) {
		it->update_to_ui_side_edit(hdlgP);
	}
}

std::string tside::generate(const std::string& prefix) const
{
	std::stringstream strstr;

	strstr << prefix << "[side]\n";
	strstr << prefix << "\tside = " << side_ + 1 << "\n";
	strstr << prefix << "\tleader = " << leader_ << "\n";
	strstr << prefix << "\tnavigation = " << navigation_ << "\n";

	strstr << prefix << "\tbuild = ";
	for (std::set<std::string>::const_iterator it2 = build_.begin(); it2 != build_.end(); ++ it2) {
		if (it2 == build_.begin()) {
			strstr << *it2;
		} else {
			strstr << ", " << *it2;
		}
	}
	strstr << "\n";
	if (!ally_.empty()) {
		strstr << prefix << "\tteam_name = ";
		for (std::set<int>::const_iterator it2 = ally_.begin(); it2 != ally_.end(); ++ it2) {
			if (it2 == ally_.begin()) {
				strstr << *it2 + 1;
			} else {
				strstr << ", " << *it2 + 1;
			}
		}
		strstr << "\n";
	}
	if (!reserve_heros_.empty()) {
		strstr << prefix << "\treserve_heros = ";
		for (std::set<int>::const_iterator it2 = reserve_heros_.begin(); it2 != reserve_heros_.end(); ++ it2) {
			if (it2 == reserve_heros_.begin()) {
				strstr << *it2;
			} else {
				strstr << ", " << *it2;
			}
		}
		strstr << "\n";
	}

	if (scenario_->multiplayer_) {
		if (!allow_player_) {
			strstr << prefix << "\tallow_player = no\n";
		}
	}

	if (controller_ == controller_tag::HUMAN || controller_ == controller_tag::AI) { 
		// 
		strstr << "\n";
		if (controller_ == controller_tag::HUMAN) {
			strstr << prefix << "\tcontroller = human\n";
			strstr << prefix << "\tshroud = $player.shroud\n";
			strstr << prefix << "\tfog = $player.fog\n";
			if (candidate_cards_.empty()) {
				strstr << prefix << "\tcandidate_cards = $player.candidate_cards\n";
			} else {
				strstr << prefix << "\tcandidate_cards = " << candidate_cards_ << "\n";
			}
			if (holded_cards_.empty()) {
				strstr << prefix << "\tholded_cards = $player.holded_cards\n";
			} else {
				strstr << prefix << "\tholded_cards = ";
				for (std::vector<size_t>::const_iterator it2 = holded_cards_.begin(); it2 != holded_cards_.end(); ++ it2) {
					if (it2 != holded_cards_.begin()) {
						strstr << ", ";
					}
					strstr << *it2;
				}
				strstr << "\n";
			}
		} else {
			strstr << prefix << "\tcontroller = ai\n";
		}
		strstr << prefix << "\tgold = " << gold_ << "\n";
		strstr << prefix << "\tincome = " << income_ << "\n";
		strstr << prefix << "\tfeature = " << generate_features() << "\n";
		strstr << prefix << "\ttechnologies = ";
		for (std::set<std::string>::const_iterator it2 = technologies_.begin(); it2 != technologies_.end(); ++ it2) {
			if (it2 == technologies_.begin()) {
				strstr << *it2;
			} else {
				strstr << ", " << *it2;
			}
		}
		strstr << "\n";
	} else if (controller_ != controller_tag::EMPTY) { 
		// PLAYER_IF
		strstr << "\n";
		strstr << prefix << "\t{PLAYER_IF " << leader_ << "}\n";
		strstr << prefix << "\t\tshroud = $player.shroud\n";
		strstr << prefix << "\t\tfog = $player.fog\n";
		if (candidate_cards_.empty()) {
			strstr << prefix << "\t\tcandidate_cards = $player.candidate_cards\n";
		} else {
			strstr << prefix << "\t\tcandidate_cards = " << candidate_cards_ << "\n";
		}
		if (holded_cards_.empty()) {
			strstr << prefix << "\t\tholded_cards = $player.holded_cards\n";
		} else {
			strstr << prefix << "\t\tholded_cards = ";
			for (std::vector<size_t>::const_iterator it2 = holded_cards_.begin(); it2 != holded_cards_.end(); ++ it2) {
				if (it2 != holded_cards_.begin()) {
					strstr << ", ";
				}
				strstr << *it2;
			}
			strstr << "\n";
		}
		strstr << prefix << "\t\tcontroller = human\n";
		strstr << prefix << "\t\tgold = 100\n";
		strstr << prefix << "\t\tincome = 0\n";
		strstr << prefix << "\t\tfeature = " << generate_features(controller_tag::HUMAN) << "\n";
		strstr << prefix << "\t{PLAYER_ELSE}\n";
		strstr << prefix << "\t\tcontroller = ai\n";
		strstr << prefix << "\t\tgold = " << gold_ << "\n";
		strstr << prefix << "\t\tincome = " << income_ << "\n";
		strstr << prefix << "\t\tfeature = " << generate_features() << "\n";
		strstr << prefix << "\t\ttechnologies = ";
		for (std::set<std::string>::const_iterator it2 = technologies_.begin(); it2 != technologies_.end(); ++ it2) {
			if (it2 == technologies_.begin()) {
				strstr << *it2;
			} else {
				strstr << ", " << *it2;
			}
		}
		strstr << "\n";
		strstr << prefix << "\t{PLAYER_ENDIF_ELSE}\n";
	} else {
		strstr << prefix << "\tcontroller = null\n";
		strstr << prefix << "\tgold = " << gold_ << "\n";
		strstr << prefix << "\tincome = " << income_ << "\n";
		strstr << prefix << "\tfeature = " << generate_features() << "\n";
	}

	// city
	strstr << "\n";
	for (std::vector<tside::tcity>::const_iterator it2 = cities_.begin(); it2 != cities_.end(); ++ it2) {
		strstr << prefix << "\t{ANONYMITY_CITY ";
		strstr << ns::cityno_map.find(it2->heros_army_[0])->second << " ";
		strstr << side_ + 1 << " ";
		strstr << "(" << it2->type_ << ") ";
		strstr << it2->loc_.x << " " << it2->loc_.y << " ";
		strstr << "(" << it2->heros_army_[0] << ") ";
		strstr << "(";
		for (std::set<std::string>::const_iterator it3 = it2->traits_.begin(); it3 != it2->traits_.end(); ++ it3) {
			if (it3 == it2->traits_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << ")";
		strstr << "}\n";
		strstr << prefix << "\t[+artifical]\n";
		strstr << prefix << "\t\tmayor = ";
		if (it2->mayor_ != HEROS_INVALID_NUMBER) {
			strstr << it2->mayor_;
		} 
		strstr << "\n";
		if (it2->soldiers_) {
			strstr << prefix << "\t\tsoldiers = " << it2->soldiers_ << "\n";
		}
		strstr << prefix << "\t\tservice_heros = ";
		for (std::set<int>::const_iterator it3 = it2->service_heros_.begin(); it3 != it2->service_heros_.end(); ++ it3) {
			if (it3 == it2->service_heros_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << "\n";
		strstr << prefix << "\t\twander_heros = ";
		for (std::set<int>::const_iterator it3 = it2->wander_heros_.begin(); it3 != it2->wander_heros_.end(); ++ it3) {
			if (it3 == it2->wander_heros_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << "\n";
		strstr << prefix << "\t\teconomy_area = ";
		for (std::set<map_location>::const_iterator it3 = it2->economy_area_.begin(); it3 != it2->economy_area_.end(); ++ it3) {
			if (it3 == it2->economy_area_.begin()) {
				strstr << "(" << it3->x << ", " << it3->y << ")";
			} else {
				strstr << ", " << "(" << it3->x << ", " << it3->y << ")";
			}
		}
		strstr << "\n";
		if (it2->character_ != NO_ESPECIAL) {
			strstr << prefix << "\t\tespecial = " << unit_types.especial(it2->character_).id_ << "\n";
		}
		strstr << prefix << "\t\tnot_recruit = ";
		for (std::vector<std::string>::const_iterator it3 = it2->not_recruit_.begin(); it3 != it2->not_recruit_.end(); ++ it3) {
			if (it3 == it2->not_recruit_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << "\n";
		if (!it2->alias_.empty()) {
			strstr << prefix << "\t\talias = _\"" << it2->alias_ << "\"\n";
		}
		strstr << prefix << "\t[/artifical]\n";
	}

	// troop
	strstr << "\n";
	for (std::vector<tside::tunit>::const_iterator it2 = troops_.begin(); it2 != troops_.end(); ++ it2) {
		strstr << prefix << "\t{ANONYMITY_UNIT ";
		if (it2->city_ != HEROS_INVALID_NUMBER) {
			strstr << ns::cityno_map.find(it2->city_)->second << " ";
		} else {
			strstr << "0 ";
		}
		strstr << "(" << it2->type_ << ") ";
		strstr << it2->loc_.x << " " << it2->loc_.y << " ";
		strstr << "(";
		for (std::vector<int>::const_iterator it3 = it2->heros_army_.begin(); it3 != it2->heros_army_.end(); ++ it3) {
			if (it3 == it2->heros_army_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << ") ";
		strstr << "(";
		for (std::set<std::string>::const_iterator it3 = it2->traits_.begin(); it3 != it2->traits_.end(); ++ it3) {
			if (it3 == it2->traits_.begin()) {
				strstr << *it3;
			} else {
				strstr << ", " << *it3;
			}
		}
		strstr << ") ";
		if (it2->character_ != NO_ESPECIAL) {
			strstr << "(" << unit_types.especial(it2->character_).id_ << ")";
		} else {
			strstr << "()";
		}
		strstr << "}\n";
	}

	strstr << prefix << "[/side]\n";

	return strstr.str();
}

bool tside::new_feature()
{
	features_.push_back(tside::arms_feature(0, 1, 0));

	return true;
}

void tside::erase_feature(int index, HWND hdlgP)
{
	features_.erase(features_.begin() + index);

	update_to_ui_side_edit(hdlgP);
}

bool tside::new_city()
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	cities_.push_back(tside::tcity());
	tside::tcity& city = cities_.back();

	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.artificals_.begin(); it != scenario.artificals_.end(); ++ it) {
		if (it->second.allocated(HEROS_INVALID_SIDE)) continue;
		city.heros_army_.push_back(it->first);
		break;
	}
	if (city.heros_army_.empty()) {
		return false;
	}
	city.type_ = editor_config::city_utypes[0].first;
	city.loc_ = map_location(1, 1);

	scenario.do_state(city.heros_army_[0], side_, tscenario::STATE_ARMY, city.heros_army_[0]);

	// if it is first city, change all troop to it.
	if (cities_.size() == 1) {
		for (std::vector<tunit>::iterator it = troops_.begin(); it != troops_.end(); ++ it) {
			it->city_ = city.heros_army_[0];
		}
	}
	return true;
}

void tside::erase_city(int index, HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tcity& city = cities_[index];
	int city_hero = city.heros_army_[0];

	scenario.do_state(city_hero, side_);
	for (std::set<int>::const_iterator it = city.service_heros_.begin(); it != city.service_heros_.end(); ++ it) {
		scenario.do_state(*it, side_);
	}
	for (std::set<int>::const_iterator it = city.wander_heros_.begin(); it != city.wander_heros_.end(); ++ it) {
		scenario.do_state(*it, side_);
	}
	cities_.erase(cities_.begin() + index);

	// update troop if it belong to this city.
	int to_city = HEROS_INVALID_NUMBER;
	if (cities_.size()) {
		to_city = cities_[0].heros_army_[0];
	}
	for (std::vector<tunit>::iterator it = troops_.begin(); it != troops_.end(); ++ it) {
		if (it->city_ == city_hero) {
			it->city_ = to_city;
		}
	}

	if (hdlgP) {
		update_to_ui_side_edit(hdlgP, false);
	}
}

bool tside::new_troop()
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	troops_.push_back(tside::tunit());
	tside::tunit& troop = troops_.back();

	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		if (it->second.allocated(HEROS_INVALID_SIDE)) continue;
		troop.heros_army_.push_back(it->first);
		break;
	}
	if (troop.heros_army_.empty()) {
		return false;
	}
	if (cities_.size()) {
		troop.city_ = cities_[0].heros_army_[0];
	} else {
		troop.city_ = HEROS_INVALID_NUMBER;
	}
	troop.type_ = editor_config::troop_utypes[0].first;
	troop.loc_ = map_location(1, 1);

	scenario.do_state(troop.heros_army_[0], side_, tscenario::STATE_ARMY, troop.heros_army_[0]);
	return true;
}

void tside::erase_troop(int index, HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	tunit& troop = troops_[index];
	for (std::vector<int>::const_iterator it = troop.heros_army_.begin(); it != troop.heros_army_.end(); ++ it) {
		scenario.do_state(*it, side_);
	}
	troops_.erase(troops_.begin() + index);

	if (hdlgP) {
		update_to_ui_side_edit(hdlgP, false);
	}
}

bool tside::operator==(const tside& that) const
{
	if (except_) return false;
	if (side_ != that.side_) return false;
	if (name_ != that.name_) return false;
	if (leader_ != that.leader_) return false;
	if (controller_ != that.controller_) return false;
	if (candidate_cards_ != that.candidate_cards_) return false;
	if (holded_cards_ != that.holded_cards_) return false;
	if (features_ != that.features_) return false;
	if (navigation_ != that.navigation_) return false;
	if (gold_ != that.gold_) return false;
	if (income_ != that.income_) return false;
	if (flag_ != that.flag_) return false;
	if (build_ != that.build_) return false;
	if (technologies_ != that.technologies_) return false;
	if (ally_ != that.ally_) return false;
	if (reserve_heros_ != that.reserve_heros_) return false;
	if (allow_player_ != that.allow_player_) return false;

	if (cities_.size() != that.cities_.size()) return false;
	for (size_t i = 0; i < cities_.size(); i ++) {
		if (cities_[i] != that.cities_[i]) return false;
	}

	if (troops_.size() != that.troops_.size()) return false;
	for (size_t i = 0; i < troops_.size(); i ++) {
		if (troops_[i] != that.troops_[i]) return false;
	}
	return true;
}

std::string tside::generate_features(controller_tag::CONTROLLER cntl) const
{
	std::stringstream strstr;
	int level;

	for (std::vector<arms_feature>::const_iterator it = features_.begin(); it != features_.end(); ++ it) {
		if (it == features_.begin()) {
			strstr << "(";
		} else {
			strstr << ", (";
		}
		strstr << editor_config::arms[it->arms_].first << ", ";
		if (cntl == controller_tag::HUMAN) {
			level = std::min<int>(it->level_ + 1, 3);
		} else {
			level = it->level_;
		}
		strstr << level << ", ";
		strstr << it->feature_ << ")";
	}
	return strstr.str();
}

void tside::tcity::from_ui(HWND hdlgP, tside& side)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_HERO);
	int select_hero = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	if (select_hero != heros_army_[0]) {
		int original = heros_army_[0];
		heros_army_.clear();
		heros_army_.push_back(select_hero);
		scenario.do_state(original, side.side_);
		scenario.do_state(heros_army_[0], side.side_, tscenario::STATE_ARMY, heros_army_[0]);

		for (std::map<int, tscenario::hero_state>::iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
			tscenario::hero_state::tstate& state = it->second.state(side.side_);

			if (state.valid() && state.city == original) {
				state.city = heros_army_[0];
			}
		}
		for (std::vector<tunit>::iterator it = side.troops_.begin(); it != side.troops_.end(); ++ it) {
			if (it->city_ == original) {
				it->city_ = heros_army_[0];
			}
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_MAYOR);
	mayor_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_TYPE);
	type_ = editor_config::city_utypes[ComboBox_GetCurSel(hctl)].first;

	soldiers_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_SOLDIERS));

	loc_.x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_X));
	loc_.y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_Y));

	traits_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_TRAIT);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			traits_.insert(editor_config::city_traits[idx].first);
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_CHARACTER);
	character_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	not_recruit_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_NOTRECRUIT);
	LVITEM lvi;
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			lvi.iItem = idx;
			lvi.mask = LVIF_PARAM;
			lvi.iSubItem = 0;
			lvi.pszText = NULL;
			lvi.cchTextMax = 0;
			ListView_GetItem(hctl, &lvi);

			not_recruit_.push_back(editor_config::troop_utypes[lvi.lParam].first);
		}
	}
}

void tside::tcity::update_to_ui_side_edit(HWND hdlgP, int index) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY);

	LVITEM lvi;
	int count = ListView_GetItemCount(hctl);
	int column = 0;

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 名称
	if (index < 0 || index >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = index;
	}
	lvi.iSubItem = column ++;
	if (alias_.empty()) {
		strcpy(text, utf8_2_ansi(gdmgr.heros_[heros_army_[0]].name().c_str()));
	} else {
		strstr.str("");
		strstr << dgettext(ns::_main.textdomain_.c_str(), alias_.c_str());
		strstr << "(" << gdmgr.heros_[heros_army_[0]].name() << ")";
		strcpy(text, utf8_2_ansi(strstr.str().c_str()));
	}
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << utf8_2_ansi(unit_types.find(type_)->type_name().c_str());
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// soldiers
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << soldiers_;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 坐标
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << "(" << loc_.x << "," << loc_.y << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特质
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	for (std::set<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
		if (it != traits_.begin()) {
			strstr << ", ";
		}
		strstr << utf8_2_ansi(ns::campaign.city_trait(*it).c_str());
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特色
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	if (character_ != NO_ESPECIAL) {
		strstr << utf8_2_ansi(unit_types.especial(character_).name_.c_str());
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 不可征兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << not_recruit_.size() << "(";
	for (std::vector<std::string>::const_iterator it = not_recruit_.begin(); it != not_recruit_.end(); ++ it) {
		if (it == not_recruit_.begin()) {
			strstr << utf8_2_ansi(unit_types.find(*it)->type_name().c_str());
		} else {
			strstr << ", " << utf8_2_ansi(unit_types.find(*it)->type_name().c_str());
		}
	}
	strstr << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 太守
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	if (mayor_ != HEROS_INVALID_NUMBER) {
		strcpy(text, utf8_2_ansi(gdmgr.heros_[mayor_].name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 在职
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << service_heros_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 在野
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << wander_heros_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 经济区
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = column ++;
	strstr.str("");
	strstr << economy_area_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tside::tcity::update_to_ui_city_edit(HWND hdlgP, tside& side, bool partial)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::stringstream strstr;
	int value, selected_row = -1;
	HWND hctl;

	if (!partial) {
		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_TYPE);
		ComboBox_ResetContent(hctl);
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = editor_config::city_utypes.begin(); it != editor_config::city_utypes.end(); ++ it) {
			ComboBox_AddString(hctl, utf8_2_ansi(it->second->type_name().c_str()));
			if (type_ == it->first) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_CHARACTER);
		ComboBox_AddString(hctl, "");
		ComboBox_SetItemData(hctl, 0, NO_ESPECIAL);
		selected_row = 0;
		const std::vector<tespecial>& characters = unit_types.especials();
		for (std::vector<tespecial>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
			const tespecial& character = *it;
			ComboBox_AddString(hctl, utf8_2_ansi(character.name_.c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, character.index_);
			if (character.index_ == character_) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_HERO);
		ComboBox_ResetContent(hctl);
		for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.artificals_.begin(); it != scenario.artificals_.end(); ++ it) {
			if (it->second.allocated(side.side_) && it->first != heros_army_[0]) continue;
			ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[it->first].name().c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
			if (it->first == heros_army_[0]) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		UpDown_SetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_SOLDIERS), soldiers_);

		hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_X);
		UpDown_SetPos(hctl, loc_.x);

		hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_Y);
		UpDown_SetPos(hctl, loc_.y);
	}

	// mayor
	hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_MAYOR);
	ComboBox_ResetContent(hctl);
	ComboBox_AddString(hctl, "");
	ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
	selected_row = 0;
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		const tscenario::hero_state::tstate& state = it->second.state(side.side_);
		if (!state.valid() || state.city != heros_army_[0]) continue;
		if (state.state != tscenario::STATE_SERVICE && state.state != tscenario::STATE_ARMY) continue;

		if (it->first == heros_army_[0] || it->first == side.leader_) continue;
		ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[it->first].name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (it->first == mayor_) {
			selected_row = ComboBox_GetCount(hctl) - 1;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	int index, count;
	LVITEM lvi;
	char text[_MAX_PATH];

	if (!partial) {
		// trait
		hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_TRAIT);
		count = ListView_GetItemCount(hctl);
		ListView_DeleteAllItems(hctl);
		index = 0;
		for (std::vector<std::pair<std::string, const config*> >::const_iterator it = editor_config::city_traits.begin(); it != editor_config::city_traits.end(); ++ it) {
			const config& cfg = *it->second;

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
			strcpy(text, utf8_2_ansi(name.c_str()));
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

		// not recruit
		hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_NOTRECRUIT);
		count = ListView_GetItemCount(hctl);
		ListView_DeleteAllItems(hctl);
		index = 0;
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = editor_config::troop_utypes.begin(); it != editor_config::troop_utypes.end(); ++ it, index ++) {
			const unit_type* ut = it->second;
			
			if (ut->especial() != NO_ESPECIAL) {
				continue;
			}
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 名称
			lvi.iItem = ListView_GetItemCount(hctl);
			lvi.iSubItem = 0;
			strstr.str("");
			strstr << utf8_2_ansi(ut->type_name().c_str());
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			lvi.lParam = (LPARAM)index;
			ListView_InsertItem(hctl, &lvi);

			// level
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 1;
			strstr.str("");
			strstr << ut->level();
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			// level
			lvi.mask = LVIF_TEXT;
			lvi.iSubItem = 2;
			strstr.str("");
			strstr << utf8_2_ansi(hero::arms_str(ut->arms()).c_str());
			strcpy(text, strstr.str().c_str());
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			if (std::find(not_recruit_.begin(), not_recruit_.end(), it->first) != not_recruit_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}
		strstr.str("");
		strstr << utf8_2_ansi(_("Cannot recruit")) << "(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
		Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITYEDIT_NOTRECRUIT), strstr.str().c_str());

		// alias
		if (!alias_.empty()) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CITYEDIT_ALIAS_MSGID), alias_.c_str());
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CITYEDIT_ALIAS), dgettext_2_ansi(ns::_main.textdomain_.c_str(), alias_.c_str()));
		}
	}

	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
	ListView_DeleteAllItems(hctl);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		if (it->second.allocated(HEROS_INVALID_SIDE)) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];
		candidate_hero::fill_row(hctl, h);
	}

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<int>::const_iterator it = service_heros_.begin(); it != service_heros_.end(); ++ it) {
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
		value = h.base_catalog_;
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

	// wander hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_WANDER);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<int>::const_iterator it = wander_heros_.begin(); it != wander_heros_.end(); ++ it) {
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
		value = h.base_catalog_;
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

	// economy area
	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAX);
	UpDown_SetPos(hctl, 1);

	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAY);
	UpDown_SetPos(hctl, 1);

	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_EA);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::set<map_location>::const_iterator it = economy_area_.begin(); it != economy_area_.end(); ++ it) {
		
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 编号
		lvi.iItem = index;
		lvi.iSubItem = 0;
		strstr.str("");
		strstr << ++ index;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		lvi.lParam = (LPARAM)posix_mku32(it->x, it->y);
		ListView_InsertItem(hctl, &lvi);

		// X
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		strstr << it->x;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// Y
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strstr.str("");
		strstr << it->y;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tside::tunit::from_ui(HWND hdlgP, tside& side)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_CITY);
	int selected_hero = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	city_ = selected_hero;

	hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_TYPE);
	type_ = editor_config::troop_utypes[ComboBox_GetCurSel(hctl)].first;

	hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_CHARACTER);
	character_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	loc_.x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_X));
	loc_.y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_Y));

	traits_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_TRAIT);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			traits_.insert(editor_config::troop_traits[idx].first);
		}
	}
}

void tside::tunit::update_to_ui_side_edit(HWND hdlgP, int index) const
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP);

	LVITEM lvi;
	int count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 主将
	if (index < 0 || index >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = index;
	}
	lvi.iSubItem = 0;
	strcpy(text, utf8_2_ansi(gdmgr.heros_[heros_army_[0]].name().c_str()));
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 副将I
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	if (heros_army_.size() >= 2) {
		strcpy(text, utf8_2_ansi(gdmgr.heros_[heros_army_[1]].name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 副将II
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	if (heros_army_.size() == 3) {
		strcpy(text, utf8_2_ansi(gdmgr.heros_[heros_army_[2]].name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	strstr.str("");
	strstr << utf8_2_ansi(unit_types.find(type_)->type_name().c_str());
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 坐标
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 4;
	strstr.str("");
	strstr << "(" << loc_.x << "," << loc_.y << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特质
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
	strstr.str("");
	for (std::set<std::string>::const_iterator it = traits_.begin(); it != traits_.end(); ++ it) {
		if (it != traits_.begin()) {
			strstr << ", ";
		}
		strstr << utf8_2_ansi(ns::campaign.troop_trait(*it).c_str());
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特色
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 6;
	strstr.str("");
	if (character_ != NO_ESPECIAL) {
		strstr << utf8_2_ansi(unit_types.especial(character_).name_.c_str());
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 城市
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 7;
	strstr.str("");
	if (city_ != HEROS_INVALID_NUMBER) {
		strstr << utf8_2_ansi(gdmgr.heros_[city_].name().c_str());
	} else {
		strstr << "(" << utf8_2_ansi(_("Roam")) << ")";
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tside::tunit::update_to_ui_troop_edit(HWND hdlgP, tside& side, bool partial)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::stringstream strstr;
	utils::string_map symbols;
	int value, selected_row = -1;
	HWND hctl;

	if (!partial) {
		hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_TYPE);
		ComboBox_ResetContent(hctl);
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = editor_config::troop_utypes.begin(); it != editor_config::troop_utypes.end(); ++ it) {
			const unit_type* ut = it->second;
			strstr.str("");
			strstr << utf8_2_ansi(ut->type_name().c_str());
			strstr << "(";
			symbols["level"] = lexical_cast_default<std::string>(ut->level());
			strstr << utf8_2_ansi(vgettext2("Lv$level", symbols).c_str());
			strstr << " " << utf8_2_ansi(hero::arms_str(ut->arms()).c_str()) << ")";
			ComboBox_AddString(hctl, strstr.str().c_str());
			if (type_ == it->first) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_CHARACTER);
		ComboBox_AddString(hctl, "");
		ComboBox_SetItemData(hctl, 0, NO_ESPECIAL);
		selected_row = 0;
		const std::vector<tespecial>& characters = unit_types.especials();
		for (std::vector<tespecial>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
			const tespecial& character = *it;
			ComboBox_AddString(hctl, utf8_2_ansi(character.name_.c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, character.index_);
			if (character.index_ == character_) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_CITY);
		ComboBox_ResetContent(hctl);
		if (side.cities_.empty()) {
			strstr.str("");
			strstr << "(" << utf8_2_ansi(_("Roam")) << ")";
			ComboBox_AddString(hctl, strstr.str().c_str());
			ComboBox_SetItemData(hctl, 0, HEROS_INVALID_NUMBER);
			selected_row = 0;
		} else {
			selected_row = -1;
			for (std::vector<tcity>::const_iterator it = side.cities_.begin(); it != side.cities_.end(); ++ it) {
				int city = it->heros_army_[0];
				ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[city].name().c_str()));
				ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, city);
				if (city_ == city) {
					selected_row = ComboBox_GetCount(hctl) - 1;
				}
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_X);
		UpDown_SetPos(hctl, loc_.x);

		hctl = GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_Y);
		UpDown_SetPos(hctl, loc_.y);
	}

	int index, count;
	LVITEM lvi;
	char text[_MAX_PATH];

	if (!partial) {
		// trait
		hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_TRAIT);
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
			strcpy(text, utf8_2_ansi(name.c_str()));
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
	}

	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CANDIDATEHERO);
	ListView_DeleteAllItems(hctl);
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		if (it->second.allocated(HEROS_INVALID_SIDE)) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];
		candidate_hero::fill_row(hctl, h);
	}

	// hero
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_HERO);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::vector<int>::const_iterator it = heros_army_.begin(); it != heros_army_.end(); ++ it) {
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
		value = h.base_catalog_;
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

void tside::arms_feature::from_ui(HWND hdlgP)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_ARMS);
	arms_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	level_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_FEATURE);
	feature_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tside::arms_feature::update_to_ui_side_edit(HWND hdlgP, int index) const
{
	std::stringstream strstr;
	char text[_MAX_PATH];
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE);
	LVITEM lvi;
	int count = ListView_GetItemCount(hctl);

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 兵科
	if (index < 0 || index >= count) {
		lvi.iItem = count;
	} else {
		lvi.iItem = index;
	}
	lvi.iSubItem = 0;
	lvi.pszText = const_cast<char*>(editor_config::arms[arms_].second.c_str());
	lvi.lParam = (LPARAM)0;
	if (lvi.iItem != count) {
		ListView_SetItem(hctl, &lvi);
	} else {
		ListView_InsertItem(hctl, &lvi);
	}

	// 级别
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	strstr.str("");
	strstr << level_;
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特技
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	strcpy(text, utf8_2_ansi(hero::feature_str(feature_).c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tside::arms_feature::update_to_ui_feature_edit(HWND hdlgP)
{
	int index, count;

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_ARMS);
	count = ComboBox_GetCount(hctl);
	for (index = 0; index < count; index ++) {
		if (ComboBox_GetItemData(hctl, index) == arms_) {
			break;
		}
	}
	ComboBox_SetCurSel(hctl, index);

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	count = ComboBox_GetCount(hctl);
	for (index = 0; index < count; index ++) {
		if (ComboBox_GetItemData(hctl, index) == level_) {
			break;
		}
	}
	ComboBox_SetCurSel(hctl, index);

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_FEATURE);
	count = ComboBox_GetCount(hctl);
	for (index = 0; index < count; index ++) {
		if (ComboBox_GetItemData(hctl, index) == feature_) {
			break;
		}
	}
	ComboBox_SetCurSel(hctl, index);
}

std::string tscenario::default_map_data()
{
	std::stringstream strstr;

	strstr << "border_size=1\n";
	strstr << "usage=map\n";
	strstr << "\n";
	for (int row = 0; row < 7; row ++) {
		for (int col = 0; col < 7; col ++) {
			if (col != 0) {
				strstr << ", ";
			}
			strstr << std::setw(12) << std::setfill(' ') << std::setiosflags(std::ios::left) << "Gg";
		}
		strstr << "\n";
	}
	return strstr.str();
}

tscenario::tscenario(const std::string& campaign_id, const std::string& id, int index)
	: tscenario_(id)
	, side_()
	, campaign_id_(campaign_id)
	, index_(index)
	, dirty_(0)
	, multiplayer_(false)
{
}

tscenario::~tscenario()
{
	event_.clear();
}

std::string tscenario::file(bool multiplayer, const std::string& campaign_id, const std::string& scenario_id, int index, bool absolute)
{
	std::stringstream strstr;
	if (!multiplayer) {
		if (absolute) {
			strstr << game_config::path << "\\data\\campaigns\\";
			strstr << campaign_id << "\\scenarios\\";
			strstr << scenario_id << ".cfg";
		} else {
			strstr << "campaigns/";
			strstr << campaign_id << "/scenarios/";
			strstr << scenario_id << ".cfg";
		}
	} else {
		if (absolute) {
			strstr << game_config::path << "\\data\\multiplayer\\mscenarios\\";
			strstr << scenario_id << ".cfg";
		} else {
			strstr << "multiplayer/mscenarios/";
			strstr << scenario_id << ".cfg";
		}
	}
	return strstr.str();
}

std::string tscenario::file(bool absolute) const
{
	return file(multiplayer_, campaign_id_, id_, index_, absolute);
}

std::string tscenario::map_file(bool multiplayer, const std::string& campaign_id, const std::string& scenario_id, int index, bool absolute)
{
	std::stringstream strstr;
	if (!multiplayer) {
		// if (ns::_main.must_exist_map()) {
			if (absolute) {
				strstr << game_config::path << "\\data\\campaigns\\";
				strstr << campaign_id << "\\maps\\";
				strstr << scenario_id << ".map";
			} else {
				strstr << "campaigns/";
				strstr << campaign_id << "/maps/";
				strstr << scenario_id << ".map";
			}
		// } else {
		//	strstr.str("");
		// }
	} else {
		if (absolute) {
			strstr << game_config::path << "\\data\\multiplayer\\mmaps\\";
			strstr << scenario_id << ".map";
		} else {
			strstr << "multiplayer/mmaps/";
			strstr << scenario_id << ".map";
		}
	}
	return strstr.str();
}

std::string tscenario::map_file(bool absolute) const
{ 
	return map_file(multiplayer_, campaign_id_, id_, index_, absolute);
}

std::string tscenario::map_data_from_file(const std::string& file) const
{
	return read_file(file, true);
}

map_location tscenario::map_data_size(const std::string& map_data) const
{
	if (map_data.empty()) return map_location();

	// Test whether there is a header section
	size_t header_offset = map_data.find("\n\n");
	if(header_offset == std::string::npos) {
		// For some reason Windows will fail to load a file with \r\n
		// lineending properly no problems on Linux with those files.
		// This workaround fixes the problem the copy later will copy
		// the second \r\n to the map, but that's no problem.
		header_offset = map_data.find("\r\n\r\n");
	}
	const size_t comma_offset = map_data.find(",");
	// The header shouldn't contain commas, so if the comma is found
	// before the header, we hit a \n\n inside or after a map.
	// This is no header, so don't parse it as it would be.
	if (header_offset == std::string::npos || comma_offset < header_offset) {
		return map_location();
	}

	std::string header_str(std::string(map_data, 0, header_offset + 1));
	config header;
	read(header, header_str);

	int border_size = header["border_size"];
	const std::string usage = header["usage"];

	if (usage != "map") {
		return map_location();
	}
	return map_location(2, 2);
}

int tscenario::null_side() const
{
	int index = 0;
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it, index ++) {
		if (it->controller_ == controller_tag::EMPTY) {
			return index;
		}
	}
	return -1;
}

bool tscenario::side_equal() const
{
	if (side_.size() != side_from_cfg_.size()) {
		return false;
	}
	for (size_t i = 0; i < side_.size(); i ++) {
		if (side_[i] != side_from_cfg_[i]) {
			return false;
		}
	}
	return true;
}

bool tscenario::event_equal() const
{
	if (event_.size() != event_from_cfg_.size()) {
		return false;
	}
	for (size_t i = 0; i < event_.size(); i ++) {
		if (event_[i] != event_from_cfg_[i]) {
			return false;
		}
	}
	return true;
}

void tscenario::from_config(int index, const config& scenario_cfg)
{
	index_ = index;

	id_ = scenario_cfg["id"].str();

	// if (ns::_main.must_exist_map()) {
		map_data_ = map_data_from_file(map_file(true));
	// } else {
	//	map_data_ = "";
	// }
	
	next_scenario_ = scenario_cfg["next_scenario"].str();
	turns_ = scenario_cfg["turns"].to_int(-1);
	if (scenario_cfg.has_attribute("duel")) {
		const std::string& duel = scenario_cfg["duel"];
		if (duel == "no") {
			duel_ = NO_DUEL;
		} else if (duel == "always") {
			duel_ = ALWAYS_DUEL;
		} else {
			duel_ = RANDOM_DUEL;
		}
	}
	enemy_no_city_ = scenario_cfg["victory_when_enemy_no_city"].to_bool(true);
	fallen_to_unstage_ = scenario_cfg["fallen_to_unstage"].to_bool();
	tent_ = scenario_cfg["tent"].to_bool();
	shroud_ = scenario_cfg["shroud"].to_bool();
	fog_ = scenario_cfg["fog"].to_bool();

	std::vector<std::string> vstr = utils::split(scenario_cfg["treasures"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int t = lexical_cast_default<int>(*it, -1);
		std::map<int, int>::iterator find = treasures_.find(t);
		if (find == treasures_.end()) {
			treasures_[t] = 1;
		} else {
			find->second ++;
		}
	}

	if (scenario_cfg.child("prelude")) {
		prelude_ = scenario_cfg.child("prelude");
	} else {
		prelude_ = config();
	}

	BOOST_FOREACH (const config &cfg, scenario_cfg.child_range("event")) {
		if (cfg["name"].str() != "prestart") continue;
		const config& objectives_cfg = cfg.child("objectives");
		if (!objectives_cfg) continue;

		objectives_.from_config(objectives_cfg);
	}

	BOOST_FOREACH (const config &side_cfg, scenario_cfg.child_range("side")) {
		side_.push_back(tside(this));
		tside& s = side_.back();

		s.from_config(side_cfg);
	}

	// form cityno_map need ns::cityno_map
	ns::cityno_map = generate_cityno_map();

	vstr = utils::parenthetical_split(scenario_cfg["roads"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> vstr1 = utils::split(*it);
		if (vstr1.size() == 2) {
			int h1 = hero_from_cityno_map(ns::cityno_map, lexical_cast_default<int>(vstr1[0]));
			int h2 = hero_from_cityno_map(ns::cityno_map, lexical_cast_default<int>(vstr1[1]));
			if (h1 != HEROS_INVALID_NUMBER && h2 != HEROS_INVALID_NUMBER && h1 != h2) {
				roads_.insert(std::make_pair(h1, h2));
			}
		}
	}

	BOOST_FOREACH (const config &event_cfg, scenario_cfg.child_range("event")) {
		if (event_cfg["name"].str() == "prestart") {
			continue;
		}

		event_.push_back(tevent());
		tevent& e = event_.back();

		e.from_config(event_cfg);
	}

	// remember side, will use to compare in future.
	side_from_cfg_ = side_;
	event_from_cfg_ = event_;
	scenario_from_cfg_ = *this;

	// maybe occure except duration read config.
	dirty_ = 0;
	set_dirty(BIT_SIDE, !side_equal());
	set_dirty(BIT_EVENT, !event_equal());
}

bool tscenario::new_side()
{
	side_.push_back(tside(this));
	tside& side = side_.back();
	side.side_ = side_.size() - 1;

	return true;
}

void tscenario::erase_side(int index, HWND hdlgP)
{
	tside& side = side_[index];

	for (int idx = side.cities_.size() - 1; idx >= 0; idx --) {
		side.erase_city(idx, NULL);
	}
	for (int idx = side.troops_.size() - 1; idx >= 0; idx --) {
		side.erase_troop(idx, NULL);
	}
	side_.erase(side_.begin() + index);
	for (size_t idx = index; idx < side_.size(); idx ++) {
		side_[idx].side_ --;
	}

	update_to_ui(hdlgP);
}

bool tscenario::new_event(const std::string& name)
{
	event_.push_back(tevent());
	tevent& e = event_.back();
	e.name_ = name;

	return true;
}

void tscenario::erase_event(int index, HWND hdlgP)
{
	tevent& e = event_[index];

	event_.erase(event_.begin() + index);

	if (hdlgP) {
		update_to_ui(hdlgP);
	}
}

void tscenario::update_to_ui(HWND hdlgP)
{
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPSCENARIO_NEXTSCENARIO);
	ComboBox_ResetContent(hctl);
	for (std::vector<tscenario>::const_iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (it->id_ != id_) {
			ComboBox_AddString(hctl, it->id_.c_str());
		}
	}
	ComboBox_AddString(hctl, "null");
	// int selected_row = ComboBox_GetCount(hctl) - 1;
	int selected_row = -1;
	for (int index = 0; index < ComboBox_GetCount(hctl); index ++) {
		ComboBox_GetLBText(hctl, index, text);
		if (!stricmp(text, next_scenario_.c_str())) {
			selected_row = index;
			break;
		}
	}
	ComboBox_SetCurSel(hctl, selected_row);

	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_ENEMYNOCITY), enemy_no_city_);
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_FALLENTOUNSTAGE), fallen_to_unstage_);
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_TENT), tent_);
	
	update_to_ui_treasures(hdlgP);
	update_to_ui_roads(hdlgP);

	// side
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		it->update_to_ui(hdlgP);
	}

	// event
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_EVENT);
	ListView_DeleteAllItems(hctl);
	for (std::vector<tevent>::const_iterator it = event_.begin(); it != event_.end(); ++ it) {
		it->update_to_ui(hdlgP);
	}
}

void tscenario::update_to_ui_treasures(HWND hdlgP)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::stringstream strstr;

	const std::vector<ttreasure>& treasures = unit_types.treasures();
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE);
	ListView_DeleteAllItems(hctl);

	int index = 0;
	for (std::map<int, int>::const_iterator it = treasures_.begin(); it != treasures_.end(); ++ it, index ++) {
		const ttreasure& t = unit_types.treasure(it->first);
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 编号
		lvi.iItem = index;
		lvi.iSubItem = 0;
		sprintf(text, "%i", index + 1);
		lvi.pszText = text;
		lvi.lParam = (LPARAM)it->first;
		ListView_InsertItem(hctl, &lvi);

		// 名称
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strcpy(text, utf8_2_ansi(t.name().c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 数量
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strstr.str("");
		strstr << it->second;
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strstr.str("");
		strstr << utf8_2_ansi(hero::feature_str(t.feature()).c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tscenario::update_to_ui_roads(HWND hdlgP)
{
	LVITEM lvi;
	char text[_MAX_PATH];
	std::stringstream strstr;

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_ROAD);
	ListView_DeleteAllItems(hctl);

	int index = 0;
	for (std::multimap<int, int>::const_iterator it = roads_.begin(); it != roads_.end(); ++ it, index ++) {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// number
		lvi.iItem = index;
		lvi.iSubItem = 0;
		sprintf(text, "%i", index + 1);
		lvi.pszText = text;
		lvi.lParam = (LPARAM)it->first;
		ListView_InsertItem(hctl, &lvi);

		// city
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 1;
		strstr.str("");
		strstr << utf8_2_ansi(gdmgr.heros_[it->first].name().c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// city
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		strstr.str("");
		strstr << utf8_2_ansi(gdmgr.heros_[it->second].name().c_str());
		strcpy(text, strstr.str().c_str());
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
	}
}

void tscenario::from_ui(HWND hdlgP)
{
	char text[_MAX_PATH];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPSCENARIO_NEXTSCENARIO);
	ComboBox_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	next_scenario_ = text;

	turns_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CAMPSCENARIO_TURNS));
	enemy_no_city_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_ENEMYNOCITY));
	fallen_to_unstage_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_FALLENTOUNSTAGE));
	tent_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_TENT));

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPSCENARIO_DUEL);
	duel_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
}

void tscenario::from_ui_treasure(HWND hdlgP, bool edit)
{
	int val = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_TREASURE_COUNT));
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASURE_TREASURE);
	int t = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	std::map<int, int>::iterator find = treasures_.find(t);
	if (edit) {
		find->second = val;
	} else {
		if (find == treasures_.end()) {
			treasures_[t] = val;
		} else {
			find->second = find->second + val;
		}
	}
}

void tscenario::from_ui_road(HWND hdlgP, bool edit)
{
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ROAD_CITY1);
	int city1 = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	hctl = GetDlgItem(hdlgP, IDC_CMB_ROAD_CITY2);
	int city2 = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	roads_.insert(std::make_pair(city1, city2));
}

void tscenario::generate()
{
	std::stringstream strstr;
	uint32_t bytertd;

	posix_file_t fp = INVALID_FILE;
	posix_file_t fp_map = INVALID_FILE;
	posix_fopen(file(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		return;
	}
	if (ns::_main.must_exist_map()) {
		posix_fopen(map_file(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp_map);
		if (fp_map == INVALID_FILE) {
			posix_fclose(fp);
			return;
		}
	}

	//
	// generate cityno
	//
	// number-->cityno
	ns::cityno_map = generate_cityno_map();

	if (!multiplayer_) {
		strstr << "[scenario]\n";
	} else {
		strstr << "#textdomain wesnoth-multiplayer\n";
		strstr << "\n";

		strstr << "[multiplayer]\n";
	}
	strstr << "\tid = " << id_ << "\n";
	strstr << "\tnext_scenario = " << next_scenario_ << "\n";
	strstr << "\tname = _ \"" << id_ << "\"\n";
	if (ns::_main.must_exist_map()) {
		strstr << "\tmap_data = \"{" << map_file() << "}\"\n";
	}
	strstr << "\tturns = " << turns_ << "\n";
	if (duel_ == NO_DUEL) {
		strstr << "\tduel = no\n";
	} else if (duel_ == ALWAYS_DUEL) {
		strstr << "\tduel = always\n";
	}
	if (!treasures_.empty()) {
		strstr << "\ttreasures = ";
		for (std::map<int, int>::const_iterator it = treasures_.begin(); it != treasures_.end(); ++ it) {
			for (int i = 0; i < it->second; i ++) {
				if (it != treasures_.begin() || i) {
					strstr << ", ";
				}
				strstr << it->first;
			}
		}
		strstr << "\n";
	}

	if (!enemy_no_city_) {
		strstr << "\tvictory_when_enemy_no_city = no\n";
	}
	if (fallen_to_unstage_) {
		strstr << "\tfallen_to_unstage = yes\n";
	}
	if (tent_) {
		strstr << "\ttent = yes\n";
	}
	
	if (!roads_.empty()) {
		std::map<int, std::set<int> > cityno_roads;
		std::set<int> roads;
		int analyzing = roads_.begin()->first;
		for (std::multimap<int, int>::const_iterator it = roads_.begin(); it != roads_.end(); ++ it) {
			if (it->first != analyzing) {
				cityno_roads.insert(std::make_pair(ns::cityno_map.find(analyzing)->second, roads));
				analyzing = it->first;
				roads.clear();
			}
			roads.insert(ns::cityno_map.find(it->second)->second);
		}
		cityno_roads.insert(std::make_pair(ns::cityno_map.find(analyzing)->second, roads));

		strstr << "\troads = ";
		for (std::map<int, std::set<int> >::const_iterator it = cityno_roads.begin(); it != cityno_roads.end(); ++ it) {
			for (std::set<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
				if (it != cityno_roads.begin() || it2 != it->second.begin()) {
					strstr << ", ";
				}
				strstr << "(" << it->first << "," << *it2 << ")";
			}
		}
		strstr << "\n";
	}

	strstr << "\n";
	// tod
	strstr << "\t{DAWN}\n";
	strstr << "\t{MORNING}\n";
	strstr << "\t{AFTERNOON}\n";
	strstr << "\t{DUSK}\n";
	strstr << "\t{FIRST_WATCH}\n";
	strstr << "\t{SECOND_WATCH}\n";

	strstr << "\n";
	// music
	strstr << "\t[music]\n";
	strstr << "\t\tname = legends_of_the_north.ogg\n";
	strstr << "\t[/music]\n";
	strstr << "\t{APPEND_MUSIC transience.ogg}\n";
	strstr << "\t{APPEND_MUSIC underground.ogg}\n";
	strstr << "\t{APPEND_MUSIC elvish-theme.ogg}\n";
	strstr << "\t{APPEND_MUSIC revelation.ogg}\n";

	strstr << "\n";

	if (!prelude_.empty()) {
		strstr << "\t[prelude]\n";
		::write(strstr, prelude_, 2);
		strstr << "\t[/prelude]\n";
	}

	strstr << "\n";
	// win/lose
	if (!objectives_.is_null()) {
		strstr << "\t[event]\n";
		strstr << "\t\tname = prestart\n";
		objectives_.generate(strstr, "\t\t");
		strstr << "\t[/event]\n";
	}

	strstr << "\n";
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		strstr << it->generate("\t");
		strstr << "\n";
	}

	strstr << "\n";

	for (std::vector<tevent>::const_iterator it = event_.begin(); it != event_.end(); ++ it) {
		if (it->is_null()) {
			continue;
		}
		it->generate(strstr, "\t");
		strstr << "\n";
	}
	if (!multiplayer_) {
		strstr << "[/scenario]\n";
	} else {
		strstr << "[/multiplayer]\n";
	}

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);

	if (fp_map != INVALID_FILE) {
		posix_fwrite(fp_map, map_data_.c_str(), map_data_.length(), bytertd);
		posix_fclose(fp_map);
	}

	dirty_ = 0;
	clear_except_dirty();
	scenario_from_cfg_ = *this;
	side_from_cfg_ = side_;
	event_from_cfg_ = event_;
}

std::map<int, int> tscenario::generate_cityno_map() const
{
	std::map<int, int> cityno_map;
	int cityno = 1;
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		for (std::vector<tside::tcity>::const_iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
			cityno_map[it2->heros_army_[0]] = cityno ++;
		}
	}
	return cityno_map;
}

int tscenario::hero_from_cityno_map(const std::map<int, int>& cityno_map, int no) const
{
	int h = HEROS_INVALID_NUMBER;
	for (std::map<int, int>::const_iterator it2 = cityno_map.begin(); it2 != cityno_map.end(); ++ it2) {
		if (it2->second == no) {
			h = it2->first;
			break;
		}
	}
	return h;
}

std::set<int> tscenario::get_cities() const
{
	std::set<int> cities;
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		for (std::vector<tside::tcity>::const_iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
			cities.insert(it2->heros_army_[0]);
		}
	}
	return cities;
}

bool tscenario::validate_roads()
{
	bool dirty = false;
	bool erased;
	const std::set<int> cities = get_cities();
	do {
		erased = false;
		for (std::multimap<int, int>::const_iterator it = roads_.begin(); it != roads_.end(); ++ it) {
			if (cities.find(it->first) == cities.end()) {
				roads_.erase(it);
				erased = true;
				dirty = true;
				break;
			}
			if (cities.find(it->second) == cities.end()) {
				roads_.erase(it);
				erased = true;
				dirty = true;
				break;
			}
		}
	} while (erased);

	set_dirty(tscenario::BIT_ROADS, scenario_from_cfg_.roads_ != roads_);
	return dirty;
}

bool tscenario::rpg_mode() const
{
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		const tside& side = *it;
		if (side.controller_ == controller_tag::HUMAN_AI) {
			return true;
		}
	}
	return false;
}

void tscenario::init_hero_state(hero_map& heros)
{
	persons_.clear();
	artificals_.clear();
	for (hero_map::iterator it = heros.begin(); it != heros.end(); ++ it) {
		if (editor_config::unallocatable_heros.find(it->number_) != editor_config::unallocatable_heros.end()) {
			continue;
		}
		if (it->gender_ != hero_gender_neutral) {
			persons_[it->number_] = hero_state();
		} else {
			artificals_[it->number_] = hero_state();
		}
	}
}

void tscenario::do_state(int h, int side, int s, int city)
{
	std::map<int, hero_state>::iterator it;
	if (gdmgr.heros_[h].gender_ != hero_gender_neutral) {
		it = persons_.find(h);
	} else {
		it = artificals_.find(h);
	}
	
	hero_state::tstate& state = it->second.state(side);

	if (s != STATE_UNKNOWN) {
		if (state.valid()) {
			state.state = s;
			state.city = city;
		} else {
			it->second.insert(side, s, city);
		}
	} else {
		it->second.erase(side);
	}
}

void tscenario::clear_except_dirty()
{
	for (std::vector<tside>::iterator it = side_.begin(); it != side_.end(); ++ it) {
		it->except_ = false;
		for (std::vector<tside::tcity>::iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
			it2->except_ = false;
		}
		for (std::vector<tside::tunit>::iterator it2 = it->troops_.begin(); it2 != it->troops_.end(); ++ it2) {
			it2->except_ = false;
		}
	}
}

void tscenario::set_dirty(int bit, bool set)
{
	if (set) {
		dirty_ |= 1 << bit;
	} else {
		dirty_ &= ~(1 << bit);
	}
	ns::set_dirty();
}

bool next_scenario_is_null(const std::string& id)
{
	if (id.empty()) return true;
	if (id == "null") return true;
	return false;
}

bool campaign_can_save(HWND hdlgP, bool save)
{
	std::stringstream strstr;
	std::string textdomain;
	if (!scenario_selector::multiplayer) {
		textdomain = ns::_main.textdomain_;
	} else {
		textdomain = "wesnoth-multiplayer";
	}

	int index;
	if (hdlgP) {
		DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA); 
		int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 

		if (!scenario_selector::multiplayer) {
			if (ns::_main.dirty_) {
				if (iSel == 0) {
					ns::_main.from_ui(pHdr->hwndDisplay);
				}
			}
		}
		index = 0;
		for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it, index ++) {
			if (it->dirty_) {
				if (iSel == index + 1) {
					it->from_ui(pHdr->hwndDisplay);
				}
			}
		}
	}
	std::set<std::string> scenario_ids;
	for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it, index ++) {
		scenario_ids.insert(it->id_);
	}

	// verify _main/_scenario
	if (!scenario_selector::multiplayer) {
		if (ns::_main.first_scenario_.empty()) {
			strstr << _("first_scenario in main is null, must set it!");
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
			return false;
		}
		if (ns::_main.mode_ == mode_tag::NONE) {
			strstr << _("mode is null, must set it!");
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
			return false;
		}
	}
	int next_scenario_nulls = 0;
	for (std::vector<tscenario>::const_iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (ns::_main.must_exist_map() && it->map_data_.empty()) {
			if (it->scenario_from_cfg_.id_ == it->id_ || !is_file(tscenario::map_file(it->multiplayer_, it->campaign_id_, it->scenario_from_cfg_.id_, it->index_, true).c_str())) {
				strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "，关卡没设置有效地图";
				posix_print_mb(strstr.str().c_str());
				return false;
			}
		}
		int i2 = 1;
		std::set<int> human, ai, human_ai;
		for (std::vector<tside>::const_iterator it2 = it->side_.begin(); it2 != it->side_.end(); ++ it2, i2 ++) {
			const tside& side = *it2;
			if (side.leader_ == HEROS_INVALID_NUMBER) {
				strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "关卡的第" << i2 << "势力没有设置君主";
				posix_print_mb(strstr.str().c_str());
				return false;
			}
			for (std::vector<tside::tcity>::const_iterator it3 = side.cities_.begin(); it3 != side.cities_.end(); ++ it3) {
				const tside::tcity& city = *it3;
				if (city.soldiers_ && side.controller_ != controller_tag::AI) {
					strstr << dgettext(textdomain.c_str(), it->id_.c_str()) << "scenario, ";
					strstr << "city(" << gdmgr.heros_[city.heros_army_[0]].name() << "), Can set soldiers only in AI controller.";
					posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
					return false;
				}
			}
			if (it2->controller_ == controller_tag::HUMAN) {
				human.insert(it2->side_);
			} else if (it2->controller_ == controller_tag::AI) {
				ai.insert(it2->side_);
			} else if (it2->controller_ == controller_tag::HUMAN_AI) {
				human_ai.insert(it2->side_);
			}
		}
		if (!scenario_selector::multiplayer) {
			if (human.size() > 1) {
				strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "关卡存在多个只限玩家势力";
				posix_print_mb(strstr.str().c_str());
				return false;
			}
		}
		if (!human.empty() && !human_ai.empty()) {
			strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "关卡既存在只限玩家又存在玩家/AI势力";
			posix_print_mb(strstr.str().c_str());
			return false;
		}
		if (next_scenario_is_null(it->next_scenario_)) {
			next_scenario_nulls ++;
		} else if (it->next_scenario_ == it->id_) {
			strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "'next_scenario is myself!";
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
			return false;
		} else if (scenario_ids.find(it->next_scenario_) == scenario_ids.end()) {
			strstr << utf8_2_ansi(dgettext(textdomain.c_str(), it->id_.c_str())) << "'next_scenario is unknown id!";
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
			return false;
		}
	}
	if (!scenario_selector::multiplayer) {
		if (next_scenario_nulls != 1) {
			strstr << _("Count of null next_scenario must be 1!");
			posix_print_mb(utf8_2_ansi(strstr.str().c_str()));
			return false;
		}
	}

	// one or more scenario.id changed, empty <campaign>/scenarios directory.
	for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (it->scenario_from_cfg_.id_ == it->id_) {
			continue;
		}
		strstr.str("");
		strstr << tscenario::file(it->multiplayer_, it->campaign_id_, it->scenario_from_cfg_.id_, it->index_, true);
		delfile1(strstr.str().c_str());

		// if exist, copy map
		const std::string src_map = tscenario::map_file(it->multiplayer_, it->campaign_id_, it->scenario_from_cfg_.id_, it->index_, true);
		const std::string dest_map = tscenario::map_file(it->multiplayer_, it->campaign_id_, it->id_, it->index_, true);
		if (is_file(src_map.c_str())) {
			if (!is_file(dest_map.c_str())) {
				copyfile(src_map.c_str(), dest_map.c_str());
				it->map_data_ = it->map_data_from_file(dest_map);
			}
			delfile1(src_map.c_str());
		}
	}

	if (save) {
		// generate
		if (ns::_main.dirty_) {
			ns::_main.generate();
		}
		index = 0;
		for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it, index ++) {
			if (it->dirty_) {
				it->generate();
			}
		}

		if (!scenario_selector::multiplayer) {
			campaign_enable_save_btn(FALSE);

			editor_config::campaign_id = ns::_main.id_;
			do_sync2();
		}
	}
	return true;
}

static bool save_if_dirty()
{
	if (campaign_get_save_btn()) {
		std::stringstream title, message;
		utils::string_map symbols;
		HWND hdlgP = gdmgr._hdlg_campaign;

		DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA); 
		int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 

		symbols["campaign"] = dgettext(ns::_main.textdomain_.c_str(), ns::_main.id_.c_str());
		title << utf8_2_ansi(vgettext2("Save campaign-$campaign", symbols).c_str());
		message << utf8_2_ansi(vgettext2("$campaign is dirty, do you want to save modify?", symbols).c_str());
		int retval = MessageBox(gdmgr._htb_campaign, message.str().c_str(), title.str().c_str(), MB_YESNO);
		if (retval == IDYES) {
			campaign_can_save(gdmgr._hdlg_campaign, true);
			return true;
		} else {
			campaign_enable_save_btn(FALSE);
			return false;
		}
	}
	return false;
}

void campaign_enter_ui(void)
{
	scenario_selector::switch_to(false);

	if (save_if_dirty()) {
		return;
	}


	// 让底下状态栏处于空闲状态
	StatusBar_Idle();

	strcpy(gdmgr.cfg_fname_, gdmgr._menu_text);
	StatusBar_SetText(gdmgr._hwnd_status, 0, gdmgr.cfg_fname_);

	campaign_refresh(gdmgr._hdlg_campaign);
	return;
}

// TRUE: 允许隐藏
// FALSE: 不允许隐藏
BOOL campaign_hide_ui(void)
{
	if (save_if_dirty()) {
		return FALSE;
	}
	return TRUE;
}

// 对话框消息处理函数
BOOL On_DlgCampaignInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	gdmgr._hdlg_campaign = hdlgP;
	ns::campaign.init_toolbar(gdmgr._hinst, hdlgP);
	campaign_enable_save_btn(FALSE);

	return FALSE;
}

void On_DlgCampaignCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch(id) {
	case IDM_NEW:
		ns::new_scenario();
		break;
	case IDM_DELETE:
		ns::delete_scenario(hdlgP);
		break;
	case IDM_HEROSTATE:
		if (ns::current_scenario >= 0) {
			DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_VISUAL2), NULL, DlgReportHeroStateProc);
		}
		break;

	case IDM_SAVE:
		campaign_can_save(hdlgP, true);
		break;
	}
	return;
}

VOID WINAPI OnSelChanging(HWND hwndDlg)
{
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA); 
    int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 
 
    // Create the new child dialog box.
	if (iSel == 0) {
		ns::_main.from_ui(pHdr->hwndDisplay);
	} else {
		ns::_scenario[iSel - 1].from_ui(pHdr->hwndDisplay);
	}
}

VOID WINAPI OnSelChanged(HWND hwndDlg)
{ 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA); 
    int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 
 
    // Destroy the current child dialog box, if any. 
	if (pHdr->hwndDisplay != NULL) {
        DestroyWindow(pHdr->hwndDisplay); 
	}
 
    // Create the new child dialog box.
	if (iSel == 0) {
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[iSel], hwndDlg, DlgCampaignMainProc);
	} else {
		pHdr->hwndDisplay = CreateDialogIndirect(gdmgr._hinst, pHdr->apRes[iSel], hwndDlg, DlgCampaignScenarioProc);
	}
	ShowWindow(pHdr->hwndDisplay, SW_RESTORE);
} 

BOOL On_DlgCampaignNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == TCN_SELCHANGE) {
		if (lpNMHdr->idFrom == IDC_TAB_CAMP_SCENARIO) {
			OnSelChanged(hdlgP);
		}
	} else if (lpNMHdr->code == TCN_SELCHANGING) {
		if (lpNMHdr->idFrom == IDC_TAB_CAMP_SCENARIO) {
			OnSelChanging(hdlgP);
		}
	} else if (lpNMHdr->code == TTN_NEEDTEXT) {
		LPTOOLTIPTEXT lpTt = (LPTOOLTIPTEXT)lpNMHdr;
		if (lpTt->hdr.idFrom == IDM_RESET) {
			strcpy(lpTt->szText, "Reset selected profile item to default");
		} else if (lpTt->hdr.idFrom == IDM_REFRESH) {
			strcpy(lpTt->szText, "Reload last saved configuration");
		} else {
			LoadString(gdmgr._hinst, (UINT)lpTt->hdr.idFrom, lpTt->szText, NUMELMS(lpTt->szText));
		}
	}

	return FALSE;
}

void On_DlgCampaignDestroy(HWND hdlgP)
{
	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (pHdr) {
		delete pHdr->apRes;
		delete pHdr;
	}
	return;
}

extern std::string mode_desc(mode_tag::tmode tag);
#endif

BOOL CALLBACK DlgCampaignProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifndef _ROSE_EDITOR
	LPPSHNOTIFY			lppsn = (LPPSHNOTIFY)lParam;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgCampaignInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgCampaignCommand);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgCampaignDestroy);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgCampaignNotify);
	}
#endif	
	return FALSE;
}

#ifndef _ROSE_EDITOR

void select_rank_cmb(HWND hctl, int rank)
{
	int count = ComboBox_GetCount(hctl);
	int index, data;

	for (index = count - 1; index >= 0; index --) {
		data = ComboBox_GetItemData(hctl, index);
		if (rank >= data) {
			break;
		}
	}
	if (index < 0) {
		index = 0;
	}
	ComboBox_SetCurSel(hctl, index);
}

BOOL On_DlgCampaignMainInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP); 
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE);

	ns::current_scenario = -1;
	campaign_enable_delete_btn(false);
	campaign_enable_herostate_btn(false);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FILE), utf8_2_ansi(_("Corresponding cfg")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ID), utf8_2_ansi(_("ID")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Name")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ABBREVIATION), utf8_2_ansi(_("Abbreviation")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_RANK), utf8_2_ansi(_("Rank")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CATALOG), utf8_2_ansi(_("scenario^Catalog")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FIRSTSCENARIO), utf8_2_ansi(_("First scenario")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGE), utf8_2_ansi(_("Image")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DESCRIPTION), utf8_2_ansi(_("Description")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MODE), utf8_2_ansi(_("Mode")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_SUBCONTINENT), dgettext_2_ansi("wesnoth-lib", "Subcontinent"));

	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPMAIN_BROWSEICON), utf8_2_ansi(_("Browse...")));
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPMAIN_BROWSEIMAGE), utf8_2_ansi(_("Browse...")));
	
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_FILE);
	Edit_SetText(hctl, ns::_main.file_.c_str());

	std::string str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ID), ns::_main.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_TEXTDOMAIN), ns::_main.textdomain_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_NAME_MSGID), ns::_main.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_NAME), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.id_.c_str())));

	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV_MSGID), ns::_main.abbrev_.c_str());
	strstr.str("");
	strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.abbrev_.c_str()));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV), strstr.str().c_str());
	if (is_file(ns::_main.icon(true).c_str())) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ICON), utf8_2_ansi(_("Exist")));
	} else {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ICON), utf8_2_ansi(_("Not exist")));
	}
	if (is_file(ns::_main.image(true).c_str())) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_IMAGE), utf8_2_ansi(_("Exist")));
	} else {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_IMAGE), utf8_2_ansi(_("Not exist")));
	}

	// description
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_DESC_MSGID), ns::_main.description().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_DESC), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.description().c_str())));

	std::set<mode_tag::tmode> mode_map;
	mode_map.insert(mode_tag::SCENARIO);
	mode_map.insert(mode_tag::RPG);
	mode_map.insert(mode_tag::TOWER);
	mode_map.insert(mode_tag::SIEGE);
	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_MODE);
	ComboBox_SetCurSel(hctl, 0);
	for (std::set<mode_tag::tmode>::const_iterator it = mode_map.begin(); it != mode_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(mode_desc(*it).c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
		if (ns::_main.mode_ == *it) {
			ComboBox_SetCurSel(hctl, ComboBox_GetCount(hctl) - 1);
		}
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_CATALOG);
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = tmain::catalog_map.begin(); it != tmain::catalog_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, ComboBox_GetCount(hctl) - 1);
		if (ns::_main.catalog_ == ComboBox_GetCount(hctl) - 1) {
			ComboBox_SetCurSel(hctl, ComboBox_GetCount(hctl) - 1);
		}
	}
	
	ns::_main.update_to_ui(hdlgP);

	return FALSE;
}

void OnCampaignMainEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_CAMPMAIN_TEXTDOMAIN) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_TEXTDOMAINSTATUS), utf8_2_ansi(_("Invalid string")));
		return;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_TEXTDOMAINSTATUS), "");
	if (ns::_main.textdomain_ == str) {
		return;
	}

	ns::_main.textdomain_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_NAME), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.id_.c_str())));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.abbrev_.c_str())));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_DESC), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), ns::_main.description().c_str())));
	
	ns::_main.set_dirty(tmain::BIT_TEXTDOMAIN, ns::_main.textdomain_ != ns::_main.main_from_cfg_.textdomain_);

	return;
}

void OnCampaignMainEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl;
	std::stringstream strstr;
	std::string that;
	int bit;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_CAMPMAIN_ABBREV_MSGID) {
		bit = tmain::BIT_ABBREV;
		that = ns::_main.main_from_cfg_.abbrev_;
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV);
	} else {
		return;
	}
	strstr.str("");
	if (text[0]) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), text));
	}
	Edit_SetText(hctl, strstr.str().c_str());
	ns::_main.set_dirty(bit, strcmp(text, that.c_str()));

	return;
}

void OnCampaignMainCmb(HWND hdlgP, int id, UINT codeNotify)
{
	int val, that;
	int index, bit;

	HWND hctl = GetDlgItem(hdlgP, id);

	if (id == IDC_CMB_CAMPMAIN_MODE) {
		bit = tmain::BIT_MODE;
		that = ns::_main.main_from_cfg_.mode_;
	} else if (id == IDC_CMB_CAMPMAIN_CATALOG) {
		bit = tmain::BIT_CATALOG;
		that = ns::_main.main_from_cfg_.catalog_;
	} else {
		return;
	}

	if (codeNotify == CBN_SELCHANGE) {
		index = ComboBox_GetCurSel(hctl);
		val = ComboBox_GetItemData(hctl, index);
		ns::_main.set_dirty(bit, val != that);
	}
	return;
}

void OnCampaignMainCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	std::string that;
	int bit;

	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	if (id == IDC_CMB_CAMPMAIN_FIRSTSCENARIO) {
		bit = tmain::BIT_FIRSTSCENARIO;
		that = ns::_main.main_from_cfg_.first_scenario_;
	} else {
		return;
	}

	ComboBox_GetLBText(hwndCtrl, ComboBox_GetCurSel(hwndCtrl), text);
	ns::_main.set_dirty(bit, strcmp(text, that.c_str()));

	return;
}

void OnCampaignMainBt(HWND hdlgP, int id, UINT codeNotify)
{
	char* ptr = GetBrowseFileName(dirname(ns::_main.icon(true).c_str()), "*.png\0*.png\0\0", TRUE);
	if (!ptr) return;

	HWND hctl;
	BOOL fok = FALSE;
	std::stringstream strstr;
	utils::string_map symbols;

	symbols["src"] = ptr;
	if (id == IDC_BT_CAMPMAIN_BROWSEICON) {
		fok = CopyFile(ptr, ns::_main.icon(true).c_str(), FALSE);
		symbols["dst"] = ns::_main.icon(true);
	} else if (id == IDC_BT_CAMPMAIN_BROWSEIMAGE) {
		fok = CopyFile(ptr, ns::_main.image(true).c_str(), FALSE);
		symbols["dst"] = ns::_main.image(true);
	} else {
		return;
	}
	if (symbols["src"].str() == symbols["dst"].str()) {
		strstr.str("");
		strstr << utf8_2_ansi(_("Selected is using file, do nothing."));
		posix_print_mb(strstr.str().c_str());
		return;
	}
	symbols["result"] = fok? "success": "fail";
	strstr.str("");
	strstr << utf8_2_ansi(vgettext2("Copy \"$src\" to \"$dst\" $result!", symbols).c_str());
	posix_print_mb(strstr.str().c_str());

	if (id == IDC_BT_CAMPMAIN_BROWSEICON) {
		fok = is_file(ns::_main.icon(true).c_str());
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ICON);
	} else {
		fok = is_file(ns::_main.image(true).c_str());
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_IMAGE);
	}
	Edit_SetText(hctl, fok? utf8_2_ansi(_("Exist")): utf8_2_ansi(_("Not exist")));
}

void OnCampaignMainBt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	bool that;
	int bit;

	if (id == IDC_CHK_CAMPMAIN_SUBCONTINENT) {
		bit = tmain::BIT_SUBCONTINENT;
		that = ns::_main.main_from_cfg_.subcontinent_;
	} else {
		return;
	}

	ns::_main.set_dirty(bit, (bool)Button_GetCheck(hwndCtrl) != that);

	return;
}

void On_DlgCampaignMainCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDC_ET_CAMPMAIN_TEXTDOMAIN:
		OnCampaignMainEt(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_ET_CAMPMAIN_ABBREV_MSGID:
		OnCampaignMainEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_CMB_CAMPMAIN_FIRSTSCENARIO:
		OnCampaignMainCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;
	case IDC_CMB_CAMPMAIN_MODE:
	case IDC_CMB_CAMPMAIN_CATALOG:
		OnCampaignMainCmb(hdlgP, id, codeNotify);
		break;
	case IDC_BT_CAMPMAIN_BROWSEICON:
	case IDC_BT_CAMPMAIN_BROWSEIMAGE:
		OnCampaignMainBt(hdlgP, id, codeNotify);
		break;

	case IDC_CHK_CAMPMAIN_SUBCONTINENT:
		OnCampaignMainBt2(hdlgP, id, hwndCtrl, codeNotify);
		break;
	}
}

BOOL CALLBACK DlgCampaignMainProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgCampaignMainInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgCampaignMainCommand);
	}
	
	return FALSE;
} 

BOOL On_DlgSideEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	char text[_MAX_PATH];
	if (ns::action_side == ma_edit) {
		strstr << utf8_2_ansi(_("Edit side"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add side"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NUMBER), utf8_2_ansi(_("Number")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Name")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEADER), utf8_2_ansi(_("Leader")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_CONTROLLER), utf8_2_ansi(_("Void")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_GOLD), utf8_2_ansi(_("Base gold")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_INCOME), utf8_2_ansi(_("Base income")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAVIGATION), dgettext_2_ansi("wesnoth-lib", "Navigation civilization"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_BUILD), utf8_2_ansi(_("Buildable")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FEATURE), dgettext_2_ansi("wesnoth-hero", "feature"));
	strstr.str("");
	strstr << _("Set") << "...";
	Static_SetText(GetDlgItem(hdlgP, IDC_BT_TECHNOLOGY), utf8_2_ansi(strstr.str().c_str()));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALLY), dgettext_2_ansi("wesnoth-lib", "Ally"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_RESERVE), utf8_2_ansi(_("Reserve")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TROOP), utf8_2_ansi(_("Troop")));

	if (!scenario_selector::multiplayer) {
		ShowWindow(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_ALLOWPLAYER), SW_HIDE);
	} else {
		Button_SetText(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_ALLOWPLAYER), utf8_2_ansi(_("Allow player to set it in lobby")));
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAME_MSGID), side.name_.c_str());
	strstr.str("");
	if (!side.name_.empty()) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), side.name_.c_str()));
	} else {
		strstr << "(" << utf8_2_ansi(gdmgr.heros_[side.leader_].name().c_str()) << ")";
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAME), strstr.str().c_str());

	// controller
	HWND hctl = GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_CONTROLLER);
	if (side.controller_ != controller_tag::EMPTY && scenario.null_side() != -1) {
		Button_Enable(hctl, FALSE);
	}

	hctl = GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_GOLD);
	UpDown_SetRange(hctl, 0, 3000);	// [0, 3000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_GOLD));

	hctl = GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_INCOME);
	UpDown_SetRange(hctl, 0, 1000);	// [0, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_INCOME));

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_NAVIGATION);
	int index = 0;
	for (std::vector<std::string>::const_iterator it = editor_config::navigation.begin(); it != editor_config::navigation.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(unit_types.find(*it)->type_name().c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, index ++);
	}
	hctl = GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_NAVIGATIONXP);
	UpDown_SetRange(hctl, 0, 9999);	// [0, 9999]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAVIGATIONXP));

	LVCOLUMN lvc;
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_BUILD);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	ns::himl_checkbox_side = ImageList_Create(15, 15, FALSE, 2, 0);
	ImageList_SetBkColor(ns::himl_checkbox_side, RGB(255, 255, 255));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNCHECK));
    ImageList_AddIcon(ns::himl_checkbox_side, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CHECK));
    ImageList_AddIcon(ns::himl_checkbox_side, hicon);

	ListView_SetImageList(hctl, ns::himl_checkbox_side, LVSIL_STATE);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// featrue
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Arms"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Level"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// ally
	int column = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_ALLY);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column, &lvc);

	ListView_SetImageList(hctl, ns::himl_checkbox_side, LVSIL_STATE);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// candidate
	candidate_hero::fill_header(hdlgP);

	// reserve
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_RESERVE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	column = 0;
	// cities
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 80;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("arms^Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Soldiers"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 58;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Coordinate")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Traits"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Character"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 100;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Cannot recruit")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Mayor"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Office")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Wander")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "economy area"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// troop
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, utf8_2_ansi(_("troop^Master")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strstr.str("");
	strstr << utf8_2_ansi(_("troop^Second")) << "I";
	strcpy(text, strstr.str().c_str());
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 2;
	strstr.str("");
	strstr << utf8_2_ansi(_("troop^Second")) << "II";
	strcpy(text, strstr.str().c_str());
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 120;
	lvc.iSubItem = 3;
	strcpy(text, utf8_2_ansi(_("arms^Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 58;
	lvc.iSubItem = 4;
	strcpy(text, utf8_2_ansi(_("Coordinate")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 5;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Traits"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 5, &lvc);

	lvc.cx = 50;
	lvc.iSubItem = 6;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Character"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 6, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 7;
	strcpy(text, utf8_2_ansi(_("City")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 7, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	side.update_to_ui_side_edit(hdlgP, false);

	return FALSE;
}

BOOL On_DlgFeature2EditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_feature == ma_edit) {
		strstr << utf8_2_ansi(_("Edit feature"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add feature"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ARMS), dgettext_2_ansi("wesnoth-lib", "Arms"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LEVEL), dgettext_2_ansi("wesnoth-lib", "Level"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FEATURE), dgettext_2_ansi("wesnoth-hero", "feature"));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::arms_feature& feature = side.features_[ns::clicked_feature];

	char text[_MAX_PATH];
	int index;
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_ARMS);
	index = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = editor_config::arms.begin(); it != editor_config::arms.end(); ++ it) {
		ComboBox_AddString(hctl, it->second.c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, index ++);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_LEVEL);
	for (index = 1; index < 7; index ++) {
		sprintf(text, "%i", index);
		ComboBox_AddString(hctl, text);
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, index);
	}

	hctl = GetDlgItem(hdlgP, IDC_CMB_FEATUREEDIT_FEATURE);
	const std::vector<int>& features = hero::valid_features();
	for (std::vector<int>::const_iterator it = features.begin(); it != features.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(hero::feature_str(*it).c_str())); 
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
	}

	feature.update_to_ui_feature_edit(hdlgP);
	return FALSE;
}

void On_DlgFeature2EditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::arms_feature& feature = side.features_[ns::clicked_feature];

	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		feature.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgFeature2EditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgFeature2EditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgFeature2EditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnFeature2AddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	if (!side.new_feature()) {
		return;
	}
	// set current city to new added city.
	ns::clicked_feature = side.features_.size() - 1;

	ns::action_feature = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FEATURE2EDIT), hdlgP, DlgFeature2EditProc, lParam)) {
		side.features_[ns::clicked_feature].update_to_ui_side_edit(hdlgP, ns::clicked_feature);
	} else {
		side.erase_feature(side.features_.size() - 1, hdlgP);
	}

	return;
}

void OnFeature2EditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ns::action_feature = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FEATURE2EDIT), hdlgP, DlgFeature2EditProc, lParam)) {
		side.features_[ns::clicked_feature].update_to_ui_side_edit(hdlgP, ns::clicked_feature);
	}

	return;
}

void OnFeature2DelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	side.erase_feature(ns::clicked_feature, hdlgP);

	return;
}

BOOL On_DlgCityEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_city == ma_edit) {
		strstr << utf8_2_ansi(_("Edit city"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add city"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TYPE), utf8_2_ansi(_("arms^Type")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MAYOR), dgettext_2_ansi("wesnoth-lib", "Mayor"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CHARACTER), dgettext_2_ansi("wesnoth-lib", "Character"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SOLDIERS), dgettext_2_ansi("wesnoth-lib", "Soldiers"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALIAS), utf8_2_ansi(_("Alias")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_COORDINATE), utf8_2_ansi(_("Coordinate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TRAITS), dgettext_2_ansi("wesnoth-lib", "Traits"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HERO), utf8_2_ansi(_("Hero")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_OFFICE), utf8_2_ansi(_("Office")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WANDER), utf8_2_ansi(_("Wander")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EA), dgettext_2_ansi("wesnoth-lib", "economy area"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_EACOORDINATE), utf8_2_ansi(_("Coordinate")));

	Static_SetText(GetDlgItem(hdlgP, IDC_BT_CITYEDIT_ADDEA), utf8_2_ansi(_("Add")));

	char text[_MAX_PATH];
	gdmgr._hpopup_candidate = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOSERVICE, utf8_2_ansi(_("To service")));
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOWANDER, utf8_2_ansi(_("To wander")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_SOLDIERS);
	UpDown_SetRange(hctl, 0, 20);
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_SOLDIERS));

	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_X);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_X));

	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_Y);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_Y));

	LVCOLUMN lvc;
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_TRAIT);
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

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	ns::himl_checkbox = ImageList_Create(15, 15, FALSE, 2, 0);
	ImageList_SetBkColor(ns::himl_checkbox, RGB(255, 255, 255));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNCHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	ListView_SetImageList(hctl, ns::himl_checkbox, LVSIL_STATE);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	candidate_hero::fill_header(hdlgP);

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// wander hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_WANDER);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// not recruit
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_NOTRECRUIT);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 90;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Level"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Arms"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, ns::himl_checkbox, LVSIL_STATE);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// economy area
	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAX);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_EAX));

	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAY);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_EAY));

	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_EA);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 30;
	lvc.iSubItem = 1;
	lvc.pszText = "X";
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 30;
	lvc.iSubItem = 2;
	lvc.pszText = "Y";
	ListView_InsertColumn(hctl, 2, &lvc);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	city.update_to_ui_city_edit(hdlgP, side, false);
	return FALSE;
}

void OnCityEditEt(HWND hdlgP, int id, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}
	if (id != IDC_ET_CITYEDIT_ALIAS_MSGID) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	HWND hctl = GetDlgItem(hdlgP, id);
	Edit_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	city.alias_ = text;
	strstr.str("");
	if (!city.alias_.empty()) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), city.alias_.c_str()));
	} else {
		strstr << city.alias_;
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CITYEDIT_ALIAS), strstr.str().c_str());

	return;
}

void On_DlgCityEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_TOSERVICE:
	case IDM_TOWANDER:
		if (id == IDM_TOSERVICE) {
			city.service_heros_.insert(ns::clicked_hero);
			scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_SERVICE, city.heros_army_[0]);
		} else {
			city.wander_heros_.insert(ns::clicked_hero);
			scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_WANDER, city.heros_army_[0]);
		}
		city.update_to_ui_city_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_CITYEDIT_SERVICE) {
			city.service_heros_.erase(ns::clicked_hero);
			scenario.do_state(ns::clicked_hero, side.side_);
		} else if (ns::type == IDC_LV_CITYEDIT_WANDER) {
			city.wander_heros_.erase(ns::clicked_hero);
			scenario.do_state(ns::clicked_hero, side.side_);
		} else if (ns::type == IDC_LV_CITYEDIT_EA) {
			city.economy_area_.erase(map_location(LOWORD(ns::clicked_hero), HIWORD(ns::clicked_hero)));
		}
		city.update_to_ui_city_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM1:
		if (ns::type == IDC_LV_CITYEDIT_SERVICE) {
			for (std::set<int>::const_iterator it = city.service_heros_.begin(); it != city.service_heros_.end(); ++ it) {
				scenario.do_state(*it, side.side_);
			}
			city.service_heros_.clear();
		} else if (ns::type == IDC_LV_CITYEDIT_WANDER) {
			for (std::set<int>::const_iterator it = city.wander_heros_.begin(); it != city.wander_heros_.end(); ++ it) {
				scenario.do_state(*it, side.side_);
			}
			city.wander_heros_.clear();
		} else if (ns::type == IDC_LV_CITYEDIT_EA) {
			city.economy_area_.clear();
		}
		city.update_to_ui_city_edit(hdlgP, side);
		break;

	case IDC_ET_CITYEDIT_ALIAS_MSGID:
		OnCityEditEt(hdlgP, id, codeNotify);
		break;

	case IDC_BT_CITYEDIT_ADDEA:
		{
			int x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAX));
			int y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_EAY));
			city.economy_area_.insert(map_location(x, y));
			
			city.update_to_ui_city_edit(hdlgP, side);
		}
		break;

	case IDC_CMB_CITYEDIT_MAYOR:
		if (codeNotify == CBN_SELCHANGE) {
			city.mayor_ = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
		}
		break;

	case IDOK:
		changed = TRUE;
		city.from_ui(hdlgP, side);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void cityedit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}
	if (lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE) &&
		lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_WANDER) &&
		lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_EA)) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
	POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);


	if (lpnmitem->iItem < 0) {
		EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_GRAYED);
	}

	TrackPopupMenuEx(gdmgr._hpopup_delete2, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	// 恢复回去
	EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_ENABLED);
	EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_ENABLED);
	
	ns::type = id;
	ns::clicked_hero = lvi.lParam;

    return;
}

void cityedit_notify_handler_dblclk(HWND hdlgP, LPNMHDR lpNMHdr)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_DBLCLK) {
		return;
	}
	if (lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE) &&
		lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_WANDER)) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	if (lpnmitem->iItem < 0) {
		return;
	}

	int number = lvi.lParam;

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE)) {
		city.service_heros_.erase(number);
	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_CITYEDIT_WANDER)) {
		city.wander_heros_.erase(number);
	}
	scenario.do_state(number, side.side_);

	city.update_to_ui_city_edit(hdlgP, side);
    return;
}

BOOL On_DlgCityEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_CITYEDIT_TRAIT) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 2) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
		} else if ((lpNMHdr->idFrom == IDC_LV_CITYEDIT_NOTRECRUIT) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
			}
			strstr << utf8_2_ansi(_("Cannot recruit")) << "(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
			Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITYEDIT_NOTRECRUIT), strstr.str().c_str());
		}
	} else if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_TOSERVICE, _("To service")));
		candidate_menu.insert(std::make_pair(IDM_TOWANDER, _("To wander")));

		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr)) {
			ns::type = DlgItem;
			ns::clicked_hero = candidate_hero::lParam;

		} else {
			cityedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}

	} else if (lpNMHdr->code == NM_DBLCLK) {
		// cityedit_notify_handler_dblclk(hdlgP, lpNMHdr);

	}
	return FALSE;
}

void On_DlgCityEditDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_checkbox);
	DestroyMenu(gdmgr._hpopup_candidate);
}

BOOL CALLBACK DlgCityEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgCityEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgCityEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgCityEditNotify);
	HANDLE_MSG(hDlg, WM_DESTROY, On_DlgCityEditDestroy);
	}
	
	return FALSE;
}

void OnCityAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	if (!side.new_city()) {
		return;
	}
	// set current city to new added city.
	ns::clicked_city = side.cities_.size() - 1;

	ns::action_city = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CITYEDIT), hdlgP, DlgCityEditProc, lParam)) {
		side.update_to_ui_side_edit(hdlgP, false);
	} else {
		side.erase_city(side.cities_.size() - 1, hdlgP);
	}

	return;
}

void OnCityEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ns::action_city = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_CITYEDIT), hdlgP, DlgCityEditProc, lParam)) {
		side.update_to_ui_side_edit(hdlgP, false);
	}

	return;
}

void OnCityDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	side.erase_city(ns::clicked_city, hdlgP);

	return;
}

BOOL On_DlgTroopEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_troop == ma_edit) {
		strstr << utf8_2_ansi(_("Edit troop"));
		ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add troop"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TYPE), utf8_2_ansi(_("arms^Type")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CHARACTER), dgettext_2_ansi("wesnoth-lib", "Character"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_COORDINATE), utf8_2_ansi(_("Coordinate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TRAITS), dgettext_2_ansi("wesnoth-lib", "Traits"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_HERO), utf8_2_ansi(_("Hero")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CANDIDATE), utf8_2_ansi(_("Candidate")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TROOPHERO), utf8_2_ansi(_("Hero")));

	char text[_MAX_PATH];
	gdmgr._hpopup_candidate = CreatePopupMenu();
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOMASTER, "到主将");
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOSECOND, "到副将I");
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOTHIRD, "到副将II");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_X);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_TROOPEDIT_X));

	hctl = GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_Y);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_TROOPEDIT_Y));

	LVCOLUMN lvc;
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_TRAIT);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
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

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	ns::himl_checkbox = ImageList_Create(15, 15, FALSE, 2, 0);
	ImageList_SetBkColor(ns::himl_checkbox, RGB(255, 255, 255));

    HICON hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_UNCHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	hicon = LoadIcon(gdmgr._hinst, MAKEINTRESOURCE(IDI_CHECK));
    ImageList_AddIcon(ns::himl_checkbox, hicon);

	ListView_SetImageList(hctl, ns::himl_checkbox, LVSIL_STATE);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	candidate_hero::fill_header(hdlgP);

	// service hero
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_HERO);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 50;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "name"));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "catalog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	troop.update_to_ui_troop_edit(hdlgP, side, false);
	return FALSE;
}

void On_DlgTroopEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_TOMASTER:
	case IDM_TOSECOND:
	case IDM_TOTHIRD:
		if (id == IDM_TOMASTER) {
			scenario.do_state(troop.heros_army_[0], side.side_);
			troop.heros_army_[0] = ns::clicked_hero;
			scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_ARMY, troop.city_);
		} else if (id == IDM_TOSECOND) {
			if (troop.heros_army_.size() == 1) {
				troop.heros_army_.push_back(ns::clicked_hero);
			} else {
				scenario.do_state(troop.heros_army_[1], side.side_);
				troop.heros_army_[1] = ns::clicked_hero;
			}
			scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_ARMY, troop.city_);
		} else if (id == IDM_TOTHIRD) {
			if (troop.heros_army_.size() == 3) {
				scenario.do_state(troop.heros_army_[2], side.side_);
				troop.heros_army_[2] = ns::clicked_hero;
			} else {
				troop.heros_army_.push_back(ns::clicked_hero);
			}
			scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_ARMY, troop.city_);
		}
		troop.update_to_ui_troop_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_TROOPEDIT_HERO) {
			std::vector<int>::iterator it = std::find(troop.heros_army_.begin(), troop.heros_army_.end(), ns::clicked_hero);
			troop.heros_army_.erase(it);
			scenario.do_state(ns::clicked_hero, side.side_);
		}
		troop.update_to_ui_troop_edit(hdlgP, side);
		break;

	case IDOK:
		changed = TRUE;
		troop.from_ui(hdlgP, side);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

void troopedit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}
	if (lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_HERO)) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
	POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	if (lpnmitem->iItem < 0) {
		EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
	} else if (troop.heros_army_.size() == 1) {
		EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_GRAYED);
	}
	EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_GRAYED);

	TrackPopupMenuEx(gdmgr._hpopup_delete2, 0, 
		point.x, 
		point.y, 
		hdlgP, 
		NULL);

	// 恢复回去
	EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM0, MF_BYCOMMAND | MF_ENABLED);
	EnableMenuItem(gdmgr._hpopup_delete2, IDM_DELETE_ITEM1, MF_BYCOMMAND | MF_ENABLED);

	ns::type = id;
	ns::clicked_hero = lvi.lParam;

    return;
}

bool is_hero_enable(HWND, int id, LPARAM)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	if (id == IDM_TOTHIRD && troop.heros_army_.size() < 2) {
		return false;
	}
	return true;
}

BOOL On_DlgTroopEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_TROOPEDIT_TRAIT) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				if (editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) < 2) {
					ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
				}
			}
		}
	} else if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_TOMASTER, _("To master")));
		candidate_menu.insert(std::make_pair(IDM_TOSECOND, _("To second")));
		candidate_menu.insert(std::make_pair(IDM_TOTHIRD, _("To third")));

		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr, is_hero_enable)) {
			ns::type = DlgItem;
			ns::clicked_hero = candidate_hero::lParam;

		} else {
			troopedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}

	}
	return FALSE;
}

void On_DlgTroopEditDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_checkbox);
	DestroyMenu(gdmgr._hpopup_candidate);
}

BOOL CALLBACK DlgTroopEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgTroopEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgTroopEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgTroopEditNotify);
	HANDLE_MSG(hDlg, WM_DESTROY, On_DlgTroopEditDestroy);
	}
	
	return FALSE;
}

void OnTroopAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	if (!side.new_troop()) {
		return;
	}
	// set current city to new added city.
	ns::clicked_troop = side.troops_.size() - 1;

	ns::action_troop = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TROOPEDIT), hdlgP, DlgTroopEditProc, lParam)) {
		side.update_to_ui_side_edit(hdlgP, false);
	} else {
		side.erase_troop(side.troops_.size() - 1, hdlgP);
	}

	return;
}

void OnTroopEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ns::action_troop = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TROOPEDIT), hdlgP, DlgTroopEditProc, lParam)) {
		side.update_to_ui_side_edit(hdlgP, false);
	}

	return;
}

void OnTroopDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	side.erase_troop(ns::clicked_troop, hdlgP);

	return;
}

void sideedit_notify_handler_rclick(HWND hdlgP, int id, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE) ||
		lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY) || 
		lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP)) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		
		// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
		POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
		MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND);
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}		

		TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);
	}

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE)) {
		ns::clicked_feature = lpnmitem->iItem;

	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY)) {
		ns::clicked_city = lpnmitem->iItem;

	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP)) {
		ns::clicked_troop = lpnmitem->iItem;
	}
	ns::type = id;
    return;
}

void sideedit_notify_handler_dblclk(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_DBLCLK) {
		return;
	}
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE) || 
		lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_RESERVE) || 
		lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY) || 
		lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP)) {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		
		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);
	}
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE)) {
		if (lpnmitem->iItem >= 0) {
			ns::clicked_feature = lpnmitem->iItem;
			OnFeature2EditBt(hdlgP);
		}
	} else if (lpNMHdr->idFrom == IDC_LV_SIDEEDIT_RESERVE) {
		if (lpnmitem->iItem >= 0) {
			std::set<int>::iterator it = side.reserve_heros_.find(lvi.lParam);
			side.reserve_heros_.erase(it);
			scenario.do_state(lvi.lParam, side.side_);
			side.update_to_ui_side_edit(hdlgP, false);
		}

	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY)) {
		if (lpnmitem->iItem >= 0) {
			ns::clicked_city = lpnmitem->iItem;
			OnCityEditBt(hdlgP);
		}

	} else if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TROOP)) {
		if (lpnmitem->iItem >= 0) {
			ns::clicked_troop = lpnmitem->iItem;
			OnTroopEditBt(hdlgP);
		}
	}
    return;
}

void OnSideEditEt(HWND hdlgP, int id, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}
	if (id != IDC_ET_SIDEEDIT_NAME_MSGID) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	HWND hctl = GetDlgItem(hdlgP, id);
	Edit_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	side.name_ = text;
	strstr.str("");
	if (!side.name_.empty()) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), side.name_.c_str()));
	} else {
		strstr << "(" << utf8_2_ansi(gdmgr.heros_[side.leader_].name().c_str()) << ")";
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAME), strstr.str().c_str());

	return;
}

void OnLeaderCmb(HWND hdlgP, int id, UINT codeNotify)
{
	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	if (!side.name_.empty()) {
		return;
	}
	HWND hctl = GetDlgItem(hdlgP, id);
	side.leader_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	std::stringstream strstr;
	strstr << "(" << utf8_2_ansi(gdmgr.heros_[side.leader_].name().c_str()) << ")";
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_NAME), strstr.str().c_str());
}

void OnSideEditTechnologyBt(HWND hdlgP, int id, int explorer)
{
	RECT rcBtn;
	LPARAM lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, id), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ttechnology_lock lock(explorer);
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TECHNOLOGY2), hdlgP, DlgTechnologyProc, lParam)) {
		explorer_technology::update_to_ui_special(hdlgP, explorer);
	}

}

void On_DlgSideEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (candidate_hero::on_command(hdlgP, id, codeNotify)) {
		return;
	}
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_ADD:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeature2AddBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityAddBt(hdlgP);
		} else {
			OnTroopAddBt(hdlgP);
		}
		break;
	case IDM_DELETE:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeature2DelBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityDelBt(hdlgP);
		} else {
			OnTroopDelBt(hdlgP);
		}
		break;
	case IDM_EDIT:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeature2EditBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityEditBt(hdlgP);
		} else {
			OnTroopEditBt(hdlgP);
		}
		break;

	case IDM_TORESERVE:
		side.reserve_heros_.insert(ns::clicked_hero);
		scenario.do_state(ns::clicked_hero, side.side_, tscenario::STATE_RESERVE);
		side.update_to_ui_side_edit(hdlgP, false);
		break;

	case IDC_ET_SIDEEDIT_NAME_MSGID:
		OnSideEditEt(hdlgP, id, codeNotify);
		break;
	case IDC_CMB_SIDEEDIT_LEADER:
		OnLeaderCmb(hdlgP, id, codeNotify);
		break;
	case IDC_BT_TECHNOLOGY:
		OnSideEditTechnologyBt(hdlgP, id, explorer_technology::SCENARIO);
		break;

	case IDOK:
		changed = TRUE;
		side.from_ui(hdlgP);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL On_DlgSideEditNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	std::stringstream strstr;
	if (lpNMHdr->code == NM_CLICK) {
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		if ((lpNMHdr->idFrom == IDC_LV_SIDEEDIT_BUILD 
			|| lpNMHdr->idFrom == IDC_LV_SIDEEDIT_ALLY) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
			}
			strstr.str("");
			if (lpNMHdr->idFrom == IDC_LV_SIDEEDIT_BUILD) {
				strstr << utf8_2_ansi(_("Buildable")) << "(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_BUILD), strstr.str().c_str());
			} else if (lpNMHdr->idFrom == IDC_LV_SIDEEDIT_ALLY) {
				strstr << dgettext_2_ansi("wesnoth-lib", "Ally") << "(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
				Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ALLY), strstr.str().c_str());
			} 
		}

	} else if (lpNMHdr->code == NM_RCLICK) {
		std::map<int, std::string> candidate_menu;
		candidate_menu.insert(std::make_pair(IDM_TORESERVE, _("To reserve")));
		if (candidate_hero::notify_handler_rclick(candidate_menu, hdlgP, DlgItem, lpNMHdr)) {
			ns::type = DlgItem;
			ns::clicked_hero = candidate_hero::lParam;

		} else {
			sideedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);
		}

	} else if (lpNMHdr->code == NM_DBLCLK) {
		sideedit_notify_handler_dblclk(hdlgP, lpNMHdr);

	}
	return FALSE;
}

void On_DlgSideEditDestroy(HWND hdlgP)
{
	ImageList_Destroy(ns::himl_checkbox_side);
}

BOOL CALLBACK DlgSideEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgSideEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgSideEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgSideEditNotify);
	HANDLE_MSG(hDlg, WM_DESTROY, On_DlgSideEditDestroy);
	}
	
	return FALSE;
}

BOOL On_DlgCampaignScenarioInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndParent = GetParent(hdlgP);
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
    SetWindowPos(hdlgP, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 

	std::stringstream strstr;
	char text[_MAX_PATH];
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FILE), utf8_2_ansi(_("Corresponding cfg")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ID), utf8_2_ansi(_("ID")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Name")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MAP), utf8_2_ansi(_("Map")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NEXTSCENARIO), utf8_2_ansi(_("Next scenario")));
	strstr.str("");
	strstr << utf8_2_ansi(_("Turns")) << "(-1: ";
	strstr << utf8_2_ansi(_("Unrestricted")) << ")";
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TURNS), strstr.str().c_str());
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DUEL), utf8_2_ansi(_("Duel")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WIN), utf8_2_ansi(_("Win condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOSE), utf8_2_ansi(_("Lose condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TREASURE), utf8_2_ansi(_("Hidden treasure")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ROAD), utf8_2_ansi(_("Road")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_ENEMYNOCITY), utf8_2_ansi(_("Victory when enemy no city")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_FALLENTOUNSTAGE), utf8_2_ansi(_("Unstage when side fallen")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_CAMPSCENARIO_TENT), utf8_2_ansi(_("Inherit hero and troop from last scenario")));
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_BROWSEMAP), utf8_2_ansi(_("Browse...")));
	strstr.str("");
	strstr << _("Prelude") << "...";
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_PRELUDE), utf8_2_ansi(strstr.str().c_str()));
	strstr.str("");
	strstr << dsgettext("wesnoth-lib", "Objectives") << "...";
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_OBJECTIVES), utf8_2_ansi(strstr.str().c_str()));

	if (scenario_selector::multiplayer) {
		ShowWindow(GetDlgItem(hdlgP, IDC_STATIC_NEXTSCENARIO), SW_HIDE);
		ShowWindow(GetDlgItem(hdlgP, IDC_CMB_CAMPSCENARIO_NEXTSCENARIO), SW_HIDE);
	}

	ns::current_scenario = TabCtrl_GetCurSel(pHdr->hwndTab) - 1;
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	campaign_enable_delete_btn(!campaign_get_save_btn() && (ns::current_scenario || ns::_scenario.size() >= 2));
	campaign_enable_herostate_btn(true);
	
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_FILE);
	Edit_SetText(hctl, scenario.file(true).c_str());

	//id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_ID), scenario.id_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME_MSGID), scenario.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), scenario.id_.c_str())));

	strstr.str("");
	if (ns::_main.must_exist_map()) {
		strstr << "(" << scenario.map_file() << ")";
		if (scenario.map_data_size(scenario.map_data_).valid()) {
			strstr << utf8_2_ansi(_("Exist"));
		} else {
			strstr << utf8_2_ansi(_("Not exist"));
		}
	} else {
		strstr << "(" << utf8_2_ansi(_("Random map")) << ")";
		Button_Enable(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_BROWSEMAP), FALSE);
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MAP), strstr.str().c_str());

	// turns
	hctl = GetDlgItem(hdlgP, IDC_UD_CAMPSCENARIO_TURNS);
	UpDown_SetRange(hctl, -1, 100);	// [-1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_TURNS));
	UpDown_SetPos(hctl, scenario.turns_);
	// duel
	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPSCENARIO_DUEL);
	std::map<int, std::string> duel_map;
	duel_map.insert(std::make_pair((int)NO_DUEL, _("Hasn't")));
	duel_map.insert(std::make_pair((int)RANDOM_DUEL, _("Random")));
	duel_map.insert(std::make_pair((int)ALWAYS_DUEL, _("Always")));
	for (std::map<int, std::string>::const_iterator it = duel_map.begin(); it != duel_map.end(); ++ it) {
		ComboBox_AddString(hctl, utf8_2_ansi(it->second.c_str()));
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (scenario.duel_ == it->first) {
			ComboBox_SetCurSel(hctl, ComboBox_GetCount(hctl) - 1);
		}
	}

	LVCOLUMN lvc;
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 70;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 40;
	lvc.iSubItem = 2;
	strcpy(text, utf8_2_ansi(_("Quantity")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	//
	// road
	//
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_ROAD);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 70;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "City"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 70;
	lvc.iSubItem = 1;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "City"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	//
	// side
	//
	int column = 0;
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = column;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Leader")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Controller"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Fog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Card"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Base gold")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Base income")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Navigation civilization"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = column;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 100;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Artifical")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("City")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = column;
	strcpy(text, utf8_2_ansi(_("Troop")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, column ++, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// event
	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_EVENT);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 100;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("Occasion")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 2;
	strcpy(text, utf8_2_ansi(_("Only one time")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	scenario.update_to_ui(hdlgP);

	return FALSE;
}

void OnCampaignScenarioEt(HWND hdlgP, int id, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	std::string that;
	int bit;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HWND hctl = GetDlgItem(hdlgP, id);

	if (id == IDC_ET_CAMPSCENARIO_TURNS) {
		bit = tscenario::BIT_TURNS;
		strstr << scenario.scenario_from_cfg_.turns_;
		that = strstr.str();
	} else {
		return;
	}

	Edit_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	scenario.set_dirty(bit, strcmp(text, that.c_str()));

	return;
}

void OnCampaignScenarioEt3(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE || id != IDC_ET_CAMPSCENARIO_ID) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	HWND hwndParent = GetParent(hdlgP);
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	std::string str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);
	if (!isvalid_id(str)) {
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_IDSTATUS), utf8_2_ansi(_("Invalid string")));
		return;
	}
	// cannot be exist id.
	for (size_t i = 0; i < ns::_scenario.size(); i ++) {
		if (i != ns::current_scenario && str == ns::_scenario[i].id_) {
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_IDSTATUS), utf8_2_ansi(_("Other scenario has holded ID")));
			return;
		}
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_IDSTATUS), "");
	if (scenario.id_ == str) {
		return;
	}
	
	// update relative scenario id.
	// cannot be exist id.
	if (ns::_main.first_scenario_ == scenario.id_) {
		ns::_main.first_scenario_ = str;
		ns::_main.set_dirty(tmain::BIT_FIRSTSCENARIO, ns::_main.main_from_cfg_.first_scenario_ != ns::_main.first_scenario_);
	}
	for (size_t i = 0; i < ns::_scenario.size(); i ++) {
		if (i != ns::current_scenario && ns::_scenario[i].next_scenario_ == scenario.id_) {
			ns::_scenario[i].next_scenario_ = str;
			ns::_scenario[i].set_dirty(tscenario::BIT_NEXTSCENARIO, ns::_scenario[i].scenario_from_cfg_.next_scenario_ != ns::_scenario[i].next_scenario_);
		}
	}

	scenario.id_ = str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_FILE), scenario.file().c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME_MSGID), scenario.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), scenario.id_.c_str())));

	// read map data from new file, check whether valid.
	std::string map_data = scenario.map_data_from_file(scenario.map_file(true));
	strstr.str("");
	strstr << "(" << scenario.map_file() << ")";
	if (scenario.map_data_size(map_data).valid()) {
		strstr << utf8_2_ansi(_("Exist"));
	} else {
		strstr << utf8_2_ansi(_("Not exist"));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MAP), strstr.str().c_str());

	// set relative dirty flag.
	scenario.set_dirty(tscenario::BIT_ID, scenario.scenario_from_cfg_.id_ != scenario.id_);

	scenario.map_data_ = map_data;
	scenario.set_dirty(tscenario::BIT_MAP, scenario.scenario_from_cfg_.map_data_ != map_data);

	// TCITEM
	TCITEM tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE; 
	tie.iImage = -1;
	strstr.str("");
	strstr << std::setw(2) << std::setfill('0') << scenario.index_ + 1;
	strstr << "(" << scenario.id_ << ")";
	strcpy(text, strstr.str().c_str());
	tie.pszText = text; 
	TabCtrl_SetItem(pHdr->hwndTab, scenario.index_ + 1, &tie);

	return;
}

void OnCampaignScenarioCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	std::string that;
	int bit;

	if (codeNotify != CBN_SELCHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	bool notequal = true;

	if (id == IDC_CMB_CAMPSCENARIO_NEXTSCENARIO) {
		bit = tscenario::BIT_NEXTSCENARIO;
		that = scenario.scenario_from_cfg_.next_scenario_;
		notequal = strcmp(text, that.c_str());
	} else if (id == IDC_CMB_CAMPSCENARIO_DUEL) {
		bit = tscenario::BIT_DUEL;
		that = scenario.scenario_from_cfg_.next_scenario_;
		notequal = scenario.scenario_from_cfg_.duel_ != ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	} else {
		return;
	}

	ComboBox_GetLBText(hwndCtrl, ComboBox_GetCurSel(hwndCtrl), text);
	scenario.set_dirty(bit, notequal);

	return;
}

void OnCampaignScenarioChk(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	if (codeNotify != BN_CLICKED) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	int bit;

	bool notequal = true;
	bool checked = Button_GetCheck(hwndCtrl);
	if (id == IDC_CHK_CAMPSCENARIO_ENEMYNOCITY) {
		notequal = scenario.scenario_from_cfg_.enemy_no_city_ != checked;
		bit = tscenario::BIT_ENEMYNOCITY;
	} else if (id == IDC_CHK_CAMPSCENARIO_FALLENTOUNSTAGE) {
		notequal = scenario.scenario_from_cfg_.fallen_to_unstage_ != checked;
		bit = tscenario::BIT_FALLENTOUNSTAGE;
	} else if (id == IDC_CHK_CAMPSCENARIO_TENT) {
		notequal = scenario.scenario_from_cfg_.tent_ != checked;
		bit = tscenario::BIT_TENT;
	} else {
		return;
	}
	
	scenario.set_dirty(bit, notequal);
	return;
}

void OnCampaignScenarioBt(HWND hdlgP, int id, UINT codeNotify)
{
	HWND hwndParent = GetParent(hdlgP);
    DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	char* ptr = GetBrowseFileName(dirname(scenario.map_file(true).c_str()), "*.map\0*.map\0\0", TRUE);
	if (!ptr) return;

	std::stringstream strstr;
	utils::string_map symbols;
	std::string map_data = scenario.map_data_from_file(ptr);
	map_location size = scenario.map_data_size(map_data);
	if (!size.valid()) {
		symbols["file"] = ptr;
		strstr.str("");
		strstr << utf8_2_ansi(vgettext2("$file isn't valid map file!", symbols).c_str());
		posix_print_mb(strstr.str().c_str());
		return;
	}
	if (scenario.map_data_ == map_data) {
		strstr.str("");
		strstr << utf8_2_ansi(_("Selected is using file, do nothing."));
		posix_print_mb(strstr.str().c_str());
		return;
	}

	HWND hctl;
	BOOL fok;
	if (id == IDC_BT_CAMPSCENARIO_BROWSEMAP) {
		fok = CopyFile(ptr, scenario.map_file(true).c_str(), FALSE);
	} else {
		return;
	}
	symbols["src"] = ptr;
	symbols["dst"] = scenario.map_file(true);
	symbols["result"] = fok? "success": "fail";
	strstr.str("");
	strstr << utf8_2_ansi(vgettext2("Copy \"$src\" to \"$dst\" $result!", symbols).c_str());
	posix_print_mb(strstr.str().c_str());

	if (id == IDC_BT_CAMPSCENARIO_BROWSEMAP) {
		fok = is_file(scenario.map_file(true).c_str());
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MAP);
	}

	strstr.str("");
	strstr << "(" << scenario.map_file() << ")" << utf8_2_ansi(_("Exist"));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MAP), strstr.str().c_str());
	
	scenario.map_data_ = map_data;
	scenario.set_dirty(tscenario::BIT_MAP, scenario.scenario_from_cfg_.map_data_ != map_data);
}

extern config edit_ctrl_2_cfg(const std::string& str);
extern std::string cfg_2_edit_ctrl(const config& cfg);

BOOL On_DlgPreludeInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	strstr << utf8_2_ansi(_("Edit prelude"));
	SetWindowText(hdlgP, strstr.str().c_str());
	ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SUMMARY), utf8_2_ansi(_("Summary")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WIN), utf8_2_ansi(_("Win condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOSE), utf8_2_ansi(_("Lose condition")));

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_PRELUDE_CFG), cfg_2_edit_ctrl(scenario.prelude_).c_str());

	return FALSE;
}

void On_DlgPreludeCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		{
			HWND hctl = GetDlgItem(hdlgP, IDC_ET_PRELUDE_CFG);
			int len = Edit_GetTextLength(hctl);
			char* text = (char*)malloc(len + 2);
			Edit_GetText(GetDlgItem(hdlgP, IDC_ET_PRELUDE_CFG), text, len + 1);
			scenario.prelude_ = edit_ctrl_2_cfg(text);
			free(text);
		}
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgPreludeProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgPreludeInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgPreludeCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnPreludeBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_PRELUDE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_PRELUDE), hdlgP, DlgPreludeProc, lParam)) {
		scenario.set_dirty(tscenario::BIT_PRELUDE, scenario.prelude_ != scenario.scenario_from_cfg_.prelude_);
	}

	return;
}

extern BOOL CALLBACK DlgObjectivesProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

void OnObjectivesBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_OBJECTIVES), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::clicked_command = &scenario.objectives_;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_OBJECTIVES), hdlgP, DlgObjectivesProc, lParam)) {
		scenario.set_dirty(tscenario::BIT_OBJECTIVES, scenario.objectives_ != scenario.scenario_from_cfg_.objectives_);
	}

	return;
}

void OnObjectviesBt(HWND hdlgP, int id, UINT codeNotify)
{

}

void OnSideAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	if (!scenario.new_side()) {
		return;
	}
	// set current side to new added side.
	ns::clicked_side = scenario.side_.size() - 1;

	ns::action_side = ma_new;

	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SIDEEDIT), hdlgP, DlgSideEditProc, lParam)) {
		scenario.validate_roads();
		scenario.update_to_ui(hdlgP);
		scenario.set_dirty(tscenario::BIT_SIDE, !scenario.side_equal()); 
	} else {
		scenario.erase_side(scenario.side_.size() - 1, hdlgP);
	}
	
	return;
}

void OnSideEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ns::action_side = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SIDEEDIT), hdlgP, DlgSideEditProc, lParam)) {
		scenario.validate_roads();
		scenario.update_to_ui(hdlgP);
		scenario.set_dirty(tscenario::BIT_SIDE, !scenario.side_equal()); 
	}
	return;
}

void OnSideDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	scenario.erase_side(ns::clicked_side, hdlgP);

	scenario.set_dirty(tscenario::BIT_SIDE, !scenario.side_equal()); 
	return;
}

BOOL On_DlgScenarioTreasureInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_treasure == ma_edit) {
		strstr << utf8_2_ansi(_("Edit hidden treasure"));
		// ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add hidden treasure"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_TREASURE_TREASURE);
	const std::vector<ttreasure>& treasures = unit_types.treasures();
	int selected = 0, index = 0;
	for (std::vector<ttreasure>::const_iterator it = treasures.begin(); it != treasures.end(); ++ it, index ++) {
		const ttreasure& t = *it;
		strstr.str("");
		strstr << utf8_2_ansi(t.name().c_str());
		strstr << "(";
		strstr << utf8_2_ansi(hero::feature_str(t.feature()).c_str());
		strstr << ")";
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, t.index());
		if (ns::action_treasure == ma_edit) {
			if (t.index() == ns::clicked_treasure) {
				selected = index;
			}
		}
	}
	ComboBox_SetCurSel(hctl, selected);
	if (ns::action_treasure == ma_edit) {
		ComboBox_Enable(hctl, FALSE);
	}

	hctl = GetDlgItem(hdlgP, IDC_UD_TREASURE_COUNT);
	UpDown_SetRange(hctl, 1, 100);	// [1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_TREASURE_COUNT));

	if (ns::action_treasure == ma_edit) {
		const std::map<int, int>::const_iterator find = scenario.treasures_.find(ns::clicked_treasure);
		UpDown_SetPos(hctl, find->second);
	} else {
		UpDown_SetPos(hctl, 1);
	}

	return FALSE;
}

void On_DlgScenarioTreasureCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	BOOL changed = FALSE;

	switch (id) {
	case IDOK:
		changed = TRUE;
		scenario.from_ui_treasure(hdlgP, ns::action_treasure == ma_edit);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgScenarioTreasureProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgScenarioTreasureInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgScenarioTreasureCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}	
	return FALSE;
}

void OnScenarioTreasureAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_treasure = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SCENARIOTREASURE), hdlgP, DlgScenarioTreasureProc, lParam)) {
		scenario.update_to_ui_treasures(hdlgP);
		scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	}

	return;
}

void OnScenarioTreasureEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_treasure = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_SCENARIOTREASURE), hdlgP, DlgScenarioTreasureProc, lParam)) {
		scenario.update_to_ui_treasures(hdlgP);
		scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	}

	return;
}

void OnScenarioTreasureDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::map<int, int>& treasures = scenario.treasures_;
	treasures.erase(ns::clicked_treasure);

	scenario.update_to_ui_treasures(hdlgP);
	scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	return;
}

BOOL On_DlgRoadInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	editor_config::move_subcfg_right_position(hdlgP, lParam);

	std::stringstream strstr;
	if (ns::action_road == ma_edit) {
		strstr << utf8_2_ansi(_("Edit road"));
		// ShowWindow(GetDlgItem(hdlgP, IDCANCEL), SW_HIDE);
	} else {
		strstr << utf8_2_ansi(_("Add road"));
	}
	SetWindowText(hdlgP, strstr.str().c_str());

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY1), dgettext_2_ansi("wesnoth-lib", "City"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY2), dgettext_2_ansi("wesnoth-lib", "City"));

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::vector<tside>& sides = scenario.side_;
	std::multimap<int, int>& roads = scenario.roads_;
	std::set<int> cities = scenario.get_cities();

	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ROAD_CITY1);
	for (std::set<int>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
		std::set<int>::iterator find = cities.find(*it);
		int great = std::distance(find, cities.end()) - 1;
		if (roads.count(*it) == great) {
			continue;
		}
		strstr.str("");
		strstr << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
	}
	if (ComboBox_GetCount(hctl)) {
		ComboBox_SetCurSel(hctl, 0);
		int city1 = ComboBox_GetItemData(hctl, 0);
		
		hctl = GetDlgItem(hdlgP, IDC_CMB_ROAD_CITY2);
		std::set<int> roaded_cities;
		typedef std::multimap<int, int>::const_iterator Itor;
		std::pair<Itor, Itor> its = roads.equal_range(city1);
		while (its.first != its.second) {
			roaded_cities.insert(its.first->second);
			++ its.first;
		}
		for (std::set<int>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
			if (*it > city1 && roaded_cities.find(*it) == roaded_cities.end()) {
				strstr.str("");
				strstr << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
				ComboBox_AddString(hctl, strstr.str().c_str());
				ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
			}
		}
		ComboBox_SetCurSel(hctl, 0);
	}

	return FALSE;
}

void OnRoadCmb(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	std::stringstream strstr;

	if (codeNotify != CBN_SELCHANGE && id != IDC_CMB_ROAD_CITY1) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	std::vector<tside>& sides = scenario.side_;
	std::multimap<int, int>& roads = scenario.roads_;
	std::set<int> cities = scenario.get_cities();

	int city1 = ComboBox_GetItemData(hwndCtrl, ComboBox_GetCurSel(hwndCtrl));
	
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_ROAD_CITY2);
	ComboBox_ResetContent(hctl);
	std::set<int> roaded_cities;
	typedef std::multimap<int, int>::const_iterator Itor;
	std::pair<Itor, Itor> its = roads.equal_range(city1);
	while (its.first != its.second) {
		roaded_cities.insert(its.first->second);
		++ its.first;
	}
	for (std::set<int>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
		if (*it > city1 && roaded_cities.find(*it) == roaded_cities.end()) {
			strstr.str("");
			strstr << utf8_2_ansi(gdmgr.heros_[*it].name().c_str());
			ComboBox_AddString(hctl, strstr.str().c_str());
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, *it);
		}
	}
	ComboBox_SetCurSel(hctl, 0);


	return;
}

void On_DlgRoadCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	BOOL changed = FALSE;

	switch (id) {
	case IDC_CMB_ROAD_CITY1:
		OnRoadCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
		scenario.from_ui_road(hdlgP, ns::action_road == ma_edit);
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgRoadProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgRoadInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgRoadCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}	
	return FALSE;
}

void OnRoadAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_ROAD), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_road = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ROAD), hdlgP, DlgRoadProc, lParam)) {
		scenario.update_to_ui_roads(hdlgP);
		scenario.set_dirty(tscenario::BIT_ROADS, scenario.scenario_from_cfg_.roads_ != scenario.roads_);
	}

	return;
}

void OnRoadEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_ROAD), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_road = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_ROAD), hdlgP, DlgRoadProc, lParam)) {
		scenario.update_to_ui_roads(hdlgP);
		scenario.set_dirty(tscenario::BIT_ROADS, scenario.scenario_from_cfg_.roads_ != scenario.roads_);
	}

	return;
}

void OnRoadDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::multimap<int, int>::iterator it = scenario.roads_.begin();
	std::advance(it, ns::clicked_road);
	scenario.roads_.erase(it);

	scenario.update_to_ui_roads(hdlgP);
	scenario.set_dirty(tscenario::BIT_ROADS, scenario.scenario_from_cfg_.roads_ != scenario.roads_);
	return;
}

void On_DlgCampaignScenarioCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDC_ET_CAMPSCENARIO_TURNS:
		OnCampaignScenarioEt(hdlgP, id, codeNotify);
		break;

	case IDC_ET_CAMPSCENARIO_ID:
		OnCampaignScenarioEt3(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_CMB_CAMPSCENARIO_NEXTSCENARIO:
	case IDC_CMB_CAMPSCENARIO_DUEL:
		OnCampaignScenarioCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_CHK_CAMPSCENARIO_ENEMYNOCITY:
	case IDC_CHK_CAMPSCENARIO_FALLENTOUNSTAGE:
	case IDC_CHK_CAMPSCENARIO_TENT:
		OnCampaignScenarioChk(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_ADD:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideAddBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnScenarioTreasureAddBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_ROAD) {
			OnRoadAddBt(hdlgP);
		}
		break;
	case IDM_DELETE:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideDelBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_EVENT) {
			OnEventDelBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnScenarioTreasureDelBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_ROAD) {
			OnRoadDelBt(hdlgP);
		}
		break;
	case IDM_EDIT:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideEditBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_EVENT) {
			OnEventEditBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnScenarioTreasureEditBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_ROAD) {
			OnRoadEditBt(hdlgP);
		}
		break;

	case IDC_BT_CAMPSCENARIO_BROWSEMAP:
		OnCampaignScenarioBt(hdlgP, id, codeNotify);
		break;
	case IDC_BT_CAMPSCENARIO_PRELUDE:
		OnPreludeBt(hdlgP);
		break;
	case IDC_BT_CAMPSCENARIO_OBJECTIVES:
		OnObjectivesBt(hdlgP);
		break;

	default:
		if (id >= IDM_NEW_EVENT0 && id < IDM_NEW_EVENT0 + (int)tevent::name_map.size()) {
			OnEventAddBt(hdlgP, tevent::name_map[id - IDM_NEW_EVENT0].id_);
			break;
		}
	}
}

void campaignscenario_notify_handler_rclick(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
	// 如果单击到的是复选框位置,把复选框状态置反
	// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
	// 认为如果x是小于16的就认为是击中复选框
	
	// ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, !ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem));
	POINT		point = {lpnmitem->ptAction.x, lpnmitem->ptAction.y};
	MapWindowPoints(lpNMHdr->hwndFrom, NULL, &point, 1);

	lvi.iItem = lpnmitem->iItem;
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = gdmgr._menu_text;
	lvi.cchTextMax = _MAX_PATH;
	ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

	std::stringstream strstr;
	int icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_SIDE) {
		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND);
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		} else if (scenario.side_.size() <= 2) {
			EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(gdmgr._hpopup_editor, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_editor, IDM_ADD, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_EDIT, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_editor, IDM_DELETE, MF_BYCOMMAND | MF_ENABLED);

		ns::clicked_side = lpnmitem->iItem;
		ns::type = IDC_LV_CAMPSCENARIO_SIDE;

	} else if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_EVENT) {

		HMENU hpopup_new = CreatePopupMenu();
		int index = 0;
		for (std::vector<tevent::tevent_name>::const_iterator it = tevent::name_map.begin(); it != tevent::name_map.end(); ++ it, index ++) {
			strstr.str("");
			strstr << it->id_ << "(" << it->name_ << ")";
			AppendMenu(hpopup_new, MF_STRING, IDM_NEW_EVENT0 + index, strstr.str().c_str());
		}
		HMENU hpopup_event = CreatePopupMenu();
		AppendMenu(hpopup_event, MF_POPUP, (UINT_PTR)(hpopup_new), utf8_2_ansi(_("Add")));
		AppendMenu(hpopup_event, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_event, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		if (icount >= tscenario::max_event_count) {
			EnableMenuItem(hpopup_event, (UINT_PTR)(hpopup_new), MF_BYCOMMAND | MF_GRAYED);
		}
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup_event, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_event, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
		

		TrackPopupMenuEx(hpopup_event, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_new);
		DestroyMenu(hpopup_event);

		ns::clicked_event = lpnmitem->iItem;
		ns::type = IDC_LV_CAMPSCENARIO_EVENT;

	} else if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_TREASURE) {
		HMENU hpopup_treasure = CreatePopupMenu();
		AppendMenu(hpopup_treasure, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
		AppendMenu(hpopup_treasure, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_treasure, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		if (lpnmitem->iItem < 0) {
			EnableMenuItem(hpopup_treasure, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_treasure, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
		

		TrackPopupMenuEx(hpopup_treasure, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_treasure);

		ns::clicked_treasure = lvi.lParam;
		ns::type = IDC_LV_CAMPSCENARIO_TREASURE;
	} else if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_ROAD) {
		HMENU hpopup_road = CreatePopupMenu();
		AppendMenu(hpopup_road, MF_STRING, IDM_ADD, utf8_2_ansi(_("Add...")));
		// AppendMenu(hpopup_road, MF_STRING, IDM_EDIT, utf8_2_ansi(_("Edit...")));
		AppendMenu(hpopup_road, MF_STRING, IDM_DELETE, utf8_2_ansi(_("Delete...")));

		std::vector<tside>& sides = scenario.side_;
		std::set<int> cities;
		for (std::vector<tside>::const_iterator it = sides.begin(); it != sides.end(); ++ it) {
			for (std::vector<tside::tcity>::const_iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
				cities.insert(it2->heros_army_[0]);
			}
		}
		int total_roads = 0;
		for (std::set<int>::const_iterator it = cities.begin(); it != cities.end(); ++ it) {
			std::set<int>::iterator find = cities.find(*it);
			int great = std::distance(find, cities.end()) - 1;
			total_roads += great;
		}
		if (ListView_GetItemCount(lpNMHdr->hwndFrom) == total_roads) {
			EnableMenuItem(hpopup_road, IDM_ADD, MF_BYCOMMAND | MF_GRAYED);
		}

		if (lpnmitem->iItem < 0) {
			// EnableMenuItem(hpopup_road, IDM_EDIT, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(hpopup_road, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
		}
		

		TrackPopupMenuEx(hpopup_road, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		DestroyMenu(hpopup_road);

		ns::clicked_road = lpnmitem->iItem;
		ns::type = IDC_LV_CAMPSCENARIO_ROAD;
	}

	return;
}

void campaignscenario_notify_handler_dblclk(HWND hdlgP, LPNMHDR lpNMHdr)
{
	LVITEM					lvi;
	LPNMITEMACTIVATE		lpnmitem;

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if ((lpNMHdr->code == NM_DBLCLK) && (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_SIDE || lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_EVENT || lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_TREASURE))  {
		lpnmitem = (LPNMITEMACTIVATE)lpNMHdr;
		// 如果单击到的是复选框位置,把复选框状态置反
		// 当前定义的图标大小是16x16. ptAction反回的(x,y)是整个列表视图内的坐标,因而y值不大好判断
		// 认为如果x是小于16的就认为是击中复选框
		
		lvi.iItem = lpnmitem->iItem;
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.iSubItem = 0;
		lvi.pszText = gdmgr._menu_text;
		lvi.cchTextMax = _MAX_PATH;
		ListView_GetItem(lpNMHdr->hwndFrom, &lvi);

		if (lpnmitem->iItem >= 0) {
			if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_SIDE) {
				ns::clicked_side = lpnmitem->iItem;
				OnSideEditBt(hdlgP);
			} else if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_EVENT) {
				ns::clicked_event = lpnmitem->iItem;
				OnEventEditBt(hdlgP);
			} else if (lpNMHdr->idFrom == IDC_LV_CAMPSCENARIO_TREASURE) {
				ns::clicked_treasure = lvi.lParam;
				OnScenarioTreasureEditBt(hdlgP);
			}
		}
	}
    return;
}

BOOL On_DlgCampaignScenarioNotify(HWND hdlgP, int DlgItem, LPNMHDR lpNMHdr)
{
	if (lpNMHdr->code == NM_RCLICK) {
		campaignscenario_notify_handler_rclick(hdlgP, lpNMHdr);

	} else if (lpNMHdr->code == NM_DBLCLK) {
		campaignscenario_notify_handler_dblclk(hdlgP, lpNMHdr);

	}
	return FALSE;
}

BOOL CALLBACK DlgCampaignScenarioProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgCampaignScenarioInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgCampaignScenarioCommand);
	HANDLE_MSG(hDlg, WM_NOTIFY, On_DlgCampaignScenarioNotify); 
	}
	
	return FALSE;
}

void campaign_refresh(HWND hdlgP)
{
	tevent::fill_name_map();
	if (tmain::catalog_map.empty()) {
		tmain::catalog_map.push_back(std::make_pair("none", _("Internal campaign")));
		tmain::catalog_map.push_back(std::make_pair("tutorial", dsgettext("wesnoth-lib", "Tutorial")));
	}

	wml_config_from_file(std::string(gdmgr.cfg_fname_), ns::campaign.game_config_);
	std::string id = offextname(basename(gdmgr.cfg_fname_));
	
	char text[_MAX_PATH];
	std::stringstream strstr;
	ns::campaign_cfg_ptr = NULL;
	BOOST_FOREACH (const config &i, editor_config::data_cfg.child_range("campaign")) {
		if (i["id"] == id) {
			ns::campaign_cfg_ptr = &i;
			break;
		}
	}
	ns::_main.from_config(*ns::campaign_cfg_ptr);
	// read hero.dat
	std::string hero_filename = get_wml_location(ns::_main.hero_data_);
	gdmgr.heros_.map_from_file(hero_filename);

	campaign_enable_new_btn(ns::_main.mode_ == mode_tag::SCENARIO);
	
	ns::_scenario.clear();
	ns::current_scenario = 0;
	BOOST_FOREACH (const config &i, ns::campaign.game_config_.child_range("scenario")) {
		ns::_scenario.push_back(tscenario(id));
		tscenario& s = ns::_scenario.back();
		s.init_hero_state(gdmgr.heros_);
		// sub-object's from_config will use ns::current_scenario, set it correctly.
		s.from_config(ns::current_scenario, i);
		ns::current_scenario ++;
	}
	ns::current_scenario = 0;

	TCITEM tie;

	DLGHDR* pHdr = (DLGHDR*)GetWindowLong(hdlgP, GWL_USERDATA);
	if (!pHdr) {
		pHdr = (DLGHDR*)malloc(sizeof(DLGHDR));
		memset(pHdr, 0, sizeof(DLGHDR));
		// Save a pointer to the DLGHDR structure. 
		SetWindowLong(hdlgP, GWL_USERDATA, (LONG) pHdr); 

		pHdr->hwndTab = GetDlgItem(hdlgP, IDC_TAB_CAMP_SCENARIO);

		RECT rc;

		// Add a tab for each of the three child dialog boxes. 
		tie.mask = TCIF_TEXT | TCIF_IMAGE; 
		tie.iImage = -1; 
		tie.pszText = "_main"; 
		TabCtrl_InsertItem(pHdr->hwndTab, 0, &tie); 

		pHdr->reserved_pages = 1;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->valid_pages = 1;

		// Lock the resources for the three child dialog boxes. 
		pHdr->apRes[0] = editor_config::DoLockDlgRes(MAKEINTRESOURCE(IDD_CAMPAIGN_MAIN));
		
		// Calculate how large to make the tab control, so 
		// the display area can accommodate all the child dialog boxes.
		GetWindowRect(hdlgP, &rc);
		GetWindowRect(pHdr->hwndTab, &pHdr->rcDisplay);
		OffsetRect(&pHdr->rcDisplay, -1 * rc.left, -1 * rc.top);
		TabCtrl_GetItemRect(pHdr->hwndTab, 0, &rc);
		OffsetRect(&pHdr->rcDisplay, 0, rc.bottom);
	}
	
	if (pHdr->hwndDisplay != NULL) {
		DestroyWindow(pHdr->hwndDisplay);
	}
	int index;
	for (index = 1; index < pHdr->valid_pages; index ++) {
		TabCtrl_DeleteItem(pHdr->hwndTab, 1);
	}

	pHdr->valid_pages = 1 + ns::_scenario.size();
	if (pHdr->valid_pages > pHdr->reserved_pages) {
		DLGTEMPLATE** apRes = pHdr->apRes;
		pHdr->apRes = (DLGTEMPLATE**)malloc(pHdr->valid_pages * sizeof(DLGTEMPLATE*));
		memcpy(pHdr->apRes, apRes, pHdr->reserved_pages * sizeof(DLGTEMPLATE*));
		pHdr->reserved_pages = pHdr->valid_pages;
		free(apRes);
	}

	for (index = 1; index < pHdr->valid_pages; index ++) {
		tie.mask = TCIF_TEXT | TCIF_IMAGE; 
		tie.iImage = -1;
		strstr.str("");
		strstr << std::setw(2) << std::setfill('0') << index;
		strstr << "(" << ns::_scenario[index - 1].id_ << ")";
		strcpy(text, strstr.str().c_str());
		tie.pszText = text; 
		TabCtrl_InsertItem(pHdr->hwndTab, index, &tie); 

		pHdr->apRes[index] = editor_config::DoLockDlgRes(MAKEINTRESOURCE(IDD_CAMPAIGN_SCENARIO));
	}

	// Simulate selection of the first item.
	if (TabCtrl_GetCurSel(pHdr->hwndTab)) {
		TabCtrl_SetCurSel(pHdr->hwndTab, 0);
	}
	OnSelChanged(hdlgP);
}

namespace ns_new {
	enum {BIT_ID, BIT_SCENARIOID};
	int dirty_;
	std::string id_;
	std::string scenario_id_;

	void set_dirty(int bit, bool set)
	{
		if (set) {
			dirty_ |= 1 << bit;
		} else {
			dirty_ &= ~(1 << bit);
		}
	}
}

BOOL On_DlgNewCampaignInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	ns_new::set_dirty(ns_new::BIT_ID, true);
	ns_new::set_dirty(ns_new::BIT_SCENARIOID, true);

	SetWindowText(hdlgP, utf8_2_ansi(_("New campaign")));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CAMPAIGNID), utf8_2_ansi(_("Campaign ID(Once set, cannot modify in future)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SCENARIOID), utf8_2_ansi(_("First scenario ID(May modify in future)")));

	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("New")));
	Button_SetText(GetDlgItem(hdlgP, IDCANCEL), utf8_2_ansi(_("Cancel")));

	Button_Enable(GetDlgItem(hdlgP, IDOK), FALSE);

	SetFocus(GetDlgItem(hdlgP, IDC_ET_NEWCAMP_ID));

	return FALSE;
}

std::string generate_scenario_description(const std::string& campaign_id, const std::string& scenario_id)
{
	std::stringstream strstr;

	strstr << utf8_2_ansi(_("scenario file:")) << " ";
	strstr << tscenario::file(false, campaign_id, scenario_id, 0, false);
	strstr << "\r\n";
	strstr << utf8_2_ansi(_("map file:")) << " ";
	strstr << tscenario::map_file(false, campaign_id, scenario_id, 0, false);
	
	return strstr.str();
}

void OnNewCampaignEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::string str;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_NEWCAMP_DESC);
	if (id == IDC_ET_NEWCAMP_FIRSTSCENARIOID) {
		hctl = GetDlgItem(hdlgP, IDC_ET_NEWCAMP_FIRSTSCENARIODESC);
	}

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);

	if (!isvalid_id(str)) {
		strstr << utf8_2_ansi(_("Invalid string"));
		if (id == IDC_ET_NEWCAMP_ID) {
			ns_new::set_dirty(ns_new::BIT_ID, true);
		} else if (id == IDC_ET_NEWCAMP_FIRSTSCENARIOID) {
			ns_new::set_dirty(ns_new::BIT_SCENARIOID, true);
		}
	} else if (id == IDC_ET_NEWCAMP_ID) {
		const config& campaigns_cfg = editor_.campaigns_config();
		const config& campaign_cfg = campaigns_cfg.find_child("campaign", "id", str);
		if (!campaign_cfg) {
			strstr << generate_scenario_description(str, ns_new::scenario_id_);
			Edit_SetText(GetDlgItem(hdlgP, IDC_ET_NEWCAMP_FIRSTSCENARIODESC), strstr.str().c_str());

			strstr.str("");
			strstr << utf8_2_ansi(_("cfg directory:")) << " <res>/data/campaigns/" << str << "/";
			strstr << "\r\n";
			strstr << utf8_2_ansi(_("bin file:")) << " <res>/xwml/campaigns/" << str << ".bin";
			ns_new::set_dirty(ns_new::BIT_ID, false);
			ns_new::id_ = str; 
		} else {
			ns_new::set_dirty(ns_new::BIT_ID, true);
			strstr << utf8_2_ansi(_("Other campaign has holded ID"));
		}
	} else if (id == IDC_ET_NEWCAMP_FIRSTSCENARIOID) {
		strstr << generate_scenario_description(ns_new::id_, str);
		ns_new::set_dirty(ns_new::BIT_SCENARIOID, false);
		ns_new::scenario_id_ = str;
	}
	Edit_SetText(hctl, strstr.str().c_str());

	Button_Enable(GetDlgItem(hdlgP, IDOK), ns_new::dirty_? FALSE: TRUE);
	return;
}

void On_DlgNewCampaignCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	switch (id) {
	case IDC_ET_NEWCAMP_ID:
	case IDC_ET_NEWCAMP_FIRSTSCENARIOID:
		OnNewCampaignEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgNewCampaignProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgNewCampaignInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgNewCampaignCommand);
	}
	
	return FALSE;
}

BOOL On_DlgNewScenarioInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	ns_new::set_dirty(ns_new::BIT_SCENARIOID, true);

	SetWindowText(hdlgP, utf8_2_ansi(_("New scenario")));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SCENARIOID), utf8_2_ansi(_("Scenario ID(May modify in future)")));

	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("New")));
	Button_SetText(GetDlgItem(hdlgP, IDCANCEL), utf8_2_ansi(_("Cancel")));

	Button_Enable(GetDlgItem(hdlgP, IDOK), FALSE);

	SetFocus(GetDlgItem(hdlgP, IDC_ET_NEWSCENARIO_ID));

	return FALSE;
}

void OnNewScenarioEt(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	std::string str;
	std::stringstream strstr;

	if (codeNotify != EN_CHANGE) {
		return;
	}
	HWND hctl = NULL;
	if (id == IDC_ET_NEWSCENARIO_ID) {
		hctl = GetDlgItem(hdlgP, IDC_ET_NEWSCENARIO_DESC);
	}
	
	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	str = text;
	std::transform(str.begin(), str.end(), str.begin(), std::tolower);

	if (!isvalid_id(str)) {
		strstr << utf8_2_ansi(_("Invalid string"));
		if (id == IDC_ET_NEWSCENARIO_ID) {
			ns_new::set_dirty(ns_new::BIT_SCENARIOID, true);
		}
	} else if (id == IDC_ET_NEWSCENARIO_ID) {
		strstr << utf8_2_ansi(_("scenario file:")) << " ";
		strstr << tscenario::file(false, ns::_main.id_, str, ns::_scenario.size(), false);
		strstr << "\r\n";
		strstr << utf8_2_ansi(_("map file:")) << " ";
		strstr << tscenario::map_file(false, ns::_main.id_, str, ns::_scenario.size(), false);
		ns_new::set_dirty(ns_new::BIT_SCENARIOID, false);
		ns_new::scenario_id_ = str;
	}
	Edit_SetText(hctl, strstr.str().c_str());

	Button_Enable(GetDlgItem(hdlgP, IDOK), ns_new::dirty_? FALSE: TRUE);
	return;
}

void On_DlgNewScenarioCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	switch (id) {
	case IDC_ET_NEWSCENARIO_ID:
		OnNewScenarioEt(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDOK:
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgNewScenarioProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgNewScenarioInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgNewScenarioCommand);
	}
	
	return FALSE;
}

namespace ns {

void new_campaign(const std::string& id, const std::string& firstscenario_id)
{
	// create campaign directory
	MakeDirectory(game_config::path + "/data/campaigns/" + id);
	MakeDirectory(game_config::path + "/data/campaigns/" + id + "/images");
	MakeDirectory(game_config::path + "/data/campaigns/" + id + "/maps");
	MakeDirectory(game_config::path + "/data/campaigns/" + id + "/scenarios");

	// create *.bin directory
	MakeDirectory(game_config::path + "/xwml/campaigns");

	// reload heros
	std::string hero_filename = get_wml_location("^xwml/hero.dat");
	gdmgr.heros_.map_from_file(hero_filename);

	_main = tmain(id, firstscenario_id);
	_main.mode_ = mode_tag::SCENARIO;
	_scenario.clear();
	_scenario.push_back(tscenario(id, firstscenario_id));
	tscenario& scenario = _scenario.back();
	scenario.init_hero_state(gdmgr.heros_);

	scenario.map_data_ = tscenario::default_map_data();
	scenario.objectives_.summary_ = "";
	scenario.objectives_.win_.push_back("Defeat all sides");
	scenario.objectives_.lose_.push_back("No city you holded");

	for (int i = 0; i < 2; i ++) {
		scenario.new_side();
		tside& side = scenario.side_.back();
		side.new_city();
		if (i == 0) {
			side.cities_[0].loc_ = map_location(2, 2);
		} else {
			side.cities_[0].loc_ = map_location(4, 4);
		}
		side.new_troop();
		if (i == 1) {
			side.troops_[0].loc_ = map_location(1, 4);
		}
		side.leader_ = side.troops_[0].heros_army_[0];
	}
	// copy logo
	std::string src = game_config::path + "/data/core/images/scenery/fire1.png";
	CopyFile(src.c_str(), ns::_main.icon(true).c_str(), FALSE);

	_main.generate();
	_scenario.front().generate();
}

bool new_scenario()
{
	HWND parent;
	std::string campaign_id;

	if (!scenario_selector::multiplayer) {
		parent = gdmgr._hdlg_campaign;
		campaign_id = _main.id_;
	} else {
		parent = gdmgr._hdlg_core;
		campaign_id = "multiplayer";
	}

	int retval = DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_NEWSCENARIO), parent, DlgNewScenarioProc);
	if (!retval) return false;

	_scenario.push_back(tscenario(campaign_id, ns_new::scenario_id_, _scenario.size()));
	tscenario& scenario = _scenario.back();
	scenario.multiplayer_ = scenario_selector::multiplayer;
	scenario.init_hero_state(gdmgr.heros_);

	scenario.map_data_ = tscenario::default_map_data();
	scenario.objectives_.summary_ = "";
	scenario.objectives_.win_.push_back("Defeat all sides");
	scenario.objectives_.lose_.push_back("No city you holded");

	current_scenario = _scenario.size() - 1;
	for (int i = 0; i < 2; i ++) {
		scenario.new_side();
		tside& side = scenario.side_.back();
		side.new_city();
		if (i == 0) {
			side.controller_ = scenario_selector::multiplayer? controller_tag::AI: controller_tag::HUMAN;
			side.cities_[0].loc_ = map_location(2, 2);
		} else {
			side.controller_ = controller_tag::AI;
			side.cities_[0].loc_ = map_location(4, 4);
		}
		side.new_troop();
		if (i == 1) {
			side.troops_[0].loc_ = map_location(1, 4);
		}
		side.leader_ = side.troops_[0].heros_army_[0];
	}

	scenario.generate();

	// sync
	editor_config::campaign_id = scenario.campaign_id_;
	sync_refresh_sync();

	return true;
}

void delete_scenario(HWND hdlgP)
{
	std::string title;
	std::stringstream strstr;
	utils::string_map symbols;
	int retval;
	
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	symbols["campaign"] = scenario.campaign_id_;
	symbols["scenario"] = scenario.id_;
	strstr.str("");
	strstr << utf8_2_ansi(vgettext2("Do you want to delete scenario: \"$scenario\" of campaign: \"$campaign\"?", symbols).c_str());

	title = utf8_2_ansi(_("Confirm delete"));
	// Confirm Multiple File Delete
	retval = MessageBox(hdlgP, strstr.str().c_str(), title.c_str(), MB_YESNO | MB_DEFBUTTON2);
	if (retval == IDYES) {
		// delete all relative scenario id
		std::string first_existed_id;
		for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
			if (it->id_ == scenario.id_) {
				continue;
			}
			if (first_existed_id.empty()) {
				first_existed_id = it->id_;
			}
			if (it->next_scenario_ == scenario.id_) {
				it->next_scenario_.clear();
				it->generate();
			}
		}
		if (!scenario_selector::multiplayer) {
			if (ns::_main.first_scenario_ == scenario.id_) {
				ns::_main.first_scenario_ = first_existed_id;
				ns::_main.generate();
			}
		}

		if (!delfile1(scenario.file(true).c_str())) {
			posix_print_mb("Failed delete %s !", scenario.file(true).c_str()); 
		}
		if (!delfile1(scenario.map_file(true).c_str())) {
			posix_print_mb("Failed delete %s !", scenario.map_file(true).c_str()); 
		}
		
		editor_config::campaign_id = scenario.campaign_id_;
		sync_refresh_sync();
	}
}

}

bool campaign_new()
{
	int retval = DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_NEWCAMPAIGN), gdmgr._hdlg_campaign, DlgNewCampaignProc);
	if (!retval) return false;

	ns::new_campaign(ns_new::id_, ns_new::scenario_id_);

	editor_config::campaign_id = ns_new::id_;
	sync_refresh_sync();

	return true;
}

std::string tscenario::state_name(int state)
{
	if (state == tscenario::STATE_UNKNOWN) {
		return _("Idle");
	} else if (state == tscenario::STATE_SERVICE) {
		return _("Service");
	} else if (state == tscenario::STATE_WANDER) {
		return _("Wander");
	} else if (state == tscenario::STATE_ARMY) {
		return _("Army");
	} else if (state == tscenario::STATE_RESERVE) {
		return _("Reserve");
	} else {
		return _("Error value");
	}
}

void fill_hero_state_row(const tscenario& scenario, HWND hctl, hero& h, const tscenario::hero_state& state, int iItem)
{
	char text[_MAX_PATH];
	std::stringstream strstr;
	LVITEM lvi;
	int index = 0;

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// number
	lvi.iItem = iItem;
	lvi.iSubItem = index ++;
	sprintf(text, "%u", h.number_);
	lvi.pszText = text;
	lvi.lParam = (LPARAM)0;
	ListView_InsertItem(hctl, &lvi);

	// name.
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = index ++;
	strcpy(text, utf8_2_ansi(h.name().c_str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// use
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = index ++;
	strstr.str("");
	for (std::map<int, tscenario::hero_state::tstate>::const_iterator it = state.states_.begin(); it != state.states_.end(); ++ it) {
		const tscenario::hero_state::tstate& s = it->second;
		if (it != state.states_.begin()) {
			strstr << ";   ";
		}
		if (it->first < (int)scenario.side_.size()) {
			hero& leader = gdmgr.heros_[scenario.side_[it->first].leader_];
			strstr << dgettext("wesnoth-lib", "Side") << ": " << leader.name() << "  ";
			strstr << dgettext("wesnoth-lib", "City") << ": ";
			if (s.city >= 0) {
				strstr << gdmgr.heros_[s.city].name();
			} else {
				strstr << "---";
			}
			strstr << "  ";
			strstr << dgettext("wesnoth-lib", "Status") << ": " << tscenario::state_name(s.state);
			continue;
		}
		std::stringstream err;
		err << "side number(" << it->first << ") is large than side count(" << scenario.side_.size() << ")"; 
		VALIDATE(false, err.str());
	}

	strcpy(text, utf8_2_ansi(strstr.str()));
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

BOOL On_DlgReportHeroStateInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
{
	std::stringstream strstr;
	
	strstr << utf8_2_ansi(_("Hero state"));
	SetWindowText(hdlgP, strstr.str().c_str());
	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("Close")));

	HWND hctl = GetDlgItem(hdlgP, IDC_LV_VISUAL2_EXPLORER);
	LVCOLUMN lvc;
	int index = 0;
	char text[_MAX_PATH];

	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = index;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 300;
	lvc.iSubItem = index;
	strcpy(text, utf8_2_ansi(_("Use")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, index ++, &lvc);

	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	int iItem = 0;
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.artificals_.begin(); it != scenario.artificals_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];
		fill_hero_state_row(scenario, hctl, h, it->second, iItem ++);
	}
	for (std::map<int, tscenario::hero_state>::const_iterator it = scenario.persons_.begin(); it != scenario.persons_.end(); ++ it) {
		hero& h = gdmgr.heros_[it->first];
		fill_hero_state_row(scenario, hctl, h, it->second, iItem ++);
	}

	return FALSE;
}

void On_DlgReportHeroStateCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	BOOL changed = FALSE;
	switch (id) {
	case IDOK:
		changed = TRUE;
	case IDCANCEL:
		EndDialog(hdlgP, changed? 1: 0);
		break;
	}
}

BOOL CALLBACK DlgReportHeroStateProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgReportHeroStateInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgReportHeroStateCommand);
	}
	
	return FALSE;
}

bool campaign_can_execute_tack(int task)
{
	if (task == TASK_NEW || task == TASK_DELETE || task == TASK_EXPLORER) {
		if (campaign_get_save_btn()) {
			return false;
		}
	}
	return true;
}

#endif // _ROSE_EDITOR