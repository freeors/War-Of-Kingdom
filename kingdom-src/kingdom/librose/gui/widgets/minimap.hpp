/* $Id: minimap.hpp 54007 2012-04-28 19:16:10Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_MINIMAP_HPP_INCLUDED
#define GUI_WIDGETS_MINIMAP_HPP_INCLUDED

#include "gui/widgets/control.hpp"

class config;

namespace gui2 {

/**
 * The basic minimap class.
 *
 * This minimap can only show a minimap, but it can't be interacted with. For
 * that the tminimap_interactive class will be created.
 */
class tminimap : public tcontrol
{
public:
	enum ttype {NONE, IMG, TILE_MAP, SURFACE};

	tminimap();

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

private:
	/** Inherited from tcontrol. */
	tpoint calculate_best_size() const
	{
		return best_size_ != tpoint(0, 0)
			? best_size_ : tcontrol::calculate_best_size();
	}

public:
	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool /*active*/) {}

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/** Inherited from tcontrol. */
	bool disable_click_dismiss() const { return false; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_map_data(ttype type, const std::string& map_data) {
		bool dirty = false;
		if (type_ != type) {
			dirty = true;
			type_ = type;
		}
		if (map_data != map_data_) { 
			dirty = true;
			map_data_ = map_data; 
		} 
		if (dirty) {
			set_dirty();
		}
	}

	std::string get_map_data() const { return map_data_; }

	const std::string& map_data() const { return map_data_; }

	void set_config(const ::config* terrain) { terrain_ = terrain; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_best_size(const tpoint& best_size) { best_size_ = best_size; }

	void set_surface(const surface& surf, int w, int h);

private:
	enum tstate { NORMAL, COUNT };

	/** The type of map_data. */
	ttype type_;

	/** The map data to be used to generate the map. */
	std::string map_data_;

	/**
	 * The config object with the terrain data.
	 *
	 * This config must be set before the object can be drawn.
	 */
	const ::config* terrain_;

	/**
	 * Gets the image for the minimap.
	 *
	 * @param w                   The wanted width of the image.
	 * @param h                   The wanted height of the image.
	 *
	 * @returns                   The image, NULL upon error.
	 */
	const surface get_image(const int w, const int h);

	/** Inherited from tcontrol. */
	void impl_draw_background(
			  surface& frame_buffer
			, int x_offset
			, int y_offset);

	/** When we're used as a fixed size item, this holds the best size. */
	tpoint best_size_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

