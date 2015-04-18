/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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
#define GETTEXT_DOMAIN "wesnoth-editor"

#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "unit_map.hpp"
#include "gui/dialogs/mkwin_theme.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "game_config.hpp"

#include <boost/bind.hpp>

const std::string widget_prefix = "studio/widget/";
static std::string miss_anim_err_str = "logic can only process map or canvas animation!";

gui2::tcontrol_definition_ptr mkwin_display::find_widget(const std::string& type, const std::string& definition)
{
	std::stringstream err;
	std::string definition2 = definition.empty()? "default": definition;
	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::current_gui->second.control_definition;
	if (!controls.count(type)) {
		err << "Cannot find widget, type: " << type;
		VALIDATE(false, err.str());
	}
	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = controls.find(type)->second.find(definition2);
	err << "Cannot find widget, type: " << type << ", definition: " << definition;
	VALIDATE(it->second.get(), err.str());
	return it->second;
}


mkwin_display::mkwin_display(mkwin_controller& controller, unit_map& units, CVideo& video, const gamemap& map,
		const config& theme_cfg, const config& level)
	: display(game_config::tile_square, &controller, video, &map, theme_cfg, level, gui2::tmkwin_theme::NUM_REPORTS)
	, controller_(controller)
	, units_(units)
	, current_widget_type_()
	, scroll_header_(true, false, "icon36")
{
	min_zoom_ = 48;
	max_zoom_ = 144;

	show_hover_over_ = false;

	// must use grid.
	set_grid(true);

	SDL_Rect screen = screen_area();
	std::string patch = get_theme_patch();
	theme_current_cfg_ = theme::set_resolution(theme_cfg, screen, patch, theme_cfg_);
	create_theme();
	
	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::current_gui->second.control_definition;
	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = controls.find("spacer")->second.find("default");
	spacer.first = "spacer";
	spacer.second = it->second;

	widget_palette_ = dynamic_cast<gui2::treport*>(get_theme_object("widget-palette"));
	reload_widget_palette();
	sheet_header_ = dynamic_cast<gui2::treport*>(get_theme_object("sheet-header"));
	reload_sheet_header();

	gui2::treport* report = dynamic_cast<gui2::treport*>(get_theme_object("scroll-header"));
	scroll_header_.set_report(report);
	reload_scroll_header();

	click_widget(spacer.first, spacer.second->id);
}

gui2::ttheme* mkwin_display::create_theme_dlg(const config& cfg)
{
	return new gui2::tmkwin_theme(*this, controller_, cfg);
}

mkwin_display::~mkwin_display()
{
}

void mkwin_display::pre_change_resolution(std::map<const std::string, bool>& actives)
{
	controller_.pre_change_resolution();
}

void mkwin_display::post_change_resolution(const std::map<const std::string, bool>& actives)
{
	widget_palette_ = dynamic_cast<gui2::treport*>(get_theme_object("widget-palette"));
	reload_widget_palette();

	sheet_header_ = dynamic_cast<gui2::treport*>(get_theme_object("sheet-header"));
	reload_sheet_header();

	gui2::treport* report = dynamic_cast<gui2::treport*>(get_theme_object("scroll-header"));
	scroll_header_.set_report(report);
	reload_scroll_header();

	controller_.post_change_resolution();
}

void mkwin_display::pre_draw()
{
}

void mkwin_display::draw_hex(const map_location& loc)
{
	display::draw_hex(loc);
}

void mkwin_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tmkwin_theme::POSITION, reports::report(reports::report::LABEL, lexical_cast<std::string>(mouseoverHex_), null_str));
	}

	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tmkwin_theme::VILLAGES, reports::report(reports::report::LABEL, ss.str(), null_str));
	ss.str("");
	ss << selected_widget_.first;
	if (selected_widget_.second.get()) {
		ss << "(" << selected_widget_.second->id << ")";
	}
	refresh_report(gui2::tmkwin_theme::UPKEEP, reports::report(reports::report::LABEL, ss.str(), null_str));
}

gui2::tbutton* create_widget_button(mkwin_controller& controller, const t_string& tooltip, const std::string& type, const std::string& definition)
{
	gui2::tbutton* widget = gui2::create_surface_button(null_str, NULL);
	widget->set_tooltip(tooltip);

	connect_signal_mouse_left_click(
		*widget
		, boost::bind(
			&mkwin_controller::click_widget
			, &controller
			, type
			, definition));
	return widget;
}

