/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/user_report.hpp"

#include "game_display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/group.hpp"
#include "gui/dialogs/hero.hpp"
#include "preferences_display.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "help.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <algorithm>

extern std::string mode_desc(mode_tag::tmode tag);

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 *
 * @begin{table}{dialog_widgets}
 * tutorial & & button & m &
 *         The button to start the tutorial. $
 *
 * campaign & & button & m &
 *         The button to start a campaign. $
 *
 * multiplayer & & button & m &
 *         The button to start multiplayer mode. $
 *
 * load & & button & m &
 *         The button to load a saved game. $
 *
 * editor & & button & m &
 *         The button to start the editor. $
 *
 * addons & & button & m &
 *         The button to start managing the addons. $
 *
 * language & & button & m &
 *         The button to select the game language. $
 *
 * credits & & button & m &
 *         The button to show Wesnoth's contributors. $
 *
 * quit & & button & m &
 *         The button to quit Wesnoth. $
 *
 * tips & & multi_page & m &
 *         A multi_page to hold all tips, when this widget is used the area of
 *         the tips doesn't need to be resized when the next or previous button
 *         is pressed. $
 *
 * -tip & & label & o &
 *         Shows the text of the current tip. $
 *
 * -source & & label & o &
 *         The source (the one who's quoted or the book referenced) of the
 *         current tip. $
 *
 * next_tip & & button & m &
 *         The button show the next tip of the day. $
 *
 * previous_tip & & button & m &
 *         The button show the previous tip of the day. $
 *
 * logo & & progress_bar & o &
 *         A progress bar to "animate" the Wesnoth logo. $
 *
 * revision_number & & control & o &
 *         A widget to show the version number when the version number is
 *         known. $
 *
 * @end{table}
 */

REGISTER_DIALOG(user_report)

tuser_report::tuser_report(game_display& disp, hero_map& heros, const config& game_config)
	: disp_(disp)
	, heros_(heros)
	, game_config_(game_config)
	, current_page_(NONE_PAGE)
	, page_panel_(NULL)
{
}

tuser_report::~tuser_report()
{
}

void tuser_report::post_build(CVideo& video, twindow& window)
{
}

void tuser_report::sheet_toggled(twidget* widget)
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

void tuser_report::set_label_int(twindow& window, const std::string& id, int value)
{
	tlabel* widget = find_widget<tlabel>(&window, id, false, true);
	widget->set_label(lexical_cast<std::string>(value));
}

void tuser_report::pre_show(CVideo& video, twindow& window)
{
	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	sheet_.insert(std::make_pair((int)PLAYER_PASS_PAGE, find_widget<ttoggle_button>(&window, "player_pass", false, true)));
	sheet_.insert(std::make_pair((int)RANK_PASS_PAGE, find_widget<ttoggle_button>(&window, "rank_pass", false, true)));
	sheet_.insert(std::make_pair((int)EMPLOYEE_PAGE, find_widget<ttoggle_button>(&window, "employ_hero", false, true)));
	sheet_.insert(std::make_pair((int)RANK_SCORE_PAGE, find_widget<ttoggle_button>(&window, "rank_score", false, true)));
	sheet_.insert(std::make_pair((int)TITLE_BOARD_PAGE, find_widget<ttoggle_button>(&window, "title_board", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tuser_report::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, PLAYER_PASS_PAGE, false);
	sheet_.begin()->second->set_value(true);
}

void tuser_report::post_show(twindow& window)
{
}

std::string version_str(int version) 
{
	int major = version / 10000;
	int minor = (version - major * 10000) / 100;
	int revision = version % 100;

	std::stringstream strstr;
	strstr << major << "." << minor << "." << revision;
	return strstr.str();
}

void tuser_report::fill_pass_table(twindow& window, const std::vector<http::pass_statistic>& passes, bool board)
{
	std::stringstream strstr;
	tlistbox* list = find_widget<tlistbox>(&window, "pass_table", false, true);
	list->clear();
	BOOST_FOREACH (const http::pass_statistic& pass, passes) {
		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = pass.username;
		data.insert(std::make_pair("username", item));

		item["label"] = format_time_date(pass.create_time);
		data.insert(std::make_pair("createtime", item));

		item["label"] = format_time_elapse(pass.duration);
		data.insert(std::make_pair("duration", item));

		strstr.str("");
		strstr << pass.score;
		item["label"] = strstr.str();
		data.insert(std::make_pair("score", item));

		strstr.str("");
		strstr << pass.coin;
		item["label"] = strstr.str();
		data.insert(std::make_pair("coin", item));

		item["label"] = version_str(pass.version);
		data.insert(std::make_pair("version", item));

		strstr.str("");
		strstr << mode_desc((mode_tag::tmode)pass.type);
		item["label"] = strstr.str();
		data.insert(std::make_pair("type", item));

		list->add_row(data);
	}
	window.invalidate_layout();
}

void tuser_report::detail_employee(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	int number = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	hero& base = heros_[number];
	hero* h_ptr = &base;
	if (group.exist_member(number)) {
		h_ptr = group.member_from_base(base).h;
	}
	
	hero& h = *h_ptr;
	gui2::thero dlg(heros_, h, &base);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

void tuser_report::detail_group(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	tgrid* grid_ptr = list.get_row_grid(list.get_selected_row());
	int uid = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();

	http::membership m = http::membership_from_uid(disp_, heros_, false, uid);
	if (m.uid < 0) {
		return;
	}

	tgroup g;
	g.from_membership(heros_, m);
	gui2::tgroup2 dlg(disp_, heros_, game_config_, g, true);
	try {
		dlg.show(disp_.video());
	} catch (twml_exception& e) {
		e.show(disp_);
	}
}

void tuser_report::fill_base(twindow& window)
{
	std::vector<http::pass_statistic> passes = http::list_pass(disp_, heros_);
	fill_pass_table(window, passes, false);
}

void tuser_report::fill_rank_pass(twindow& window)
{
	std::vector<http::pass_statistic> passes = http::list_board_pass(disp_, heros_);
	fill_pass_table(window, passes, true);
}

void tuser_report::fill_employee(twindow& window)
{
	std::stringstream strstr;
	
	std::map<int, http::temployee> employees = http::list_employee(disp_, heros_);

	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
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
		if (!employees.empty()) {
			std::map<int, http::temployee>::iterator find = employees.find(h.number_);
			if (find != employees.end()) {
				employee_ptr = &find->second;
			}
		}
		const tgroup::tmember* member_ptr = NULL;
		if (group.exist_member(h.number_)) {
			member_ptr = &group.member_from_base(h);
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
			strstr << help::tintegrate::generate_img(ut->icon()) << "\n";
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
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("level", list_item));

		// cost
		strstr.str("");
		if (employee_ptr) {
			strstr << employee_ptr->score;
			strstr << help::tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
		} else {
			strstr << "--";
		}
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
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("tactic", list_item));

		// ownership
		strstr.str("");
		if (employee_ptr) {
			if (employee_ptr->username == group.leader().name()) {
				strstr << help::tintegrate::generate_format(employee_ptr->username, "green");
			} else {
				strstr << employee_ptr->username;
			}
			if (employee_ptr->lock) {
				strstr << help::tintegrate::generate_img("misc/lock.png");;
			}
		} else {
			strstr << help::tintegrate::generate_img("misc/unknown.png");
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("ownership", list_item));

		strstr.str("");
		strstr << help::tintegrate::generate_img("misc/browse.png");;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("browse", list_item));

		list->add_row(list_item_item);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		twidget* widget = grid_ptr->find("human", false);
		widget->set_visible(twidget::INVISIBLE);

		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(h.number_);

		connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "browse", true)
				, boost::bind(
					&tuser_report::detail_employee
					, this
					, boost::ref(window)));
	}

	window.invalidate_layout();
}

