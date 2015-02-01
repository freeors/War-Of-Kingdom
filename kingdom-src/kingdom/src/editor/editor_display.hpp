/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef EDITOR_EDITOR_DISPLAY_HPP_INCLUDED
#define EDITOR_EDITOR_DISPLAY_HPP_INCLUDED

#include "editor_map.hpp"
#include "hex_display.hpp"
#include "editor_palettes.hpp"

namespace gui2 {

class treport;

}

namespace editor {

class editor_controller;

class editor_display : public hex_display
{
public:
	editor_display(editor_controller& controller, CVideo& video, const editor_map& map, const config& theme_cfg,
			const config& level);
	~editor_display();

	gui2::ttheme* create_theme_dlg(const config& cfg);
	void post_change_resolution(const std::map<const std::string, bool>& actives);

	bool in_theme() const { return true; }

	void add_brush_loc(const map_location& hex);
	void set_brush_locs(const std::set<map_location>& hexes);
	void clear_brush_locs();
	void remove_brush_loc(const map_location& hex);
	const editor_map& map() const { return static_cast<const editor_map&>(get_map()); }
	void rebuild_terrain(const map_location &loc);
	void set_toolbar_hint(const std::string& value) { toolbar_hint_ = value; }

	void reload_terrain_palette(const t_translation::t_list& terrains);
	
	/** Scroll the terrain-palette up one step if possible. */
	void scroll_up();

	/** Scroll the terrain-palette down one step if possible. */
	void scroll_down();

	/** Scroll the terrain-palette to the top. */
	void scroll_top();

	/** Scroll the terrain-palette to the bottom. */
	void scroll_bottom();

protected:
	void pre_draw();
	/**
	* The editor uses different rules for terrain highlighting (e.g. selections)
	*/
	image::TYPE get_image_type(const map_location& loc);

	void draw_hex(const map_location& loc);

	const SDL_Rect& get_clip_rect();
	void draw_sidebar();

	editor_controller& controller_;
	std::set<map_location> brush_locations_;
	std::string toolbar_hint_;

	gui2::treport* terrain_palette_;
};

} //end namespace editor
#endif
