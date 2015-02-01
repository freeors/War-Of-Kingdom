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

#include "unit.hpp"
#include "gettext.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "gui/dialogs/mkwin_theme.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "game_config.hpp"

#include <boost/bind.hpp>

extern const std::string widget_prefix = "studio/widget/";

std::string form_widget_png(const std::string& type, const std::string& definition)
{
	if (type != "grid") {
		return widget_prefix + type + ".png";
	} else {
		return "buttons/studio/grid.png";
	}
}

unit::unit(mkwin_controller& controller, mkwin_display& disp, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent)
	: controller_(controller)
	, disp_(disp)
	, widget_(widget)
	, type_(WIDGET)
	, parent_(parent)
{
}

unit::unit(mkwin_controller& controller, mkwin_display& disp, int type, unit* parent)
	: controller_(controller)
	, disp_(disp)
	, widget_()
	, type_(type)
	, parent_(parent)
{
}

unit::unit(const unit& that)
	: base_unit(that)
	, controller_(that.controller_)
	, disp_(that.disp_)
	, type_(that.type_)
	, parent_(that.parent_)
	, widget_(that.widget_)
	, cell_(that.cell_)
{
	// caller require call set_parent to set self-parent.

	for (std::vector<tchild>::const_iterator it = that.children_.begin(); it != that.children_.end(); ++ it) {
		const tchild& child = *it;
		children_.push_back(tchild());
		tchild& child2 = children_.back();
		child2.window = new unit(*child.window);
		child2.window->parent_ = this;

		for (std::vector<unit*>::const_iterator it2 = child.rows.begin(); it2 != child.rows.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = this;
			child2.rows.push_back(u);
		}
		for (std::vector<unit*>::const_iterator it2 = child.cols.begin(); it2 != child.cols.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = this;
			child2.cols.push_back(u);
		}
		for (std::vector<unit*>::const_iterator it2 = child.units.begin(); it2 != child.units.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = this;
			child2.units.push_back(u);
		}
	}
}

unit::~unit()
{
	for (std::vector<tchild>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		it->clear(true);
	}
	if (this == controller_.copied_unit()) {
		controller_.set_copied_unit(NULL);
	}
}

void unit::redraw_unit()
{
	if (!loc_.valid()) {
		return;
	}

	if (refreshing_) {
		return;
	}
	trefreshing_lock lock(*this);

	const int xsrc = disp_.get_location_x(loc_);
	const int ysrc = disp_.get_location_y(loc_);

	surface surf;
	int zoom = disp_.hex_width();

	if (this == controller_.copied_unit()) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
			loc_, xsrc, ysrc, image::get_image("misc/square72.png", image::SCALED_TO_ZOOM));
	}

	if (type_ == WIDGET) {
		if (is_spacer()) {

		} else {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
				loc_, xsrc, ysrc, image::get_image(image(), image::SCALED_TO_ZOOM));

			unsigned h_flag = cell_.widget.cell.flags_ & gui2::tgrid::HORIZONTAL_MASK;
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
				loc_, xsrc + zoom - 32, ysrc, image::get_image(gui2::horizontal_layout.find(h_flag)->second.icon));

			unsigned v_flag = cell_.widget.cell.flags_ & gui2::tgrid::VERTICAL_MASK;
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
				loc_, xsrc + zoom - 16, ysrc, image::get_image(gui2::vertical_layout.find(v_flag)->second.icon));

			surface text_surf;
			if (!cell_.widget.id.empty()) {
				surface text_surf = font::get_rendered_text2(cell_.widget.id, -1, 10, font::BLACK_COLOR);
				if (text_surf->w > zoom) {
					SDL_Rect r = create_rect(0, 0, zoom, text_surf->h);
					text_surf = cut_surface(text_surf, r); 
				}
				disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
					loc_, xsrc, ysrc + zoom / 2, text_surf);
			}
			if (!cell_.widget.label.empty()) {
				text_surf = font::get_rendered_text2(cell_.widget.label, -1, 10, font::BLUE_COLOR);
				if (text_surf->w > zoom) {
					SDL_Rect r = create_rect(0, 0, zoom, text_surf->h);
					text_surf = cut_surface(text_surf, r); 
				}
				disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
					loc_, xsrc, ysrc + zoom / 2 + 10, text_surf);
			}
		}

		if (has_child()) {
			surf = create_neutral_surface(disp_.default_zoom_, disp_.default_zoom_);
			blit_integer_surface(children_.size(), surf, 0, 0);
		}

	} else if (type_ == WINDOW) {
		if (controller_.parent_nodes().size() == 1) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
				loc_, xsrc, ysrc, image::get_image(image(), image::SCALED_TO_ZOOM));

			if (!cell_.window.id.empty()) {
				surface text_surf = font::get_rendered_text2(cell_.window.id, -1, 10, font::BLACK_COLOR);
				disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
					loc_, xsrc, ysrc + zoom / 2, text_surf);
			}
		}

	} else if (type_ == COLUMN) {
		surf = create_neutral_surface(disp_.default_zoom_, disp_.default_zoom_);
		blit_integer_surface(loc_.x, surf, 0, 0);
		blit_integer_surface(cell_.column.grow_factor, surf, 0, 12);

	} else if (type_ == ROW) {
		surf = create_neutral_surface(disp_.default_zoom_, disp_.default_zoom_);
		blit_integer_surface(loc_.y, surf, 0, 0);
		blit_integer_surface(cell_.row.grow_factor, surf, 0, 12);

	}
	if (surf) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, xsrc, ysrc, surf);
	}

	int ellipse_floating = 0;
	if (loc_ == controller_.selected_hex()) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
			xsrc, ysrc - ellipse_floating, image::get_image("misc/ellipse-top.png", image::SCALED_TO_ZOOM));
	
		disp_.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			xsrc, ysrc - ellipse_floating, image::get_image("misc/ellipse-bottom.png", image::SCALED_TO_ZOOM));
	}
}

