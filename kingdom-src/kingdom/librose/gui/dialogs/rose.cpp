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

#include "gui/dialogs/rose.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "preferences_display.hpp"
#include "help.hpp"
#include "version.hpp"
#include <hero.hpp>
#include <time.h>
#include "sound.hpp"

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

REGISTER_DIALOG(rose)

trose::trose(display& disp, hero& player_hero)
	: disp_(disp)
	, player_hero_(player_hero)
	, window_(NULL)
{
}

void trose::post_build(CVideo& video, twindow& window)
{
}

static const char* menu_items[] = {
	"report",
	"edit_dialog",
	"player",
	"side",
	"multiplayer",
	"edit_theme",
	"signin",
	"design",
	"shop",
	"language",
	"message",
	"preferences",
	"help",
	"editor",
	"quit"
};
static int nb_items = sizeof(menu_items) / sizeof(menu_items[0]);

static std::vector<int> ids;
void trose::pre_show(CVideo& video, twindow& window)
{
	ids.clear();
	window_ = &window;

	set_restore(false);
	window.set_click_dismiss(false);
	window.set_escape_disabled(true);

	std::stringstream strstr;
	std::string color = game_config::local_only? "red": "green";

	strstr.str("");
	tlabel* label = find_widget<tlabel>(&window, "coin", true, true);
	strstr << tintegrate::generate_format(group.coin(), color, 17);
	label->set_label(strstr.str());

	strstr.str("");
	label = find_widget<tlabel>(&window, "score", true, true);
	strstr << tintegrate::generate_format(group.score(), color, 17);
	label->set_label(strstr.str());

	strstr.str("");
	label = find_widget<tlabel>(&window, "signin_data", true, true);
	if (!game_config::local_only) {
		strstr << tintegrate::generate_format(group.signin().continue_days, "green", 17) << "/";
		if (!group.signin().break_days) {
			color = "green";
		} else if (group.signin().break_days < game_config::max_breaks) {
			color = "yellow";
		} else {
			color = "red";
		}
		strstr << tintegrate::generate_format(group.signin().break_days, color, 17);
	} else {
		strstr << tintegrate::generate_format("---/-", "white", 17);
	}
	label->set_label(strstr.str());

	tcontrol* control = find_widget<tcontrol>(&window, "icon_vip", true, true);

	// Set the version number
	control = find_widget<tcontrol>(&window, "revision_number", false, false);
	if (control) {
		control->set_label(_("V") + game_config::version);
		// control->set_label(_("v") + game_config::version + "-alpha");
	}
	window.canvas()[0].set_variable("revision_number", variant(_("Version") + std::string(" ") + game_config::version));

	if (!game_config::images::game_title.empty()) {
		window.canvas()[0].set_variable("background_image",	variant(game_config::images::game_title));
	}

	/***** Set the logo *****/
	tcontrol* logo = find_widget<tcontrol>(&window, "logo", false, false);
	if (logo) {
		logo->set_label(game_config::logo_png);
	}

	label = find_widget<tlabel>(&window, "player_name", false, true);
	label->set_label(player_hero_.name());

	tbutton* b;
	for (int item = 0; item < nb_items; item ++) {
		b = find_widget<tbutton>(&window, menu_items[item], false, false);
		if (!b) {
			continue;
		}
		std::string str;
		if (!strcmp(menu_items[item], "player")) {
			str = player_hero_.image(true);

		} else if (!strcmp(menu_items[item], "edit_dialog")) {
			str = std::string("icons/studio/") + menu_items[item] + ".png";

		} else if (!strcmp(menu_items[item], "edit_theme")) {
			str = std::string("icons/studio/") + menu_items[item] + ".png";

		} else if (!strcmp(menu_items[item], "signin")) {
			if (group.signin().today) {
				str = std::string("icons/") + "signin-ok" + ".png";
			} else {
				str = std::string("icons/") + "signin" + ".png";
			}

		} else {
			if (!strcmp(menu_items[item], "message")) {
				if (group.message_count()) {
					b->set_label("misc/red-dot12.png");
				}
			}
			str = std::string("icons/") + menu_items[item] + ".png";
		}

		for (int i = 0; i < 4; i ++) {
			b->canvas()[i].set_variable("image", variant(str));
		}
	}

	if (game_config::tiny_gui) {
		b = find_widget<tbutton>(&window, "design", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}
		b = find_widget<tbutton>(&window, "editor", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}

	} else if (!preferences::developer()) {
		b = find_widget<tbutton>(&window, "design", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}
	}

#if defined(__APPLE__) && TARGET_OS_IPHONE
	b = find_widget<tbutton>(&window, "quit", false, false);
	if (b) {
		b->set_visible(twidget::INVISIBLE);
	}
#endif

	for (int item = 0; item < nb_items; item ++) {
		std::string id = menu_items[item];
		int retval = twindow::NONE;
		if (id == "editor") {
			retval = START_MAP_EDITOR;
		} else if (id == "quit") {
			retval = QUIT_GAME;
		} else if (id == "help") {
			retval = HELP;
		} else if (id == "edit_dialog") {
			retval = EDIT_DIALOG;
		} else if (id == "player") {
			retval = PLAYER;
		} else if (id == "side") {
			retval = PLAYER_SIDE;
		} else if (id == "multiplayer") {
			retval = MULTIPLAYER;
		} else if (id == "edit_theme") {
			retval = EDIT_THEME;
		} else if (id == "report") {
			retval = REPORT;
		} else if (id == "language") {
			retval = CHANGE_LANGUAGE;
		} else if (id == "message") {
			retval = MESSAGE;
		} else if (id == "preferences") {
			retval = EDIT_PREFERENCES;
		} else if (id == "signin") {
			retval = SIGNIN;
		} else if (id == "design") {
			retval = DESIGN;
		} else if (id == "shop") {
			retval = INAPP_PURCHASE;
		}

		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, id, false)
			, boost::bind(
				&trose::set_retval
				, this
				, boost::ref(window)
				, retval));
	}

	join();
}

void trose::post_show(twindow& window)
{
}

bool trose::handle(tlobby::ttype type, const config& data)
{
	if (type != tlobby::t_data) {
		return false;
	}
	if (const config& c = data.child("whisper")) {
		tbutton* b = find_widget<tbutton>(window_, "message", false, false);
		if (b->label().empty()) {
			b->set_label("misc/red-dot12.png");
		}
		sound::play_UI_sound(game_config::sounds::receive_message);
	}
	return false;
}

void trose::set_retval(twindow& window, int retval)
{
	if (retval == INAPP_PURCHASE) {
		tbutton* b = find_widget<tbutton>(&window, "player", false, false);
		config cfg;
		cfg["id"] = "focus";
		int id = b->insert_animation(cfg, false);
		ids.push_back(id);

		return;

	} else if (retval == REPORT) {
		tbutton* b = find_widget<tbutton>(&window, "player", false, false);
		config cfg;
		cfg["id"] = "focus";
		int id = b->insert_animation(cfg, true);
		ids.push_back(id);

		return;

	} else if (retval == PLAYER_SIDE) {
		if (!ids.empty()) {
			tbutton* b = find_widget<tbutton>(&window, "player", false, false);
			b->erase_animation(ids.front());
			ids.erase(ids.begin());
		}

		return;

	}

	window.set_retval(retval);
}

} // namespace gui2

