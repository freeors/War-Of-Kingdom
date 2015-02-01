/* $Id: display.cpp 47842 2010-12-05 18:10:07Z mordante $ */
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

/**
 * @file
 * Routines to set up the display, scroll and zoom the map.
 */

#include "hex_display.hpp"
#include "map.hpp"
#include "game_config.hpp"

hex_display::hex_display(controller_base* controller, CVideo& video, const gamemap* map, const config& theme_cfg, const config& level, size_t num_reports)
	: display(game_config::tile_hex, controller, video, map, theme_cfg, level, num_reports)
{
}

const SDL_Rect& hex_display::max_map_area() const
{
	static SDL_Rect max_area = {0, 0, 0, 0};

	// hex_size() is always a multiple of 4
	// and hex_width() a multiple of 3,
	// so there shouldn't be off-by-one-errors
	// due to rounding.
	// To display a hex fully on screen,
	// a little bit extra space is needed.
	// Also added the border two times.
	max_area.w = static_cast<int>((map_->w() + 2 * border_.size + 1.0 / 3.0) * hex_width());
	max_area.h = static_cast<int>((map_->h() + 2 * border_.size + 0.5) * zoom_);

	return max_area;
}

const map_location hex_display::pixel_position_to_hex(int x, int y) const
{
	// adjust for the border
	x -= static_cast<int>(border_.size * hex_width());
	y -= static_cast<int>(border_.size * hex_size());
	// The editor can modify the border and this will result in a negative y
	// value. Instead of adding extra cases we just shift the hex. Since the
	// editor doesn't use the direction this is no problem.
	const int offset = y < 0 ? 1 : 0;
	if(offset) {
		x += hex_width();
		y += hex_size();
	}
	const int s = hex_size();
	const int tesselation_x_size = hex_width() * 2;
	const int tesselation_y_size = s;
	const int x_base = x / tesselation_x_size * 2;
	const int x_mod  = x % tesselation_x_size;
	const int y_base = y / tesselation_y_size;
	const int y_mod  = y % tesselation_y_size;

	int x_modifier = 0;
	int y_modifier = 0;

	if (y_mod < tesselation_y_size / 2) {
		if ((x_mod * 2 + y_mod) < (s / 2)) {
			x_modifier = -1;
			y_modifier = -1;
		} else if ((x_mod * 2 - y_mod) < (s * 3 / 2)) {
			x_modifier = 0;
			y_modifier = 0;
		} else {
			x_modifier = 1;
			y_modifier = -1;
		}

	} else {
		if ((x_mod * 2 - (y_mod - s / 2)) < 0) {
			x_modifier = -1;
			y_modifier = 0;
		} else if ((x_mod * 2 + (y_mod - s / 2)) < s * 2) {
			x_modifier = 0;
			y_modifier = 0;
		} else {
			x_modifier = 1;
			y_modifier = 0;
		}
	}

	return map_location(x_base + x_modifier - offset, y_base + y_modifier - offset);


	// NOTE: This code to get nearest_hex and second_nearest_hex
	// is not used anymore. However, it can be useful later.
	// So, keep it here for the moment.
	/*
	if(nearest_hex != NULL) {
		// Our x and y use the map_area as reference.
		// The coordinates given by get_location use the screen as reference,
		// so we need to convert it.
		const int centerx = (get_location_x(res) - map_area().x + xpos_) + hex_size()/2 - hex_width();
		const int centery = (get_location_y(res) - map_area().y + ypos_) + hex_size()/2 - hex_size();
		const int x_offset = x - centerx;
		const int y_offset = y - centery;
		if(y_offset > 0) {
			if(x_offset > y_offset/2) {
				*nearest_hex = map_location::SOUTH_EAST;
				if(second_nearest_hex != NULL) {
					if(x_offset/2 > y_offset) {
						*second_nearest_hex = map_location::NORTH_EAST;
					} else {
						*second_nearest_hex = map_location::SOUTH;
					}
				}
			} else if(-x_offset > y_offset/2) {
				*nearest_hex = map_location::SOUTH_WEST;
				if(second_nearest_hex != NULL) {
					if(-x_offset/2 > y_offset) {
						*second_nearest_hex = map_location::NORTH_WEST;
					} else {
						*second_nearest_hex = map_location::SOUTH;
					}
				}
			} else {
				*nearest_hex = map_location::SOUTH;
				if(second_nearest_hex != NULL) {
					if(x_offset > 0) {
						*second_nearest_hex = map_location::SOUTH_EAST;
					} else {
						*second_nearest_hex = map_location::SOUTH_WEST;
					}
				}
			}
		} else { // y_offset <= 0
			if(x_offset > -y_offset/2) {
				*nearest_hex = map_location::NORTH_EAST;
				if(second_nearest_hex != NULL) {
					if(x_offset/2 > -y_offset) {
						*second_nearest_hex = map_location::SOUTH_EAST;
					} else {
						*second_nearest_hex = map_location::NORTH;
					}
				}
			} else if(-x_offset > -y_offset/2) {
				*nearest_hex = map_location::NORTH_WEST;
				if(second_nearest_hex != NULL) {
					if(-x_offset/2 > -y_offset) {
						*second_nearest_hex = map_location::SOUTH_WEST;
					} else {
						*second_nearest_hex = map_location::NORTH;
					}
				}
			} else {
				*nearest_hex = map_location::NORTH;
				if(second_nearest_hex != NULL) {
					if(x_offset > 0) {
						*second_nearest_hex = map_location::NORTH_EAST;
					} else {
						*second_nearest_hex = map_location::NORTH_WEST;
					}
				}
			}
		}
	}
	*/
}

