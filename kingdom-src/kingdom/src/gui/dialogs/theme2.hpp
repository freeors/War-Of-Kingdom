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

#ifndef GUI_DIALOGS_THEME_HPP_INCLUDED
#define GUI_DIALOGS_THEME_HPP_INCLUDED

#include "gui/dialogs/theme.hpp"
#include "config.hpp"
#include "gui/widgets/widget.hpp"

class game_display;
class play_controller;

namespace editor {
class editor_display;
class editor_controller;
}

namespace gui2 {

class treport;

class tgame_theme: public ttheme
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


	tgame_theme(game_display& disp, play_controller& controller, const config& cfg);

	void report_ptr(treport** unit_ptr, treport** hero_ptr, treport** ctrl_bar_ptr);

private:
	void app_pre_show();

private:
	play_controller& controller_;
};

class teditor_theme: public ttheme
{
public:
	teditor_theme(editor::editor_display& disp, editor::editor_controller& controller, const config& cfg);

private:
	void app_pre_show();

private:
	editor::editor_controller& controller_;
};

} //end namespace gui2

#endif
