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

#include "gui/dialogs/hero.hpp"

#include "display.hpp"
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
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "preferences_display.hpp"
#include "hero.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

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

REGISTER_DIALOG(hero)

thero::thero(hero_map& heros, hero& h)
	: heros_(heros)
	, h_(h)
	, current_page_(NONE_PAGE)
	, page_panel_(NULL)
{
}

thero::~thero()
{
}

void thero::post_build(CVideo& video, twindow& window)
{
}

void thero::sheet_toggled(twidget* widget)
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

void thero::set_label_int(twindow& window, const std::string& id, int value)
{
	tlabel* widget = find_widget<tlabel>(&window, id, false, true);
	widget->set_label(lexical_cast<std::string>(value));
}

void thero::pre_show(CVideo& video, twindow& window)
{
	std::stringstream strstr;

	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	tcontrol* control = find_widget<tcontrol>(&window, "portrait", false, true);
	control->set_label(h_.image(true));

	control = find_widget<tcontrol>(&window, "utype", false, true);
	if (h_.utype_ != HEROS_NO_UTYPE) {
		control->set_label(unit_types.keytype(h_.utype_)->icon());
	} else {
		control->set_label("");
	}
	
	/**** Set the version number ****/
	tlabel* label = find_widget<tlabel>(&window, "name", false, true);
	strstr.str("");
	strstr << h_.name();
	strstr << "(" << hero::gender_str(h_.gender_) << ")";
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "character", false, true);
	if (h_.character_ != HEROS_NO_CHARACTER) {
		label->set_label(unit_types.character(h_.character_).name());
	} else {
		label->set_label("      ");
	}

	sheet_.insert(std::make_pair(BASE_PAGE, find_widget<ttoggle_button>(&window, "base", false, true)));
	sheet_.insert(std::make_pair(BIOGRAPHY_PAGE, find_widget<ttoggle_button>(&window, "biography", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&thero::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, BASE_PAGE, false);
	sheet_.begin()->second->set_value(true);
}

void thero::post_show(twindow& window)
{
}

void thero::fill_base(twindow& window)
{
	std::stringstream strstr;
	tcontrol* control;

	set_label_int(window, "leadership", fxptoi9(h_.leadership_));
	set_label_int(window, "force", fxptoi9(h_.force_));
	set_label_int(window, "intellect", fxptoi9(h_.intellect_));
	set_label_int(window, "politics", fxptoi9(h_.politics_));
	set_label_int(window, "charm", fxptoi9(h_.charm_));

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		strstr.str("");
		strstr << "text_arms" << i;
		control = find_widget<tcontrol>(&window, strstr.str(), false, false);
		if (control) {
			control->set_label(hero::adaptability_str2(h_.arms_[i]));
		}
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		strstr.str("");
		strstr << "text_skill" << i;
		control = find_widget<tcontrol>(&window, strstr.str(), false, false);
		if (control) {
			control->set_label(hero::adaptability_str2(h_.skill_[i]));
		}
	}

	tlabel* label = find_widget<tlabel>(&window, "feature", false, true);
	label->set_label(hero::feature_str(h_.feature_));

	label = find_widget<tlabel>(&window, "feature_description", false, true);
	label->set_label(hero::feature_desc_str(h_.feature_));

	label = find_widget<tlabel>(&window, "tactic", false, true);
	if (h_.tactic_ != HEROS_NO_TACTIC) {
		label->set_label(unit_types.tactic(h_.tactic_).name());
	} else {
		label->set_label(" ");
	}
	label = find_widget<tlabel>(&window, "tactic_description", false, true);
	if (h_.tactic_ != HEROS_NO_TACTIC) {
		label->set_label(unit_types.tactic(h_.tactic_).description());
	} else {
		label->set_label("");
	}
}

void thero::fill_biography(twindow& window)
{
	std::stringstream strstr;

	ttext_box* text_box = find_widget<ttext_box>(&window, "treasure", false, true);
	if (h_.treasure_ != HEROS_NO_TREASURE) {
		const ttreasure& t = unit_types.treasure(h_.treasure_);
		strstr.str("");
		strstr << t.name();
		strstr << "(";
		strstr << hero::feature_str(t.feature());
		strstr << ")";
		text_box->set_value(strstr.str());
	}
	text_box->set_active(false);

	tscroll_label* biography = find_widget<tscroll_label>(&window, "biography_text", false, true);
	biography->set_label(h_.biography());
}

void thero::swap_page(twindow& window, int page, bool swap)
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

	if (page == BASE_PAGE) {
		fill_base(window);

	} else if (page == BIOGRAPHY_PAGE) {
		fill_biography(window);

	}	
}

} // namespace gui2

