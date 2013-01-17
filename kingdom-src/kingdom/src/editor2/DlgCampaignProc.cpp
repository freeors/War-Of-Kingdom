#define GETTEXT_DOMAIN "wesnoth-maker"

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
#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "map_location.hpp"


void campaign_refresh(HWND hdlgP);

BOOL CALLBACK DlgCampaignMainProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgCampaignScenarioProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

const std::string null_str = "";

namespace ns {
	int empty_leader = 228;

	tcampaign campaign;
	tmain _main;
	std::vector<tscenario> _scenario;
	const config* campaign_cfg_ptr;

	std::map<int, int> cityno_map;

	HIMAGELIST himl_checkbox_side;
	HIMAGELIST himl_checkbox;

	int clicked_side;
	int clicked_event;
	int clicked_treasure;
	int clicked_feature;
	int clicked_city;
	int clicked_troop;
	int clicked_hero;

	int type;
	int current_scenario;
	int action_side;
	int action_event;
	int action_treasure;
	int action_feature;
	int action_city;
	int action_troop;

	void set_dirty() 
	{
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
	}

	void new_campaign(const std::string& id, const std::string& firstscenario_id);
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

	ToolBar_AddButtons(gdmgr._htb_campaign, 1, &gdmgr._tbBtns_campaign);

	ToolBar_AutoSize(gdmgr._htb_campaign);
	
	ToolBar_SetExtendedStyle(gdmgr._htb_campaign, TBSTYLE_EX_DRAWDDARROWS);
	
	ToolBar_SetImageList(gdmgr._htb_campaign, gdmgr._himl_24x24, 0);
	ToolBar_SetDisabledImageList(gdmgr._htb_campaign, gdmgr._himl_24x24_dis);
	
	ShowWindow(gdmgr._htb_campaign, SW_SHOW);

	return gdmgr._htb_campaign;
}

void tcampaign::init_hero_state(hero_map& heros)
{
	const unit_type_data::unit_type_map& types = unit_types.types();

	if (arms_.empty()) {
		const std::vector<std::string>& ids = unit_types.arms_ids();
		int index = 0;
		for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
			arms_.push_back(std::make_pair(*it, utf8_2_ansi(hero::arms_str(index ++).c_str())));
		}
	}

	artifical_utype_.clear();
	const std::map<std::string, const unit_type*>& artifical_types = unit_types.artifical_types();
	const unit_type* keep_type = unit_types.find_keep();
	for (std::map<std::string, const unit_type*>::const_iterator it = artifical_types.begin(); it != artifical_types.end(); ++ it) {
		if (it->second != keep_type) {
			artifical_utype_.push_back(std::make_pair<std::string, const unit_type*>(it->first, it->second));
		}
	}

	city_utypes_.clear();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		if (ut.packer()) {
			continue;
		}
		if (ut.master() != HEROS_INVALID_NUMBER) {
			continue;
		}
		if (!ut.can_recruit()) {
			continue;
		}
		city_utypes_.push_back(std::make_pair<std::string, const unit_type*>(it->first, &ut));
	}

	troop_utypes_.clear();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const unit_type& ut = it->second;
		if (ut.packer()) {
			continue;
		}
		if (ut.master() != HEROS_INVALID_NUMBER) {
			continue;
		}
		if (ut.can_recruit() || ut.can_reside()) {
			continue;
		}
		troop_utypes_.push_back(std::make_pair<std::string, const unit_type*>(it->first, &ut));
	}

	if (navigation_.empty()) {
		const navigation_types& navigation_types = unit_types.navigation_threshold();
		for (navigation_types::const_iterator it = navigation_types.begin(); it != navigation_types.end(); ++ it) {
			navigation_.push_back(*it);
		}
	}

	const traits_map& traits = unit_types.traits();
	city_traits_.clear();
	for (traits_map::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		city_traits_.push_back(std::make_pair(it->first, &it->second));
	}
	troop_traits_.clear();
	for (traits_map::const_iterator it = traits.begin(); it != traits.end(); ++ it) {
		troop_traits_.push_back(std::make_pair(it->first, &it->second));
	}

	if (reserved_heros_.empty()) {
		reserved_heros_.insert(214); // 情报
		reserved_heros_.insert(228); // 黄巾
		reserved_heros_.insert(229); // 文官
		reserved_heros_.insert(230); // 系统

		reserved_heros_.insert(272); // 市场
		reserved_heros_.insert(273); // 城墙
		reserved_heros_.insert(274); // 主楼
		reserved_heros_.insert(275); // 箭塔
		reserved_heros_.insert(276); // 技法所
		reserved_heros_.insert(277); // (保留)
	}

	persons_.clear();
	artificals_.clear();
	for (hero_map::iterator it = heros.begin(); it != heros.end(); ++ it) {
		if (reserved_heros_.find(it->number_) != reserved_heros_.end()) {
			continue;
		}
		if (it->gender_ != hero_gender_neutral) {
			persons_[it->number_] = hero_state();
		} else {
			artificals_[it->number_] = hero_state();
		}
	}

	return;
}

void tcampaign::do_state(int h, bool allocated, int city, int state)
{
	std::map<int, hero_state>::iterator it;
	if (gdmgr.heros_[h].gender_ != hero_gender_neutral) {
		it = persons_.find(h);
	} else {
		it = artificals_.find(h);
	}
	it->second.allocated_ = allocated;
	if (allocated) {
		it->second.city_ = city;
		it->second.state_ = state;
	} else {
		it->second.city_ = -1;
		it->second.state_ = STATE_UNKNOWN;
	}
}

const std::string& tcampaign::arms(const std::string& id)
{
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = arms_.begin(); it != arms_.end(); ++ it) {
		if (id == it->first) {
			return it->second;
		}
	}
	return null_str;
}

int tcampaign::arms_int(const std::string& id)
{
	int index = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = arms_.begin(); it != arms_.end(); ++ it, index ++) {
		if (id == it->first) {
			return index;
		}
	}
	return index;
}

std::string tcampaign::city_trait(const std::string& id)
{
	for (std::vector<std::pair<std::string, const config*> >::const_iterator it = city_traits_.begin(); it != city_traits_.end(); ++ it) {
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
	for (std::vector<std::pair<std::string, const config*> >::const_iterator it = troop_traits_.begin(); it != troop_traits_.end(); ++ it) {
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
	rank_ = campaign_cfg["rank"].to_int();
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
	rpg_mode_ = campaign_cfg["rpg_mode"].to_bool();
	
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
	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_RANK);
	rank_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ABBREV_MSGID), text, sizeof(text) / sizeof(text[0]));
	abbrev_ = text;

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_FIRSTSCENARIO);
	ComboBox_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	first_scenario_ = text;

	rpg_mode_ = Button_GetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_RPGMODE));
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
	strstr << "\trank = " << rank_ << "\n";
	strstr << "\tname = _ \"" << id_ << "\"\n";
	strstr << "\tabbrev = _ \"" << abbrev_ << "\"\n";
	strstr << "\tdefine = " << define << "\n";
	strstr << "\tfirst_scenario = " << first_scenario_ << "\n";
	strstr << "\ticon = \"" << icon() << "\"\n";
	strstr << "\timage = \"" << image() << "\"\n";
	strstr << "\tdescription = _ \"" << id_ << " description\"\n";
	strstr << "\thero_data = \"" << hero_data_ << "\"\n";
	strstr << "\trpg_mode = " << (rpg_mode_? "yes": "no") << "\n";
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

