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

#include "gui/dialogs/title_screen.hpp"

#include "display.hpp"
#include "game_config.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
// #include "log.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/create_hero.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "preferences_display.hpp"

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

REGISTER_DIALOG(title_screen)

static bool hotkey(twindow& window, const ttitle_screen::tresult result)
{
	window.set_retval(static_cast<twindow::tretval>(result));

	return true;
}

ttitle_screen::ttitle_screen(display& gui, hero_map& heros, hero& player_hero)
	: gui_(gui)
	, heros_(heros)
	, player_hero_(player_hero)
{
}

ttitle_screen::~ttitle_screen()
{
}

static void animate_logo(
		  unsigned long& timer_id
		, unsigned& percentage
		, tprogress_bar& progress_bar
		, twindow& window)
{
	assert(percentage <= 100);
	++percentage;
	progress_bar.set_percentage(percentage);

	/*
	 * The progress bar may overlap (actually underlap) other widgets, which
	 * the update invalidates, so make sure the whole window is redrawn to fix
	 * this possible problem. Of course this is expensive but the logo is
	 * animated once so the cost is only once.
	 */
	window.set_dirty();

	if (percentage == 100) {
		remove_timer(timer_id);
		timer_id = 0;
	}
}

static bool fullscreen(CVideo& video)
{
	preferences::set_fullscreen(video , !preferences::fullscreen());

	// Setting to fullscreen doesn't seem to generate a resize event.
	const SDL_Rect& rect = screen_area();

	SDL_Event event;
	event.type = SDL_VIDEORESIZE;
	event.resize.type = SDL_VIDEORESIZE;
	event.resize.w = rect.w;
	event.resize.h = rect.h;

	SDL_PushEvent(&event);

	return true;
}

void ttitle_screen::post_build(CVideo& video, twindow& window)
{
}

static int nb_items = 10;
static const char* menu_items[] = {
	"campaign",
	"randommap",
	"multiplayer",
	"load",
	"language",
	"preferences",
	"tutorial",
	"editor",
	"credits",
	"quit"
};

const size_t max_login_size = 20;

void ttitle_screen::pre_show(CVideo& video, twindow& window)
{
	set_restore(false);
	window.set_click_dismiss(false);
	window.set_enter_disabled(true);
	window.set_escape_disabled(true);

	/**** Set the version number ****/
	if (tcontrol* control
			= find_widget<tcontrol>(&window, "revision_number", false, false)) {

		control->set_label(_("Version ") + game_config::revision);
		// control->set_label(_("Version ") + game_config::revision + "-alpha");
		// control->set_label(_("Version ") + game_config::revision + "-beta");
	}
	window.canvas()[0].set_variable("revision_number", variant(_("Version") + std::string(" ") + game_config::revision));

	if (game_config::images::game_title.empty()) {
		
	} else {
		window.canvas()[0].set_variable("background_image",
			variant(game_config::images::game_title));
	}

	/***** Set the logo *****/
	tcontrol* logo = find_widget<tcontrol>(&window, "logo", false, false);
	if (logo) {
		logo->set_label("misc/logo.png");
	}

	std::string player_name;
	tbutton* b = find_widget<tbutton>(&window, "player", false, false);
	if (b) {
		for (int i = 0; i < 4; i ++) {
			player_name = player_hero_.image(true);
			b->canvas()[i].set_variable("image", variant(player_name));
		}
	}
	player_name = player_hero_.name();
	ttext_box* user_widget = find_widget<ttext_box>(&window, "player_name", false, true);
	user_widget->set_value(player_name);
	// user_widget->set_maximum_length(max_login_size);
	user_widget->set_active(false);

	for (int item = 0; item < nb_items; item ++) {
		tbutton* b = find_widget<tbutton>(&window, menu_items[item], false, false);
		if (!b) {
			continue;
		}
		std::string str = std::string("icons/") + menu_items[item] + ".png";
		int i;
		for (i = 0; i < 4; i ++) {
			b->canvas()[i].set_variable("image", variant(str));
		}
	}

	if (game_config::tiny_gui) {
		tbutton* b = find_widget<tbutton>(&window, "editor", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}
		b = find_widget<tbutton>(&window, "help", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}
		b = find_widget<tbutton>(&window, "credits", false, false);
		if (b) {
			b->set_visible(twidget::INVISIBLE);
		}
		b = find_widget<tbutton>(&window, "quit", false, false);
		if (b) {
			// b->set_visible(twidget::INVISIBLE);
		}
	}

#if defined(__APPLE__) && TARGET_OS_IPHONE
	b = find_widget<tbutton>(&window, "editor", false, false);
	if (b) {
		b->set_visible(twidget::INVISIBLE);
	}
	b = find_widget<tbutton>(&window, "help", false, false);
	if (b) {
		b->set_visible(twidget::INVISIBLE);
	}
	b = find_widget<tbutton>(&window, "quit", false, false);
	if (b) {
		b->set_visible(twidget::INVISIBLE);
	}
#endif

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "player", false)
		, boost::bind(
		&ttitle_screen::player
			, this
			, boost::ref(window)));
}

void ttitle_screen::post_show(twindow& window)
{
}

void ttitle_screen::player(twindow& window)
{
	gui2::tcreate_hero dlg(gui_, heros_, player_hero_);
	dlg.show(gui_.video());
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	std::string player_name;
	tbutton* b = find_widget<tbutton>(&window, "player", false, false);
	if (b) {
		for (int i = 0; i < 4; i ++) {
			player_name = player_hero_.image(true);
			b->canvas()[i].set_variable("image", variant(player_name));
		}
		b->set_dirty();
	}
	player_name = player_hero_.name();
	ttext_box* user_widget = find_widget<ttext_box>(&window, "player_name", false, true);
	user_widget->set_value(player_name);
}

} // namespace gui2

