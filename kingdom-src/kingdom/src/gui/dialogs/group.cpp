/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/group.hpp"

#include "filesystem.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/hero.hpp"
#include "gui/dialogs/player_city.hpp"
#include "gui/dialogs/personnel.hpp"
#include "gui/dialogs/exile.hpp"
#include "gui/dialogs/message.hpp"
#include <preferences.hpp>
#include "multiplayer.hpp"
#include <time.h>
#include "actions.hpp"

#include <boost/bind.hpp>

const char* interiors_png[] = {
	"misc/military.png",
	"misc/culture.png",
	"misc/economy.png",
	"misc/science.png"
};

std::string hero_level_str(int level)
{
	std::stringstream ss;

	if (level / game_config::levels_per_rank >= 2) {
		ss << _("rank^Gold");
	} else if (level / game_config::levels_per_rank >= 1) {
		ss << _("rank^Silver");
	} else {
		ss << _("rank^Copper");
	}
	ss << "(" << (level % game_config::levels_per_rank + 1) << ")";
	return ss.str();
}

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

REGISTER_DIALOG(group2)

tgroup2::tgroup2(display& disp, hero_map& heros, const config& game_config, tgroup& group, bool browse)
	: disp_(disp)
	, heros_(heros)
	, game_config_(game_config)
	, group_(group)
	, browse_(browse)
	, map_data_()
	, current_page_(NONE_PAGE)
	, page_panel_(NULL)
	, selected_number_(0)
	, associate_members_()
	, employees_()
{
}

static const std::string tag_color = "blue";

void tgroup2::sheet_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_page = toggle->get_data();

	if (!toggle->get_value()) {
		// At most select one page. recheck it!
		toggle->set_value(true);
	} else {
		for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
			if (it->second == toggle) {
				continue;
			}
			it->second->set_value(false);
		}
		swap_page(*toggle->get_window(), toggled_page, true);
	}
}

std::string tgroup2::get_selected_map_data() const
{
	if (verify_siege_map_data(game_config_, map_data_)) {
		return map_data_;
	}

	const config& map_scenario = game_config_.find_child("multiplayer", "id", game_config::campaign_id_siege);
	return map_scenario["map_data"].str();
}

void tgroup2::update_map(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "map_list", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("_toggle", true));
	int index = toggle->get_data();

	tbutton* edit = find_widget<tbutton>(&window, "edit_map", false, true);
	edit->set_active(index == 0);

	if (index == 0) {
		map_data_ = group_.map();
		if (!map_data_.empty() && !verify_siege_map_data(game_config_, map_data_)) {
			map_data_ = null_str;
		}
	} else {
		config::const_child_itors levels = game_config_.child_range("multiplayer");
		std::advance(levels.first, index - 1);

		const config& level = *levels.first;
		map_data_ = level["map_data"].str();
	}

	tminimap& minimap = find_widget<tminimap>(&window, "minimap", false);
	minimap.set_config(&game_config_);
	minimap.set_map_data(tminimap::TILE_MAP, map_data_);
}

void tgroup2::player_city(twindow& window)
{
	gui2::tplayer_city dlg(disp_, heros_);
	try {
		dlg.show(disp_.video());
	} catch(twml_exception& e) {
		e.show(disp_);
	}

	ttext_box* box = find_widget<ttext_box>(&window, "city", false, true);
	box->set_value(group_.city().name());
}

void tgroup2::layout(twindow& window)
{
	set_retval(window, LAYOUT);
}

void tgroup2::edit_map(twindow& window)
{
	set_retval(window, START_MAP_EDITOR);
}

void tgroup2::personnel(twindow& window)
{
	gui2::tpersonnel dlg(disp_, heros_);
	try {
		dlg.show(disp_.video());
	} catch(twml_exception& e) {
		e.show(disp_);
	}
}

void tgroup2::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	selected_number_ = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	gui2::tbutton* button = find_widget<tbutton>(&window, "upgrade", false, true);
	const tgroup::tmember& m = group_.member(heros_[selected_number_]);
	if (selected_number_ != HEROS_INVALID_NUMBER && m.level < game_config::max_member_level) {
		const upgrade::trequire& require = upgrade::member_require(m.level);
		button->set_active(sum_score(group_.coin(), group_.score()) >= require.score);
	} else {
		button->set_active(false);
	}
}

void tgroup2::refresh_title_flag(twindow& window) const
{
	std::stringstream strstr;
	utils::string_map symbols;
	
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr.str("");
	symbols["leader"] = group_.leader().name();
	strstr << vgettext("wesnoth-lib", "$leader's side", symbols);
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "flag", false, true);
	strstr.str("");
	strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << group_.coin();
	strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << group_.score();

	strstr << "    " << tintegrate::generate_img("misc/salary.png~SCALE(24, 24)");
	int min_heros = 12;
	int increase_per_level = 2;
	int salary = 0;
	if ((int)group_.part_members(heros_, false).size() > min_heros + group_.noble() * increase_per_level) {
		salary = game_config::salary_score * (group_.part_members(heros_, false).size() - min_heros - group_.noble() * increase_per_level);
	}
	strstr << tintegrate::generate_format(salary, "red");
	
	std::vector<int> increase = group_.interior_increase_from_layout_str();
	int develop = 0;
	for (std::vector<int>::const_iterator it = increase.begin(); it != increase.end(); ++ it) {
		develop += *it * game_config::develop_score;
	}
	strstr << "  " << tintegrate::generate_img("misc/interior.png~SCALE(24, 24)");
	strstr << tintegrate::generate_format(develop, "red");

	// tax
	strstr << "  " << tintegrate::generate_img("misc/tax.png~SCALE(24, 24)");
	strstr << tintegrate::generate_format(group_.tax(), "green");

	// end
	strstr << ")";
	if (browse_) {
		strstr << "  (" << tintegrate::generate_format(_("Browse"), "red") << ")";
	}
	label->set_label(strstr.str());

}

class tcount_lock
{
public:
	tcount_lock()
	{
		count ++;
	}
	~tcount_lock()
	{
		count --;
	}

	static int count;
};

int tcount_lock::count = 0;