tside::tside()
	: side_(-1)
	, name_()
	, leader_(HEROS_INVALID_NUMBER)
	, controller_(HUMAN)
	, shroud_(false)
	, fog_(false)
	, holded_cards_()
	, navigation_(0)
	, gold_(0)
	, income_(0)
	, build_()
	, technologies_()
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
	foreach (const config &cfg, direct_side_cfg.child_range("if")) {
		const config& else_cfg = cfg.child("else");
		side_cfg.merge_attributes(else_cfg);
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
	if (side_cfg["controller"].str() == "null") {
		controller_ = EMPTY;
	}
	shroud_ = side_cfg["shroud"].to_bool();
	fog_ = side_cfg["fog"].to_bool();

	str = side_cfg["holded_cards"].str();
	std::vector<std::string> vstr = utils::split(str);
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		holded_cards_.push_back(atoi(i->c_str()));
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
	const std::map<std::string, technology>& technologies = unit_types.technologies();
	for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
		const std::string& id = *i;
		std::map<std::string, technology>::const_iterator find = technologies.find(id);
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

	foreach (const config &cfg, side_cfg.child_range("artifical")) {
		// str = cfg["type"].str();
		cities_.push_back(tcity());
		tcity& c = cities_.back();

		// c.type_ = str;
		vstr = utils::split(cfg["heros_army"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.heros_army_.push_back(number);
			ns::campaign.do_state(number, true, number, tcampaign::STATE_ARMY);
		}
		c.mayor_ = cfg["mayor"].to_int(HEROS_INVALID_NUMBER);
		cityno = cfg["cityno"].to_int();
		cityno_map[cityno] = c.heros_army_[0];
		c.loc_.x = cfg["x"].to_int();
		c.loc_.y = cfg["y"].to_int();
		vstr = utils::split(cfg["traits"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			c.traits_.insert(*i);
		}
		str = cfg["character"].str();
		c.character_ = unit_types.character_from_id(str);
		vstr = utils::split(cfg["not_recruit"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			c.not_recruit_.push_back(*i);
		}
		// service_heros
		vstr = utils::split(cfg["service_heros"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.service_heros_.insert(number);
			ns::campaign.do_state(number, true, c.heros_army_[0], tcampaign::STATE_SERVICE);
		}
		// wander_heros
		vstr = utils::split(cfg["wander_heros"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			c.wander_heros_.insert(number);
			ns::campaign.do_state(number, true, c.heros_army_[0], tcampaign::STATE_WANDER);
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
			c.type_ = ns::campaign.city_utypes_[0].first;
			strstr << "”，但查不到该兵种，强制设为“" << utf8_2_ansi(unit_types.find(c.type_)->type_name().c_str()) << "”";
			posix_print_mb(strstr.str().c_str());
			c.except_ = true;
		} else {
			c.type_ = str;
		}
	}

	foreach (const config &cfg, side_cfg.child_range("unit")) {
		troops_.push_back(tunit());
		tunit& u = troops_.back();

		cityno = cfg["cityno"].to_int();
		if (cityno == HEROS_DEFAULT_CITY) {
			u.city_ = HEROS_INVALID_NUMBER;
		} else {
			u.city_ = cityno_map.find(cityno)->second;
		}
		vstr = utils::split(cfg["heros_army"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			int number = lexical_cast_default<int>(*i);
			u.heros_army_.push_back(number);
			ns::campaign.do_state(number, true, u.city_, tcampaign::STATE_ARMY);
		}
		u.loc_.x = cfg["x"].to_int();
		u.loc_.y = cfg["y"].to_int();
		vstr = utils::split(cfg["traits"].str());
		for (std::vector<std::string>::const_iterator i = vstr.begin(); i != vstr.end(); ++ i) {
			u.traits_.insert(*i);
		}
		str = cfg["character"].str();
		u.character_ = unit_types.character_from_id(str);

		str = cfg["type"].str();
		const unit_type* ut = unit_types.find(str);
		if (!ut) {
			std::stringstream strstr;
			strstr << utf8_2_ansi(gdmgr.heros_[u.heros_army_[0]].name().c_str()) << "部队原兵种标识是“" << str;
			u.type_ = ns::campaign.troop_utypes_[0].first;
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
	if (leader_ == ns::empty_leader) {
		controller_ = EMPTY;
	} else {
		controller_ = HUMAN;
	}

	gold_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_GOLD));
	income_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_INCOME));

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_NAVIGATION);
	navigation_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl)) * 10000;
	navigation_ += UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_NAVIGATIONXP));

	build_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_BUILD);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			build_.insert(ns::campaign.artifical_utype_[idx].first);
		}
	}

	technologies_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TECHNOLOGY);
	const std::map<std::string, technology>& technologies = unit_types.technologies();
	int idx = 0;
	for (std::map<std::string, technology>::const_iterator it = technologies.begin(); it != technologies.end(); ++ it, idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			technologies_.insert(it->first);
		}
	}
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

	// 空白
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
	strcpy(text, (controller_ == EMPTY)? utf8_2_ansi(_("Yes")): utf8_2_ansi(_("No")));
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

	if (!partial) {
		strstr << side_ + 1;
		Edit_SetText(GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_SIDE), strstr.str().c_str());

		// controller
		Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_SIDEEDIT_CONTROLLER), (controller_ == EMPTY)? TRUE: FALSE);

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
	if (controller_ == EMPTY || ns::_scenario[ns::current_scenario].null_side() == -1) {
		int number = ns::empty_leader;
		strstr.str("");
		strstr << "(" << utf8_2_ansi(gdmgr.heros_[number].name().c_str()) << ")";
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, 0, number);
		if (leader_ == number) {
			selected_row = 0;
		}
	}
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

	if (!partial) {
		// build
		hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_BUILD);
		ListView_DeleteAllItems(hctl);
		int index = 0;
		LVITEM lvi;
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = ns::campaign.artifical_utype_.begin(); it != ns::campaign.artifical_utype_.end(); ++ it) {
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 姓名
			lvi.iItem = index ++;
			lvi.iSubItem = 0;
			strcpy(text, utf8_2_ansi(it->second->type_name().c_str()));
			lvi.pszText = text;
			lvi.lParam = (LPARAM)0;
			ListView_InsertItem(hctl, &lvi);

			if (build_.find(it->first) != build_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}

		// technology
		hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TECHNOLOGY);
		ListView_DeleteAllItems(hctl);
		index = 0;
		const std::map<std::string, technology>& technologies = unit_types.technologies();
		for (std::map<std::string, technology>::const_iterator it = technologies.begin(); it != technologies.end(); ++ it) {
			const technology& t = it->second;
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			// 名称
			lvi.iItem = index;
			lvi.iSubItem = 0;
			strcpy(text, utf8_2_ansi(t.name().c_str()));
			lvi.pszText = text;
			lvi.lParam = (LPARAM)0;
			ListView_InsertItem(hctl, &lvi);

			// 描述
			lvi.mask = LVIF_TEXT;
			lvi.iItem = index ++;
			lvi.iSubItem = 1;
			strcpy(text, utf8_2_ansi(t.description().c_str()));
			lvi.pszText = text;
			ListView_SetItem(hctl, &lvi);

			if (technologies_.find(it->first) != technologies_.end()) {
				ListView_SetCheckState(hctl, lvi.iItem, TRUE);
			} else {
				ListView_SetCheckState(hctl, lvi.iItem, FALSE);
			}
		}
		strstr.str("");
		strstr << dgettext_2_ansi("wesnoth-lib", "Technology") << "(" << editor_config::ListView_GetCheckedCount(hctl) << ")";
		Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TECHNOLOGY), strstr.str().c_str());
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
	cities_.push_back(tside::tcity());
	tside::tcity& city = cities_.back();

	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.artificals_.begin(); it != ns::campaign.artificals_.end(); ++ it) {
		if (it->second.allocated_) continue;
		city.heros_army_.push_back(it->first);
		break;
	}
	if (city.heros_army_.empty()) {
		return false;
	}
	city.type_ = ns::campaign.city_utypes_[0].first;
	city.loc_ = map_location(1, 1);

	ns::campaign.do_state(city.heros_army_[0], true, city.heros_army_[0], tcampaign::STATE_ARMY);

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
	tcity& city = cities_[index];
	int city_hero = city.heros_army_[0];
	ns::campaign.do_state(city_hero, false);
	for (std::set<int>::const_iterator it = city.service_heros_.begin(); it != city.service_heros_.end(); ++ it) {
		ns::campaign.do_state(*it, false);
	}
	for (std::set<int>::const_iterator it = city.wander_heros_.begin(); it != city.wander_heros_.end(); ++ it) {
		ns::campaign.do_state(*it, false);
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
		update_to_ui_side_edit(hdlgP);
	}
}

bool tside::new_troop()
{
	troops_.push_back(tside::tunit());
	tside::tunit& troop = troops_.back();

	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		if (it->second.allocated_) continue;
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
	troop.type_ = ns::campaign.troop_utypes_[0].first;
	troop.loc_ = map_location(1, 1);

	ns::campaign.do_state(troop.heros_army_[0], true, troop.heros_army_[0], tcampaign::STATE_ARMY);
	return true;
}

void tside::erase_troop(int index, HWND hdlgP)
{
	tunit& troop = troops_[index];
	for (std::vector<int>::const_iterator it = troop.heros_army_.begin(); it != troop.heros_army_.end(); ++ it) {
		ns::campaign.do_state(*it, false);
	}
	troops_.erase(troops_.begin() + index);

	if (hdlgP) {
		update_to_ui_side_edit(hdlgP);
	}
}

