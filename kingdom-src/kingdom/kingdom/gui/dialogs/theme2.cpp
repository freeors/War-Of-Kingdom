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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/theme2.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "hotkeys.hpp"

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

void tgame_theme::app_pre_show()
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
	hotkey::insert_hotkey(HOTKEY_INTERIOR, "interior", _("Interior"));
	hotkey::insert_hotkey(HOTKEY_TECHNOLOGY_TREE, "technologytree", _("Adjust"));
	hotkey::insert_hotkey(HOTKEY_UPLOAD, "upload", _("Upload"));
	hotkey::insert_hotkey(HOTKEY_FINAL_BATTLE, "finalbattle", _("Final Battle"));
	hotkey::insert_hotkey(HOTKEY_EMPLOY, "employ", _("Employ"));
	hotkey::insert_hotkey(HOTKEY_LIST, "list", _("List"));
	hotkey::insert_hotkey(HOTKEY_TACTIC0, "tactic0", _("Tactic"));
	hotkey::insert_hotkey(HOTKEY_TACTIC1, "tactic1", _("Tactic"));
	hotkey::insert_hotkey(HOTKEY_TACTIC2, "tactic2", _("Tactic"));
	hotkey::insert_hotkey(HOTKEY_BOMB, "bomb", _("Bomb"));
	hotkey::insert_hotkey(HOTKEY_RPG, "rpg", _("RPG"));
	hotkey::insert_hotkey(HOTKEY_RPG_DETAIL, "rpg-detail", _("RPG Detail"));
	hotkey::insert_hotkey(HOTKEY_RPG_TREASURE, "assemble-treasure", _("Assemble Treasure"));
	hotkey::insert_hotkey(HOTKEY_RPG_EXCHANGE, "rpg-exchange", _("RPG Exchange"));
	hotkey::insert_hotkey(HOTKEY_RPG_INDEPENDENCE, "rpg-independence", _("RPG Independence"));
	hotkey::insert_hotkey(HOTKEY_BUILD_M, "build_m", _("Build"));
	hotkey::insert_hotkey(HOTKEY_BUILD, "build", _("Build"));
	hotkey::insert_hotkey(HOTKEY_GUARD, "guard", _("Guard"));
	hotkey::insert_hotkey(HOTKEY_ABOLISH, "abolish", _("Abolish"));
	hotkey::insert_hotkey(HOTKEY_EXTRACT, "extract", _("Extract"));
	hotkey::insert_hotkey(HOTKEY_ADVANCE, "advance", _("Advance"));
	hotkey::insert_hotkey(HOTKEY_DEMOLISH, "demolish", _("Demolish"));
	hotkey::insert_hotkey(HOTKEY_ARMORY, "armory", _("Reform"));
	hotkey::insert_hotkey(HOTKEY_UNIT_DETAIL, "unit_detail", _("Unit Detail"));
	hotkey::insert_hotkey(HOTKEY_PLAY_CARD, "card", _("Play Card"));
	hotkey::insert_hotkey(HOTKEY_RECRUIT, "recruit", _("Recruit"));
	hotkey::insert_hotkey(HOTKEY_EXPEDITE, "expedite", _("Expedite"));
	hotkey::insert_hotkey(HOTKEY_MOVE, "move", _("Move Hero"));
	hotkey::insert_hotkey(HOTKEY_ENDTURN, "endturn", _("End Turn"));
	hotkey::insert_hotkey(HOTKEY_PLAY_REPLAY, "playreplay", _("Play"));
	hotkey::insert_hotkey(HOTKEY_STOP_REPLAY, "stopreplay", _("Play"));

	// rpg
	tbutton* widget = find_widget<tbutton>(window_, "rpg", true, true);
	click_generic_handler(*widget, null_str);

	// chat
	widget = find_widget<tbutton>(window_, "chat", true, true);
	widget->set_surface(image::get_image("buttons/chat.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	// card
	widget = find_widget<tbutton>(window_, "card", true, true);
	widget->set_surface(image::get_image("buttons/card.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	// undo
	widget = find_widget<tbutton>(window_, "undo", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/undo.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(*widget, null_str);
	}

	widget = find_widget<tbutton>(window_, "unit_detail", true, true);
	widget->set_surface(image::get_image("buttons/desc.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	// endturn
	widget = find_widget<tbutton>(window_, "endturn", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/endturn.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(*widget, null_str);
	}

	// play replay
	widget = find_widget<tbutton>(window_, "playreplay", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/ctrl-play.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(*widget, null_str);
	}
	// pause replay
	widget = find_widget<tbutton>(window_, "stopreplay", false, false);
	if (widget) {
		widget->set_surface(image::get_image("buttons/ctrl-pause.png"), widget->fix_width(), widget->fix_height());
		click_generic_handler(*widget, null_str);
	}

	widget = find_widget<tbutton>(window_, "tactic0", true, true);
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "tactic2", true, true);
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "tactic1", true, true);
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "bomb", true, true);
	click_generic_handler(*widget, null_str);
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
	// prepare status report.
	reports_.insert(std::make_pair(tgame_theme::VILLAGES, "villages"));
	reports_.insert(std::make_pair(tgame_theme::POSITION, "position"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_EDITOR_TOOL_PAINT, "editor-tool-paint", _("Paint Tool"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_TOOL_FILL, "editor-tool-fill", _("Fill Tool"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_TOOL_SELECT, "editor-tool-select", _("Selection Tool"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_TOOL_STARTING_POSITION, "editor-tool-starting-position", _("Set Starting Positions Tool"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_PASTE, "editor-paste", _("Paste"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_MAP, "editor-map", _("Map"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_TERRAIN_GROUP, "editor-terrain-group", _("Switch terrain group"));
	hotkey::insert_hotkey(HOTKEY_EDITOR_BRUSH, "editor-brush", _("Switch brush"));

	tbutton* widget = find_widget<tbutton>(window_, "up", true, true);
	widget->set_surface(image::get_image("misc/arrow-up.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-terrain-group", true, true);
	widget->set_surface(image::get_image("buttons/button_selectable_45_border-pressed-both.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "down", true, true);
	widget->set_surface(image::get_image("misc/arrow-down.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-map", true, true);
	widget->set_surface(image::get_image("buttons/map.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "system", true, true);
	widget->set_surface(image::get_image("buttons/system.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "editor-brush", true, true);
	widget->set_surface(image::get_image("buttons/button_selectable_45_border-pressed-both.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "undo", true, true);
	widget->set_surface(image::get_image("buttons/undo.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "redo", true, true);
	widget->set_surface(image::get_image("buttons/redo.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "zoomin", true, true);
	widget->set_surface(image::get_image("buttons/zoomin.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = find_widget<tbutton>(window_, "zoomout", true, true);
	widget->set_surface(image::get_image("buttons/zoomout.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);
}

} //end namespace gui2