void tgroup2::pre_show(CVideo& /*video*/, twindow& window)
{
	tcount_lock::count = 0;

	std::stringstream strstr;

	refresh_title_flag(window);

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "base", false, true);
	sheet_.insert(std::make_pair((int)BASE_PAGE, toggle));

	toggle = find_widget<ttoggle_button>(&window, "tag_city", false, true);
	sheet_.insert(std::make_pair((int)CITY_PAGE, toggle));

	toggle = find_widget<ttoggle_button>(&window, "fix", false, true);
	strstr.str("");
	strstr << _("member^Fix") << "(" << tintegrate::generate_format(group_.part_members(heros_, false).size(), tag_color) << ")";
	toggle->set_label(strstr.str());
	sheet_.insert(std::make_pair((int)FIX_PAGE, toggle));

	toggle = find_widget<ttoggle_button>(&window, "tag_employ", false, true);
	strstr.str("");
	strstr << _("Employ") << "(" << tintegrate::generate_format(group_.part_members(heros_, true).size(), tag_color) << ")";
	toggle->set_label(strstr.str());
	sheet_.insert(std::make_pair((int)ROAM_PAGE, toggle));

	toggle = find_widget<ttoggle_button>(&window, "associate", false, true);
	strstr.str("");
	strstr << _("Associate") << "(" << tintegrate::generate_format(group_.associates().size(), tag_color) << ")";
	toggle->set_label(strstr.str());
	sheet_.insert(std::make_pair((int)ASSOCIATE_PAGE, toggle));

	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tgroup2::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, BASE_PAGE, false);
	sheet_.begin()->second->set_value(true);
}

bool tgroup2::text_box_int(twindow& window, const std::string& id, const std::string& name, int& value, int min, int max)
{
	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->get_value();
	value = atoi(str.c_str());
	if (value < min || value > max) {
		std::stringstream err;
		utils::string_map symbols;
		symbols["key"] = tintegrate::generate_format(name);
		err << min;
		symbols["min"] = err.str();
		err.str("");
		err << max;
		symbols["max"] = err.str();;

		err.str("");
		err << vgettext("wesnoth-lib", "Range is [$min, $max], invalid '$key' value:", symbols) << " " << str;
		gui2::show_message(disp_.video(), "", err.str());
		return false;
	}
	return true;
}

void tgroup2::exchange(twindow& window, bool score_2_coin)
{
	utils::string_map symbols;
	std::stringstream strstr;

	int score = preferences::score();
	int coin = preferences::coin();
	int inc_coin, inc_score;
	if (score_2_coin) {
		if (!text_box_int(window, "score_from", _("kingdom^Score"), inc_score, game_config::coin_score_rate, score)) {
			return;
		}
		inc_score -= inc_score % game_config::coin_score_rate;
		inc_coin = inc_score / game_config::coin_score_rate;

		strstr.str("");
		strstr << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << tintegrate::generate_format(inc_score, "yellow");
		symbols["from"] = strstr.str();

		strstr.str("");
		strstr << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << tintegrate::generate_format(inc_coin, "green");
		symbols["to"] = strstr.str();

	} else {
		if (!text_box_int(window, "coin_from", _("kingdom^Coin"), inc_coin, 1, coin)) {
			return;
		}
		inc_score = inc_coin * game_config::coin_score_rate;

		strstr.str("");
		strstr << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << tintegrate::generate_format(inc_coin, "yellow");
		symbols["from"] = strstr.str();

		strstr.str("");
		strstr << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << tintegrate::generate_format(inc_score, "green");
		symbols["to"] = strstr.str();
	}
	std::string message = vgettext("wesnoth-lib", "Do you want to exchange $from to $to?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	http::membership member = http::score(disp_, heros_, score_2_coin? 0: inc_coin, score_2_coin? inc_score: 0, false);
	if (member.uid < 0) {
		return;
	}
	group.from_local_membership(disp_, heros_, member, false);

	refresh_title_flag(window);

	symbols["do"] = tintegrate::generate_format(_("score^Exchange"));
	message = vgettext("wesnoth-lib", "$do successfully!", symbols);
	gui2::show_message(disp_.video(), "", message);
}

bool tgroup2::do_draw(int lack, int coin, int score)
{
	std::set<int> candidate, candidate_roam;
	std::vector<std::pair<int, int> > m2;

	VALIDATE(lack == 1 || lack == game_config::least_fix_members + game_config::least_roam_members, "tgroup2::do_draw, invalid lack!");
	bool reload = lack != 1;

	for (int number = hero::number_normal_min; number < (int)hero_map::map_size_from_dat; number ++) {
		const hero& h = heros_[number];
		if (h.get_flag(hero_flag_robber) || h.get_flag(hero_flag_employee) || h.get_flag(hero_flag_npc)) {
			continue;
		}
		if (group_.exist_exile(h.number_)) {
			continue;
		}
		if (reload) {
			if (h.is_roam_member()) {
				candidate_roam.insert(number);
			} else {
				candidate.insert(number);
			}	
		} else {
			if (group_.exist_member(h.number_)) {
				continue;
			}
			candidate.insert(number);
		}
	}
	while (lack > game_config::least_fix_members && !candidate_roam.empty()) {
		int pos = rand() % candidate_roam.size();
		std::set<int>::iterator it = candidate_roam.begin();
		std::advance(it, pos);
		hero& h = heros_[*it];

		m2.push_back(std::make_pair(h.number_, 0));
		
		candidate_roam.erase(it);
		lack --;
	}
	
	while (lack > 0 && !candidate.empty()) {
		int pos = rand() % candidate.size();
		std::set<int>::iterator it = candidate.begin();
		std::advance(it, pos);
		hero& h = heros_[*it];

		m2.push_back(std::make_pair(h.number_, 0));

		candidate.erase(it);
		lack --;
	}

	if (!m2.empty()) {
		http::membership member = http::member(disp_, heros_, reload? http::member_reload: http::member_insert, m2, coin, score);
		if (member.uid < 0) {
			return false;
		}
		group.from_local_membership(disp_, heros_, member, false);
	}

	return true;
}

void tgroup2::fill_2_least(twindow& window)
{
	int count = game_config::least_fix_members + game_config::least_roam_members;
	int coin_income = 0, score_income = 0;

	const std::vector<tgroup::tmember>& members = group_.members();
	for (std::vector<tgroup::tmember>::const_iterator it = members.begin(); it != members.end(); ++ it) {
		const tgroup::tmember& m = *it;
		if (m.base->get_flag(hero_flag_employee)) {
			continue;
		}
		upgrade::trequire cost = upgrade::member_upgrade_cost(0, m.level);
		coin_income += cost.coin;
		score_income += cost.score;
	}

	game_config::recycle_score_income(coin_income, score_income);

	if (coin_income || score_income) {
		utils::string_map symbols;
		std::string message;
		symbols["coin"] = tintegrate::generate_format(coin_income, "green");
		symbols["score"] = tintegrate::generate_format(score_income, "green");
		symbols["count"] = tintegrate::generate_format(count, "yellow");
		message = vgettext("wesnoth-lib", "May get $coin coin and $score score, do you want to redraw $count hero?", symbols);
		int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return;
		}
	}

	if (!do_draw(count, coin_income, score_income)) {
		return;
	}

	fill_member_table(window, group_.part_members(heros_, false));
}