std::string unit::image() const
{
	if (type_ == COLUMN) {
		return widget_prefix + "column.png";
	} else if (type_ == ROW) {
		return widget_prefix + "row.png";
	} else if (type_ == WINDOW) {
		return widget_prefix + "window.png";
	} else if (widget_.first == "grid") {
		return widget_prefix + "grid.png";
	} else {
		return form_widget_png(widget_.first, widget_.second->id);
	}
}

void unit::set_child(int number, const tchild& child)
{
	// don't delete units that in children_[number].
	// unit's pointer is share new and old.
	children_[number] = child;
}

int unit::children_layers() const
{
	int max = 0;
	if (!children_.empty()) {
		const tchild& child = children_[0];
		int val;
		for (std::vector<unit*>::const_iterator it2 = child.units.begin(); it2 != child.units.end(); ++ it2) {
			const unit& u = **it2;
			val = u.children_layers();
			if (val > max) {
				max = val;
			}
		}
		return 1 + max;
	}
	return 0;
}

void unit::insert_child(int w, int h)
{
	tchild child;

	child.window = new unit(controller_, disp_, WINDOW, this);
	child.window->set_location(map_location(0, 0));
	for (int x = 1; x < w; x ++) {
		child.cols.push_back(new unit(controller_, disp_, COLUMN, this));
		child.cols.back()->set_location(map_location(x, 0));
	}
	for (int y = 1; y < h; y ++) {
		int pitch = y * w;
		for (int x = 0; x < w; x ++) {
			if (x) {
				child.units.push_back(new unit(controller_, disp_, disp_.spacer, this));
				child.cols.back()->set_location(map_location(x, y));
			} else {
				child.rows.push_back(new unit(controller_, disp_, ROW, this));
				child.rows.back()->set_location(map_location(0, y));
			}
		}
	}
	children_.push_back(child);
}

void unit::erase_child(int index)
{
	std::vector<tchild>::iterator it = children_.begin();
	std::advance(it, index);
	children_.erase(it);
}

std::string unit::child_tag(int index) const
{
	std::stringstream ss;
	if (is_grid()) {
		return "GRID";
	} else if (is_stacked_widget()) {
		ss << "S#" << index;
		return ss.str();
	} else if (is_listbox()) {
		if (!index) {
			return "Header";
		} else if (index == 1) {
			return "Body";
		} else if (index == 2) {
			return "Footer";
		}
	} else if (is_panel() || is_scrollbar_panel()) {
		return "Panel";
	} else if (is_tree_view()) {
		return "Node";
	}
	return null_str;
}

void unit::generate(config& cfg) const
{
	if (type_ == WIDGET) {
		generate_widget(cfg);
	} else if (type_ == WINDOW) {
		generate_window(cfg);
	} else if (type_ == ROW) {
		generate_row(cfg);
	} else if (type_ == COLUMN) {
		generate_column(cfg);
	}
}

std::string formual_fill_str(const std::string& core)
{
	return std::string("(") + core + ")";
}