bool tside::operator==(const tside& that) const
{
	if (except_) return false;
	if (side_ != that.side_) return false;
	if (name_ != that.name_) return false;
	if (leader_ != that.leader_) return false;
	if (controller_ != that.controller_) return false;
	if (holded_cards_ != that.holded_cards_) return false;
	if (features_ != that.features_) return false;
	if (navigation_ != that.navigation_) return false;
	if (gold_ != that.gold_) return false;
	if (income_ != that.income_) return false;
	if (flag_ != that.flag_) return false;
	if (build_ != that.build_) return false;
	if (technologies_ != that.technologies_) return false;

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

std::string tside::generate_features(tside::CONTROLLER cntl) const
{
	std::stringstream strstr;
	int level;

	for (std::vector<arms_feature>::const_iterator it = features_.begin(); it != features_.end(); ++ it) {
		if (it == features_.begin()) {
			strstr << "(";
		} else {
			strstr << ", (";
		}
		strstr << ns::campaign.arms_[it->arms_].first << ", ";
		if (cntl == tside::HUMAN) {
			level = std::min<int>(it->level_ + 2, 6); 
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
	HWND hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_HERO);
	int select_hero = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));
	if (select_hero != heros_army_[0]) {
		int original = heros_army_[0];
		heros_army_.clear();
		heros_army_.push_back(select_hero);
		ns::campaign.do_state(original, false);
		ns::campaign.do_state(heros_army_[0], true, heros_army_[0], tcampaign::STATE_ARMY);

		for (std::map<int, tcampaign::hero_state>::iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
			if (it->second.city_ == original) {
				it->second.city_ = heros_army_[0];
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
	type_ = ns::campaign.city_utypes_[ComboBox_GetCurSel(hctl)].first;

	loc_.x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_X));
	loc_.y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CITYEDIT_Y));

	traits_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_TRAIT);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			traits_.insert(ns::campaign.city_traits_[idx].first);
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

			not_recruit_.push_back(ns::campaign.troop_utypes_[lvi.lParam].first);
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

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	// 名称
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

	// 兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	strstr.str("");
	strstr << utf8_2_ansi(unit_types.find(type_)->type_name().c_str());
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 坐标
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 2;
	strstr.str("");
	strstr << "(" << loc_.x << "," << loc_.y << ")";
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 特质
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 3;
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
	lvi.iSubItem = 4;
	strstr.str("");
	if (character_ != NO_CHARACTER) {
		strstr << utf8_2_ansi(unit_types.character(character_).name_.c_str());
	}
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 不可征兵种
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 5;
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
	lvi.iSubItem = 6;
	if (mayor_ != HEROS_INVALID_NUMBER) {
		strcpy(text, utf8_2_ansi(gdmgr.heros_[mayor_].name().c_str()));
		lvi.pszText = text;
	} else {
		lvi.pszText = NULL;
	}
	ListView_SetItem(hctl, &lvi);

	// 在职
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 7;
	strstr.str("");
	strstr << service_heros_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 在野
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 8;
	strstr.str("");
	strstr << wander_heros_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);

	// 经济区
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 9;
	strstr.str("");
	strstr << economy_area_.size();
	strcpy(text, strstr.str().c_str());
	lvi.pszText = text;
	ListView_SetItem(hctl, &lvi);
}