void tgroup2::draw(twindow& window)
{
	tcount_lock lock;
	if (lock.count != 1) {
		gui2::show_message(disp_.video(), "", "tgroup2::draw, lock.count != 1, check repeat called!");
		lock.count = 1;
		return;
	}

	utils::string_map symbols;

	if (sum_score(group.coin(), group.score()) < game_config::score_used_draw) {
		symbols["score"] = tintegrate::generate_format(game_config::score_used_draw, "green");
		symbols["do"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "group^Draw"), "yellow");
		std::string message = vgettext("wesnoth-lib", "Repertory is less than $score score, cannot $do.", symbols);
		gui2::show_message(disp_.video(), "", message);
		return;
	}
	if (current_page_ != FIX_PAGE) {
		return;
	}
	symbols["score"] = tintegrate::generate_format(game_config::score_used_draw, "red");
	symbols["do"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "group^Draw"), "yellow");
	std::string message = vgettext("wesnoth-lib", "Do you want to spend $score score to $do?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	if (!do_draw(1, 0, -1 * game_config::score_used_draw)) {
		return;
	}

	fill_member_table(window, group_.part_members(heros_, false));
}

void tgroup2::employ_employee(twindow& window)
{
	http::temployee& e = employees_.find(selected_number_)->second;
	bool employ = e.uid == HTTP_INVALID_UID;
	hero& h = heros_[selected_number_];

	std::map<int, http::temployee> employees;
	bool ret;

	if (employ) {
		ret = http::employee_employ(disp_, heros_, h, e.score, &employees);
	} else {
		ret = http::employee_fire(disp_, heros_, h, e.level, &employees);
	}
	if (ret) {
		employees_ = employees;

		tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
		fill_employee_table(window, list.get_selected_row());
	}
}

void tgroup2::upgrade(twindow& window)
{
	hero* upgrade_hero = NULL;
	std::stringstream upgrade_description;
	utils::string_map symbols;


	if (current_page_ == BASE_PAGE) {
		if (!group_.upgrade_leader(disp_, heros_)) {
			return;
		}

		upgrade_hero = &group_.leader();
		symbols["to"] = unit_types.leader_noble(group_.noble()).name();
		upgrade_description << vgettext("wesnoth-lib", "Upgrade to $to", symbols);

	} else if (current_page_ == FIX_PAGE) {
		hero& h = heros_[selected_number_];
		if (!group_.upgrade_member(disp_, heros_, h)) {
			return;
		}

		upgrade_hero = &h;
		tgroup::tmember& m = group_.member(h);
		// symbols["to"] = tintegrate::generate_format(hero_level_str(m.level), "yellow");
		symbols["to"] = hero_level_str(m.level);
		upgrade_description << vgettext("wesnoth-lib", "Upgrade to $to", symbols);

	} else if (current_page_ == ROAM_PAGE) {
		tgroup::tmember& m = group_.member_from_base(heros_[selected_number_]);
		std::map<int, http::temployee> employees;
		if (!group_.upgrade_internal(disp_, heros_, &m, &employees)) {
			return;
		}
		employees_ = employees;

		upgrade_hero = m.h;
		symbols["to"] = hero_level_str(m.level);
		upgrade_description << vgettext("wesnoth-lib", "Upgrade to $to", symbols);

	}

	enable_window_ui(window, false);
	start_upgrade_anim(*upgrade_hero, upgrade_description.str());
}