void mkwin_display::reload_widget_palette()
{
	widget_palette_->erase_children();
	
	const gui2::tpoint& unit_size = widget_palette_->get_unit_size();
	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::current_gui->second.control_definition;

	static std::set<std::string> exclude;
	if (exclude.empty()) {
		exclude.insert(spacer.first);
		exclude.insert("window");
		exclude.insert("repeating_button");
		exclude.insert("horizontal_scrollbar");
		exclude.insert("vertical_scrollbar");
		exclude.insert("slider");
		exclude.insert("matrix");
		exclude.insert("horizontal_listbox");
	}
	std::stringstream ss;
	surface default_surf(image::get_image(widget_prefix + "default.png"));
	for (gui2::tgui_definition::tcontrol_definition_map::const_iterator it = controls.begin(); it != controls.end(); ++ it) {
		const std::string& type = it->first;
		if (exclude.find(type) != exclude.end()) {
			continue;
		}
		for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
			const std::string& definition = it2->first;
			const std::string filename = form_widget_png(type, definition);
			surface surf(image::get_image(filename));
			surf = generate_pip_surface(default_surf, surf);

			ss.str("");
			ss << definition << "(" << it2->second->description << ")";
			gui2::tbutton* widget = create_widget_button(controller_, ss.str(), type, definition);
			widget->set_surface(surf, unit_size.x, unit_size.y);
			widget_palette_->insert_child(*widget);
		}
	}

	widget_palette_->replacement_children();
	scroll_top(*widget_palette_);
}

gui2::ttoggle_button* create_sheet_button(mkwin_controller& controller, const t_string& tooltip, int index)
{
	gui2::ttoggle_button* widget = gui2::create_toggle_button(null_str, "sheet", NULL);
	widget->set_tooltip(tooltip);
	widget->set_radio(true);

	widget->set_callback_state_change(boost::bind(&mkwin_controller::sheet_toggled, &controller, _1, index, true));
	return widget;
}

void mkwin_display::reload_sheet_header()
{
	static int max_level = 15;
	sheet_header_->erase_children();
	
	for (int i = 0; i < max_level; i ++) {
		gui2::ttoggle_button* widget = create_sheet_button(controller_, null_str, i);
		if (!i) {
			widget->set_label("TOP");
			widget->set_value(true);
		}
		sheet_header_->insert_child(*widget);
	}

	sheet_header_->replacement_children();

	refresh_sheet_header(controller_.parent_nodes(), controller_.unit_nodes(), NULL);
}

void mkwin_display::refresh_sheet_header(const std::vector<tsheet_node>& parent_nodes, std::vector<tsheet_node>& unit_nodes, unit* u)
{
	static int max_level = 15;
	const gui2::tgrid::tchild* children = sheet_header_->content_grid()->children();
	size_t size = sheet_header_->content_grid()->children_vsize();
	int fixes = parent_nodes.size();

	std::stringstream ss;
	for (int i = 1; i < fixes; i ++) {
		gui2::ttoggle_button* widget = dynamic_cast<gui2::ttoggle_button*>(children[i].widget_);

		const tsheet_node& node = parent_nodes[i];
		widget->set_label(node.parent->child_tag(node.number));
		widget->set_visible(gui2::twidget::VISIBLE);
		widget->set_cookie(0);
	}

	unit_nodes.clear();
	if (!u || !u->has_child()) {
		for (int i = fixes; i < (int)size; i ++) {
			children[i].widget_->set_visible(gui2::twidget::INVISIBLE);
		}
	} else {
		bool grid = u->is_grid();
		int childs = grid? 1: u->children().size();
		for (int i = fixes; i < (int)size; i ++) {
			gui2::ttoggle_button* widget = dynamic_cast<gui2::ttoggle_button*>(children[i].widget_);
			if (i < fixes + childs) {
				int number = i - fixes;
				unit_nodes.push_back(tsheet_node(u, number));
				widget->set_label(u->child_tag(number));
				widget->set_visible(gui2::twidget::VISIBLE);
			} else {
				widget->set_visible(gui2::twidget::INVISIBLE);
			}
		}
	}
	sheet_header_->replacement_children();
}

gui2::ttoggle_button* mkwin_display::sheet_widget(int index) const
{
	const gui2::tgrid::tchild* children = sheet_header_->content_grid()->children();
	return dynamic_cast<gui2::ttoggle_button*>(children[index].widget_);
}

gui2::ttoggle_button* mkwin_display::scroll_header_widget(int index) const
{
	const gui2::tgrid::tchild* children = scroll_header_.report()->content_grid()->children();
	return dynamic_cast<gui2::ttoggle_button*>(children[index].widget_);
}

void mkwin_display::reload_scroll_header()
{
	std::vector<std::string> images;
	images.push_back("buttons/studio/widget-palette.png");
	images.push_back("buttons/studio/object-list.png");

	scroll_header_.erase_children();
	
	int index = 0;
	for (std::vector<std::string>::const_iterator it = images.begin(); it != images.end(); ++ it, index ++) {
		gui2::tcontrol* widget = scroll_header_.create_child(null_str, null_str, reinterpret_cast<void*>(index), null_str);
		widget->set_label(*it);
		scroll_header_.insert_child(*widget, index);

	}
	scroll_header_.select(0);

	scroll_header_.replacement_children();
}

void mkwin_display::scroll_top(gui2::treport& widget)
{
	widget.scroll_vertical_scrollbar(gui2::tscrollbar_::BEGIN);
}

void mkwin_display::scroll_bottom(gui2::treport& widget)
{
	widget.scroll_vertical_scrollbar(gui2::tscrollbar_::END);
}

