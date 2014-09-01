/* $Id: editor_layout.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#define GETTEXT_DOMAIN "wesnoth-editor"

/**
 * @file
 * Set various sizes for the screen-layout of the map editor.
 */

#include "display.hpp"
#include "editor_layout.hpp"


namespace {
	const size_t default_terrain_size = 36;
	const size_t default_palette_width = 3;
}

namespace editor {

size_specs::size_specs()
	: terrain_size(default_terrain_size)
	, terrain_padding(9)
	, terrain_space(terrain_size + terrain_padding)
	, terrain_width(default_palette_width)
	, palette_x(0)
	, palette_y(0)
	, palette_h(20)
	, palette_w(10)
{
}

void adjust_sizes(display &disp, size_specs &sizes) {
	/** @todo Hardcoded coordinates for terrain palette, make it themeable. */
	const SDL_Rect& rect = disp.get_theme().terrain_panel_location(disp.screen_area());

	int left_space = (rect.w - sizes.terrain_space * sizes.terrain_width + sizes.terrain_padding) / 2;

	sizes.palette_x = rect.x + left_space;
	sizes.palette_y = rect.y;
	sizes.palette_w = rect.w - left_space;
	sizes.palette_h = rect.h;
}

} // end namespace editor