void tside::tcity::update_to_ui_city_edit(HWND hdlgP, tside& side, bool partial)
{
	std::stringstream strstr;
	int value, selected_row = -1;
	HWND hctl;

	if (!partial) {
		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_TYPE);
		ComboBox_ResetContent(hctl);
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = ns::campaign.city_utypes_.begin(); it != ns::campaign.city_utypes_.end(); ++ it) {
			ComboBox_AddString(hctl, utf8_2_ansi(it->second->type_name().c_str()));
			if (type_ == it->first) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_CHARACTER);
		ComboBox_AddString(hctl, "");
		ComboBox_SetItemData(hctl, 0, NO_CHARACTER);
		selected_row = 0;
		const std::vector<tcharacter>& characters = unit_types.characters();
		for (std::vector<tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
			const tcharacter& character = *it;
			ComboBox_AddString(hctl, utf8_2_ansi(character.name_.c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, character.index_);
			if (character.index_ == character_) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

		hctl = GetDlgItem(hdlgP, IDC_CMB_CITYEDIT_HERO);
		ComboBox_ResetContent(hctl);
		for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.artificals_.begin(); it != ns::campaign.artificals_.end(); ++ it) {
			if (it->second.allocated_ && it->first != heros_army_[0]) continue;
			ComboBox_AddString(hctl, utf8_2_ansi(gdmgr.heros_[it->first].name().c_str()));
			ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
			if (it->first == heros_army_[0]) {
				selected_row = ComboBox_GetCount(hctl) - 1;
			}
		}
		ComboBox_SetCurSel(hctl, selected_row);

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
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		if (it->second.city_ != heros_army_[0]) continue;
		if (it->second.state_ != tcampaign::STATE_SERVICE && it->second.state_ != tcampaign::STATE_ARMY) continue;
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
		for (std::vector<std::pair<std::string, const config*> >::const_iterator it = ns::campaign.city_traits_.begin(); it != ns::campaign.city_traits_.end(); ++ it) {
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
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = ns::campaign.troop_utypes_.begin(); it != ns::campaign.troop_utypes_.end(); ++ it, index ++) {
			const unit_type* ut = it->second;
			
			if (ut->character() != NO_CHARACTER) {
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
	}

	// candidate hero
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_CANDIDATE);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		if (it->second.allocated_) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)it->first;
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

		// 势力特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.side_feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
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
	type_ = ns::campaign.troop_utypes_[ComboBox_GetCurSel(hctl)].first;

	hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_CHARACTER);
	character_ = ComboBox_GetItemData(hctl, ComboBox_GetCurSel(hctl));

	loc_.x = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_X));
	loc_.y = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_TROOPEDIT_Y));

	traits_.clear();
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_TRAIT);
	for (int idx = 0; idx < ListView_GetItemCount(hctl); idx ++) {
		if (ListView_GetCheckState(hctl, idx)) {
			traits_.insert(ns::campaign.troop_traits_[idx].first);
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
	if (character_ != NO_CHARACTER) {
		strstr << utf8_2_ansi(unit_types.character(character_).name_.c_str());
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
	std::stringstream strstr;
	utils::string_map symbols;
	int value, selected_row = -1;
	HWND hctl;

	if (!partial) {
		hctl = GetDlgItem(hdlgP, IDC_CMB_TROOPEDIT_TYPE);
		ComboBox_ResetContent(hctl);
		for (std::vector<std::pair<std::string, const unit_type*> >::const_iterator it = ns::campaign.troop_utypes_.begin(); it != ns::campaign.troop_utypes_.end(); ++ it) {
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
		ComboBox_SetItemData(hctl, 0, NO_CHARACTER);
		selected_row = 0;
		const std::vector<tcharacter>& characters = unit_types.characters();
		for (std::vector<tcharacter>::const_iterator it = characters.begin(); it != characters.end(); ++ it) {
			const tcharacter& character = *it;
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
		for (std::vector<std::pair<std::string, const config*> >::const_iterator it = ns::campaign.troop_traits_.begin(); it != ns::campaign.troop_traits_.end(); ++ it) {
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
	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_CANDIDATE);
	count = ListView_GetItemCount(hctl);
	ListView_DeleteAllItems(hctl);
	index = 0;
	for (std::map<int, tcampaign::hero_state>::const_iterator it = ns::campaign.persons_.begin(); it != ns::campaign.persons_.end(); ++ it) {
		if (it->second.allocated_) {
			continue;
		}
		hero& h = gdmgr.heros_[it->first];

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// 姓名
		lvi.iItem = index ++;
		lvi.iSubItem = 0;
		strcpy(text, utf8_2_ansi(h.name().c_str()));
		lvi.pszText = text;
		lvi.lParam = (LPARAM)it->first;
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

		// 势力特技
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 3;
		strcpy(text, utf8_2_ansi(hero::feature_str(h.side_feature_).c_str()));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);

		// 统帅
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 4;
		sprintf(text, "%u.%u", fxptoi9(h.leadership_), fxpmod9(h.leadership_));
		lvi.pszText = text;
		ListView_SetItem(hctl, &lvi);
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
	lvi.pszText = const_cast<char*>(ns::campaign.arms_[arms_].second.c_str());
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

tscenario::tscenario(const std::string& campaign_id, const std::string& id)
	: tscenario_(id)
	, side_()
	, campaign_id_(campaign_id)
	, index_(0)
	, dirty_(0)
{
}

tscenario::~tscenario()
{
	event_.clear();
}

std::string tscenario::file(bool absolute) const
{
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\campaigns\\";
		strstr << campaign_id_ << "\\scenarios\\";
		strstr << std::setw(2) << std::setfill('0') << index_ + 1 << "_";
		strstr << id_ << ".cfg";
	} else {
		strstr << "campaigns/";
		strstr << campaign_id_ << "/scenarios/";
		strstr << std::setw(2) << std::setfill('0') << index_ + 1 << "_";
		strstr << id_ << ".cfg";
	}
	return strstr.str();
}

std::string tscenario::map_file(bool absolute) const
{ 
	std::stringstream strstr;
	if (absolute) {
		strstr << game_config::path << "\\data\\campaigns\\";
		strstr << campaign_id_ << "\\maps\\";
		strstr << std::setw(2) << std::setfill('0') << index_ + 1 << "_";
		strstr << id_ << ".map";
	} else {
		strstr << "campaigns/";
		strstr << campaign_id_ << "/maps/";
		strstr << std::setw(2) << std::setfill('0') << index_ + 1 << "_";
		strstr << id_ << ".map";
	}
	return strstr.str();
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
		if (it->controller_ == tside::EMPTY) {
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

	// map_data_ = scenario_cfg["map_data"].str();
	map_data_ = map_data_from_file(map_file(true));

	next_scenario_ = scenario_cfg["next_scenario"].str();
	turns_ = scenario_cfg["turns"].to_int();
	maximal_defeated_activity_ = scenario_cfg["maximal_defeated_activity"].to_int();

	const std::vector<std::string> vstr = utils::split(scenario_cfg["treasures"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int t = lexical_cast_default<int>(*it, -1);
		std::map<int, int>::iterator find = treasures_.find(t);
		if (find == treasures_.end()) {
			treasures_[t] = 1;
		} else {
			find->second ++;
		}
	}

	foreach (const config &cfg, scenario_cfg.child_range("event")) {
		if (cfg["name"].str() != "prestart") continue;
		const config& objectives_cfg = cfg.child("objectives");
		if (!objectives_cfg) continue;

		const config& win_cfg = objectives_cfg.find_child("objective", "condition", "win");
		std::vector<t_string_base::trans_str> trans = win_cfg["description"].t_str().valuex();
		for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
			// only support one textdomain
			win_ = ti->str;
			break;
		}
		const config& lose_cfg = objectives_cfg.find_child("objective", "condition", "lose");
		trans = lose_cfg["description"].t_str().valuex();
		for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
			// only support one textdomain
			lose_ = ti->str;
			break;
		}
	}

	foreach (const config &side_cfg, scenario_cfg.child_range("side")) {
		side_.push_back(tside());
		tside& s = side_.back();

		s.from_config(side_cfg);
	}

	// form cityno_map need ns::cityno_map
	ns::cityno_map = generate_cityno_map();

	foreach (const config &event_cfg, scenario_cfg.child_range("event")) {
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
	side_.push_back(tside());
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

	update_to_ui_treasures(hdlgP);

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

	const treasure_map& treasures = unit_types.treasures();
	HWND hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE);
	ListView_DeleteAllItems(hctl);

	int index = 0;
	for (std::map<int, int>::const_iterator it = treasures_.begin(); it != treasures_.end(); ++ it, index ++) {
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
		strstr.str("");
		strstr << utf8_2_ansi(hero::treasure_str(it->first).c_str());
		strcpy(text, strstr.str().c_str());
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
		const std::vector<int>& features = treasures.find(it->first)->second;
		for (std::vector<int>::const_iterator it2 = features.begin(); it2 != features.end(); ++ it2) {
			if (it2 != features.begin()) {
				strstr << ", ";
			}
			strstr << utf8_2_ansi(hero::feature_str(*it2).c_str());
		}
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
	maximal_defeated_activity_ = UpDown_GetPos(GetDlgItem(hdlgP, IDC_UD_CAMPSCENARIO_MDACTIVITY));

	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_WIN_MSGID), text, sizeof(text) / sizeof(text[0]));
	win_ = text;
	Edit_GetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_LOSE_MSGID), text, sizeof(text) / sizeof(text[0]));
	lose_ = text;
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
	posix_fopen(map_file(true).c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp_map);
	if (fp_map == INVALID_FILE) {
		posix_fclose(fp);
		return;
	}

	//
	// generate cityno
	//
	// number-->cityno
	ns::cityno_map = generate_cityno_map();

	strstr << "[scenario]\n";
	strstr << "\tid = " << id_ << "\n";
	strstr << "\tnext_scenario = " << next_scenario_ << "\n";
	strstr << "\tname = _ \"" << id_ << "\"\n";
	strstr << "\tmap_data = \"{" << map_file() << "}\"\n";
	strstr << "\tturns = " << turns_ << "\n";
	strstr << "\tmaximal_defeated_activity = " << maximal_defeated_activity_ << "\n";
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
	// win/lose
	strstr << "\t[event]\n";
	strstr << "\t\tname = prestart\n";
	strstr << "\t\t[objectives]\n";
	strstr << "\t\t\t[objective]\n";
	strstr << "\t\t\t\tdescription = _ \"" << win_ << "\"\n";
	strstr << "\t\t\t\tcondition = win\n";
	strstr << "\t\t\t[/objective]\n";
	strstr << "\t\t\t[objective]\n";
	strstr << "\t\t\t\tdescription = _ \"" << lose_ << "\"\n";
	strstr << "\t\t\t\tcondition = lose\n";
	strstr << "\t\t\t[/objective]\n";
	strstr << "\t\t[/objectives]\n";
	strstr << "\t[/event]\n";

	strstr << "\n";
	for (std::vector<tside>::const_iterator it = side_.begin(); it != side_.end(); ++ it) {
		strstr << "\t[side]\n";
		strstr << "\t\tside = " << it->side_ + 1 << "\n";
		strstr << "\t\tleader = " << it->leader_ << "\n";
		strstr << "\t\tnavigation = " << it->navigation_ << "\n";
		strstr << "\t\tbuild = ";
		for (std::set<std::string>::const_iterator it2 = it->build_.begin(); it2 != it->build_.end(); ++ it2) {
			if (it2 == it->build_.begin()) {
				strstr << *it2;
			} else {
				strstr << ", " << *it2;
			}
		}
		strstr << "\n";

		if (it->controller_ != tside::EMPTY) { 
			// PLAYER_IF
			strstr << "\n";
			strstr << "\t\t{PLAYER_IF " << it->leader_ << "}\n";
			strstr << "\t\t\tshroud = $player.shroud\n";
			strstr << "\t\t\tfog = $player.fog\n";
			strstr << "\t\t\tcandidate_cards = $player.candidate_cards\n";
			strstr << "\t\t\tholded_cards = $player.holded_cards\n";
			strstr << "\t\t\tcontroller = human\n";
			strstr << "\t\t\tgold = 100\n";
			strstr << "\t\t\tincome = 0\n";
			strstr << "\t\t\tfeature = " << it->generate_features(tside::HUMAN) << "\n";
			strstr << "\t\t{PLAYER_ELSE}\n";
			strstr << "\t\t\tcontroller = ai\n";
			strstr << "\t\t\tgold = " << it->gold_ << "\n";
			strstr << "\t\t\tincome = " << it->income_ << "\n";
			strstr << "\t\t\tfeature = " << it->generate_features() << "\n";
			strstr << "\t\t\ttechnologies = ";
			for (std::set<std::string>::const_iterator it2 = it->technologies_.begin(); it2 != it->technologies_.end(); ++ it2) {
				if (it2 == it->technologies_.begin()) {
					strstr << *it2;
				} else {
					strstr << ", " << *it2;
				}
			}
			strstr << "\n";
			strstr << "\t\t{PLAYER_ENDIF_ELSE}\n";
		} else {
			strstr << "\t\tcontroller = null\n";
			strstr << "\t\tgold = " << it->gold_ << "\n";
			strstr << "\t\tincome = " << it->income_ << "\n";
			strstr << "\t\tfeature = " << it->generate_features() << "\n";
		}

		// city
		strstr << "\n";
		for (std::vector<tside::tcity>::const_iterator it2 = it->cities_.begin(); it2 != it->cities_.end(); ++ it2) {
			strstr << "\t\t{ANONYMITY_CITY ";
			strstr << ns::cityno_map.find(it2->heros_army_[0])->second << " ";
			strstr << it->side_ + 1 << " ";
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
			strstr << "\t\t[+artifical]\n";
			strstr << "\t\t\tmayor = ";
			if (it2->mayor_ != HEROS_INVALID_NUMBER) {
				strstr << it2->mayor_;
			} 
			strstr << "\n";
			strstr << "\t\t\tservice_heros = ";
			for (std::set<int>::const_iterator it3 = it2->service_heros_.begin(); it3 != it2->service_heros_.end(); ++ it3) {
				if (it3 == it2->service_heros_.begin()) {
					strstr << *it3;
				} else {
					strstr << ", " << *it3;
				}
			}
			strstr << "\n";
			strstr << "\t\t\twander_heros = ";
			for (std::set<int>::const_iterator it3 = it2->wander_heros_.begin(); it3 != it2->wander_heros_.end(); ++ it3) {
				if (it3 == it2->wander_heros_.begin()) {
					strstr << *it3;
				} else {
					strstr << ", " << *it3;
				}
			}
			strstr << "\n";
			strstr << "\t\t\teconomy_area = ";
			for (std::set<map_location>::const_iterator it3 = it2->economy_area_.begin(); it3 != it2->economy_area_.end(); ++ it3) {
				if (it3 == it2->economy_area_.begin()) {
					strstr << "(" << it3->x << ", " << it3->y << ")";
				} else {
					strstr << ", " << "(" << it3->x << ", " << it3->y << ")";
				}
			}
			strstr << "\n";
			if (it2->character_ != NO_CHARACTER) {
				strstr << "\t\t\tcharacter = " << unit_types.character(it2->character_).id_ << "\n";
			}
			strstr << "\t\t\tnot_recruit = ";
			for (std::vector<std::string>::const_iterator it3 = it2->not_recruit_.begin(); it3 != it2->not_recruit_.end(); ++ it3) {
				if (it3 == it2->not_recruit_.begin()) {
					strstr << *it3;
				} else {
					strstr << ", " << *it3;
				}
			}
			strstr << "\n";
			strstr << "\t\t[/artifical]\n";
		}

		// troop
		strstr << "\n";
		for (std::vector<tside::tunit>::const_iterator it2 = it->troops_.begin(); it2 != it->troops_.end(); ++ it2) {
			strstr << "\t\t{ANONYMITY_UNIT ";
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
			if (it2->character_ != NO_CHARACTER) {
				strstr << "(" << unit_types.character(it2->character_).id_ << ")";
			} else {
				strstr << "()";
			}
			strstr << "}\n";
		}

		strstr << "\t[/side]\n";
		strstr << "\n";
	}
/*
	strstr << "\n";
	// recommend
	strstr << "\t[event]\n";
	strstr << "\t\tname = new turn\n";
	strstr << "\t\tfirst_time_only = no\n";
	strstr << "\t\t[recommend]\n";
	strstr << "\t\t[/recommend]\n";
	strstr << "\t\t[ai]\n";
	strstr << "\t\t\tselfish = no\n";
	strstr << "\t\t[/ai]\n";
	strstr << "\t[/event]\n";

	strstr << "\n";
	// diplomatism
	strstr << "\t[event]\n";
	strstr << "\t\tname = diplomatism\n";
	strstr << "\t\tfirst_time_only = no\n";
	strstr << "\t\t[ally]\n";
	strstr << "\t\t[/ally]\n";
	strstr << "\t[/event]\n";
*/
	strstr << "\n";

	for (std::vector<tevent>::const_iterator it = event_.begin(); it != event_.end(); ++ it) {
		if (it->is_null()) {
			continue;
		}
		it->generate(strstr, "\t");
		strstr << "\n";
	}
	strstr << "[/scenario]\n";

	posix_fwrite(fp, strstr.str().c_str(), strstr.str().length(), bytertd);
	posix_fclose(fp);

	posix_fwrite(fp_map, map_data_.c_str(), map_data_.length(), bytertd);
	posix_fclose(fp_map);

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

void OnSaveBt(HWND hdlgP)
{
	std::stringstream strstr;
	// verify _main/_scenario
	for (std::vector<tscenario>::const_iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it) {
		if (it->map_data_.empty()) {
			strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), it->id_.c_str())) << "，关卡没设置有效地图";
			posix_print_mb(strstr.str().c_str());
			return;
		}
		int i2 = 1;
		for (std::vector<tside>::const_iterator it2 = it->side_.begin(); it2 != it->side_.end(); ++ it2, i2 ++) {
			if (it2->leader_ == HEROS_INVALID_NUMBER) {
				strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), it->id_.c_str())) << "关卡的第" << i2 << "势力没有设置君主";
				posix_print_mb(strstr.str().c_str());
				return;
			}
			if (it2->cities_.empty() && it2->troops_.empty()) {
				strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), it->id_.c_str())) << "关卡的第" << i2 << "势力既没有城市也没有部队";
				posix_print_mb(strstr.str().c_str());
				return;
			}
		}
	}
	

	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hdlgP, GWL_USERDATA); 
    int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 

	// one or more scenario.id changed, empty <campaign>/scenarios directory.
	std::string scenarios_dir = game_config::path + "\\data\\campaigns\\" + ns::_main.id_ + "\\scenarios\\";
	int index = 0;
	for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it, index ++) {
		if (it->scenario_from_cfg_.id_ != it->id_) {
			strstr.str("");
			strstr << scenarios_dir;
			strstr << std::setw(2) << std::setfill('0') << it->index_ + 1 << "_";
			strstr << it->scenario_from_cfg_.id_ << ".cfg";

			delfile1(strstr.str().c_str());
		}
	}

	if (ns::_main.dirty_) {
		if (iSel == 0) {
			ns::_main.from_ui(pHdr->hwndDisplay);
		}
		ns::_main.generate();
	}
	index = 0;
	for (std::vector<tscenario>::iterator it = ns::_scenario.begin(); it != ns::_scenario.end(); ++ it, index ++) {
		if (it->dirty_) {
			if (iSel == index + 1) {
				it->from_ui(pHdr->hwndDisplay);
			}
			it->generate();
		}
	}
	campaign_enable_save_btn(FALSE);

	editor_config::campaign_id = ns::_main.id_;
	do_sync2();
}

