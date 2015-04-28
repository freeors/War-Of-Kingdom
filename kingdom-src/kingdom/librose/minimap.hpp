/* $Id: minimap.hpp 46186 2010-09-01 21:12:38Z silene $ */
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
#ifndef MINIMAP_HPP_INCLUDED
#define MINIMAP_HPP_INCLUDED

#include <cstddef>
#include "map_location.hpp"

class gamemap;
class display;
struct surface;


namespace image {
	///function to create the minimap for a given map
	///the surface returned must be freed by the user
	surface getMinimap(int w, int h, const gamemap &map_, const display* disp = NULL);
}

#endif