void tgroup2::detail(twindow& window)
{
	hero* h_ptr, *base_ptr;
	if (current_page_ == FIX_PAGE) {
		h_ptr = &heros_[selected_number_];
		base_ptr = group_.member(*h_ptr).base;
	} else {
		h_ptr = base_ptr = &heros_[selected_number_];
		if (group_.exist_member(selected_number_)) {
			h_ptr = group_.member_from_base(*base_ptr).h;
		}
	}
	hero& h = *h_ptr;
	gui2::thero dlg(heros_, h, base_ptr);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

void tgroup2::exile(twindow& window)
{
	gui2::texile dlg(disp_, heros_, group_);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
	if (dlg.dirty()) {
		fill_member_table(window, group_.part_members(heros_, false));
	}
}

void tgroup2::discard_member(twindow& window)
{
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	if ((int)list->get_item_count() <= game_config::least_fix_members + game_config::least_roam_members) {
		fill_2_least(window);
		return;
	}

	hero& h = heros_[selected_number_];

	int coin_income = 0, score_income = game_config::score_used_draw;
	
	const tgroup::tmember& m = group_.member(h);
	upgrade::trequire cost = upgrade::member_upgrade_cost(0, m.level);
	coin_income += cost.coin;
	score_income += cost.score;

	game_config::recycle_score_income(coin_income, score_income);

	utils::string_map symbols;
	std::stringstream strstr;
	symbols["name"] = tintegrate::generate_format(h.name(), "red");
	symbols["coin"] = tintegrate::generate_format(coin_income, "green");
	symbols["score"] = tintegrate::generate_format(score_income, "green");
	strstr << vgettext("wesnoth-lib", "May get $coin coin and $score score, do you want to discard $name?", symbols) << "\n\n";
	symbols["location"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "Exile"), "yellow");
	if ((int)group_.exiles().size() < game_config::max_exile) {
		strstr << vgettext("wesnoth-lib", "$name will put into $location.", symbols);
	} else {
		strstr << vgettext("wesnoth-lib", "$location is full, $name will lose.", symbols);
	}
	int res = gui2::show_message(disp_.video(), "", strstr.str(), gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	std::vector<std::pair<int, int> > m2;
	m2.push_back(std::make_pair(m.base->number_, m.level));
	http::membership member = http::member(disp_, heros_, http::member_erase, m2, coin_income, score_income);
	if (member.uid < 0) {
		return;
	}
	group.from_local_membership(disp_, heros_, member, false);

	fill_member_table(window, group_.part_members(heros_, false));
}

void tgroup2::post_show(twindow& window)
{
}

void tgroup2::fill_member_table(twindow& window, const std::vector<tgroup::tmember*>& members, int cursel)
{
	int least = game_config::least_fix_members + game_config::least_roam_members;
	std::stringstream strstr;
	int value;
	
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();

	for (std::vector<tgroup::tmember*>::const_iterator it = members.begin(); it != members.end(); ++ it) {
		const tgroup::tmember& m = **it;
		hero& h = *m.h;

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << h.image();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		strstr.str("");
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			strstr << tintegrate::generate_img(ut->icon()) << "\n";
		}
		strstr << h.name();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// level
		strstr.str("");
		strstr << hero_level_str(m.level);
		strstr << "\n";
		if (m.level < game_config::max_member_level) {
			const upgrade::trequire& require = upgrade::member_require(m.level);
			strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << require.coin;
			strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << require.score << ")";
		} else {
			strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << "--";
			strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << "--" << ")";
		}

		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("level", list_item));

		// cost
		strstr.str("");
		value = h.cost_;
		strstr << value;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		// leadership
		strstr.str("");
		strstr << fxptoi9(h.leadership_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("leadership", list_item));

		// charm
		strstr.str("");
		strstr << fxptoi9(h.charm_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("charm", list_item));

		// feature
		strstr.str("");
		strstr << hero::feature_str(h.feature_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("feature", list_item));

		// tactic
		strstr.str("");
		if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		} else if (m.base->tactic_ != HEROS_NO_TACTIC) {
			strstr << tintegrate::generate_format(unit_types.tactic(m.base->tactic_).name(), "red");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("tactic", list_item));

		list->add_row(list_item_item);

		tgrid* grid = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid->find("human", false);
		widget->set_visible(twidget::INVISIBLE);

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid->find("_toggle", true));
		toggle->set_data(h.number_);
	}

	list->set_callback_value_change(dialog_callback<tgroup2, &tgroup2::type_selected>);
	if (cursel >= (int)members.size()) {
		cursel = (int)members.size() - 1;
	}
	if (!members.empty()) {
		list->select_row(cursel);
	}
	selected_number_ = members.size()? members[cursel]->h->number_: HEROS_INVALID_NUMBER;

	refresh_title_flag(window);

	tbutton* button = find_widget<tbutton>(&window, "draw", false, true);
	button->set_active((int)list->get_item_count() >= least? true: false);
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "upgrade", false, true);
	if (selected_number_ != HEROS_INVALID_NUMBER && members[cursel]->level < game_config::max_member_level) {
		const upgrade::trequire& require = upgrade::member_require(members.front()->level);
		button->set_active(sum_score(group_.coin(), group_.score()) >= require.score);
	} else {
		button->set_active(false);
	}
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "discard", false, true);
	strstr.str("");
	if ((int)list->get_item_count() <= least) {
		strstr << tintegrate::generate_img("misc/refresh.png~SCALE(24, 24)");
		strstr << tintegrate::generate_format(least, "blue");
	} else {
		strstr << tintegrate::generate_format(_("Discard"), "blue");
	}
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "detail", false, true);
	button->set_active(list->get_item_count()? true: false);

	button = find_widget<tbutton>(&window, "exile", false, true);
	strstr.str("");
	strstr << dsgettext("wesnoth-lib", "Exile");
	strstr << "(";
	strstr << tintegrate::generate_format(group_.exiles().size(), (int)group_.exiles().size() < game_config::max_exile? "green": "red");
	strstr << ")";
	button->set_label(strstr.str());
	button->set_active(!browse_);

	ttoggle_button* toggle = NULL;
	strstr.str("");
	toggle = find_widget<ttoggle_button>(&window, "fix", false, true);
	strstr << _("member^Fix");
	strstr << "(" << tintegrate::generate_format(list->get_item_count(), tag_color) << ")";
	toggle->set_label(strstr.str());
}

void tgroup2::refresh_employee_other(twindow& window)
{
	std::stringstream strstr;
	const hero& h = heros_[selected_number_];

	const http::temployee* employee_ptr = NULL;
	if (!employees_.empty()) {
		std::map<int, http::temployee>::iterator find = employees_.find(h.number_);
		if (find != employees_.end()) {
			employee_ptr = &find->second;
		}
	}

	tbutton* button = find_widget<tbutton>(&window, "action", false, true);
	bool enable = false;
	strstr.str("");
	if (employee_ptr && !employee_ptr->lock) {
		if (employee_ptr->uid == HTTP_INVALID_UID) {
			if ((int)group_.part_members(heros_, true).size() < game_config::max_employees) {
				enable = true;
				strstr << tintegrate::generate_format(_("Employ"), "blue");
			}
		} else if (employee_ptr->uid == group_.leader().uid()) {
			enable = true;
			strstr << tintegrate::generate_format(_("Fire"), "blue");
		}
	}
	button->set_active(enable);
	if (strstr.str().empty()) {
		strstr << "---";
	}
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "upgrade", false, true);
	if (employee_ptr && employee_ptr->uid == group_.leader().uid() && employee_ptr->level < game_config::max_member_level) {
		const upgrade::trequire& require = upgrade::member_require(employee_ptr->level);
		button->set_active(sum_score(group_.coin(), group_.score()) >= require.score);
	} else {
		button->set_active(false);
	}
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}
}

void tgroup2::employee_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	selected_number_ = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	refresh_employee_other(window);
}