void tuser_report::fill_score_board(twindow& window)
{
	std::vector<http::membership> members = http::list_board_score(disp_, heros_);
	std::stringstream strstr;
	utils::string_map symbols;
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();
	for (std::vector<http::membership>::const_iterator it = members.begin(); it != members.end(); ++ it) {
		const http::membership& m = *it;

		std::map<std::string, string_map> data;
		string_map item;

		strstr.str("");
		strstr << m.name;
		if (m.vip > 0) {
			strstr << help::tintegrate::generate_img("misc/vip.png~SCALE(32, 32)");
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("username", item));

		strstr.str("");
		if (m.noble >= 0 && m.noble <= unit_types.max_noble_level()) {
			strstr << unit_types.leader_noble(m.noble).name();
			symbols["level"] = lexical_cast_default<std::string>(unit_types.max_noble_level() - m.noble + 1);
			strstr << "(" << vgettext("wesnoth-lib", "noble^Lv$level", symbols) << ")";
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("noble", item));

		strstr.str("");
		std::vector<std::string> vstr = utils::split(m.member);
		strstr << vstr.size();
		item["label"] = strstr.str();
		data.insert(std::make_pair("hero", item));

		strstr.str("");
		strstr << m.score;
		item["label"] = strstr.str();
		data.insert(std::make_pair("score", item));

		strstr.str("");
		strstr << m.coin;
		item["label"] = strstr.str();
		data.insert(std::make_pair("coin", item));

		strstr.str("");
		strstr << m.credit;
		item["label"] = strstr.str();
		data.insert(std::make_pair("credit", item));

		strstr.str("");
		strstr << help::tintegrate::generate_img("misc/browse.png");;
		item["label"] = strstr.str();
		data.insert(std::make_pair("browse", item));

		list->add_row(data);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(m.uid);
		
		connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "browse", true)
				, boost::bind(
					&tuser_report::detail_group
					, this
					, boost::ref(window)));
	}

	window.invalidate_layout();
}