static bool save_if_dirty()
{
	if (ToolBar_GetState(gdmgr._htb_campaign, IDM_SAVE) & TBSTATE_ENABLED) {
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
			OnSaveBt(gdmgr._hdlg_campaign);
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
	case IDM_SAVE:
		OnSaveBt(hdlgP);
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

BOOL CALLBACK DlgCampaignProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPPSHNOTIFY			lppsn = (LPPSHNOTIFY)lParam;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		return On_DlgCampaignInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgCampaignCommand);
	HANDLE_MSG(hdlgP, WM_DESTROY, On_DlgCampaignDestroy);
	HANDLE_MSG(hdlgP, WM_NOTIFY, On_DlgCampaignNotify);
	}
	
	return FALSE;
} 

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

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FILE), utf8_2_ansi(_("Corresponding cfg")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ID), utf8_2_ansi(_("ID")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DOMAIN), utf8_2_ansi(_("Domain")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_NAME), utf8_2_ansi(_("Name")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_ABBREVIATION), utf8_2_ansi(_("Abbreviation")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_RANK), utf8_2_ansi(_("Rank")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FIRSTSCENARIO), utf8_2_ansi(_("First scenario")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_IMAGE), utf8_2_ansi(_("Image")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_DESCRIPTION), utf8_2_ansi(_("Description")));
	Button_SetText(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_RPGMODE), utf8_2_ansi(_("RPG mode")));

	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPMAIN_BROWSEICON), utf8_2_ansi(_("Browse...")));
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPMAIN_BROWSEIMAGE), utf8_2_ansi(_("Browse...")));
	
	std::stringstream strstr;
	HWND hctl = GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_FILE);
	Edit_SetText(hctl, ns::_main.file_.c_str());

	hctl = GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_RANK);
	strstr.str("");
	strstr << utf8_2_ansi(_("Tutorial")) << "[0, 99]";
	ComboBox_AddString(hctl, strstr.str().c_str());
	ComboBox_AddString(hctl, "SLG[100, 199]");
	strstr.str("");
	strstr << utf8_2_ansi(_("Multi-scenario")) << "[200, 299]";
	ComboBox_AddString(hctl, strstr.str().c_str());
	strstr.str("");
	strstr << utf8_2_ansi(_("Test")) << "[300, ---)";
	ComboBox_AddString(hctl, strstr.str().c_str());
	ComboBox_SetItemData(hctl, 0, 0);
	ComboBox_SetItemData(hctl, 1, 100);
	ComboBox_SetItemData(hctl, 2, 200);
	ComboBox_SetItemData(hctl, 3, 300);

	std::string str;
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_ID), ns::_main.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPMAIN_TEXTDOMAIN), ns::_main.textdomain_.c_str());
	select_rank_cmb(GetDlgItem(hdlgP, IDC_CMB_CAMPMAIN_RANK), ns::_main.rank_);
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
	Button_SetCheck(GetDlgItem(hdlgP, IDC_CHK_CAMPMAIN_RPGMODE), ns::_main.rpg_mode_);

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

void OnCampaignMainChk(HWND hdlgP, int id, UINT codeNotify)
{
	bool val, that;
	int bit;

	HWND hctl = GetDlgItem(hdlgP, id);

	if (id == IDC_CHK_CAMPMAIN_RPGMODE) {
		bit = tmain::BIT_RPGMODE;
		that = ns::_main.main_from_cfg_.rpg_mode_;
	} else {
		return;
	}

	if (codeNotify == BN_CLICKED) {
		val = Button_GetCheck(hctl);
		ns::_main.set_dirty(bit, val != that);
	}
	return;
}

