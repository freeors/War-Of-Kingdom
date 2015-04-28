/* $Id: resources.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef RESOURCES_H_
#define RESOURCES_H_

#include <vector>

class game_display;
class gamemap;
class game_state;
class LuaKernel;
class play_controller;
class team;
class tod_manager;
class unit_map;
class hero_map;
class persist_manager;
struct undo_action;

namespace soundsource { class manager; }

namespace resources
{
	extern game_display *screen;
	extern soundsource::manager *soundsources;
	extern gamemap *game_map;
	extern unit_map *units;
	extern hero_map* heros;
	extern std::vector<team> *teams;
	extern game_state *state_of_game;
	extern LuaKernel *lua_kernel;
	extern play_controller *controller;
	extern tod_manager *tod_manager;
	extern persist_manager *persist;
}

#endif