std::string formual_extract_str(const std::string& str)
{
	size_t s = str.size();
	if (s < 2 || str.at(0) != '(' || str.at(s -1) != ')') {
		return str;
	}
	return str.substr(1, s - 2);
}

void unit::generate_window(config& cfg) const
{
	cfg["id"] = cell_.window.id;
	cfg["description"] = t_string(cell_.window.description, cell_.window.textdomain);

	config& res_cfg = cfg.add_child("resolution");

	res_cfg["definition"] = cell_.window.definition;
	if (cell_.window.click_dismiss) {
		res_cfg["click_dismiss"] = true;
	}

	if (cell_.window.automatic_placement) {
		if (cell_.window.horizontal_placement != gui2::tgrid::HORIZONTAL_ALIGN_CENTER) {
			res_cfg["horizontal_placement"] = gui2::horizontal_layout.find(cell_.window.horizontal_placement)->second.id;
		}
		if (cell_.window.vertical_placement != gui2::tgrid::VERTICAL_ALIGN_CENTER) {
			res_cfg["vertical_placement"] = gui2::vertical_layout.find(cell_.window.vertical_placement)->second.id;
		}
	} else {
		res_cfg["automatic_placement"] = false;
		if (!cell_.window.x.empty()) {
			res_cfg["x"] = formual_fill_str(cell_.window.x);
		}
		if (!cell_.window.y.empty()) {
			res_cfg["y"] = formual_fill_str(cell_.window.y);
		}
		if (!cell_.window.width.empty()) {
			res_cfg["width"] = formual_fill_str(cell_.window.width);
		}
		if (!cell_.window.height.empty()) {
			res_cfg["height"] = formual_fill_str(cell_.window.height);
		}
	}

	config tmp;
	const std::vector<gui2::tlinked_group>& linked_groups = controller_.linked_groups();
	for (std::vector<gui2::tlinked_group>::const_iterator it = linked_groups.begin(); it != linked_groups.end(); ++ it) {
		const gui2::tlinked_group& linked = *it;

		tmp.clear();
		tmp["id"] = linked.id;
		if (linked.fixed_width) {
			tmp["fixed_width"] = true;
		}
		if (linked.fixed_height) {
			tmp["fixed_height"] = true;
		}
		res_cfg.add_child("linked_group", tmp);
	}

	tmp.clear();
	tmp["id"] = "tooltip_large";
	res_cfg.add_child("tooltip", tmp);

	tmp.clear();
	tmp["id"] = "helptip_large";
	res_cfg.add_child("helptip", tmp);

}

void unit::generate_row(config& cfg) const
{
	if (cell_.row.grow_factor) {
		cfg["grow_factor"] = cell_.row.grow_factor;
	}
}

void unit::generate_column(config& cfg) const
{
	if (cell_.column.grow_factor) {
		cfg["grow_factor"] = cell_.column.grow_factor;
	}
}