void tgroup2::fill_employee_table(twindow& window, int cursel)
{
	std::stringstream strstr;
	
	if (!browse_ && employees_.empty()) {
		employees_ = http::list_employee(disp_, heros_);
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();

	std::vector<int> numbers;
	for (hero_map::iterator it = heros_.begin(); it != heros_.end(); ++ it) {
		hero& h = *it;
		if (h.number_ >= hero_map::map_size_from_dat) {
			break;
		}
		if (!h.get_flag(hero_flag_employee)) {
			continue;
		}
		const http::temployee* employee_ptr = NULL;
		if (!employees_.empty()) {
			std::map<int, http::temployee>::iterator find = employees_.find(h.number_);
			if (find != employees_.end()) {
				employee_ptr = &find->second;
			}
		}
		const tgroup::tmember* member_ptr = NULL;
		if (group_.exist_member(h.number_)) {
			member_ptr = &group_.member_from_base(h);
		} else if (browse_) {
			continue;
		}

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << h.image();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		strstr.str("");
		if (h.utype_ != HEROS_NO_UTYPE) {
			const unit_type* ut = unit_types.keytype(h.utype_);
			strstr << tintegrate::generate_img(ut->icon()) << "\n";
		}
		strstr << h.name();
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// level
		strstr.str("");
		int level = -1;
		if (employee_ptr) {
			level = employee_ptr->level;
		} else if (member_ptr) {
			level = member_ptr->level;
		}
		if (level == -1) {
			strstr << "--";
		} else if (level / game_config::levels_per_rank >= 2) {
			strstr << _("rank^Gold");
		} else if (level / game_config::levels_per_rank >= 1) {
			strstr << _("rank^Silver");
		} else {
			strstr << _("rank^Copper");
		}
		if (level != -1) {
			strstr << "(" << (level % game_config::levels_per_rank + 1) << ")";
		}
		if (member_ptr) {
			strstr << "\n";
			if (level < game_config::max_member_level) {
				const upgrade::trequire& require = upgrade::member_require(level);
				strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << require.coin;
				strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << require.score << ")";
			} else {
				strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << "--";
				strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << "--" << ")";
			}
		}

		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("level", list_item));

		// cost
		strstr.str("");
		if (employee_ptr) {
			strstr << employee_ptr->score;
			strstr << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		// leadership
		strstr.str("");
		if (member_ptr) {
			strstr << fxptoi9(member_ptr->h->leadership_);
		} else {
			strstr << fxptoi9(h.leadership_);
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("leadership", list_item));

		// charm
		strstr.str("");
		if (member_ptr) {
			strstr << fxptoi9(member_ptr->h->charm_);
		} else {
			strstr << fxptoi9(h.charm_);
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("charm", list_item));

		// feature
		strstr.str("");
		strstr << hero::feature_str(h.feature_);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("feature", list_item));

		// tactic
		strstr.str("");
		if (member_ptr) {
			if (member_ptr->h->tactic_ != HEROS_NO_TACTIC) {
				strstr << unit_types.tactic(member_ptr->h->tactic_).name();
			} else if (member_ptr->base->tactic_ != HEROS_NO_TACTIC) {
				strstr << tintegrate::generate_format(unit_types.tactic(member_ptr->base->tactic_).name(), "red");
			}
		} else if (h.tactic_ != HEROS_NO_TACTIC) {
			strstr << unit_types.tactic(h.tactic_).name();
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("tactic", list_item));

		// ownership
		strstr.str("");
		if (employee_ptr) {
			if (employee_ptr->username == group_.leader().name()) {
				strstr << tintegrate::generate_format(employee_ptr->username, "green");
			} else {
				strstr << employee_ptr->username;
			}
			if (employee_ptr->lock) {
				strstr << tintegrate::generate_img("misc/lock.png");
			}
		} else {
			strstr << tintegrate::generate_img("misc/unknown.png");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("ownership", list_item));

		list->add_row(list_item_item);

		tgrid* grid = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid->find("human", false);
		widget->set_visible(twidget::INVISIBLE);

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid->find("_toggle", true));
		numbers.push_back(h.number_);
		toggle->set_data(numbers.back());
	}

	list->set_callback_value_change(dialog_callback<tgroup2, &tgroup2::employee_selected>);
	if (cursel >= (int)numbers.size()) {
		cursel = (int)numbers.size() - 1;
	}
	if (numbers.size()) {
		list->select_row(cursel);
	}
	selected_number_ = numbers.size()? numbers[cursel]: HEROS_INVALID_NUMBER;

	refresh_title_flag(window);
	refresh_employee_other(window);

	tbutton* button = find_widget<tbutton>(&window, "detail", false, true);
	button->set_active(list->get_item_count()? true: false);

	ttoggle_button* toggle = NULL;
	strstr.str("");
	toggle = find_widget<ttoggle_button>(&window, "tag_employ", false, true);
	strstr << _("Employ");
	strstr << "(" << tintegrate::generate_format(group_.part_members(heros_, true).size(), tag_color) << ")";
	toggle->set_label(strstr.str());
}

void tgroup2::fill_other(twindow& window)
{
	std::stringstream strstr;

	tbutton* button = find_widget<tbutton>(&window, "draw", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::draw
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("group^Draw"), "blue");
	button->set_label(strstr.str());
	if (current_page_ == ROAM_PAGE) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "upgrade", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::upgrade
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Upgrade"), "blue");
	button->set_label(strstr.str());

	button = find_widget<tbutton>(&window, "discard", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::discard_member
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "detail", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::detail
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "exile", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::exile
		, this
		, boost::ref(window)));

	window.invalidate_layout();
}

void tgroup2::refresh_leader_noble(twindow& window)
{
	const int noble = group_.noble();
	std::stringstream strstr;

	strstr.str("");
	strstr << tintegrate::generate_format(_("Noble"), "green") << _(":") << " ";
	strstr << unit_types.leader_noble(noble).name();
	if (noble < unit_types.max_noble_level()) {
		const upgrade::trequire& require = upgrade::noble_require(noble);
		strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << require.coin;
		strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << require.score << ")";
	} else {
		strstr << "(" << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)") << "--";
		strstr << "  " << tintegrate::generate_img("misc/score.png~SCALE(24, 24)") << "--" << ")";
	}
	tcontrol* control = find_widget<tcontrol>(&window, "noble", false, true);
	control->set_label(strstr.str());


	tbutton* button = find_widget<tbutton>(&window, "upgrade", false, true);
	button->set_active(noble < unit_types.max_noble_level());
}

void tgroup2::associate_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);

	selected_associate_ = list.get_selected_row();

	const tgroup::tassociate& a = group_.associates()[selected_associate_];
	tbutton* button = find_widget<tbutton>(&window, "do_agreement", false, true);
	refresh_button_according_associate(*button, a);

	button = find_widget<tbutton>(&window, "remove", false, true);
	if (a.agreement == tgroup::tassociate::ally || a.agreement == tgroup::tassociate::requestterminate) {
		button->set_active(false);
	} else {
		button->set_active(true);
	}
}

void tgroup2::refresh_button_according_associate(tbutton& button, const tgroup::tassociate& a)
{
	bool enable = true;
	std::string str;
	if (a.agreement == tgroup::tassociate::none) {
		str = _("Request ally");
	} else if (a.agreement == tgroup::tassociate::requestally) {
		str = _("Undo");
	} else if (a.agreement == tgroup::tassociate::ally) {
		str = _("Request terminate");
	} else {
		str = _("Undo");
	}
	button.set_label(tintegrate::generate_format(str, "blue"));
	button.set_active(enable);
}

void tgroup2::do_agreement(twindow& window)
{
	tgroup::tassociate& a = group_.associates()[selected_associate_];
	int tag;
	if (a.agreement == tgroup::tassociate::none) {
		tag = http::associate_tag_requestally;
	} else if (a.agreement == tgroup::tassociate::requestally) {
		if (a.t) {
			tag = http::associate_tag_undo;
		} else {
			tag = http::associate_tag_ally;
		}
	} else if (a.agreement == tgroup::tassociate::ally) {
		tag = http::associate_tag_requestterminate;
	} else if (a.agreement == tgroup::tassociate::requestterminate) {
		if (a.t) {
			tag = http::associate_tag_undo;
		} else {
			tag = http::associate_tag_terminate;
		}
	} else {
		return;
	}
	http::membership m = http::associate(disp_, heros_, a.username, tag);
	if (m.uid < 0) {
		return;
	}
	if (m.associate != preferences::associate()) {
		group_.associate_from_str(m.associate);
		preferences::set_associate(m.associate);
	}

	// to this, associate memeber's associate is dirty. so don't diplay it on main window.
	fill_associate_table(window, selected_associate_);
}

