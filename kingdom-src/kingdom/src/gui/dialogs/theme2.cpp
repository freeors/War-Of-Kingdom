/* $Id: lobby_player_info.cpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/theme2.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"

#include "play_controller.hpp"
#include "editor/editor_display.hpp"
#include "editor/editor_controller.hpp"

#include "gettext.hpp"

#include <boost/bind.hpp>

namespace gui2 {

tgame_theme::tgame_theme(game_display& disp, play_controller& controller, const config& cfg)
	: ttheme(disp, controller, cfg)
	, controller_(controller)
{
}

void click_generic_handler(controller_base& controller, twidget& widget, const std::string& minor)
{
	connect_signal_mouse_left_click(
		widget
		, boost::bind(
			&controller_base::execute_command
			, &controller
			, hotkey::get_hotkey(widget.id()).get_id()
			, -1
			, null_str));
}

void tgame_theme::app_pre_show()
{
	// prepare status.
	reports_.insert(std::make_pair(UNIT_NAME, "unit_name"));
	reports_.insert(std::make_pair(UNIT_TYPE, "unit_type"));
	reports_.insert(std::make_pair(UNIT_STATUS, "unit_status"));
	reports_.insert(std::make_pair(UNIT_HP, "unit_hp"));
	reports_.insert(std::make_pair(UNIT_XP, "unit_xp"));
	reports_.insert(std::make_pair(UNIT_SECOND, "unit_second"));
	reports_.insert(std::make_pair(UNIT_IMAGE, "unit_image"));
	reports_.insert(std::make_pair(TIME_OF_DAY, "time_of_day"));
	reports_.insert(std::make_pair(TURN, "turn"));
	reports_.insert(std::make_pair(GOLD, "gold"));
	reports_.insert(std::make_pair(VILLAGES, "villages"));
	reports_.insert(std::make_pair(UPKEEP, "upkeep"));
	reports_.insert(std::make_pair(INCOME, "income"));
	reports_.insert(std::make_pair(TECH_INCOME, "tech_income"));
	reports_.insert(std::make_pair(TACTIC, "tactic"));
	reports_.insert(std::make_pair(POSITION, "position"));
	reports_.insert(std::make_pair(STRATUM, "stratum"));
	reports_.insert(std::make_pair(MERITORIOUS, "meritorious"));
	reports_.insert(std::make_pair(SIDE_PLAYING, "side_playing"));
	reports_.insert(std::make_pair(OBSERVERS, "observers"));
	reports_.insert(std::make_pair(REPORT_COUNTDOWN, "report_countdown"));
	reports_.insert(std::make_pair(REPORT_CLOCK, "report_clock"));
	reports_.insert(std::make_pair(EDITOR_SELECTED_TERRAIN, "selected_terrain"));
	reports_.insert(std::make_pair(EDITOR_LEFT_BUTTON_FUNCTION, "edit_left_button_function"));
	reports_.insert(std::make_pair(EDITOR_TOOL_HINT, "editor_tool_hint"));

	// rpg
	tbutton* widget = find_widget<tbutton>(window_, "rpg", true, true);
	click_generic_handler(controller_, *widget, null_str);

	// chat
	widget = find_widget<tbutton>(window_, "chat", true, true);
	widget->set_surface(image::get_image("buttons/chat.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	// card
	widget = find_widget<tbutton>(window_, "card", true, true);
	widget->set_surface(image::get_image("buttons/card.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	// undo
	widget = find_widget<tbutton>(window_, "undo", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/undo.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(controller_, *widget, null_str);
	}

	widget = find_widget<tbutton>(window_, "unit_detail", true, true);
	widget->set_surface(image::get_image("buttons/desc.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	// endturn
	widget = find_widget<tbutton>(window_, "endturn", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/endturn.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(controller_, *widget, null_str);
	}

	// play replay
	widget = find_widget<tbutton>(window_, "playreplay", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/ctrl-play.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(controller_, *widget, null_str);
	}
	// pause replay
	widget = find_widget<tbutton>(window_, "stopreplay", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/ctrl-pause.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(controller_, *widget, null_str);
	}

	widget = find_widget<tbutton>(window_, "tactic0", true, true);
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "tactic2", true, true);
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "tactic1", true, true);
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "bomb", true, true);
	click_generic_handler(controller_, *widget, null_str);
}

void tgame_theme::report_ptr(treport** unit_ptr, treport** hero_ptr, treport**)
{
	*unit_ptr = find_widget<treport>(window_, "access-unit", false, false);
	*hero_ptr = find_widget<treport>(window_, "access-hero", false, false);
}

teditor_theme::teditor_theme(editor::editor_display& disp, editor::editor_controller& controller, const config& cfg)
	: ttheme(disp, controller, cfg)
	, controller_(controller)
{
}

void teditor_theme::app_pre_show()
{
	// prepare status.
	reports_.insert(std::make_pair(tgame_theme::VILLAGES, "villages"));
	reports_.insert(std::make_pair(tgame_theme::POSITION, "position"));

	tbutton* widget = find_widget<tbutton>(window_, "up", true, true);
	widget->set_surface(image::get_image("buttons/arrow-up.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-terrain-group", true, true);
	widget->set_surface(image::get_image("buttons/button_selectable_45_border-pressed-both.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "down", true, true);
	widget->set_surface(image::get_image("buttons/arrow-down.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-map", true, true);
	widget->set_surface(image::get_image("buttons/map.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "system", true, true);
	widget->set_surface(image::get_image("buttons/system.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-brush", true, true);
	widget->set_surface(image::get_image("buttons/button_selectable_45_border-pressed-both.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "undo", true, true);
	widget->set_surface(image::get_image("buttons/undo_button_editor.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "redo", true, true);
	widget->set_surface(image::get_image("buttons/redo_button_editor.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "zoomin", true, true);
	widget->set_surface(image::get_image("buttons/zoom_in_button_editor.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);

	widget = find_widget<tbutton>(window_, "zoomout", true, true);
	widget->set_surface(image::get_image("buttons/zoom_out_button_editor.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(controller_, *widget, null_str);
}

} //end namespace gui2