void unit::generate_widget(config& cfg) const
{
	std::stringstream ss;
	cfg.clear();

	if (cell_.widget.cell.border_size_ && cell_.widget.cell.flags_ & gui2::tgrid::BORDER_ALL) {
		ss.str("");
		if ((cell_.widget.cell.flags_ & gui2::tgrid::BORDER_ALL) == gui2::tgrid::BORDER_ALL) {
			ss << "all";
		} else {
			bool first = true;
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_LEFT) {
				first = false;
				ss << "left";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_RIGHT) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "right";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_TOP) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "top";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_BOTTOM) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "bottom";
			}
		}

		cfg["border"] = ss.str();
		cfg["border_size"] = (int)cell_.widget.cell.border_size_;
	}

	unsigned h_flag = cell_.widget.cell.flags_ & gui2::tgrid::HORIZONTAL_MASK;
	if (h_flag == gui2::tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT) {
		cfg["horizontal_grow"] = true;
	} else if (h_flag != gui2::tgrid::HORIZONTAL_ALIGN_CENTER) {
		cfg["horizontal_alignment"] = gui2::horizontal_layout.find(h_flag)->second.id;
	}
	unsigned v_flag = cell_.widget.cell.flags_ & gui2::tgrid::VERTICAL_MASK;
	if (v_flag == gui2::tgrid::VERTICAL_GROW_SEND_TO_CLIENT) {
		cfg["vertical_grow"] = true;
	} else if (v_flag != gui2::tgrid::VERTICAL_ALIGN_CENTER) {
		cfg["vertical_alignment"] = gui2::vertical_layout.find(v_flag)->second.id;
	}

	config& sub = cfg.add_child(widget_.first);
	if (!cell_.widget.id.empty()) {
		sub["id"] = cell_.widget.id;
	}
	if (widget_.second.get()) {
		sub["definition"] = widget_.second->id;

		if (!cell_.widget.linked_group.empty()) {
			sub["linked_group"] = cell_.widget.linked_group;
		}
		if (!cell_.widget.width.empty()) {
			sub["width"] = formual_fill_str(cell_.widget.width);
		}
		if (!cell_.widget.height.empty()) {
			sub["height"] = formual_fill_str(cell_.widget.height);
		}
	} 
	if (is_scroll()) {
		if (cell_.widget.horizontal_mode != gui2::tscrollbar_container::auto_visible_first_run) {
			sub["horizontal_scrollbar_mode"] = gui2::horizontal_mode.find(cell_.widget.horizontal_mode)->second.id;
		}
		if (cell_.widget.vertical_mode != gui2::tscrollbar_container::auto_visible_first_run) {
			sub["vertical_scrollbar_mode"] = gui2::vertical_mode.find(cell_.widget.vertical_mode)->second.id;
		}
	}
	if (is_grid()) {
		generate_grid(sub);

	} else if (is_stacked_widget()) {
		generate_stacked_widget(sub);

	} else if (is_listbox()) {
		generate_listbox(sub);

	} else if (is_panel()) {
		generate_toggle_panel(sub);

	} else if (is_scrollbar_panel()) {
		generate_scrollbar_panel(sub);

	} else if (is_tree_view()) {
		generate_tree_view(sub);

	} else if (is_drawing()) {
		generate_drawing(sub);

	} else {
		if (!cell_.widget.label.empty()) {
			if (cell_.widget.label_textdomain.empty()) {
				sub["label"] = cell_.widget.label;
			} else {
				sub["label"] = t_string(cell_.widget.label, cell_.widget.label_textdomain);
			}
		}
		if (!cell_.widget.tooltip.empty()) {
			if (cell_.widget.tooltip_textdomain.empty()) {
				sub["tooltip"] = cell_.widget.tooltip;
			} else {
				sub["tooltip"] = t_string(cell_.widget.tooltip, cell_.widget.tooltip_textdomain);
			}
		}
	}
}

void unit::generate_grid(config& cfg) const
{
	// [grid]
	//		[..cfg..]
	// [/grid]
	const tchild& child = children_[0];
	child.generate(cfg);
}

void unit::generate_stacked_widget(config& cfg) const
{
	// [stacked_widget]
	//		[..cfg..]
	// [/stacked_widget]

	config& stack_cfg = cfg.add_child("stack");
	for (std::vector<tchild>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		const tchild& child = *it;
		if (child.is_all_spacer()) {
			continue;
		}
		config& layer_cfg = stack_cfg.add_child("layer");
		child.generate(layer_cfg);
	}
}

void unit::generate_listbox(config& cfg) const
{
	// [listbox]
	//		[..cfg..]
	// [/listbox]

	if (!children_[0].is_all_spacer()) {
		children_[0].generate(cfg.add_child("header"));
	}
	children_[1].generate(cfg.add_child("list_definition"));
	if (!children_[2].is_all_spacer()) {
		children_[2].generate(cfg.add_child("footer"));
	}
}

void unit::generate_toggle_panel(config& cfg) const
{
	// [toggle_panel]
	//		[..cfg..]
	// [/toggle_panel]

	config& grid_cfg = cfg.add_child("grid");
	const tchild& child = children_[0];
	child.generate(grid_cfg);
}

void unit::generate_scrollbar_panel(config& cfg) const
{
	// [scrollbar_panel]
	//		[..cfg..]
	// [/scrollbar_panel]

	config& grid_cfg = cfg.add_child("definition");
	const tchild& child = children_[0];
	child.generate(grid_cfg);
}

void unit::generate_tree_view(config& cfg) const
{
	// [tree_view]
	//		[..cfg..]
	// [/tree_view]
	cfg["indention_step_size"] = cell_.widget.tree_view.indention_step_size;
	config& node_cfg = cfg.add_child("node");
	node_cfg["id"] = cell_.widget.tree_view.node_id;

	config& node_grid = node_cfg.add_child("node_definition");
	const tchild& child = children_[0];
	child.generate(node_grid);
}