void OnCampaignMainCmb(HWND hdlgP, int id, UINT codeNotify)
{
	int val, that;
	int index, bit;

	HWND hctl = GetDlgItem(hdlgP, id);

	if (id == IDC_CMB_CAMPMAIN_RANK) {
		bit = tmain::BIT_RANK;
		that = ns::_main.main_from_cfg_.rank_;
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
	case IDC_CHK_CAMPMAIN_RPGMODE:
		OnCampaignMainChk(hdlgP, id ,codeNotify);
		break;
	case IDC_CMB_CAMPMAIN_RANK:
		OnCampaignMainCmb(hdlgP, id, codeNotify);
		break;
	case IDC_BT_CAMPMAIN_BROWSEICON:
	case IDC_BT_CAMPMAIN_BROWSEIMAGE:
		OnCampaignMainBt(hdlgP, id, codeNotify);
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
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_FEATURE), dgettext_2_ansi("wesnoth-hero", "feature"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TECHNOLOGY), dgettext_2_ansi("wesnoth-lib", "Technology"));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CITY), utf8_2_ansi(_("City")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TROOP), utf8_2_ansi(_("Troop")));

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
	if (side.controller_ != tside::EMPTY && scenario.null_side() != -1) {
		Button_Enable(hctl, FALSE);
	}

	hctl = GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_GOLD);
	UpDown_SetRange(hctl, 0, 1000);	// [0, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_GOLD));

	hctl = GetDlgItem(hdlgP, IDC_UD_SIDEEDIT_INCOME);
	UpDown_SetRange(hctl, 0, 1000);	// [0, 1000]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_SIDEEDIT_INCOME));

	hctl = GetDlgItem(hdlgP, IDC_CMB_SIDEEDIT_NAVIGATION);
	int index = 0;
	for (std::vector<std::string>::const_iterator it = ns::campaign.navigation_.begin(); it != ns::campaign.navigation_.end(); ++ it) {
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

	// technology
	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_TECHNOLOGY);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 100;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("Description")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

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

	hctl = GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_CITY);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("arms^Type")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 58;
	lvc.iSubItem = 2;
	strcpy(text, utf8_2_ansi(_("Coordinate")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 70;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Traits"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Character"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 100;
	lvc.iSubItem = 5;
	strcpy(text, utf8_2_ansi(_("Cannot recruit")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 5, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 6;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Mayor"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 6, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 7;
	strcpy(text, utf8_2_ansi(_("Office")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 7, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 8;
	strcpy(text, utf8_2_ansi(_("Wander")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 8, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 9;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "economy area"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 9, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

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

BOOL On_DlgFeatureEditInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
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
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = ns::campaign.arms_.begin(); it != ns::campaign.arms_.end(); ++ it) {
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

void On_DlgFeatureEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
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

BOOL CALLBACK DlgFeatureEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
	case WM_INITDIALOG:
		return On_DlgFeatureEditInitDialog(hDlg, (HWND)(wParam), lParam);
	HANDLE_MSG(hDlg, WM_COMMAND, On_DlgFeatureEditCommand);
	HANDLE_MSG(hDlg, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	}
	
	return FALSE;
}

void OnFeatureAddBt(HWND hdlgP)
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
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FEATUREEDIT), hdlgP, DlgFeatureEditProc, lParam)) {
		side.features_[ns::clicked_feature].update_to_ui_side_edit(hdlgP, ns::clicked_feature);
	} else {
		side.erase_feature(side.features_.size() - 1, hdlgP);
	}

	return;
}

void OnFeatureEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	ns::action_feature = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_FEATUREEDIT), hdlgP, DlgFeatureEditProc, lParam)) {
		side.features_[ns::clicked_feature].update_to_ui_side_edit(hdlgP, ns::clicked_feature);
	}

	return;
}

