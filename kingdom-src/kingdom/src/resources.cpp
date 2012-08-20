/* $Id: resources.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2009 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "resources.hpp"

namespace resources
{
	game_display *screen;
	soundsource::manager *soundsources;
	gamemap *game_map;
	unit_map *units;
	hero_map* heros;
	hero_map* heros_start;
	std::vector<team> *teams;
	game_state *state_of_game;
	LuaKernel *lua_kernel;
	play_controller *controller;
	::tod_manager *tod_manager;
	std::vector<undo_action> *undo_stack;
	persist_manager *persist;
}