void unit::generate_drawing(config& cfg) const
{
	// [drawing]
	//		[..cfg..]
	// [/drawing]

	config& draw_cfg = cfg.add_child("draw");
	config& image_cfg = draw_cfg.add_child("image");
	if (!cell_.widget.draw.x.empty()) {
		image_cfg["x"] = formual_fill_str(cell_.widget.draw.x);
	}
	if (!cell_.widget.draw.y.empty()) {
		image_cfg["y"] = formual_fill_str(cell_.widget.draw.y);
	}
	if (!cell_.widget.draw.w.empty()) {
		image_cfg["w"] = formual_fill_str(cell_.widget.draw.w);
	}
	if (!cell_.widget.draw.h.empty()) {
		image_cfg["h"] = formual_fill_str(cell_.widget.draw.h);
	}
	image_cfg["name"] = formual_fill_str(cell_.widget.draw.name);
}

void unit::tchild::generate(config& cfg) const
{
	config unit_cfg;
	config* row_cfg = NULL;
	int current_y = -1;
	for (int y = 0; y < (int)rows.size(); y ++) {
		int pitch = y * (int)cols.size();
		for (int x = 0; x < (int)cols.size(); x ++) {
			unit* u = units[pitch + x];
			if (u->get_location().y != current_y) {
				row_cfg = &cfg.add_child("row");
				current_y = u->get_location().y;

				unit* row = rows[y];
				row->generate(*row_cfg);
			}
			u->generate(unit_cfg);
			if (y == 0) {
				unit* column = cols[x];
				column->generate(unit_cfg);
			}
			row_cfg->add_child("column", unit_cfg);
		}
		// fixed. one grid must exist one [row].
		if (!cfg.child_count("row")) {
			cfg.add_child("row");
		}
	}
}

void unit::from(const config& cfg)
{
	if (type_ == WIDGET) {
		from_widget(cfg);
	} else if (type_ == WINDOW) {
		from_window(cfg);
	} else if (type_ == ROW) {
		from_row(cfg);
	} else if (type_ == COLUMN) {
		from_column(cfg);
	}
}

void unit::from_window(const config& cfg)
{
	// [window] <= cfg
	// [/window]

	set_location(map_location(0, 0));
	cell_.window.id = cfg["id"].str();
	t_string description = cfg["description"].t_str();
	split_t_string(description, cell_.window.textdomain, cell_.window.description);
	
	const config& res_cfg = cfg.child("resolution");

	cell_.window.definition = res_cfg["definition"].str();
	cell_.window.click_dismiss = res_cfg["click_dismiss"].to_bool();
	cell_.window.automatic_placement = res_cfg["automatic_placement"].to_bool(true);
	if (cell_.window.automatic_placement) {
		cell_.window.horizontal_placement = gui2::implementation::get_h_align(res_cfg["horizontal_placement"]);
		cell_.window.vertical_placement = gui2::implementation::get_v_align(res_cfg["vertical_placement"]);
	} else {
		cell_.window.x = formual_extract_str(res_cfg["x"].str());
		cell_.window.y = formual_extract_str(res_cfg["y"].str());
		cell_.window.width = formual_extract_str(res_cfg["width"].str());
		cell_.window.height = formual_extract_str(res_cfg["height"].str());
	}
}

void unit::from_row(const config& cfg)
{
	cell_.row.grow_factor = cfg["grow_factor"].to_int();
}

void unit::from_column(const config& cfg)
{
	cell_.column.grow_factor = cfg["grow_factor"].to_int();
}

void unit::from_widget(const config& cfg)
{
	cell_.widget.cell.flags_ = gui2::implementation::read_flags(cfg);
	cell_.widget.cell.border_size_ = cfg["border_size"].to_int();

	const config& sub_cfg = cfg.child(widget_.first);
	cell_.widget.id = sub_cfg["id"].str();
	cell_.widget.linked_group = sub_cfg["linked_group"].str();
	cell_.widget.width = formual_extract_str(sub_cfg["width"].str());
	cell_.widget.height = formual_extract_str(sub_cfg["height"].str());
	split_t_string(sub_cfg["label"].t_str(), cell_.widget.label_textdomain, cell_.widget.label);
	split_t_string(sub_cfg["tooltip"].t_str(), cell_.widget.tooltip_textdomain, cell_.widget.tooltip);

	if (is_scroll()) {
		cell_.widget.vertical_mode = gui2::implementation::get_scrollbar_mode(sub_cfg["vertical_scrollbar_mode"]);
		cell_.widget.horizontal_mode = gui2::implementation::get_scrollbar_mode(sub_cfg["horizontal_scrollbar_mode"]);
	}
	if (is_grid()) {
		from_grid(sub_cfg);

	} else if (is_stacked_widget()) {
		from_stacked_widget(sub_cfg);

	} else if (is_listbox()) {
		from_listbox(sub_cfg);

	} else if (is_panel()) {
		from_toggle_panel(sub_cfg);

	} else if (is_scrollbar_panel()) {
		from_scrollbar_panel(sub_cfg);

	} else if (is_tree_view()) {
		from_tree_view(sub_cfg);

	} else if (is_drawing()) {
		from_drawing(sub_cfg);
	}
}