int hex_display::get_location_x(const map_location& loc) const
{
	return static_cast<int>(map_area().x + (loc.x + border_.size) * hex_width() - xpos_);
}

int hex_display::get_location_y(const map_location& loc) const
{
	return static_cast<int>(map_area().y + (loc.y + border_.size) * zoom_ - ypos_ + (is_odd(loc.x) ? zoom_/2 : 0));
}

const rect_of_hexes hex_display::hexes_under_rect(const SDL_Rect& r) const
{
	rect_of_hexes res;

	if (r.w<=0 || r.h<=0) {
		// empty rect, return dummy values giving begin=end
		res.left = 0;
		res.right = -1; // end is right+1
		res.top[0] = 0;
		res.top[1] = 0;
		res.bottom[0] = 0;
		res.bottom[1] = 0;
		return res;
	}

	SDL_Rect map_rect = map_area();
	// translate rect coordinates from screen-based to map_area-based
	int x = xpos_ - map_rect.x + r.x;
	int y = ypos_ - map_rect.y + r.y;
	// we use the "double" type to avoid important rounding error (size of an hex!)
	// we will also need to use std::floor to avoid bad rounding at border (negative values)
	double tile_width = hex_width();
	double tile_size = hex_size();
	double border = border_.size;
	// the "-0.25" is for the horizontal imbrication of hexes (1/4 overlaps).
	// res.left = static_cast<int>(std::floor(-border + x / tile_width - 0.25));
	res.left = static_cast<int>(std::floor(-border + x / tile_width - 0.333));
	// we remove 1 pixel of the rectangle dimensions
	// (the rounded division take one pixel more than needed)
	res.right = static_cast<int>(std::floor(-border + (x + r.w-1) / tile_width));

	// for odd x, we must shift up one half-hex. Since x will vary along the edge,
	// we store here the y values for even and odd x, respectively
	res.top[0] = static_cast<int>(std::floor(-border + y / tile_size));
	res.top[1] = static_cast<int>(std::floor(-border + y / tile_size - 0.5));
	res.bottom[0] = static_cast<int>(std::floor(-border + (y + r.h-1) / tile_size));
	res.bottom[1] = static_cast<int>(std::floor(-border + (y + r.h-1) / tile_size - 0.5));

	// TODO: in some rare cases (1/16), a corner of the big rect is on a tile
	// (the 72x72 rectangle containing the hex) but not on the hex itself
	// Can maybe be optimized by using pixel_position_to_hex

	return res;
}

void hex_display::draw_border(const map_location& loc, const int xpos, const int ypos)
{
	/**
	 * at the moment the border must be between 0.0 and 0.5
	 * and the image should always be prepared for a 0.5 border.
	 * This way this code doesn't need modifications for other border sizes.
	 */

	// First handle the corners :
	if(loc.x == -1 && loc.y == -1) { // top left corner
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.corner_image_top_left, image::SCALED_TO_ZOOM));
	} else if(loc.x == get_map().w() && loc.y == -1) { // top right corner
		// We use the map idea of odd and even, and map coords are internal coords + 1
		if(loc.x%2 == 0) {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos + zoom_/2,
				image::get_image(border_.corner_image_top_right_odd, image::SCALED_TO_ZOOM));
		} else {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
				image::get_image(border_.corner_image_top_right_even, image::SCALED_TO_ZOOM));
		}
	} else if(loc.x == -1 && loc.y == get_map().h()) { // bottom left corner
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.corner_image_bottom_left, image::SCALED_TO_ZOOM));

	} else if(loc.x == get_map().w() && loc.y == get_map().h()) { // bottom right corner
		// We use the map idea of odd and even, and map coords are internal coords + 1
		if(loc.x%2 == 1) {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
				image::get_image(border_.corner_image_bottom_right_even, image::SCALED_TO_ZOOM));
		} else {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
				image::get_image(border_.corner_image_bottom_right_odd, image::SCALED_TO_ZOOM));
		}

	// Now handle the sides:
	} else if(loc.x == -1) { // left side
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.border_image_left, image::SCALED_TO_ZOOM));
	} else if(loc.x == get_map().w()) { // right side
		drawing_buffer_add(LAYER_BORDER, loc, xpos + zoom_/4, ypos,
			image::get_image(border_.border_image_right, image::SCALED_TO_ZOOM));
	} else if(loc.y == -1) { // top side
		// We use the map idea of odd and even, and map coords are internal coords + 1
		if(loc.x%2 == 1) {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
				image::get_image(border_.border_image_top_even, image::SCALED_TO_ZOOM));
		} else {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos + zoom_/2,
				image::get_image(border_.border_image_top_odd, image::SCALED_TO_ZOOM));
		}
	} else if(loc.y == get_map().h()) { // bottom side
		// We use the map idea of odd and even, and map coords are internal coords + 1
		if(loc.x%2 == 1) {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos,
				image::get_image(border_.border_image_bottom_even, image::SCALED_TO_ZOOM));
		} else {
			drawing_buffer_add(LAYER_BORDER, loc, xpos, ypos + zoom_/2,
				image::get_image(border_.border_image_bottom_odd, image::SCALED_TO_ZOOM));
		}
	}
}

double hex_display::minimap_shift_x(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const
{
	return - border_.size * hex_width() - (map_out_rect.w - map_rect.w) / 2;
}

double hex_display::minimap_shift_y(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const
{
	return - (border_.size + 0.25) * hex_size() - (map_out_rect.h - map_rect.h) / 2;
}