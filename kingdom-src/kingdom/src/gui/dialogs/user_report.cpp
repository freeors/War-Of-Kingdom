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
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/scrollbar_panel.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "preferences_display.hpp"
#include "hero.hpp"
#include "formula_string_utils.hpp"
#include "savegame.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <algorithm>

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

tuser_report::tuser_report(game_display& gui, hero_map& heros)
	: gui_(gui)
	, heros_(heros)
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

	sheet_.insert(std::make_pair(PLAYER_PASS_PAGE, find_widget<ttoggle_button>(&window, "player_pass", false, true)));
	sheet_.insert(std::make_pair(RANK_PASS_PAGE, find_widget<ttoggle_button>(&window, "rank_pass", false, true)));
	sheet_.insert(std::make_pair(RANK_SCORE_PAGE, find_widget<ttoggle_button>(&window, "rank_score", false, true)));
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
	BOOST_FOREACH (const http::pass_statistic pass, passes) {
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
		if (pass.type == NONE_MODE) {
			strstr << dsgettext("wesnoth", "Scenario");
		} else if (pass.type == RPG_MODE) {
			strstr << dsgettext("wesnoth", "RPG");
		} else if (pass.type == TOWER_MODE) {
			strstr << dsgettext("wesnoth", "Tower");
		} else {
			strstr << "Unknown";
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("type", item));

		list->add_row(data);
	}
	window.invalidate_layout();
}

void tuser_report::fill_base(twindow& window)
{
	std::vector<http::pass_statistic> passes = http::list_pass(gui_);
	fill_pass_table(window, passes, false);
}

void tuser_report::fill_biography(twindow& window)
{
	std::vector<http::board_statistic> boards = http::list_board(gui_, http::BOARD_PASS);
	std::vector<http::pass_statistic> passes;
	for (std::vector<http::board_statistic>::const_iterator it = boards.begin(); it != boards.end(); ++ it) {
		http::pass_statistic pass;
		pass.username = it->username;
		pass.create_time = it->pass.create_time;
		pass.duration = it->pass.duration;
		pass.coin = it->pass.coin;
		pass.score = it->pass.score;
		pass.version = it->pass.version;
		pass.type = it->pass.type;
		passes.push_back(pass);
	}
	fill_pass_table(window, passes, true);
}

void tuser_report::fill_score_board(twindow& window)
{
	std::vector<http::board_statistic> scores = http::list_board(gui_, http::BOARD_SCORE);
	std::stringstream strstr;
	tlistbox* list = find_widget<tlistbox>(&window, "score_table", false, true);
	list->clear();
	BOOST_FOREACH (const http::board_statistic score, scores) {
		std::map<std::string, string_map> data;
		string_map item;

		item["label"] = score.username;
		data.insert(std::make_pair("username", item));

		item["label"] = score.score.vip? _("yes") : _("no");
		data.insert(std::make_pair("vip", item));

		strstr.str("");
		strstr << score.score.score;
		item["label"] = strstr.str();
		data.insert(std::make_pair("score", item));

		strstr.str("");
		strstr << score.score.coin;
		item["label"] = strstr.str();
		data.insert(std::make_pair("coin", item));

		list->add_row(data);
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
		fill_biography(window);

	} else if (page == RANK_SCORE_PAGE) {
		fill_score_board(window);

	}

	current_page_ = page;
}

} // namespace gui2

