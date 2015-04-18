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
#include "gui/widgets/listbox.hpp"

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
	hotkey::insert_hotkey(HOTKEY_RECT_SETTING, "rect_setting", _("Rect Setting"));
	hotkey::insert_hotkey(HOTKEY_SPECIAL_SETTING, "special_setting", _("Special Setting"));
	hotkey::insert_hotkey(HOTKEY_LINKED_GROUP, "linked_group", _("Linked group"));
	hotkey::insert_hotkey(HOTKEY_MODE_SETTING, "mode_setting", _("Mode Setting"));
	hotkey::insert_hotkey(HOTKEY_ERASE, "erase", _("Erase"));

	hotkey::insert_hotkey(HOTKEY_INSERT_TOP, "insert_top", _("Insert top"));
	hotkey::insert_hotkey(HOTKEY_INSERT_BOTTOM, "insert_bottom", _("Inert bottom"));
	hotkey::insert_hotkey(HOTKEY_ERASE_ROW, "erase_row", _("Erase row"));
	hotkey::insert_hotkey(HOTKEY_INSERT_LEFT, "insert_left", _("Insert left"));
	hotkey::insert_hotkey(HOTKEY_INSERT_RIGHT, "insert_right", _("Insert right"));
	hotkey::insert_hotkey(HOTKEY_ERASE_COLUMN, "erase_column", _("Erase column"));

	hotkey::insert_hotkey(HOTKEY_INSERT_CHILD, "insert_child", _("Insert child"));
	hotkey::insert_hotkey(HOTKEY_ERASE_CHILD, "erase_child", _("Erase child"));

	// widget page
	tbutton* widget = dynamic_cast<tbutton*>(get_object("select"));
	widget->set_surface(image::get_image("buttons/studio/select.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("status"));
	widget->set_surface(image::get_image("buttons/status.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("grid"));
	widget->set_surface(image::get_image("buttons/studio/grid.png"), widget->fix_width(), widget->fix_height());
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("rclick"));
	click_generic_handler(*widget, null_str);

	tlistbox* list = dynamic_cast<tlistbox*>(get_object("object-list"));
	list->set_callback_value_change(dialog_callback<tmkwin_theme, &tmkwin_theme::object_selected>);

	load_widget_page();
}

void tmkwin_theme::load_widget_page()
{
	set_object_visible("widget_layer", twidget::VISIBLE);
	set_object_visible("object_layer", twidget::INVISIBLE);
}

void tmkwin_theme::load_object_page(const unit_map& units)
{
	set_object_visible("widget_layer", twidget::INVISIBLE);
	set_object_visible("object_layer", twidget::VISIBLE);

	fill_object_list(units);
}

void tmkwin_theme::object_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "object-list", false);
	int cursel = list.get_selected_row();
	if (cursel < 0) {
		return;
	}
	tgrid* grid = list.get_row_grid(cursel);

	unit* u = reinterpret_cast<unit*>(grid->cookie());
	controller_.select_unit(u);
}

void tmkwin_theme::fill_object_list(const unit_map& units)
{
	tlistbox* list = dynamic_cast<tlistbox*>(get_object("object-list"));
	list->clear();

	const map_location& selected_hex = controller_.selected_hex();
	std::stringstream ss;
	int index = 0;
	int cursel = 0;
	for (unit_map::const_iterator it = units.begin(); it != units.end(); ++ it) {
		unit& u = *dynamic_cast<unit*>(&*it);
		if (u.type() != unit::WIDGET) {
			continue;
		}
		if (u.get_location() == selected_hex) {
			cursel = index;
		}
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = str_cast(index ++);
		list_item_item.insert(std::make_pair("number", list_item));

		list_item["label"] = u.cell().id;
		list_item_item.insert(std::make_pair("id", list_item));

		ss.str("");
		const SDL_Rect& rect = u.get_rect();
		// ss << rect.x << "," << rect.y << "," << rect.w << "," << rect.h;
		ss << rect.x << "," << rect.y;
		list_item["label"] = ss.str();
		list_item_item.insert(std::make_pair("rect", list_item));

		list->add_row(list_item_item);

		tgrid* grid = list->get_row_grid(list->get_item_count() - 1);
		grid->set_cookie(reinterpret_cast<void*>(&u));
	}
	if (list->get_item_count()) {
		list->select_row(cursel);
	}
	list->invalidate_layout(true);
	// window.invalidate_layout();
}

} //end namespace gui2