void tgroup2::insert_associate(twindow& window)
{
	std::stringstream strstr;
	
	ttext_box* widget = find_widget<ttext_box>(&window, "username", false, true);
	std::string username = widget->get_value();
	
	strstr.str("");
	if (username.empty()) {
		strstr << dsgettext("wesnoth-lib", "Username doesn't allow empty!");
	} else if (username == group_.leader().name()) {
		strstr << dsgettext("wesnoth-lib", "Username cannot be myself!");
	} else {
		const std::vector<tgroup::tassociate>& associates = group_.associates();
		for (size_t n = 0; n < associates.size(); n ++) {
			const tgroup::tassociate& a = associates[n];
			if (a.username == username) {
				strstr << dsgettext("wesnoth-lib", "Username has existd in associate list, cannot append!");
				break;
			}
		}
	}
	if (!strstr.str().empty()) {
		gui2::show_message(disp_.video(), "", strstr.str());
		return;
	}

	http::membership m = http::membership_hero(disp_, heros_, false, username);
	if (m.uid < 0) {
		utils::string_map symbols;
		symbols["username"] = tintegrate::generate_format(username, "yellow");
		strstr << vgettext("wesnoth-lib", "$username doesn't exist!", symbols);
		gui2::show_message(disp_.video(), "", strstr.str());
		return;
	}

	http::membership m2 = http::associate(disp_, heros_, username, http::associate_tag_insert);
	if (m2.uid < 0) {
		return;
	}
	group_.associate_from_str(m2.associate);
	preferences::set_associate(m2.associate);

	associate_members_.push_back(m);

	// to this, associate memeber's associate is dirty. so don't diplay it on main window.
	fill_associate_table(window, ++ selected_associate_);
}

void tgroup2::remove_associate(twindow& window)
{
	tgroup::tassociate& a = group_.associates()[selected_associate_];
	http::membership m = http::associate(disp_, heros_, a.username, http::associate_tag_erase);
	if (m.uid < 0) {
		return;
	}
	if (m.associate != preferences::associate()) {
		group_.associate_from_str(m.associate);
		preferences::set_associate(m.associate);
	}

	std::vector<http::membership>::iterator it = associate_members_.begin();
	std::advance(it, selected_associate_);
	associate_members_.erase(it);

	// to this, associate memeber's associate is dirty. so don't diplay it on main window.
	fill_associate_table(window, selected_associate_);
}

void tgroup2::give_score(twindow& window)
{
}

void tgroup2::refresh_associate(twindow& window)
{
	// only local group can execute it. so group_ must be local group.
	http::membership m = http::membership_hero2(disp_, heros_);
	if (m.uid < 0) {
		return;
	}

	bool dirty = false;
	const std::vector<tgroup::tassociate>& associates = group_.associates();
	if (associate_members_.size() == associates.size()) {
		for (size_t n = 0; n < associates.size(); n ++) {
			const tgroup::tassociate& a = associates[n];
			if (a.username != associate_members_[n].name) {
				dirty = true;
				break;
			}
		}
	} else {
		dirty = true;
	}

	if (dirty) {
		associate_members_.clear();
	}
	fill_associate_table(window, selected_associate_);
}

void tgroup2::detail_associate(twindow& window)
{
	tgroup::tassociate& a = group_.associates()[selected_associate_];
	http::membership m = http::membership_from_uid(disp_, heros_, false, a.uid);
	if (m.uid < 0) {
		return;
	}

	http::avatar_hero(disp_, heros_, m.uid, true);

	tgroup g;
	g.from_membership(heros_, m);
	gui2::tgroup2 dlg(disp_, heros_, game_config_, g, true);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}

	associate_members_[selected_associate_] = m;
	// to this, associate memeber's associate is dirty. so don't diplay it on main window.
	fill_associate_table(window, selected_associate_);
}

void tgroup2::fill_associate_table(twindow& window, int cursel)
{
	std::stringstream strstr;
	utils::string_map symbols;
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	list->clear();

	if (!browse_ && associate_members_.empty() && !group_.associates().empty()) {
		strstr.str("");
		for (std::vector<tgroup::tassociate>::const_iterator it = group_.associates().begin(); it != group_.associates().end(); ++ it) {
			if (it != group_.associates().begin()) {
				strstr << ",";
			}
			strstr << it->username;
		}
		associate_members_ = http::membershiplist_vector(disp_, heros_, true, strstr.str(), false);
	}

	for (std::vector<tgroup::tassociate>::iterator it = group_.associates().begin(); it != group_.associates().end(); ++ it) {
		const tgroup::tassociate& a = *it;
		int index = std::distance(group_.associates().begin(), it);

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		int gender = hero_gender_male;
		if (!associate_members_.empty()) {
			gender = hero::gender_from_filed_str(associate_members_[index].field);
		}
		strstr.str("");
		strstr << hero::image_from_uid(a.uid, gender, false);
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		// name
		strstr.str("");
		if (!associate_members_.empty() && associate_members_[index].vip) {
			strstr << tintegrate::generate_img("misc/vip.png~SCALE(32, 32)") << "\n";
		}
		strstr << a.username;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("name", list_item));

		// hero count
		strstr.str("");
		if (!associate_members_.empty()) {
			std::vector<std::string> vstr = utils::split(associate_members_[index].member);
			strstr << vstr.size();
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("hero", list_item));

		// coin
		strstr.str("");
		if (!associate_members_.empty()) {
			strstr << associate_members_[index].coin;
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("coin", list_item));

		// score
		strstr.str("");
		if (!associate_members_.empty()) {
			strstr << associate_members_[index].score;
		} else {
			strstr << "--";
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("score", list_item));

		// agreement
		strstr.str("");
		if (a.agreement == tgroup::tassociate::requestally) {
			if (a.t) {
				strstr << _("Request ally") << _("process^going");
			} else {
				strstr << _("Be requested ally") << _("process^going");
			}
		} else if (a.agreement == tgroup::tassociate::ally) {
			strstr << _("Ally");
		} else if (a.agreement == tgroup::tassociate::requestterminate) {
			if (a.t) {
				strstr << _("Request terminate") << _("process^going");
			} else {
				strstr << _("Be requested terminate") << _("process^going");
			}
		} else {
			strstr << _("None");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("agreement", list_item));

		// date
		strstr.str("");
		if (a.t) {
			symbols.clear();
			if (a.agreement == tgroup::tassociate::requestally) {
				symbols["time"] = tintegrate::generate_format(format_time_date(a.t), "yellow");
				strstr << vgettext("wesnoth-lib", "Initiation time: $time", symbols);
			} else if (a.agreement == tgroup::tassociate::requestterminate) {
				int terminate_threshold = 2 * 24 * 3600;
				time_t now = time(NULL);
				symbols["space"] = tintegrate::generate_format(format_time_elapse(a.t + terminate_threshold - now), "yellow");
				strstr << vgettext("wesnoth-lib", "Termiante automatically after $space", symbols);
			} else {
				strstr << format_time_date(a.t);
			}
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("date", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid_ptr->find("human", false);
		widget->set_visible(twidget::INVISIBLE);
	}

	list->set_callback_value_change(dialog_callback<tgroup2, &tgroup2::associate_selected>);
	if (cursel >= (int)group_.associates().size()) {
		selected_associate_ = (int)group_.associates().size() - 1;
	} else if (cursel >= 0) {
		selected_associate_ = cursel;
	} else {
		selected_associate_ = 0;
	}
	if (!group_.associates().empty()) {
		list->select_row(selected_associate_);
	}

	refresh_title_flag(window);

	const tgroup::tassociate* a = NULL;
	if (!group_.associates().empty()) {
		a = &group_.associates()[selected_associate_];
	}

	tbutton* button = find_widget<tbutton>(&window, "do_agreement", false, true);
	if (a) {
		refresh_button_according_associate(*button, *a);
	} else {
		button->set_active(false);
	}

	button = find_widget<tbutton>(&window, "remove", false, true);
	if (!list->get_item_count() || a->agreement == tgroup::tassociate::ally || a->agreement == tgroup::tassociate::requestterminate) {
		button->set_active(false);
	} else {
		button->set_active(true);
	}

	button = find_widget<tbutton>(&window, "give", false, true);
	// button->set_active((!associate_members_.empty() && list->get_item_count())? true: false);
	button->set_active(false);

	button = find_widget<tbutton>(&window, "detail", false, true);
	button->set_active(list->get_item_count()? true: false);

	strstr.str("");
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "associate", false, true);
	strstr << _("Associate");
	strstr << "(" << tintegrate::generate_format(list->get_item_count(), tag_color) << ")";
	toggle->set_label(strstr.str());
}