void OnFeatureDelBt(HWND hdlgP)
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
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOSERVICE, "到在职");
	AppendMenu(gdmgr._hpopup_candidate, MF_STRING, IDM_TOWANDER, "到在野");

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	HWND hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_X);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_X));

	hctl = GetDlgItem(hdlgP, IDC_UD_CITYEDIT_Y);
	UpDown_SetRange(hctl, 1, 300);	// [1, 300]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CITYEDIT_Y));

	LVCOLUMN lvc;
	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_TRAIT);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
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

	hctl = GetDlgItem(hdlgP, IDC_LV_CITYEDIT_CANDIDATE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
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

	lvc.cx = 40;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "side feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

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

void On_DlgCityEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tcity& city = side.cities_[ns::clicked_city];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_TOSERVICE:
	case IDM_TOWANDER:
		if (id == IDM_TOSERVICE) {
			city.service_heros_.insert(ns::clicked_hero);
			ns::campaign.do_state(ns::clicked_hero, true, city.heros_army_[0], tcampaign::STATE_SERVICE);
		} else {
			city.wander_heros_.insert(ns::clicked_hero);
			ns::campaign.do_state(ns::clicked_hero, true, city.heros_army_[0], tcampaign::STATE_WANDER);
		}
		city.update_to_ui_city_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_CITYEDIT_SERVICE) {
			city.service_heros_.erase(ns::clicked_hero);
			ns::campaign.do_state(ns::clicked_hero, false);
		} else if (ns::type == IDC_LV_CITYEDIT_WANDER) {
			city.wander_heros_.erase(ns::clicked_hero);
			ns::campaign.do_state(ns::clicked_hero, false);
		} else if (ns::type == IDC_LV_CITYEDIT_EA) {
			city.economy_area_.erase(map_location(LOWORD(ns::clicked_hero), HIWORD(ns::clicked_hero)));
		}
		city.update_to_ui_city_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM1:
		if (ns::type == IDC_LV_CITYEDIT_SERVICE) {
			for (std::set<int>::const_iterator it = city.service_heros_.begin(); it != city.service_heros_.end(); ++ it) {
				ns::campaign.do_state(*it, false);
			}
			city.service_heros_.clear();
		} else if (ns::type == IDC_LV_CITYEDIT_WANDER) {
			for (std::set<int>::const_iterator it = city.wander_heros_.begin(); it != city.wander_heros_.end(); ++ it) {
				ns::campaign.do_state(*it, false);
			}
			city.wander_heros_.clear();
		} else if (ns::type == IDC_LV_CITYEDIT_EA) {
			city.economy_area_.clear();
		}
		city.update_to_ui_city_edit(hdlgP, side);
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
	int						icount;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}
	if (lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_CANDIDATE) &&
		lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_CITYEDIT_SERVICE) &&
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

	icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_CITYEDIT_CANDIDATE)) {
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSERVICE, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOWANDER, MF_BYCOMMAND | MF_GRAYED);
		}		

		TrackPopupMenuEx(gdmgr._hpopup_candidate, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSERVICE, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOWANDER, MF_BYCOMMAND | MF_ENABLED);
	} else {
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
	}
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
	ns::campaign.do_state(number, false);

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
		cityedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);

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
		side.update_to_ui_side_edit(hdlgP);
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
		side.update_to_ui_side_edit(hdlgP);
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
	lvc.cx = 60;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
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

	hctl = GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_CANDIDATE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 60;
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

	lvc.cx = 40;
	lvc.iSubItem = 3;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "side feature"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-hero", "leadership"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	// ListView_SetImageList(hctl, gdmgr._himl, LVSIL_SMALL);
	ListView_SetImageList(hctl, NULL, LVSIL_SMALL);

	// 默认情况下，鼠标右键只是光亮该行的最前一个子项，并且只有在该子项上才能触发NM_RCLICK。改为光亮整行，并且在整行都能触发NM_RCLICK。
	ListView_SetExtendedListViewStyleEx(hctl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

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
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_TOMASTER:
	case IDM_TOSECOND:
	case IDM_TOTHIRD:
		if (id == IDM_TOMASTER) {
			ns::campaign.do_state(troop.heros_army_[0], false);
			troop.heros_army_[0] = ns::clicked_hero;
			ns::campaign.do_state(ns::clicked_hero, true, troop.city_, tcampaign::STATE_SERVICE);
		} else if (id == IDM_TOSECOND) {
			if (troop.heros_army_.size() == 1) {
				troop.heros_army_.push_back(ns::clicked_hero);
			} else {
				ns::campaign.do_state(troop.heros_army_[1], false);
				troop.heros_army_[1] = ns::clicked_hero;
			}
			ns::campaign.do_state(ns::clicked_hero, true, troop.city_, tcampaign::STATE_WANDER);
		} else if (id == IDM_TOTHIRD) {
			if (troop.heros_army_.size() == 3) {
				ns::campaign.do_state(troop.heros_army_[2], false);
				troop.heros_army_[2] = ns::clicked_hero;
			} else {
				troop.heros_army_.push_back(ns::clicked_hero);
			}
			ns::campaign.do_state(ns::clicked_hero, true, troop.city_, tcampaign::STATE_WANDER);
		}
		troop.update_to_ui_troop_edit(hdlgP, side);
		break;
	case IDM_DELETE_ITEM0:
		if (ns::type == IDC_LV_TROOPEDIT_HERO) {
			std::vector<int>::iterator it = std::find(troop.heros_army_.begin(), troop.heros_army_.end(), ns::clicked_hero);
			troop.heros_army_.erase(it);
			ns::campaign.do_state(ns::clicked_hero, false);
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
	int						icount;

	if (lpNMHdr->code != NM_RCLICK) {
		return;
	}
	if (lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_CANDIDATE) && 
		lpNMHdr->hwndFrom != GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_HERO)) {
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

	icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];
	tside::tunit& troop = side.troops_[ns::clicked_troop];

	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_TROOPEDIT_CANDIDATE)) {
		if (lpnmitem->iItem < 0) {
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOMASTER, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSECOND, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOTHIRD, MF_BYCOMMAND | MF_GRAYED);
		} else if (troop.heros_army_.size() < 2) {
			EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOTHIRD, MF_BYCOMMAND | MF_GRAYED);
		}

		TrackPopupMenuEx(gdmgr._hpopup_candidate, 0, 
			point.x, 
			point.y, 
			hdlgP, 
			NULL);

		// 恢复回去
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOMASTER, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOSECOND, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(gdmgr._hpopup_candidate, IDM_TOTHIRD, MF_BYCOMMAND | MF_ENABLED);
	} else {
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
	}
	ns::type = id;
	ns::clicked_hero = lvi.lParam;

    return;
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
		troopedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);

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
		side.update_to_ui_side_edit(hdlgP);
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
		side.update_to_ui_side_edit(hdlgP);
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
	int						icount;

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

		icount = ListView_GetItemCount(lpNMHdr->hwndFrom);

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

	// NM_表示对通用控件都通用,范围丛(0, 99)
	// TVN_表示只能TreeView通用,范围丛(400, 499)
	if (lpNMHdr->hwndFrom == GetDlgItem(hdlgP, IDC_LV_SIDEEDIT_FEATURE) || 
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
			OnFeatureEditBt(hdlgP);
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

void On_DlgSideEditCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];
	tside& side = scenario.side_[ns::clicked_side];

	BOOL changed = FALSE;

	switch (id) {
	case IDM_ADD:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeatureAddBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityAddBt(hdlgP);
		} else {
			OnTroopAddBt(hdlgP);
		}
		break;
	case IDM_DELETE:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeatureDelBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityDelBt(hdlgP);
		} else {
			OnTroopDelBt(hdlgP);
		}
		break;
	case IDM_EDIT:
		if (ns::type == IDC_LV_SIDEEDIT_FEATURE) {
			OnFeatureEditBt(hdlgP);
		} else if (ns::type == IDC_LV_SIDEEDIT_CITY) {
			OnCityEditBt(hdlgP);
		} else {
			OnTroopEditBt(hdlgP);
		}
		break;

	case IDC_ET_SIDEEDIT_NAME_MSGID:
		OnSideEditEt(hdlgP, id, codeNotify);
		break;
	case IDC_CMB_SIDEEDIT_LEADER:
		OnLeaderCmb(hdlgP, id, codeNotify);
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
		if ((lpNMHdr->idFrom == IDC_LV_SIDEEDIT_BUILD || lpNMHdr->idFrom == IDC_LV_SIDEEDIT_TECHNOLOGY) && (lpnmitem->ptAction.x <= 14)) {
			if (ListView_GetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem)) {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, FALSE);
			} else {
				ListView_SetCheckState(lpNMHdr->hwndFrom, lpnmitem->iItem, TRUE);
			}
			strstr.str("");
			strstr << dgettext_2_ansi("wesnoth-lib", "Technology") << "(" << editor_config::ListView_GetCheckedCount(lpNMHdr->hwndFrom) << ")";
			Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TECHNOLOGY), strstr.str().c_str());
		}

	} else if (lpNMHdr->code == NM_RCLICK) {
		sideedit_notify_handler_rclick(hdlgP, DlgItem, lpNMHdr);

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
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_MDACTIVITY), utf8_2_ansi(_("Maximal defeated activity")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_WIN), utf8_2_ansi(_("Win condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_LOSE), utf8_2_ansi(_("Lose condition")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_TREASURE), utf8_2_ansi(_("Hidden treasure")));
	Button_SetText(GetDlgItem(hdlgP, IDC_BT_CAMPSCENARIO_BROWSEMAP), utf8_2_ansi(_("Browse...")));

	// int index = TabCtrl_GetCurSel(pHdr->hwndTab) - 1;
	ns::current_scenario = TabCtrl_GetCurSel(pHdr->hwndTab) - 1;
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	HWND hctl = GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_FILE);
	Edit_SetText(hctl, scenario.file(true).c_str());

	//id
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_ID), scenario.id_.c_str());
	// name
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME_MSGID), scenario.id_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_NAME), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), scenario.id_.c_str())));

	strstr.str("");
	strstr << "(" << scenario.map_file() << ")";
	if (scenario.map_data_size(scenario.map_data_).valid()) {
		strstr << utf8_2_ansi(_("Exist"));
	} else {
		strstr << utf8_2_ansi(_("Not exist"));
	}
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MAP), strstr.str().c_str());

	// turns
	hctl = GetDlgItem(hdlgP, IDC_UD_CAMPSCENARIO_TURNS);
	UpDown_SetRange(hctl, -1, 100);	// [-1, 100]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_TURNS));
	UpDown_SetPos(hctl, scenario.turns_);
	// maximal_defeated_activity
	hctl = GetDlgItem(hdlgP, IDC_UD_CAMPSCENARIO_MDACTIVITY);
	UpDown_SetRange(hctl, 0, 200);	// [0, 200]
	UpDown_SetBuddy(hctl, GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_MDACTIVITY));
	UpDown_SetPos(hctl, scenario.maximal_defeated_activity_);
	// win/lose
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_WIN_MSGID), scenario.win_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_WIN), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), scenario.win_.c_str())));
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_LOSE_MSGID), scenario.lose_.c_str());
	Edit_SetText(GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_LOSE), utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), scenario.lose_.c_str())));

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
	lvc.cx = 60;
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

	hctl = GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_SIDE);
	lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 40;
	strcpy(text, utf8_2_ansi(_("Number")));
	lvc.pszText = text;
	lvc.cchTextMax = 0;
	lvc.iSubItem = 0;
	ListView_InsertColumn(hctl, 0, &lvc);

	lvc.mask= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 60;
	lvc.iSubItem = 1;
	strcpy(text, utf8_2_ansi(_("Name")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 1, &lvc);

	lvc.cx = 48;
	lvc.iSubItem = 2;
	strcpy(text, utf8_2_ansi(_("Leader")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 2, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 3;
	strcpy(text, utf8_2_ansi(_("Void")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 3, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 4;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Fog"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 4, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 5;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Card"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 5, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 6;
	strcpy(text, utf8_2_ansi(_("Base gold")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 6, &lvc);

	lvc.cx = 54;
	lvc.iSubItem = 7;
	strcpy(text, utf8_2_ansi(_("Base income")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 7, &lvc);

	lvc.cx = 60;
	lvc.iSubItem = 8;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Navigation civilization"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 8, &lvc);

	lvc.cx = 40;
	lvc.iSubItem = 9;
	strcpy(text, dgettext_2_ansi("wesnoth-lib", "Character"));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 9, &lvc);

	lvc.cx = 100;
	lvc.iSubItem = 10;
	strcpy(text, utf8_2_ansi(_("Artifical")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 10, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = 11;
	strcpy(text, utf8_2_ansi(_("City")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 11, &lvc);

	lvc.cx = 80;
	lvc.iSubItem = 12;
	strcpy(text, utf8_2_ansi(_("Troop")));
	lvc.pszText = text;
	ListView_InsertColumn(hctl, 12, &lvc);

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
	lvc.cx = 70;
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
	} else if (id == IDC_ET_CAMPSCENARIO_MDACTIVITY) {
		bit = tscenario::BIT_MDACTIVITY;
		strstr << scenario.scenario_from_cfg_.maximal_defeated_activity_;
		that = strstr.str();
	} else {
		return;
	}

	Edit_GetText(hctl, text, sizeof(text) / sizeof(text[0]));
	scenario.set_dirty(bit, strcmp(text, that.c_str()));

	return;
}

void OnCampaignScenarioEt2(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	char text[_MAX_PATH];
	HWND hctl;
	std::stringstream strstr;
	std::string that;
	int bit;

	if (codeNotify != EN_CHANGE) {
		return;
	}

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	Edit_GetText(hwndCtrl, text, sizeof(text) / sizeof(text[0]));
	if (id == IDC_ET_CAMPSCENARIO_WIN_MSGID) {
		bit = tscenario::BIT_WIN;
		that = scenario.scenario_from_cfg_.win_;
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_WIN);
	} else if (id == IDC_ET_CAMPSCENARIO_LOSE_MSGID) {
		bit = tscenario::BIT_LOSE;
		that = scenario.scenario_from_cfg_.lose_;
		hctl = GetDlgItem(hdlgP, IDC_ET_CAMPSCENARIO_LOSE);
	} else {
		return;
	}
	strstr.str("");
	if (text[0]) {
		strstr << utf8_2_ansi(dgettext(ns::_main.textdomain_.c_str(), text));
	}
	Edit_SetText(hctl, strstr.str().c_str());
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

	if (id == IDC_CMB_CAMPSCENARIO_NEXTSCENARIO) {
		bit = tscenario::BIT_NEXTSCENARIO;
		that = scenario.scenario_from_cfg_.next_scenario_;
	} else {
		return;
	}

	ComboBox_GetLBText(hwndCtrl, ComboBox_GetCurSel(hwndCtrl), text);
	scenario.set_dirty(bit, strcmp(text, that.c_str()));

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
		scenario.side_[ns::clicked_side].update_to_ui(hdlgP);
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
		side.update_to_ui(hdlgP);
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

BOOL On_DlgTreasureInitDialog(HWND hdlgP, HWND hwndFocus, LPARAM lParam)
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
	const treasure_map& treasures = unit_types.treasures();
	int selected = 0, index = 0;
	for (treasure_map::const_iterator it = treasures.begin(); it != treasures.end(); ++ it, index ++) {
		strstr.str("");
		strstr << utf8_2_ansi(hero::treasure_str(it->first).c_str());
		strstr << "(";
		const std::vector<int>& features = treasures.find(it->first)->second;
		for (std::vector<int>::const_iterator it2 = features.begin(); it2 != features.end(); ++ it2) {
			if (it2 != features.begin()) {
				strstr << ", ";
			}
			strstr << utf8_2_ansi(hero::feature_str(*it2).c_str());
		}
		strstr << ")";
		ComboBox_AddString(hctl, strstr.str().c_str());
		ComboBox_SetItemData(hctl, ComboBox_GetCount(hctl) - 1, it->first);
		if (ns::action_treasure == ma_edit) {
			if (it->first == ns::clicked_treasure) {
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

void On_DlgTreasureCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
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

BOOL CALLBACK DlgTreasureProc(HWND hdlgP, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_INITDIALOG:
		return On_DlgTreasureInitDialog(hdlgP, (HWND)(wParam), lParam);
	HANDLE_MSG(hdlgP, WM_COMMAND, On_DlgTreasureCommand);
	HANDLE_MSG(hdlgP, WM_DRAWITEM, editor_config::On_DlgDrawItem);
	// HANDLE_MSG(hdlgP, WM_DESTROY,  On_DlgTreasureDestroy);
	}	
	return FALSE;
}

void OnTreasureAddBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_treasure = ma_new;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TREASURE), hdlgP, DlgTreasureProc, lParam)) {
		scenario.update_to_ui_treasures(hdlgP);
		scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	}

	return;
}

void OnTreasureEditBt(HWND hdlgP)
{
	RECT		rcBtn;
	LPARAM		lParam;
	
	GetWindowRect(GetDlgItem(hdlgP, IDC_LV_CAMPSCENARIO_TREASURE), &rcBtn);
	lParam = posix_mku32((rcBtn.left > 0)? rcBtn.left: rcBtn.right, rcBtn.top);

	tscenario& scenario = ns::_scenario[ns::current_scenario];

	ns::action_treasure = ma_edit;
	if (DialogBoxParam(gdmgr._hinst, MAKEINTRESOURCE(IDD_TREASURE), hdlgP, DlgTreasureProc, lParam)) {
		scenario.update_to_ui_treasures(hdlgP);
		scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	}

	return;
}

void OnTreasureDelBt(HWND hdlgP)
{
	tscenario& scenario = ns::_scenario[ns::current_scenario];

	std::map<int, int>& treasures = scenario.treasures_;
	treasures.erase(ns::clicked_treasure);

	scenario.update_to_ui_treasures(hdlgP);
	scenario.set_dirty(tscenario::BIT_TREASURES, scenario.scenario_from_cfg_.treasures_ != scenario.treasures_);
	return;
}

void On_DlgCampaignScenarioCommand(HWND hdlgP, int id, HWND hwndCtrl, UINT codeNotify)
{
	switch (id) {
	case IDC_ET_CAMPSCENARIO_TURNS:
	case IDC_ET_CAMPSCENARIO_MDACTIVITY:
		OnCampaignScenarioEt(hdlgP, id, codeNotify);
		break;

	case IDC_ET_CAMPSCENARIO_WIN_MSGID:
	case IDC_ET_CAMPSCENARIO_LOSE_MSGID:
		OnCampaignScenarioEt2(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_ET_CAMPSCENARIO_ID:
		OnCampaignScenarioEt3(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDC_CMB_CAMPSCENARIO_NEXTSCENARIO:
		OnCampaignScenarioCmb(hdlgP, id, hwndCtrl, codeNotify);
		break;

	case IDM_NEW_ITEM0:
	case IDM_NEW_ITEM1:
		OnEventAddBt(hdlgP, tevent::name_map[id - IDM_NEW_ITEM0].first);
		break;

	case IDM_ADD:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideAddBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnTreasureAddBt(hdlgP);
		}
		break;
	case IDM_DELETE:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideDelBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_EVENT) {
			OnEventDelBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnTreasureDelBt(hdlgP);
		}
		break;
	case IDM_EDIT:
		if (ns::type == IDC_LV_CAMPSCENARIO_SIDE) {
			OnSideEditBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_EVENT) {
			OnEventEditBt(hdlgP);
		} else if (ns::type == IDC_LV_CAMPSCENARIO_TREASURE) {
			OnTreasureEditBt(hdlgP);
		}
		break;

	case IDC_BT_CAMPSCENARIO_BROWSEMAP:
		OnCampaignScenarioBt(hdlgP, id, codeNotify);
		break;
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
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = tevent::name_map.begin(); it != tevent::name_map.end(); ++ it, index ++) {
			strstr.str("");
			strstr << it->first << "(" << it->second << ")";
			AppendMenu(hpopup_new, MF_STRING, IDM_NEW_ITEM0 + index, strstr.str().c_str());
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
				OnTreasureEditBt(hdlgP);
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
	wml_config_from_file(std::string(gdmgr.cfg_fname_), ns::campaign.game_config_);
	std::string id = offextname(basename(gdmgr.cfg_fname_));
	
	char text[_MAX_PATH];
	std::stringstream strstr;
	ns::campaign_cfg_ptr = NULL;
	foreach (const config &i, editor_config::data_cfg.child_range("campaign")) {
		if (i["id"] == id) {
			ns::campaign_cfg_ptr = &i;
			break;
		}
	}
	ns::_main.from_config(*ns::campaign_cfg_ptr);
	// read hero.dat
	std::string hero_filename = get_wml_location(ns::_main.hero_data_);
	gdmgr.heros_.map_from_file(hero_filename);
	ns::campaign.init_hero_state(gdmgr.heros_);

	ns::_scenario.clear();
	int index = 0;
	foreach (const config &i, ns::campaign.game_config_.child_range("scenario")) {
		ns::_scenario.push_back(tscenario(id));
		tscenario& s = ns::_scenario.back();
		s.from_config(index ++, i);
		
	}

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
	enum {BIT_ID, BIT_FIRSTSCENARIOID};
	int dirty_;
	std::string id_;
	std::string firstscenario_id_;

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
	ns_new::set_dirty(ns_new::BIT_FIRSTSCENARIOID, true);

	SetWindowText(hdlgP, utf8_2_ansi(_("New campaign")));

	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_CAMPAIGNID), utf8_2_ansi(_("Campaign ID(Once set, cannot modify in future)")));
	Static_SetText(GetDlgItem(hdlgP, IDC_STATIC_SCENARIOID), utf8_2_ansi(_("First scenario ID(May modify in future)")));

	Button_SetText(GetDlgItem(hdlgP, IDOK), utf8_2_ansi(_("New")));
	Button_SetText(GetDlgItem(hdlgP, IDCANCEL), utf8_2_ansi(_("Cancel")));

	Button_Enable(GetDlgItem(hdlgP, IDOK), FALSE);

	SetFocus(GetDlgItem(hdlgP, IDC_ET_NEWCAMP_ID));

	return FALSE;
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
			ns_new::set_dirty(ns_new::BIT_FIRSTSCENARIOID, true);
		}
	} else if (id == IDC_ET_NEWCAMP_ID) {
		const config& campaigns_cfg = editor_.campaigns_config();
		const config& campaign_cfg = campaigns_cfg.find_child("campaign", "id", str);
		if (!campaign_cfg) {
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
		strstr << utf8_2_ansi(_("scenario file:")) << " <campaign>/scenarios/01_" << str << ".cfg";
		strstr << "\r\n";
		strstr << utf8_2_ansi(_("map file:")) << " <campaign>/maps/01_" << str << ".map";
		ns_new::set_dirty(ns_new::BIT_FIRSTSCENARIOID, false);
		ns_new::firstscenario_id_ = str;
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
	ns::campaign.init_hero_state(gdmgr.heros_);

	_main = tmain(id, firstscenario_id);
	_main.rpg_mode_ = true;
	_scenario.clear();
	_scenario.push_back(tscenario(id, firstscenario_id));
	tscenario& scenario = _scenario.back();

	scenario.map_data_ = tscenario::default_map_data();
	scenario.win_ = "Defeat all sides";
	scenario.lose_ = "No city you holded";

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

}

bool campaign_new()
{
	int retval = DialogBox(gdmgr._hinst, MAKEINTRESOURCE(IDD_NEWCAMPAIGN), gdmgr._hdlg_campaign, DlgNewCampaignProc);
	if (!retval) return false;

	ns::new_campaign(ns_new::id_, ns_new::firstscenario_id_);
	sync_refresh_sync();

	return true;
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