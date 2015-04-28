/* $Id: editor_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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
#define GETTEXT_DOMAIN "kingdom-lib"

#include "editor_display.hpp"
#include "builder.hpp"
#include "editor_controller.hpp"
#include "gui/dialogs/theme2.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/report.hpp"

#include <boost/bind.hpp>

namespace editor {

gui2::tbutton* create_terrain_button(editor_controller& controller, const t_string& tooltip, int cookie)
{
	gui2::tbutton* widget = gui2::create_surface_button(null_str, NULL);
	widget->set_tooltip(tooltip);

	connect_signal_mouse_left_click(
		*widget
		, boost::bind(
			&editor_controller::click_terrain
			, &controller
			, cookie));

	return widget;
}

editor_display::editor_display(editor_controller& controller, CVideo& video, const editor_map& map,
		const config& theme_cfg, const config& level)
	: hex_display(&controller, video, &map, theme_cfg, level, gui2::tgame_theme::NUM_REPORTS)
	, controller_(controller)
	, brush_locations_()
	, toolbar_hint_()
{
	std::string patch = get_theme_patch();
	theme_current_cfg_ = theme::set_resolution(theme_cfg, w(), h(), patch, theme_cfg_);
	create_theme();
	terrain_palette_ = dynamic_cast<gui2::treport*>(get_theme_object("terrain-palette"));
	terrain_palette_->multiline_init(false, "surface");

	clear_screen();
}

gui2::ttheme* editor_display::create_theme_dlg(const config& cfg)
{
	return new gui2::teditor_theme(*this, controller_, cfg);
}

editor_display::~editor_display()
{
}

void editor_display::post_change_resolution(const std::map<const std::string, bool>& actives)
{
	terrain_palette_ = dynamic_cast<gui2::treport*>(get_theme_object("terrain-palette"));
	terrain_palette_->multiline_init(false, "surface");
	controller_.reload_terrain_palette();
}

void editor_display::add_brush_loc(const map_location& hex)
{
	brush_locations_.insert(hex);
	invalidate(hex);
}

void editor_display::set_brush_locs(const std::set<map_location>& hexes)
{
	invalidate(brush_locations_);
	brush_locations_ = hexes;
	invalidate(brush_locations_);
}

void editor_display::clear_brush_locs()
{
	invalidate(brush_locations_);
	brush_locations_.clear();
}

void editor_display::remove_brush_loc(const map_location& hex)
{
	brush_locations_.erase(hex);
	invalidate(hex);
}

void editor_display::rebuild_terrain(const map_location &loc) 
{
	builder_->rebuild_terrain(loc);
}

void editor_display::reload_terrain_palette(const t_translation::t_list& terrains)
{
	terrain_palette_->erase_children();
	int n = 0;
	for (t_translation::t_list::const_iterator it = terrains.begin(); it != terrains.end(); ++ it, n ++) {
		const t_translation::t_terrain& terrain = *it;
		const std::string filename = image::terrain_prefix + map_->get_terrain_info(terrain).editor_image() + ".png";
		surface image(image::get_image(filename));

		const gui2::tpoint& unit_size = terrain_palette_->get_unit_size();
		gui2::tbutton* widget = create_terrain_button(controller_, map_->get_terrain_editor_string(terrain), n);
		widget->set_surface(image::get_image(filename), unit_size.x, unit_size.y);
		terrain_palette_->insert_child(*widget);
	}
	terrain_palette_->replacement_children();
	scroll_top();
}

void editor_display::scroll_up()
{
	terrain_palette_->scroll_vertical_scrollbar(gui2::tscrollbar_::HALF_JUMP_BACKWARDS);
}

void editor_display::scroll_down()
{
	terrain_palette_->scroll_vertical_scrollbar(gui2::tscrollbar_::HALF_JUMP_FORWARD);
}

void editor_display::scroll_top()
{
	terrain_palette_->scroll_vertical_scrollbar(gui2::tscrollbar_::BEGIN);
}

void editor_display::scroll_bottom()
{
	terrain_palette_->scroll_vertical_scrollbar(gui2::tscrollbar_::END);
}

image::TYPE editor_display::get_image_type(const map_location& loc)
{
	if (brush_locations_.find(loc) != brush_locations_.end()) {
		return image::BRIGHTENED;
	} else if (map().in_selection(loc)) {
		return image::BRIGHTENED;
		// return image::SEMI_BRIGHTENED;
	}
	return image::TOD_COLORED;
}

void editor_display::draw_hex(const map_location& loc)
{
	int xpos = get_location_x(loc);
	int ypos = get_location_y(loc);
	display::draw_hex(loc);
	if (map().on_board_with_border(loc)) {
		if (map().in_selection(loc)) {
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, xpos, ypos,
				image::get_image("editor/selection-overlay.png", image::TOD_COLORED));
		}

		if (brush_locations_.find(loc) != brush_locations_.end()) {
			static const image::locator brush(game_config::terrain::editor_brush);
			drawing_buffer_add(LAYER_MOUSEOVER_OVERLAY, loc, xpos, ypos,
					image::get_image(brush, image::SCALED_TO_HEX));
		}
	}
}

const SDL_Rect& editor_display::get_clip_rect()
{
	return map_outside_area();
}

void editor_display::draw_sidebar()
{
	// Fill in the terrain report
	if (get_map().on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tgame_theme::POSITION, reports::report(reports::report::LABEL, lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	refresh_report(gui2::tgame_theme::VILLAGES, reports::report(reports::report::LABEL, lexical_cast<std::string>(get_map().villages().size()), null_str));
	refresh_report(gui2::tgame_theme::EDITOR_TOOL_HINT, reports::report(reports::report::LABEL, toolbar_hint_, null_str));
}

} //end namespace editor