void unit::tchild::from(mkwin_controller& controller, mkwin_display& disp, unit* parent, const config& cfg)
{
	window = new unit(controller, disp, unit::WINDOW, parent);
	// generate require x, y. for example, according it to next line.
	window->set_location(map_location(0, 0));

	bool first_row = true;
	std::string type, definition;
	BOOST_FOREACH (const config& row, cfg.child_range("row")) {
		rows.push_back(new unit(controller, disp, unit::ROW, parent));
		rows.back()->set_location(map_location(0, rows.size()));
		rows.back()->from(row);
		
		BOOST_FOREACH (const config& col, row.child_range("column")) {
			if (first_row) {
				cols.push_back(new unit(controller, disp, unit::COLUMN, parent));
				cols.back()->set_location(map_location(cols.size(), 0));
				cols.back()->from(col);
			}
			int colno = 1;
			BOOST_FOREACH (const config::any_child& c, col.all_children_range()) {
				type = c.key;
				definition = c.cfg["definition"].str();
				gui2::tcontrol_definition_ptr ptr;
				if (type != "grid") {
					ptr = mkwin_display::find_widget(type, definition);
				}
				units.push_back(new unit(controller, disp, std::make_pair(type, ptr), parent));
				units.back()->set_location(map_location(colno ++, rows.size()));
				units.back()->from(col);
			}
		}
		first_row = false;
	}
}

void unit::from_grid(const config& cfg)
{
	children_.push_back(tchild());
	unit::tchild& child = children_.back();
	child.from(controller_, disp_, this, cfg);
}

void unit::from_stacked_widget(const config& cfg)
{
	const config& stack_cfg = cfg.child("stack");
	if (!stack_cfg) {
		return;
	}
	BOOST_FOREACH (const config& layer, stack_cfg.child_range("layer")) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, this, layer);
	}
}

void unit::from_listbox(const config& cfg)
{
	const config& header = cfg.child("header");
	if (header) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, this, header);
	} else {
		insert_child(mkwin_controller::default_width, mkwin_controller::default_height);
	}

	children_.push_back(tchild());
	children_.back().from(controller_, disp_, this, cfg.child("list_definition"));

	const config& footer = cfg.child("footer");
	if (footer) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, this, footer);
	} else {
		insert_child(mkwin_controller::default_width, mkwin_controller::default_height);
	}
}

void unit::from_toggle_panel(const config& cfg)
{
	children_.push_back(tchild());
	children_.back().from(controller_, disp_, this, cfg.child("grid"));
}

void unit::from_scrollbar_panel(const config& cfg)
{
	children_.push_back(tchild());
	children_.back().from(controller_, disp_, this, cfg.child("definition"));
}

void unit::from_tree_view(const config& cfg)
{
	cell_.widget.tree_view.indention_step_size = cfg["indention_step_size"].to_int();
	const config& node_cfg = cfg.child("node");
	if (!node_cfg) {
		return;
	}
	cell_.widget.tree_view.node_id = node_cfg["id"].str();

	children_.push_back(tchild());
	children_.back().from(controller_, disp_, this, node_cfg.child("node_definition"));
}

void unit::from_drawing(const config& cfg)
{
	const config& draw_cfg = cfg.child("draw");
	if (!draw_cfg) {
		return;
	}
	const config& image_cfg = draw_cfg.child("image");
	if (!image_cfg) {
		return;
	}
	cell_.widget.draw.x = formual_extract_str(image_cfg["x"].str());
	cell_.widget.draw.y = formual_extract_str(image_cfg["y"].str());
	cell_.widget.draw.w = formual_extract_str(image_cfg["w"].str());
	cell_.widget.draw.h = formual_extract_str(image_cfg["h"].str());
	cell_.widget.draw.name = formual_extract_str(image_cfg["name"].str());
}