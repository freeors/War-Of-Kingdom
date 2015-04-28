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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/help_screen.hpp"

#include "game_display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "preferences_display.hpp"
#include "help.hpp"
#include "version.hpp"
#include "multiplayer.hpp"
#include <time.h>
#include "unit_display.hpp"

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

REGISTER_DIALOG(help_screen)

thelp_screen::thelp_screen(display& disp, hero_map& heros, hero& player_hero)
	: disp_(disp)
	, heros_(heros)
	, player_hero_(player_hero)
	, title_screen_anim_id_(-1)
{
}

thelp_screen::~thelp_screen()
{
	if (title_screen_anim_id_ >= 0) {
		disp_.erase_area_anim(title_screen_anim_id_);
	}
}

void thelp_screen::post_build(CVideo& video, twindow& window)
{
}

static const char* menu_items[] = {
	"tutorial",
	"guide",
	"history",
	"close"
};
static int nb_items = sizeof(menu_items) / sizeof(menu_items[0]);

void thelp_screen::pre_show(CVideo& video, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	tbutton* b;
	for (int item = 0; item < nb_items; item ++) {
		b = find_widget<tbutton>(&window, menu_items[item], false, false);
		if (!b) {
			continue;
		}
		std::string str = std::string("icons/") + menu_items[item] + ".png";

		for (int i = 0; i < 4; i ++) {
			b->canvas()[i].set_variable("image", variant(str));
		}
	}

	for (int item = 0; item < nb_items; item ++) {
		std::string id = menu_items[item];
		int retval = twindow::NONE;
		if (id == "tutorial") {
			retval = TUTORIAL;

		} else if (id == "guide") {
			retval = GUIDE;

		} else if (id == "history") {
			retval = HISTORY;

		} else if (id == "close") {
			retval = CLOSE;
		} 

		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, id, false)
			, boost::bind(
				&thelp_screen::set_retval
				, this
				, boost::ref(window)
				, retval));
	}
}

void thelp_screen::post_show(twindow& window)
{
}

void thelp_screen::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

} // namespace gui2

