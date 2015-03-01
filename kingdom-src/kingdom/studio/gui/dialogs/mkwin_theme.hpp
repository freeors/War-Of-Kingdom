/* $Id: lobby_player_info.hpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#ifndef GUI_DIALOGS_MKWIN_THEME_HPP_INCLUDED
#define GUI_DIALOGS_MKWIN_THEME_HPP_INCLUDED

#include "gui/dialogs/theme.hpp"
#include "config.hpp"
#include "gui/widgets/widget.hpp"

class mkwin_display;
class mkwin_controller;
class unit_map;

namespace gui2 {

class treport;

class tmkwin_theme: public ttheme
{
public:
	enum { UNIT_NAME, UNIT_TYPE,
		    UNIT_STATUS,
		    UNIT_HP, UNIT_XP,
		    UNIT_SECOND,
		    UNIT_IMAGE, 
			
			TIME_OF_DAY,
		    TURN, GOLD, VILLAGES, UPKEEP,
		    INCOME, TECH_INCOME, TACTIC, POSITION, STRATUM, MERITORIOUS, SIDE_PLAYING, OBSERVERS,
			REPORT_COUNTDOWN, REPORT_CLOCK, EDITOR_SELECTED_TERRAIN, 
			EDITOR_LEFT_BUTTON_FUNCTION, EDITOR_TOOL_HINT, NUM_REPORTS
	};
	enum { UNIT_REPORTS_BEGIN = UNIT_NAME, UNIT_REPORTS_END = UNIT_IMAGE + 1 };
	enum { STATUS_REPORTS_BEGIN = TIME_OF_DAY, STATUS_REPORTS_END = EDITOR_TOOL_HINT };

	enum {
		HOTKEY_SELECT = HOTKEY_MIN,
		HOTKEY_STATUS,
		HOTKEY_GRID,
		HOTKEY_RCLICK,

		HOTKEY_RUN,
		HOTKEY_SETTING, HOTKEY_SPECIAL_SETTING, HOTKEY_RECT_SETTING, HOTKEY_LINKED_GROUP, HOTKEY_MODE_SETTING, HOTKEY_ERASE,
		HOTKEY_INSERT_TOP, HOTKEY_INSERT_BOTTOM, HOTKEY_ERASE_ROW,
		HOTKEY_INSERT_LEFT, HOTKEY_INSERT_RIGHT, HOTKEY_ERASE_COLUMN,
		HOTKEY_INSERT_CHILD, HOTKEY_ERASE_CHILD
	};

	tmkwin_theme(mkwin_display& disp, mkwin_controller& controller, const config& cfg);
	void report_ptr(treport** unit_ptr, treport** hero_ptr, treport** ctrl_bar_ptr);

	void load_widget_page();
	void load_object_page(const unit_map& units);

	void fill_object_list(const unit_map& units);

private:
	void app_pre_show();
	void object_selected(twindow& window);

private:
	mkwin_controller& controller_;

};

} //end namespace gui2

#endif
