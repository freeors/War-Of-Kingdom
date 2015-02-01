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
#include "gui/dialogs/mkwin_theme.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"

#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "hotkeys.hpp"

#include "gettext.hpp"

#include <boost/bind.hpp>

namespace gui2 {

tmkwin_theme::tmkwin_theme(mkwin_display& disp, mkwin_controller& controller, const config& cfg)
	: ttheme(disp, controller, cfg)
	, controller_(controller)
{
}

void tmkwin_theme::app_pre_show()
{
	// prepare status report.
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

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_SELECT, "select", _("Select"));
	hotkey::insert_hotkey(HOTKEY_STATUS, "status", _("Status"));
	hotkey::insert_hotkey(HOTKEY_GRID, "grid", _("Grid"));
	hotkey::insert_hotkey(HOTKEY_RCLICK, "rclick", _("Right Click"));

	hotkey::insert_hotkey(HOTKEY_RUN, "run", _("Run"));
	hotkey::insert_hotkey(HOTKEY_SETTING, "setting", _("Setting"));
	hotkey::insert_hotkey(HOTKEY_SPECIAL_SETTING, "special_setting", _("Special Setting"));
	hotkey::insert_hotkey(HOTKEY_LINKED_GROUP, "linked_group", _("Linked group"));
	hotkey::insert_hotkey(HOTKEY_ERASE, "erase", _("Erase"));

	hotkey::insert_hotkey(HOTKEY_INSERT_TOP, "insert_top", _("Insert top"));
	hotkey::insert_hotkey(HOTKEY_INSERT_BOTTOM, "insert_bottom", _("Inert bottom"));
	hotkey::insert_hotkey(HOTKEY_ERASE_ROW, "erase_row", _("Erase row"));
	hotkey::insert_hotkey(HOTKEY_INSERT_LEFT, "insert_left", _("Insert left"));
	hotkey::insert_hotkey(HOTKEY_INSERT_RIGHT, "insert_right", _("Insert right"));
	hotkey::insert_hotkey(HOTKEY_ERASE_COLUMN, "erase_column", _("Erase column"));

	hotkey::insert_hotkey(HOTKEY_INSERT_CHILD, "insert_child", _("Insert child"));
	hotkey::insert_hotkey(HOTKEY_ERASE_CHILD, "erase_child", _("Erase child"));


	tbutton* widget = find_widget<tbutton>(window_, "select", true, true);
	widget->set_surface(image::get_image("buttons/studio/select.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "status", true, true);
	widget->set_surface(image::get_image("buttons/status.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "grid", true, true);
	widget->set_surface(image::get_image("buttons/studio/grid.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "rclick", true, true);
	click_generic_handler(*widget, null_str);
}

} //end namespace gui2