void mkwin_display::click_widget(const std::string& type, const std::string& definition)
{
	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::current_gui->second.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(type)->second;
	gui2::tcontrol_definition_ptr widget = definitions.find(definition)->second;

	if (selected_widget_.second != widget) {
		selected_widget_ = std::make_pair(type, widget);
		surface surf;
		if (type != spacer.first) {
			surf = image::get_image(form_widget_png(type, widget->id));
		}
		set_mouse_overlay(surf);
	}
}

void mkwin_display::click_grid(const std::string& type)
{
	if (selected_widget_.first != type) {
		selected_widget_ = std::make_pair(type, gui2::tcontrol_definition_ptr());
        surface surf = image::get_image("buttons/studio/grid.png");
		set_mouse_overlay(surf);
	}
}

void mkwin_display::sheet_toggled(gui2::ttoggle_button& widget)
{
	const gui2::tgrid::tchild* children = sheet_header_->content_grid()->children();
	size_t size = sheet_header_->content_grid()->children_vsize();
	for (int i = 0; i < (int)size; i ++) {
		gui2::ttoggle_button* that = dynamic_cast<gui2::ttoggle_button*>(children[i].widget_);
		if (that != &widget && that->get_value()) {
			that->set_value(false);
		}
	}
}

void mkwin_display::set_mouse_overlay(surface& image_fg)
{
	if (!image_fg) {
		set_mouseover_hex_overlay(NULL);
		using_mouseover_hex_overlay_ = NULL;
		return;
	}

	// Create a transparent surface of the right size.
	surface image = create_compatible_surface(image_fg, image_fg->w, image_fg->h);
	sdl_fill_rect(image, NULL, SDL_MapRGBA(image->format, 0, 0, 0, 0));

	// For efficiency the size of the tile is cached.
	// We assume all tiles are of the same size.
	// The zoom factor can change, so it's not cached.
	// NOTE: when zooming and not moving the mouse, there are glitches.
	// Since the optimal alpha factor is unknown, it has to be calculated
	// on the fly, and caching the surfaces makes no sense yet.
	static const Uint8 alpha = 196;
	static const int half_size = zoom_ / 2;
	static const int offset = 2;
	static const int new_size = half_size - 2;

	// Blit left side
	image_fg = scale_surface(image_fg, new_size, new_size);
	SDL_Rect rcDestLeft = create_rect(offset, offset, 0, 0);
	sdl_blit ( image_fg, NULL, image, &rcDestLeft );

	// Add the alpha factor and scale the image
	image = adjust_surface_alpha(image, alpha);

	// Set as mouseover
	set_mouseover_hex_overlay(image);
	using_mouseover_hex_overlay_ = image;
}

void mkwin_display::post_zoom()
{
	if (!controller_.in_theme_top()) {
		return;
	}
	double factor = double(zoom_) / double(last_zoom);
	SDL_Rect new_rect;
	// don't use units_, set_rect maybe change map_'s sequence.
	std::vector<unit*> top_units;
	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* u = dynamic_cast<unit*>(&*it);
		top_units.push_back(u);
	}
	for (std::vector<unit*>::const_iterator it = top_units.begin(); it != top_units.end(); ++ it) {
		unit* u = *it;
		const SDL_Rect& rect = u->get_rect();
		new_rect = create_rect(rect.x * factor, rect.y * factor, rect.w * factor, rect.h * factor);
		u->set_rect(new_rect);
	}
}

void mkwin_display::pre_draw(rect_of_hexes& hexes)
{
	// generate all units in current win.
	draw_area_unit_size_ = units_.units_from_rect(draw_area_unit_, draw_area_rect_);
}

void mkwin_display::draw_invalidated()
{
	map_location loc_n, loc_ne;
	std::vector<map_location> unit_invals;

	for (size_t i = 0; i < draw_area_unit_size_; i ++) {
		const base_unit* u = draw_area_unit_[i];

		const std::set<map_location>& draw_locs = u->get_draw_locations();
		invalidate(draw_locs);

		const map_location& loc = u->get_location();
		// draw_area_val(loc.x, loc.y) = INVALIDATE_UNIT;
		unit_invals.push_back(loc);
	}

	display::draw_invalidated();
	redraw_units(unit_invals);
}

void mkwin_display::redraw_units(const std::vector<map_location>& invalidated_unit_locs)
{
	// Units can overlap multiple hexes, so we need
	// to redraw them last and in the good sequence.
	BOOST_FOREACH (const map_location& loc, invalidated_unit_locs) {
		base_unit* u = units_.find_base_unit(loc, false);
		if (u) {
			u->redraw_unit();
		}
		u = units_.find_base_unit(loc, true);
		if (u) {
			u->redraw_unit();
		}
	}
	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		tanim_type type = it->second->type();
		VALIDATE(type == anim_map || type == anim_canvas, miss_anim_err_str);
		if (type != anim_map) {
			continue;
		}

		it->second->update_last_draw_time();
		area_anim::rt.type = it->second->type();
		it->second->redraw(screen_.getSurface(), empty_rect);
	}
}