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

#include "gui/dialogs/story_screen.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/auxiliary/tips.hpp"
#include "gui/dialogs/debug_clock.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/multi_page.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "preferences_display.hpp"

#include <boost/bind.hpp>

#include <algorithm>

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)
#define WRN_CF LOG_STREAM(warn, log_config)

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

REGISTER_DIALOG(story_screen)

int count_segments(const config::const_child_itors& story)
{
	config::const_child_iterator itor = story.first;
	int count = 0;
	while(itor != story.second) {
		++itor;
		++count;
	}
	return count;
}

void show_story(display& disp, const std::string& scenario_name, const config::const_child_itors& story)
{
	const int total_segments = count_segments(story);
	int segment_count = 0;
	config::const_child_iterator itor = story.first;
	// storyscreen::START_POSITION startpos = storyscreen::START_BEGINNING;
	while (itor != story.second) {
		gui2::tstory_screen dlg(*itor);
		dlg.show(disp.video());

		tstory_screen::legacy_result result = dlg.get_legacy_result();

		switch (result) {
		case tstory_screen::NEXT:
			if (itor != story.second) {
				++ itor;
				++ segment_count;
				// startpos = storyscreen::START_BEGINNING;
			}
			break;
		case tstory_screen::BACK:
			if (itor != story.first) {
				-- itor;
				-- segment_count;
				// startpos = storyscreen::START_END;
			}
			break;
		case tstory_screen::QUIT:
			return;
		}
	}
}

tstory_screen::tstory_screen(const config& part)
	: part_(part)
	, logo_timer_id_(0)
	, legacy_result_(QUIT)
{
}

tstory_screen::~tstory_screen()
{
	if(logo_timer_id_) {
		remove_timer(logo_timer_id_);
	}
}

void tstory_screen::post_build(CVideo& video, twindow& window)
{
}

void tstory_screen::pre_show(CVideo& video, twindow& window)
{
	// set_restore(false);
	window.set_click_dismiss(false);
	window.set_enter_disabled(true);
	window.set_escape_disabled(true);


	if (!part_["story"].str().empty()) {
		window.canvas()[0].set_variable("background_image", variant(part_["story"].str()));
	} else {
		window.canvas()[0].set_variable("background_image", variant(game_config::images::game_title));
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "next_tip", false)
			, boost::bind(
				  &tstory_screen::update_tip
				, this
				, boost::ref(window)
				, true));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "previous_tip", false)
			, boost::bind(
				  &tstory_screen::update_tip
				, this
				, boost::ref(window)
				, false));

}

void tstory_screen::update_tip(twindow& window, const bool previous)
{
	
}

} // namespace gui2