void tuser_report::fill_title_board(twindow& window)
{
	std::vector<http::ttitle_record> titles = http::list_title(disp_, heros_);
	std::stringstream strstr;
	utils::string_map symbols;
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();
	for (std::vector<http::ttitle_record>::const_iterator it = titles.begin(); it != titles.end(); ++ it) {
		const http::ttitle_record& l = *it;

		std::map<std::string, string_map> data;
		string_map item;

		strstr.str("");
		strstr << form_title_str(l.title);
		item["label"] = strstr.str();
		data.insert(std::make_pair("title", item));

		strstr.str("");
		strstr << l.username;
		if (l.vip > 0) {
			strstr << help::tintegrate::generate_img("misc/vip.png~SCALE(32, 32)");
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("username", item));

		strstr.str("");
		std::vector<std::string> vstr = utils::split(l.member);
		strstr << vstr.size();
		item["label"] = strstr.str();
		data.insert(std::make_pair("hero", item));

		strstr.str("");
		strstr << l.score;
		item["label"] = strstr.str();
		data.insert(std::make_pair("score", item));

		strstr.str("");
		strstr << l.coin;
		item["label"] = strstr.str();
		data.insert(std::make_pair("coin", item));

		strstr.str("");
		strstr << l.credit;
		item["label"] = strstr.str();
		data.insert(std::make_pair("credit", item));

		strstr.str("");
		strstr << help::tintegrate::generate_img("misc/browse.png");;
		item["label"] = strstr.str();
		data.insert(std::make_pair("browse", item));

		list->add_row(data);

		tgrid* grid_ptr = list->get_row_grid(list->get_item_count() - 1);
		ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
		toggle->set_data(l.uid);
		
		connect_signal_mouse_left_click(
				find_widget<tbutton>(grid_ptr, "browse", true)
				, boost::bind(
					&tuser_report::detail_group
					, this
					, boost::ref(window)));
	}

	window.invalidate_layout();
}

void tuser_report::swap_page(twindow& window, int page, bool swap)
{
	if (page < MIN_PAGE || page > MAX_PAGE) {
		return;
	}
	int index = page - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	window.alternate_uh(page_panel_, index);
	window.alternate_bh(swap? page_panel_: NULL, index);

	if (page == PLAYER_PASS_PAGE) {
		fill_base(window);

	} else if (page == RANK_PASS_PAGE) {
		fill_rank_pass(window);

	} else if (page == EMPLOYEE_PAGE) {
		fill_employee(window);

	} else if (page == RANK_SCORE_PAGE) {
		fill_score_board(window);

	} else if (page == TITLE_BOARD_PAGE) {
		fill_title_board(window);

	}

	current_page_ = page;
}

} // namespace gui2