void tgroup2::fill_base(twindow& window)
{
	std::stringstream strstr;

	tbutton* button = find_widget<tbutton>(&window, "upgrade", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::upgrade
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Upgrade"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "personnel", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::personnel
		, this
		, boost::ref(window)));
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}
	
	refresh_leader_noble(window);

	tlabel* label = find_widget<tlabel>(&window, "city", false, true);
	strstr.str("");
	strstr << tintegrate::generate_format(_("City"), "green") << _(":") << " ";
	strstr << group_.city().name();
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "exchange", false, true);
	strstr.str("");
	strstr << tintegrate::generate_format(_("Exchange rate"), "yellow", 18, true) << " ";
	strstr << tintegrate::generate_format(game_config::coin_score_rate, "green", 18, true);
	label->set_label(strstr.str());

	ttext_box* text = find_widget<ttext_box>(&window, "score_from", false, true);
	text->set_value(str_cast(game_config::coin_score_rate));

	label = find_widget<tlabel>(&window, "score_2_coin", false, true);
	strstr.str("");
	strstr << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
	strstr << tintegrate::generate_img("misc/to.png");
	strstr << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)");
	label->set_label(strstr.str());

	text = find_widget<ttext_box>(&window, "coin_from", false, true);
	text->set_value("1");

	label = find_widget<tlabel>(&window, "coin_2_score", false, true);
	strstr.str("");
	strstr << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)");
	strstr << tintegrate::generate_img("misc/to.png");
	strstr << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
	label->set_label(strstr.str());

	button = find_widget<tbutton>(&window, "score_exchange_coin", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::exchange
		, this
		, boost::ref(window)
		, true));
	strstr.str("");
	strstr << tintegrate::generate_format(_("score^Exchange"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "coin_exchange_score", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::exchange
		, this
		, boost::ref(window)
		, false));
	strstr.str("");
	strstr << tintegrate::generate_format(_("score^Exchange"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}
}

void tgroup2::fill_city(twindow& window)
{
	std::stringstream strstr;
	utils::string_map symbols;

	std::vector<int> increase = group_.interior_increase_from_layout_str();
	std::string increase_color = "green";
	if (group_.coin() <= 0) {
		if (group_.score() < game_config::develop_score) {
			increase_color = "red";
		} else {
			int total = 0;
			for (std::vector<int>::const_iterator it = increase.begin(); it != increase.end(); ++ it) {
				total += *it * game_config::develop_score;
			}
			if (group_.score() < total) {
				increase_color = "yellow";
			}
		}
	}
	strstr.str("");
	tlabel* label = find_widget<tlabel>(&window, "interior", false, true);
	for (int n = 0; n < tgroup::interior_count; n ++) {
		if (n) {
			strstr << "    ";
		}
		strstr << tintegrate::generate_img(interiors_png[n]);
		strstr << tintegrate::generate_format(group_.interior(n), "green");
		if (increase[n]) {
			strstr << " +";
			strstr << tintegrate::generate_format(increase[n], increase_color);
		}
	}
	label->set_label(strstr.str());

	ttext_box* box = find_widget<ttext_box>(&window, "city", false, true);
	box->set_value(group_.city().name());
	box->set_active(false);

	int military = group_.interior(tgroup::interior_military);

	tbutton* button = find_widget<tbutton>(&window, "config", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::player_city
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_img("misc/config.png~SCALE(24, 24)");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "layout", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::layout
		, this
		, boost::ref(window)));
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	const ttechnology& s = group_.stratagem_from_layout_str();
	label = find_widget<tlabel>(&window, "stratagem", false, true);
	label->set_label(s.name());

	tscroll_label* label2 = find_widget<tscroll_label>(&window, "stratagem_description", false, true);
	label2->set_label(s.description());


	label = find_widget<tlabel>(&window, "set_map_tip", false, true);
	strstr.str("");
	symbols["layout"] = tintegrate::generate_format(_("group^Layout"), "yellow");
	symbols["save"] = tintegrate::generate_format(_("Save Layout"), "yellow");
	strstr << vgettext("wesnoth-lib", "1: Select the map want to set.\n2: Play $layout, execute $save.", symbols);
	label->set_label(strstr.str());	

	tlistbox& list = find_widget<tlistbox>(&window, "map_list", false);
	list.set_callback_value_change(dialog_callback<tgroup2, &tgroup2::update_map>);

	button = find_widget<tbutton>(&window, "edit_map", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::edit_map
		, this
		, boost::ref(window)));
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	// Standard maps
	int index = 0;
	string_map item;
	item.insert(std::make_pair("label", tintegrate::generate_format(_("Using map"), "yellow")));
	list.add_row(item);

	tgrid* grid_ptr = list.get_row_grid(list.get_item_count() - 1);
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("_toggle", true));
	toggle->set_data(index);

	BOOST_FOREACH (const config &map, game_config_.child_range("multiplayer")) {
		index ++;
		const std::string& mode = map["mode"].str();
		if (mode != "siege") {
			continue;
		}
		
		item.clear();
		item.insert(std::make_pair("label", map["name"].str()));
		list.add_row(item);

		grid_ptr = list.get_row_grid(list.get_item_count() - 1);
		toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("_toggle", true));
		toggle->set_data(index);
	}
	update_map(window);
}

