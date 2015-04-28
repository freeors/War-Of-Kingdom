/* $Id: game_config.cpp 46969 2010-10-08 19:45:32Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"
#include "game_config.hpp"
#include "config.hpp"
#include "util.hpp"
#include "version.hpp"
#include "gettext.hpp"
#include "serialization/string_utils.hpp"

namespace game_config
{
std::vector<std::string> foot_speed_prefix;
std::string foot_teleport_enter;
std::string foot_teleport_exit;

const std::string observer_team_name = "observer";

Uint32 red_to_green(int val, bool for_text)
{
	const std::vector<Uint32>& color_scale = for_text? red_green_scale_text: red_green_scale;
	val = std::max<int>(0, std::min<int>(val, 100));
	int lvl = (color_scale.size()-1) * val / 100;
	return color_scale[lvl];
}

}
