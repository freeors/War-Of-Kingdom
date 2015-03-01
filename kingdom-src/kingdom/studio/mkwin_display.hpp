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

#ifndef STUDIO_MKWIN_DISPLAY_HPP_INCLUDED
#define STUDIO_MKWIN_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "gui/auxiliary/widget_definition.hpp"
#include "gui/widgets/report.hpp"

class mkwin_controller;
struct tsheet_node;
class unit_map;
class base_unit;
class unit;

namespace gui2 {
class treport;
class ttoggle_button;
}

class mkwin_display : public display
{
public:
	static gui2::tcontrol_definition_ptr find_widget(const std::string& type, const std::string& definition);

	mkwin_display(mkwin_controller& controller, unit_map& units, CVideo& video, const gamemap& map, const config& theme_cfg, const config& level);
	~mkwin_display();

	gui2::ttheme* create_theme_dlg(const config& cfg);
	void pre_change_resolution(std::map<const std::string, bool>& actives);
	void post_change_resolution(const std::map<const std::string, bool>& actives);

	bool in_theme() const { return true; }
	void click_widget(const std::string& type, const std::string& definition);
	void click_grid(const std::string& type);
	void sheet_toggled(gui2::ttoggle_button& widget);

	const std::pair<std::string, gui2::tcontrol_definition_ptr>& selected_widget() const { return selected_widget_; }
	void refresh_sheet_header(const std::vector<tsheet_node>& parent_nodes, std::vector<tsheet_node>& unit_nodes, unit* u);

	gui2::ttoggle_button* sheet_widget(int index) const;
	void resume_mouseover_hex_overlay() { set_mouseover_hex_overlay(using_mouseover_hex_overlay_); }

	gui2::ttoggle_button* scroll_header_widget(int index) const;

	std::pair<std::string, gui2::tcontrol_definition_ptr> spacer;

protected:
	void pre_draw();
	/**
	* The editor uses different rules for terrain highlighting (e.g. selections)
	*/
	// image::TYPE get_image_type(const map_location& loc);

	void draw_hex(const map_location& loc);

	// const SDL_Rect& get_clip_rect();
	void pre_draw(rect_of_hexes& hexes);
	void draw_sidebar();
	void draw_invalidated();
	void redraw_units(const std::vector<map_location>& invalidated_unit_locs);
	void post_zoom();

	void set_mouse_overlay(surface& image_fg);
	
private:
	void reload_widget_palette();
	void reload_sheet_header();
	void reload_scroll_header();
	void scroll_top(gui2::treport& widget);
	void scroll_bottom(gui2::treport& widget);

private:
	mkwin_controller& controller_;
	unit_map& units_;
	gui2::treport* widget_palette_;
	gui2::treport* sheet_header_;
	gui2::ttabbar scroll_header_;

	std::string current_widget_type_;
	std::pair<std::string, gui2::tcontrol_definition_ptr> selected_widget_;
	surface using_mouseover_hex_overlay_;
};

#endif