void tgroup2::fill_fix(twindow& window)
{
	fill_member_table(window, group_.part_members(heros_, false));
	fill_other(window);
}

void tgroup2::fill_employee(twindow& window)
{
	std::stringstream strstr;
	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "PS"), "red") << " ";
	strstr << tintegrate::generate_format(dsgettext("wesnoth-lib", "If defend city fail, you will lose a employee."), "yellow");
	label->set_label(strstr.str());

	fill_employee_table(window, 0);
	
	tbutton* button = find_widget<tbutton>(&window, "action", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::employ_employee
		, this
		, boost::ref(window)));

	button = find_widget<tbutton>(&window, "upgrade", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::upgrade
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Upgrade"), "blue");
	button->set_label(strstr.str());

	button = find_widget<tbutton>(&window, "detail", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::detail
		, this
		, boost::ref(window)));

	window.invalidate_layout();
}

void tgroup2::fill_associate(twindow& window)
{
	fill_associate_table(window);

	std::stringstream strstr;
	tbutton* button = find_widget<tbutton>(&window, "insert", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::insert_associate
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Append"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "do_agreement", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tgroup2::do_agreement
			, this
			, boost::ref(window)));
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "refresh", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::refresh_associate
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Refresh"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "remove", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::remove_associate
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Remove"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "give", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::give_score
		, this
		, boost::ref(window)));
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&window, "detail", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgroup2::detail_associate
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Detail"), "blue");
	button->set_label(strstr.str());
	if (browse_) {
		button->set_visible(twidget::INVISIBLE);
	}
}

void tgroup2::swap_page(twindow& window, int page, bool swap)
{
	if (page < MIN_PAGE || page > MAX_PAGE) {
		return;
	}
	int index = page - MIN_PAGE;

	if (page_panel_->current_page() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	page_panel_->swap_uh(window, index);
	if (swap) {
		page_panel_->swap_bh(window);
	}

	current_page_ = page;

	if (page == BASE_PAGE) {
		fill_base(window);

	} else if (page == CITY_PAGE) {
		fill_city(window);

	} else if (page == FIX_PAGE) {
		fill_fix(window);

	} else if (page == ROAM_PAGE) {
		fill_employee(window);

	} else if (page == ASSOCIATE_PAGE) {
		fill_associate(window);
	}
}

void tgroup2::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

void tgroup2::enable_window_ui(twindow& window, bool enable)
{
	for (std::map<int, ttoggle_button*>::const_iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		ttoggle_button& toggle = *it->second;
		toggle.set_active(enable);
	}

	if (current_page_ == BASE_PAGE) {
		tbutton* button = find_widget<tbutton>(&window, "upgrade", false, true);
		button->set_active(enable);

		button = find_widget<tbutton>(&window, "personnel", false, true);
		button->set_active(enable);
		
		button = find_widget<tbutton>(&window, "score_exchange_coin", false, true);
		button->set_active(enable);

		button = find_widget<tbutton>(&window, "coin_exchange_score", false, true);
		button->set_active(enable);

	} else if (current_page_ == FIX_PAGE) {
		const std::vector<tgroup::tmember*>& members = group_.part_members(heros_, false);
		int least = game_config::least_fix_members + game_config::least_roam_members;
		
		tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
		int cursel = list->get_selected_row();
		tbutton* button = find_widget<tbutton>(&window, "draw", false, true);
		button->set_active(enable && ((int)list->get_item_count() >= least? true: false));

		button = find_widget<tbutton>(&window, "upgrade", false, true);
		if (selected_number_ != HEROS_INVALID_NUMBER && members[cursel]->level < game_config::max_member_level) {
			const upgrade::trequire& require = upgrade::member_require(members.front()->level);
			button->set_active(enable && sum_score(group_.coin(), group_.score()) >= require.score);
		} else {
			button->set_active(false);
		}

		button = find_widget<tbutton>(&window, "discard", false, true);
		button->set_active(enable);

		button = find_widget<tbutton>(&window, "detail", false, true);
		button->set_active(enable && (list->get_item_count()? true: false));

		button = find_widget<tbutton>(&window, "exile", false, true);
		button->set_active(enable && !browse_);

	} else if (current_page_ == ROAM_PAGE) {
		tbutton* button = find_widget<tbutton>(&window, "action", false, true);
		button->set_active(enable);

		button = find_widget<tbutton>(&window, "upgrade", false, true);
		button->set_active(enable);

		button = find_widget<tbutton>(&window, "detail", false, true);
		button->set_active(enable);
	}
	find_widget<tbutton>(&window, "ok", false, true)->set_active(enable);
}

void tgroup2::upgrade_anim_finish(animation* anim)
{
	twindow& window = *page_panel_->get_window();
	enable_window_ui(window, true);

	if (current_page_ == BASE_PAGE) {
		refresh_title_flag(window);
		refresh_leader_noble(window);

	} else if (current_page_ == FIX_PAGE) {
		tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
		fill_member_table(window, group_.part_members(heros_, false), list.get_selected_row());

	} else if (current_page_ == ROAM_PAGE) {
		tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
		fill_employee_table(window, list.get_selected_row());
	}
}

void tgroup2::start_upgrade_anim(hero& h, const std::string& description)
{
	float_animation* anim = start_float_anim_th(disp_, area_anim::UPGRADE);
	anim->set_scale(800, 600, true, false);
	anim->set_callback_finish(boost::bind(&tgroup2::upgrade_anim_finish, this, _1));

	std::stringstream strstr;
	anim->replace_image_name("__bg.png", "tactic/bg-single-increase.png");
	anim->replace_image_name("__id.png", h.image(true));
	anim->replace_static_text("__formation_name", h.name());
	anim->replace_static_text("__formation_description", description);

	start_float_anim_bh(*anim);
}

} // namespace gui2
