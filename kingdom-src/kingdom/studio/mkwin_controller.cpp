/* $Id: editor_controller.cpp 47755 2010-11-29 12:57:31Z shadowmaster $ */
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

#include "mkwin_controller.hpp"
#include "mkwin_display.hpp"

#include "gettext.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "hotkeys.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/mkwin_theme.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/system.hpp"
#include "gui/dialogs/test.hpp"
#include "gui/dialogs/row_setting.hpp"
#include "gui/dialogs/column_setting.hpp"
#include "gui/dialogs/window_setting.hpp"
#include "gui/dialogs/control_setting.hpp"
#include "gui/dialogs/linked_group.hpp"
#include "gui/dialogs/mode_setting.hpp"
#include "gui/dialogs/drawing_setting.hpp"
#include "gui/dialogs/scroll_setting.hpp"
#include "gui/dialogs/rect_setting.hpp"
#include "gui/dialogs/grid_setting.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "serialization/parser.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

void square_fill_frame(t_translation::t_map& tiles, size_t start, const t_translation::t_terrain& terrain, bool front, bool back)
{
	if (front) {
		// first column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[start][n] = terrain;
		}
		// first line(border)
		for (size_t n = start + 1; n < tiles.size() - start; n ++) {
			tiles[n][start] = terrain;
		}
	}

	if (back) {
		// last column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[tiles.size() - start - 1][n] = terrain;
		}
		// last line(border)
		for (size_t n = start; n < tiles.size() - start - 1; n ++) {
			tiles[n][tiles[0].size() - start - 1] = terrain;
		}
	}
}

std::string generate_map_data(int width, int height)
{
	VALIDATE(width > 0 && height > 0, "map must not be empty!");

	const t_translation::t_terrain normal = t_translation::read_terrain_code("Gg");
	const t_translation::t_terrain border = t_translation::read_terrain_code("Gs");
	const t_translation::t_terrain control = t_translation::read_terrain_code("Gd");
	const t_translation::t_terrain forbidden = t_translation::read_terrain_code("Gll");

	t_translation::t_map tiles(width + 2, t_translation::t_list(height + 2, normal));
	square_fill_frame(tiles, 0, border, true, true);
	square_fill_frame(tiles, 1, control, true, false);

	const size_t border_size = 1;
	tiles[border_size][border_size] = forbidden;

	// tiles[border_size][tiles[0].size() - border_size - 1] = forbidden;

	// tiles[tiles.size() - border_size - 1][border_size] = forbidden;
	// tiles[tiles.size() - border_size - 1][tiles[0].size() - border_size - 1] = forbidden;

	std::string str = gamemap::default_map_header + t_translation::write_game_map(t_translation::t_map(tiles));

	return str;
}

std::string generate_anchor_rect(int x, int y, int w, int h)
{
	std::stringstream ss;
	ss << x << "," << y << "," << x + w << "," << y + h;
	return ss.str();
}

mkwin_controller::mkwin_controller(const config &top_config, CVideo& video, bool theme)
	: controller_base(SDL_GetTicks(), top_config, video)
	, gui_(NULL)
	, map_(top_config, null_str)
	, units_(*this, map_, !theme)
	, current_unit_(NULL)
	, parent_nodes_(1, tsheet_node(NULL))
	, unit_nodes_()
	, copied_unit_(NULL)
	, last_unit_(NULL)
	, top_()
	, theme_(theme)
	, theme_widget_start_(0)
	, top_rect_cfg_(NULL)
	, modes_()
	, callback_sstr_()
	, do_quit_(false)
	, quit_mode_(EXIT_ERROR)
{
	gui2::init_layout_mode();

	textdomains_.push_back("wesnoth-lib");
	textdomains_.push_back("wesnoth-editor");
	textdomains_.push_back("wesnoth");

	insert_mode(tmode::def_id);
	tadjust::init_fields();

	int original_width = default_width;
	int original_height = default_height;
	if (theme_) {
		original_width = 1 + ceil(1.0 * theme::XDim / image::tile_size);
		original_height = 1 + ceil(1.0 * theme::YDim / image::tile_size);
	}
	map_ = gamemap(top_config, generate_map_data(original_width, original_height));

	units_.create_coor_map(map_.w(), map_.h());

	init_gui(video);
	init_music(top_config);

	fill_spacer();
	
	if (theme_) {
		theme_widget_start_ = original_width + original_height - 1;
		int zoom = gui_->hex_width();
		SDL_Rect rect = create_rect(zoom, zoom, zoom * 2, zoom * 2);
		unit2* tmp = new unit2(*this, *gui_, units_, std::make_pair("main_map", gui2::tcontrol_definition_ptr()), current_unit_, rect);
		gui2::tcell_setting& cell = tmp->cell();
		
		cell.id = theme::id_main_map;
		cell.cfg["ref"] = theme::id_screen;
		cell.cfg["rect"] = "=,=,+144,+144";
		cell.cfg["xanchor"] = gui2::horizontal_anchor.find(theme::TOP_ANCHORED)->second.id;
		cell.cfg["yanchor"] = gui2::vertical_anchor.find(theme::TOP_ANCHORED)->second.id;
		units_.insert2(*gui_, tmp);
	}
	set_status();
	gui_->show_context_menu();

}

void mkwin_controller::init_gui(CVideo& video)
{
	const config &theme_cfg = get_theme(game_config_, "mkwin");
	gui_ = new mkwin_display(*this, units_, video, map_, theme_cfg, config());
}

void mkwin_controller::init_music(const config& game_config)
{
	const config &cfg = game_config.child("editor_music");
	if (!cfg) {
		return;
	}
	BOOST_FOREACH (const config &i, cfg.child_range("music")) {
		sound::play_music_config(i);
	}
	sound::commit_music_changes();
}


mkwin_controller::~mkwin_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = NULL;
	}
}

EXIT_STATUS mkwin_controller::main_loop()
{
	try {
		while (!do_quit_) {
			play_slice();
		}
	} catch (twml_exception& e) {
		e.show(gui());
	}
	return quit_mode_;
}

events::mouse_handler_base& mkwin_controller::get_mouse_handler_base()
{
	return *this;
}

void mkwin_controller::execute_command2(int command, const std::string& sparam)
{
	using namespace gui2;
	unit* u = units_.find_unit(selected_hex_);

	switch (command) {
		case tmkwin_theme::HOTKEY_SELECT:
			click_widget(gui_->spacer.first, gui_->spacer.second->id);
			break;
		case tmkwin_theme::HOTKEY_GRID:
			click_widget("grid", null_str);
			break;
		case tmkwin_theme::HOTKEY_RCLICK:
			do_right_click();
			break;

		case tmkwin_theme::HOTKEY_INSERT_TOP:
			change_map(gui2::tgrid::BORDER_TOP, true, selected_hex_.y);
			break;
		case tmkwin_theme::HOTKEY_INSERT_BOTTOM:
			change_map(gui2::tgrid::BORDER_BOTTOM, true, selected_hex_.y);
			break;
		case tmkwin_theme::HOTKEY_ERASE_ROW:
			change_map(gui2::tgrid::BORDER_BOTTOM, false, selected_hex_.y);
			break;

		case tmkwin_theme::HOTKEY_INSERT_LEFT:
			change_map(gui2::tgrid::BORDER_LEFT, true, selected_hex_.x);
			break;
		case tmkwin_theme::HOTKEY_INSERT_RIGHT:
			change_map(gui2::tgrid::BORDER_RIGHT, true, selected_hex_.x);
			break;
		case tmkwin_theme::HOTKEY_ERASE_COLUMN:
			change_map(gui2::tgrid::BORDER_RIGHT, false, selected_hex_.x);
			break;

		case tmkwin_theme::HOTKEY_SETTING:
			widget_setting(false);
			break;
		case tmkwin_theme::HOTKEY_LINKED_GROUP:
			linked_group_setting();
			break;
		case tmkwin_theme::HOTKEY_MODE_SETTING:
			mode_setting();
			break;
		case tmkwin_theme::HOTKEY_SPECIAL_SETTING:
			widget_setting(true);
			break;
		case tmkwin_theme::HOTKEY_RECT_SETTING:
			rect_setting();
			break;

		case HOTKEY_COPY:
			copy_widget();
			break;
		case HOTKEY_PASTE:
			paste_widget();
			break;
		case tmkwin_theme::HOTKEY_ERASE:
			erase_widget();
			break;

		case tmkwin_theme::HOTKEY_INSERT_CHILD:
			u->insert_child(default_width, default_height);
			gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, u);
			break;
		case tmkwin_theme::HOTKEY_ERASE_CHILD:
			parent_nodes_.back().parent->erase_child(parent_nodes_.back().number);
			units_.clear();
			simulate_toggle_sheet(parent_nodes_.size() - 2, false);
			break;

		case tmkwin_theme::HOTKEY_RUN:
			run();
			break;
		case HOTKEY_SYSTEM:
			system();
			break;

		default:
			controller_base::execute_command2(command, sparam);
	}
}

void mkwin_controller::widget_setting(bool special)
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);

	gui2::tsetting_dialog *dlg = NULL;
	if (special) {
		if (u->is_drawing()) {
			dlg = new gui2::tdrawing_setting(*gui_, *u);
		} else {
			dlg = new gui2::tscroll_setting(*gui_, *u);
		}

	} else if (u->type() == unit::WIDGET) {
		dlg = new gui2::tcontrol_setting(*gui_, *u, textdomains_, linked_groups_);

	} else if (u->type() == unit::ROW) {
		dlg = new gui2::trow_setting(*gui_, *u);

	} else if (u->type() == unit::COLUMN) {
		dlg = new gui2::tcolumn_setting(*gui_, *u);

	} else if (u->type() == unit::WINDOW) {
		if (!current_unit_) {
			dlg = new gui2::twindow_setting(*gui_, *u, textdomains_);
		} else {
			const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = current_unit_->widget();
			dlg = new gui2::tgrid_setting(*this, *gui_, units_, *u, widget.first);
		}
	}

	dlg->show(gui_->video());
	int res = dlg->get_retval();
	if (res != gui2::twindow::OK) {
		delete dlg;
		return;
	}
	u->set_cell(dlg->cell());
	delete dlg;
}

void mkwin_controller::rect_setting()
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);

	{
		gui2::trect_setting dlg(*this, *gui_, units_, *u, true);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		u->set_cell(dlg.cell());
	}

	tswitch_units_lock lock(*this);

	std::vector<unit*> top_units = form_top_units();
	config theme_cfg = generate_theme_cfg(top_units);
	SDL_Rect screen = create_rect(0, 0, theme::XDim, theme::YDim);
	std::string patch = gui_->get_theme_patch();
	config theme_cfg2;
	const config* theme_current_cfg = theme::set_resolution(theme_cfg.child("theme"), screen, patch, theme_cfg2);

	bool valid = false;
	std::vector<std::string> mistaken_ids;
	double factor = gui_->get_zoom_factor();
	int zoom = gui_->hex_size();

	// set_rect maybe result sort
	int index = 0;

	BOOST_FOREACH (const config::any_child& v, theme_current_cfg->all_children_range()) {
		if (!valid) {
			if (v.key == "main_map_border") {
				valid = true;
			}
			continue;
		}
		unit* u = top_units[index ++];
		SDL_Rect rect = theme::calculate_relative_loc(v.cfg, theme::XDim, theme::YDim);
		if (!rect.w || !rect.h) {
			mistaken_ids.push_back(u->cell().id);
			break;
		} else {
			rect = create_rect(rect.x * factor, rect.y * factor, rect.w * factor, rect.h * factor);
			rect.x += zoom;
			rect.y += zoom;
			u->set_rect(rect);
		}
	}
	if (!mistaken_ids.empty()) {
		std::stringstream ids;
		for (std::vector<std::string>::const_iterator it = mistaken_ids.begin(); it != mistaken_ids.end(); ++ it) {
			const std::string& id = *it;
			if (id.empty()) {
				continue;
			}
			if (!ids.str().empty()) {
				ids << ", ";
			}
			ids << tintegrate::generate_format(id, "red");
		}
		utils::string_map symbols;
		symbols["ids"] = ids.str();
		symbols["count"] = tintegrate::generate_format(mistaken_ids.size(), "yellow");
		const std::string err = vgettext("$ids $count widgets are invalid rect setting!", symbols);
		gui2::show_message(gui_->video(), "", err);
	}

	gui2::tmkwin_theme* theme = dynamic_cast<gui2::tmkwin_theme*>(gui_->get_theme());
	theme->fill_object_list(units_);
}

void mkwin_controller::linked_group_setting()
{
	gui2::tlinked_group2 dlg(*gui_, linked_groups_);
	dlg.show(gui_->video());
	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}
	linked_groups_ = dlg.linked_groups();
}

void mkwin_controller::mode_setting()
{
	gui2::tmode_setting dlg(*this, *gui_, units_);
	dlg.show(gui_->video());
	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}
}

void mkwin_controller::copy_widget()
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);
	set_copied_unit(u);

	gui_->show_context_menu();
}

void mkwin_controller::paste_widget()
{
	if (!selected_hex_.valid() || !copied_unit_) {
		return;
	}

	if (copied_unit_->type() == unit::WIDGET) {
		units_.erase(selected_hex_);
		unit* clone = new unit(*copied_unit_);
		// copied_unit_ and u maybe in different page.
		clone->set_parent(current_unit_);	
		units_.insert(selected_hex_, clone);

	} else if (copied_unit_->type() == unit::ROW) {
		int w = map_.w();
		map_location from(0, copied_unit_->get_location().y);
		map_location to(0, selected_hex_.y);
		for (int x = 0; x < w; x ++) {
			from.x = x;
			to.x = x;
			unit* from_u = units_.find_unit(from);
			units_.erase(to);
			unit* clone = new unit(*from_u);
			units_.insert(to, clone);
		}

	} else if (copied_unit_->type() == unit::COLUMN) {
		int h = map_.h();
		map_location from(copied_unit_->get_location().x, 0);
		map_location to(selected_hex_.x, 0);
		for (int y = 0; y < h; y ++) {
			from.y = y;
			to.y = y;
			unit* from_u = units_.find_unit(from);
			units_.erase(to);
			unit* clone = new unit(*from_u);
			units_.insert(to, clone);
		}

	}

	gui_->show_context_menu();
}

void mkwin_controller::erase_widget()
{
	if (!selected_hex_.valid()) {
		return;
	}

	units_.erase(selected_hex_);
	if (in_theme_top()) {
		selected_hex_ = map_location();
		gui2::tmkwin_theme* theme = dynamic_cast<gui2::tmkwin_theme*>(gui_->get_theme());
		theme->fill_object_list(units_);
	} else {
		units_.insert(selected_hex_, new unit(*this, *gui_, units_, gui_->spacer, current_unit_));
		unit* u = units_.find_unit(selected_hex_);
		replace_child_unit(u);
	}

	gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, units_.find_unit(selected_hex_, true));
	gui_->show_context_menu();
}

void mkwin_controller::set_status()
{
	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("status"));
	int w = widget->get_width();
	int h = widget->get_height();
	surface result = make_neutral_surface(image::get_image("misc/background45.png"));
	result = scale_surface(result, w, h);
	if (copied_unit_) {
		const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = copied_unit_->widget();
		std::string png = copied_unit_->image();
		surface fg = scale_surface(image::get_image(png), w / 2, h / 2);
		if (fg) {
			SDL_Rect dst_clip = empty_rect;
			sdl_blit(fg, NULL, result, &dst_clip);
		}
	}
	gui_->widget_set_surface("status", result);
}

void mkwin_controller::set_copied_unit(unit* u)
{
	if (copied_unit_ == u) {
		return;
	}
	copied_unit_ = u;
	if (gui_) {
		set_status();
	}
}

void mkwin_controller::fill_spacer()
{
	int width = map_.w();
	int height = map_.h();

	int zoom = gui_->hex_width();
	for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			map_location loc = map_location(x, y);
			if (!units_.find_base_unit(loc, true)) {
				if (x && y) {
					if (!in_theme_top()) {
						units_.insert(loc, new unit(*this, *gui_, units_, gui_->spacer, current_unit_));
					}
				} else {
					int type = unit::NONE;
					if (!x && !y) {
						type = unit::WINDOW;
					} else if (x) {
						type = unit::COLUMN;
					} else if (y) {
						type = unit::ROW;
					}
					if (!in_theme_top()) {
						units_.insert(loc, new unit(*this, *gui_, units_, type, current_unit_));
					} else {
						SDL_Rect rect = create_rect(x * zoom, y * zoom, zoom, zoom);
						units_.insert2(*gui_, new unit2(*this, *gui_, units_, type, current_unit_, rect));
					}
				}
			}
		}
	}
}

void mkwin_controller::change_map(int dir, bool extend, int index)
{
	int map_w = map_.w();
	int map_h = map_.h();

	int new_w = map_w;
	int new_h = map_h;
	int start, stop;
	bool horizontal;

	if (dir == gui2::tgrid::BORDER_LEFT) {
		new_w ++;
		start = index;
		stop = map_w - 1;
		horizontal = true;
	} else if (dir == gui2::tgrid::BORDER_RIGHT) {
		if (extend) {
			new_w ++;
		} else {
			new_w --;
			for (int y = 0; y < map_h; y ++) {
				units_.erase(map_location(index, y));
			}
		}
		start = index + 1;
		stop = map_w - 1;
		horizontal = true;
	} else if (dir == gui2::tgrid::BORDER_TOP) {
		new_h ++;
		start = index;
		stop = map_h - 1;
		horizontal = false;
	} else if (dir == gui2::tgrid::BORDER_BOTTOM) {
		if (extend) {
			new_h ++;
		} else {
			new_h --;
			for (int x = 0; x < map_w; x ++) {
				units_.erase(map_location(x, index));
			}
		}
		start = index + 1;
		stop = map_h - 1;
		horizontal = false;
	}

	units_.move_mass(horizontal, extend, start, stop);
	reload_map(new_w, new_h, true);
	fill_spacer();

	if (current_unit_) {
		const tsheet_node& node = parent_nodes_.back();
		unit::tchild child;
		units_.save_map_to(child, false);
		node.parent->set_child(node.number, child);
	}

	if (selected_hex_.x >= new_w) {
		selected_hex_.x = new_w - 1;
	}
	if (selected_hex_.y >= new_h) {
		selected_hex_.y = new_h - 1;
	}
	gui_->show_context_menu();
}

void mkwin_controller::move_line(bool horizontal, int index)
{
	units_.move_line(horizontal, index);

	if (horizontal) {
		selected_hex_.y ++;
		if (selected_hex_.y == map_.h()) {
			selected_hex_.y = 1;
		}
	} else {
		selected_hex_.x ++;
		if (selected_hex_.x == map_.w()) {
			selected_hex_.x = 1;
		}
	}
	gui_->show_context_menu();
}

void mkwin_controller::swap_line(bool horizontal, int index)
{
	units_.swap_line(horizontal, index);

	if (horizontal) {
		selected_hex_.y ++;
		if (selected_hex_.y == map_.h()) {
			selected_hex_.y = 1;
		}
	} else {
		selected_hex_.x ++;
		if (selected_hex_.x == map_.w()) {
			selected_hex_.x = 1;
		}
	}
	gui_->show_context_menu();
}

class tclick_dismiss
{
public:
	tclick_dismiss(gui2::tcell_setting& cell)
		: cell_(cell)
	{
		original_ = cell.window.click_dismiss;
		cell_.window.click_dismiss = true;
	}
	~tclick_dismiss()
	{
		cell_.window.click_dismiss = original_;
	}

private:
	gui2::tcell_setting& cell_;
	bool original_;
};

void mkwin_controller::run()
{
	if (!window_has_valid(true)) {
		return;
	}
	if (theme_) {
		std::stringstream ss;
		ss << dgettext("wesnoth-lib", "Success");
		gui2::show_message(gui_->video(), "", ss.str());
		return;
	}

	unit* window = get_window();
	// tclick_dismiss lock(window->cell());

	config top = generate_window_cfg();
	gui2::reload_test_window("test", top.child("window"));

	gui2::ttest dlg;
	dlg.show(gui_->video());
}

unit* mkwin_controller::get_window() const
{
	unit* window = NULL;
	if (current_unit_) {
		window = top_.window;
	} else {
		window = units_.find_unit(map_location(0, 0));
	}
	return window;
}

bool mkwin_controller::callback_valid_id(const unit* u, bool show_error)
{
	std::stringstream err;
	utils::string_map symbols;

	const std::string id = u->cell().id;

	if (!id.empty()) {
		symbols["id"] = id;
		if (callback_sstr_.find(id) == callback_sstr_.end()) {
			callback_sstr_.insert(id);
		} else {
			err << vgettext("wesnoth-lib", "Duplicated '$id' value!", symbols);
		}
	} else if (!u->cell().cfg["rect"].empty()) {
		std::stringstream ss;
		if (!u->parent()) {
			ss << "#" << u->get_map_index();
		} else {
			ss << "(" << u->get_location().x << "," << u->get_location().y << ")";
		}
		ss << " type: " << u->widget().first;
		symbols["widget"] = tintegrate::generate_format(ss.str(), "yellow");
		err << vgettext("wesnoth-lib", "widget($widget) has rect, must set id!", symbols);
	}
	if (!err.str().empty()) {
		if (show_error) {
			gui2::show_message(gui_->video(), "", err.str());
		}
		return false;
	}
	return true;
}

bool mkwin_controller::enumerate_child2(const unit::tchild& child, bool show_error) const
{
	if (!fcallback_(child.window, show_error)) {
		return false;
	}

	for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(*it2, show_error)) {
				return false;
			}
		}
		if (!fcallback_(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::enumerate_child(const std::vector<unit*>& top_units, bool show_error) const
{
	for (std::vector<unit*>::const_iterator it = top_units.begin(); it != top_units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(*it2, show_error)) {
				return false;
			}
		}
		if (!fcallback_(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::window_has_valid(bool show_error)
{
	std::stringstream err;
	err << _("Window isn't ready!") << "\n\n";

	const unit* window = get_window();
	if (!window) {
		return false;
	}

	if (window->cell().id.empty() || window->cell().id == gui2::untitled) {
		if (show_error) {
			gui2::show_id_error(*gui_, "id", err.str());
		}
		return false;
	}

	if (!theme_) {
		if (window->cell().window.description.empty()) {
			if (show_error) {
				gui2::show_id_error(*gui_, "description", err.str());
			}
			return false;
		}
		if (window->cell().window.textdomain.empty()) {
			if (show_error) {
				gui2::show_id_error(*gui_, "textdomain", err.str());
			}
			return false;
		}
	} else {
		// all widget that has rect must set id.
		std::vector<unit*> top_units = form_top_units();

		{
			tenumerate_lock lock(*this, boost::bind(&mkwin_controller::callback_valid_id, this, _1, _2));
			if (!enumerate_child(top_units, show_error)) {
				return false;
			}
		}

	}
	return true;
}

void mkwin_controller::system()
{
	enum {_LOAD, _SAVE, _SAVE_AS, _PREFERENCES, _QUIT};

	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;

	bool window_dirty = false;
	if (!theme_) {
		const unit* window = get_window();
		window_dirty = original_.first != window->cell().window.textdomain || original_.second != generate_window_cfg().child("window");
	}

	{
		std::string str = dgettext("wesnoth-lib", "Save");

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Load")));
		rets.push_back(_LOAD);

		items.push_back(gui2::tsystem::titem(str, window_has_valid(false) && window_dirty));
		rets.push_back(_SAVE);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Save As"), window_has_valid(false)));
		rets.push_back(_SAVE_AS);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Preferences")));
		rets.push_back(_PREFERENCES);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Quit")));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(items);
		try {
			dlg.show(gui_->video());
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}
	if (rets[retval] == _LOAD) {
		load_window();	

	} else if (rets[retval] == _SAVE) {
		save_window(false);
		
	} else if (rets[retval] == _SAVE_AS) {
		save_window(true);

	} else if (rets[retval] == _PREFERENCES) {
		preferences::show_preferences_dialog(*gui_);
		gui_->redraw_everything();

	} else if (rets[retval] == _QUIT) {
		quit_confirm(EXIT_NORMAL);
	}
}

#include "loadscreen.hpp"

config window_cfg_from_gui_bin()
{
	config cfg;
	wml_config_from_file(game_config::path + "/xwml/" + "gui.bin", cfg);
	const config& gui_cfg = cfg.find_child("gui", "id", "default");
	return gui_cfg.find_child("window", "id", "book");
}

const config& mkwin_controller::find_rect_cfg(const std::string& widget, const std::string& id) const
{
	return find_rect_cfg2(*top_rect_cfg_, widget, id);
}

const config& mkwin_controller::find_rect_cfg2(const config& cfg, const std::string& widget, const std::string& id) const
{
	config::all_children_itors itors = cfg.all_children_range();
	for (config::all_children_iterator i = itors.first; i != itors.second; ++ i) {
		const config& icfg = i->cfg;
		if (i->cfg["id"] == id && i->key == widget) {
			return icfg;
		}

		// Recursively look in children.
		const config& c = find_rect_cfg2(icfg, widget, id);
		if (!c.empty()) {
			return c;
		}
	}

	return null_cfg;
}

void mkwin_controller::load_window()
{
	bool browse_cfg = true;
	std::stringstream err;

	config window_cfg;
	if (browse_cfg) {
		std::string initial = game_config::preferences_dir_utf8 + "/editor/maps";
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, true, initial, dgettext("wesnoth-lib", "Choose a Window to Open"));
		param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir-res.png");
		gui2::tbrowse dlg(*gui_, param);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		std::string file = param.result;

		config cfg;
		scoped_istream stream = istream_file(file, true);
		read(cfg, *stream);

		if (theme_) {
			const config& window_cfg1 = cfg.child("theme");
			if (!window_cfg1) {
				err << dgettext("wesnoth-lib", "Invalid theme cfg file!");
				gui2::show_message(gui_->video(), "", err.str());
				return;
			}
			window_cfg = window_cfg1;

		} else {
			const config& window_cfg1 = cfg.child("window");
			if (!window_cfg1) {
				err << dgettext("wesnoth-lib", "Invalid dialog cfg file!");
				gui2::show_message(gui_->video(), "", err.str());
				return;
			}
			window_cfg = window_cfg1;
		}
		
	} else {
		window_cfg = window_cfg_from_gui_bin();
	}

	if (theme_) {
		const config& res_cfg = window_cfg.child("resolution");
		if (!res_cfg) {
			return;
		}

		if (current_unit_) {
			simulate_toggle_sheet(0, true);
		}
		units_.save_map_to(top_, true);
		// clear units.
		for (std::vector<unit*>::const_iterator it = top_.units.begin(); it != top_.units.end(); ++ it) {
			delete *it;
		}
		top_.units.clear();

		SDL_Rect screen = create_rect(0, 0, theme::XDim, theme::YDim);
		std::string patch = gui_->get_theme_patch();
		config theme_cfg2;
		const config* theme_current_cfg = theme::set_resolution(window_cfg, screen, patch, theme_cfg2);

		reload_map(top_.cols.size() + 1, top_.rows.size() + 1, true);
		units_.restore_map_from(top_);

		{
			tfind_rect_cfg_lock lock(*this, res_cfg);
			top_.from(*this, *gui_, units_, NULL, *theme_current_cfg);
		}
		top_.window->from(window_cfg);

	} else {
		const config& res_cfg = window_cfg.child("resolution");
		if (!res_cfg) {
			return;
		}
		const config& top_grid = res_cfg.child("grid");
		if (!top_grid) {
			return;
		}

		top_.clear(false);
		linked_groups_.clear();
		units_.clear();
		
		top_.from(*this, *gui_, units_, NULL, top_grid);
		top_.window->from(window_cfg);
		BOOST_FOREACH (const config& linked, res_cfg.child_range("linked_group")) {
			linked_groups_.push_back(gui2::tlinked_group(linked));
		}

		simulate_toggle_sheet(0, false);
	}

	original_.first = top_.window->cell().window.textdomain;
	original_.second = window_cfg;
}

void mkwin_controller::save_window(bool as)
{
	const unit* u = get_window();
	std::string fname = game_config::preferences_dir_utf8 + "/editor/maps/";
	{
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, false, fname, dgettext("wesnoth-lib", "Choose a Window to Save"), dgettext("wesnoth-lib", "Save"));
		param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir-res.png");
		gui2::tbrowse dlg(*gui_, param);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		fname = param.result;
	}

	std::stringstream ss;

	ss << "#textdomain " << u->cell().window.textdomain << "\n";
	ss << "\n";

	config top;
	if (!theme_) {
		top = generate_window_cfg();
	} else {
		top = generate_theme_cfg(form_top_units());
	}
	::write(ss, top, 0, u->cell().window.textdomain);
	write_file(fname, ss.str());

	original_.first = u->cell().window.textdomain;
	if (!theme_) {
		original_.second = top.child("window");
	} else {
		original_.second = top.child("theme");
	}
}

void mkwin_controller::quit_confirm(EXIT_STATUS mode)
{
	std::string message = _("Do you really want to quit?");
	const int res = gui2::show_message(gui().video(), _("Quit"), message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::CANCEL) {
		do_quit_ = true;
		quit_mode_ = mode;
	}
}

void mkwin_controller::mouse_motion(int x, int y, const bool browse, bool update)
{
	mouse_handler_base::mouse_motion(x, y, browse, update);

	if (mouse_handler_base::mouse_motion_default(x, y, update)) return;
	map_location hex_clicked = gui_->hex_clicked_on(x, y);
	last_hex_ = hex_clicked;
	last_unit_ = units_.unit_clicked_on(x, y, last_hex_);

	gui_->highlight_hex(hex_clicked);

	if (cursor::get() != cursor::WAIT) {
		if (current_unit_ && !last_hex_.x && !last_hex_.y) {
			// cursor::set(cursor::ILLEGAL);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else if (!last_hex_.x || !last_hex_.y || !map_.on_board(last_hex_)) {
			// cursor::set(cursor::INTERIOR);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else {
			// no selected unit or we can't move it
			cursor::set(cursor::NORMAL);
			gui_->resume_mouseover_hex_overlay();
		}
	}
}

void mkwin_controller::select_unit(unit* u)
{
	gui_->scroll_to_tile(*u->get_draw_locations().begin(), display::ONSCREEN, true, true);

	selected_hex_ = u->get_location();
	gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, u);
	gui_->show_context_menu();
}

void mkwin_controller::derive_create(unit* u)
{
	int create_childs = 0;
	if (u->is_grid() || u->is_stacked_widget() || u->is_panel() || u->is_scrollbar_panel() || u->is_tree_view()) {
		create_childs = 1;
	} else if (u->is_listbox()) {
		create_childs = 3;
	}
	for (int i = 0; i < create_childs; i ++) {
		u->insert_child(default_width, default_height);
	}
}

void mkwin_controller::replace_child_unit(unit* u)
{
	if (!current_unit_) {
		return;
	}
	const tsheet_node& node = parent_nodes_.back();
	unit::tchild& child = node.parent->child(node.number);
	int pitch = (u->get_location().y - 1) * child.cols.size();
	int at = pitch + u->get_location().x - 1;
	child.units[at] = u;
}

bool mkwin_controller::left_click(int x, int y, const bool browse)
{
	if (mouse_handler_base::left_click(x, y, browse)) {
		return false;
	}
	if (!map_.on_board(last_hex_)) {
		return false;
	}
	if (in_theme_top()) {
		int type = last_unit_? dynamic_cast<unit*>(last_unit_)->type(): unit::NONE;
		if (type == unit::WINDOW || type == unit::WIDGET) {
			selected_hex_ = last_unit_->get_location();
		} else {
			selected_hex_ = map_location::null_location;
		}
	} else {
		selected_hex_ = last_hex_;
	}
	
	unit* u = NULL;
	if (last_hex_.x && last_hex_.y) {
		u = units_.find_unit(selected_hex_);
		if (in_theme_top()) {
			if (gui_->selected_widget() != gui_->spacer && !u) {
				int xmap = x;
				int ymap = y;
				gui_->pixel_screen_to_map(xmap, ymap);
				SDL_Rect rect = create_rect(xmap, ymap, gui_->hex_width() / 2, gui_->hex_width() / 2);
				u = new unit2(*this, *gui_, units_, gui_->selected_widget(), current_unit_, rect);
				gui2::tcell_setting& cell = u->cell();
				cell.cfg["ref"] = theme::id_screen;
				cell.cfg["rect"] = generate_anchor_rect(rect.x, rect.y, rect.w, rect.h);
				units_.insert2(*gui_, u);
				derive_create(u);

				selected_hex_ = u->get_location();

				gui2::tmkwin_theme* theme = dynamic_cast<gui2::tmkwin_theme*>(gui_->get_theme());
				theme->fill_object_list(units_);
			}

		} else if (gui_->selected_widget() != gui_->spacer && (!u || u->is_spacer())) {
			if (u) {
				units_.erase(selected_hex_);
			}
			units_.insert(selected_hex_, new unit(*this, *gui_, units_, gui_->selected_widget(), current_unit_));

			u = units_.find_unit(selected_hex_);
			derive_create(u);

			replace_child_unit(u);
		}
	}
	gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, u);
	gui_->show_context_menu();

	return true;
}

bool mkwin_controller::right_click(int x, int y, const bool browse)
{
	if (mouse_handler_base::right_click(x, y, browse)) {
		const SDL_Rect& rect = gui_->map_outside_area();
		if (!point_in_rect(x, y, rect)) {
			return false;
		}
	}

	do_right_click();
	
	return false;
}

void mkwin_controller::do_right_click()
{
	if (current_unit_) {
		simulate_toggle_sheet(0, true);
	}

	selected_hex_ = map_location();
	gui_->show_context_menu();
	gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, NULL);
}

void mkwin_controller::pre_change_resolution()
{
	if (current_unit_) {
		simulate_toggle_sheet(0, true);
	}

	units_.save_map_to(top_, true);
}

void mkwin_controller::post_change_resolution()
{
	reload_map(top_.cols.size() + 1, top_.rows.size() + 1, true);
	units_.restore_map_from(top_);
	set_status();

	selected_hex_ = map_location();
	gui_->show_context_menu();
}

void mkwin_controller::click_widget(const std::string& type, const std::string& definition)
{
	if (type != "grid") {
		gui_->click_widget(type, definition);
	} else {
		gui_->click_grid(type);
	}
}

void mkwin_controller::simulate_toggle_sheet(int index, bool save)
{
	gui2::ttoggle_button* widget = gui_->sheet_widget(index);
	widget->set_value(true);
	sheet_toggled(widget, index, save);
}

void mkwin_controller::sheet_toggled(gui2::twidget* widget, int index, bool save)
{
	gui2::ttoggle_button* toggle = dynamic_cast<gui2::ttoggle_button*>(widget);
	gui_->sheet_toggled(*toggle);
	switch_sheet_to(index, save);
}

void mkwin_controller::switch_sheet_to(int index, bool save)
{
	tsheet_node orignal = parent_nodes_.back();
	if (index > 0) {
		VALIDATE(parent_nodes_.back().parent == current_unit_, "Last node's parent must be current_unit!");

		int orignal_nodes = (int)parent_nodes_.size();

		VALIDATE(index != orignal_nodes - 1, "radio toggle_button must not be re-enable!");

		if (index >= orignal_nodes) {
			// foreword
			current_unit_ = units_.find_unit(selected_hex_, true);

			// append unit_node to it. (only one page that selected!)
			parent_nodes_.push_back(unit_nodes_[index - orignal_nodes]);

			if (index > orignal_nodes) {
				gui_->sheet_widget(index)->set_value(false);
				gui_->sheet_widget(orignal_nodes)->set_value(true);
			}
			selected_hex_ = map_location(1, 1);

		} else {
			// backword
			current_unit_ = parent_nodes_[index].parent;

			selected_hex_ = parent_nodes_[index + 1].parent->get_location();

			std::vector<tsheet_node>::iterator it = parent_nodes_.begin();
			std::advance(it, index + 1);
			parent_nodes_.erase(it, parent_nodes_.end());
		}

		if (orignal_nodes == 1) {
			// save current to top_
			if (save) {
				units_.save_map_to(top_, true);
			}
			units_.set_consistent(true);

		} else if (save) {
			unit::tchild child;
			units_.save_map_to(child, true);
			orignal.parent->set_child(orignal.number, child);
		}

		const unit::tchild& child = current_unit_->child(parent_nodes_.back().number);
		reload_map(child.cols.size() + 1, child.rows.size() + 1, true);
		units_.restore_map_from(child);

	} else {
		// restore from top_
		if (save) {
			unit::tchild child;
			units_.save_map_to(child, true);
			orignal.parent->set_child(orignal.number, child);
		}
		if (theme_) {
			units_.set_consistent(false);
		}

		reload_map(top_.cols.size() + 1, top_.rows.size() + 1, true);
		units_.restore_map_from(top_);

		current_unit_ = NULL;
		if (parent_nodes_.size() >= 2) {
			selected_hex_ = parent_nodes_[1].parent->get_location();
		
			std::vector<tsheet_node>::iterator it = parent_nodes_.begin();
			std::advance(it, 1);
			parent_nodes_.erase(it, parent_nodes_.end());
		} else {
			selected_hex_ = map_location();
		}
	}

	gui_->refresh_sheet_header(parent_nodes_, unit_nodes_, units_.find_unit(selected_hex_, true));
	gui_->show_context_menu();
}

bool mkwin_controller::toggle_tabbar(gui2::twidget* widget)
{
	long index = reinterpret_cast<long>(widget->cookie());

	gui2::tmkwin_theme* theme = dynamic_cast<gui2::tmkwin_theme*>(gui_->get_theme());
	if (index == 0) {
		theme->load_widget_page();
	} else if (index == 1) {
		theme->load_object_page(units_);
	}
	return true;
}

config mkwin_controller::generate_window_cfg() const
{
	config top, tmp;
	config& window_cfg = top.add_child("window");
	units_.find_unit(map_location(0, 0))->generate(window_cfg);
	config& res_cfg = window_cfg.child("resolution");

	config& top_grid = res_cfg.add_child("grid");

	config unit_cfg;
	config* row_cfg = NULL;
	int current_y = -1;
	for (int y = 1; y < map_.h(); y ++) {
		for (int x = 1; x < map_.w(); x ++) {
			unit* u = units_.find_unit(map_location(x, y));
			if (u->get_location().y != current_y) {
				row_cfg = &top_grid.add_child("row");
				current_y = u->get_location().y;

				unit* row = units_.find_unit(map_location(0, y));
				row->generate(*row_cfg);
			}
			unit_cfg.clear();
			u->generate(unit_cfg);
			if (y == 1) {
				unit* column = units_.find_unit(map_location(x, 0));
				column->generate(unit_cfg);
			}
			row_cfg->add_child("column", unit_cfg);
		}
	}

	return top;
}

config mkwin_controller::generate_theme_cfg(const std::vector<unit*>& units) const
{
	config top;
	config& theme_cfg = top.add_child("theme");
	get_window()->generate(theme_cfg);
	config& res_cfg = theme_cfg.child("resolution");

	// default(1024x768)
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		const unit* u = *it;
		if (u->type() != unit::WIDGET) {
			continue;
		}
		u->generate(res_cfg);
	}
	
	std::stringstream ss;
	size_t size = modes_.size();
	config* parent_cfg = &theme_cfg;
	for (size_t n = 1; n < size; n ++) {
		const tmode& mode = modes_[n];
		config* cfg;
		if (n < tmode::res_count) {
			config& part = parent_cfg->add_child("partialresolution");
			ss.str("");
			ss << mode.res.width << "x" << mode.res.height;
			part["id"] = ss.str();
			ss.str("");
			ss << theme::XDim << "x" << theme::YDim;
			part["inherits"] = ss.str();
			part["width"] = mode.res.width;
			part["height"] = mode.res.height;
			cfg = &part;
		} else {
			if (!(n % tmode::res_count)) {
				parent_cfg = &theme_cfg.add_child(mode.id);
			}
			ss.str("");
			ss << mode.res.width << "x" << mode.res.height;
			cfg = &parent_cfg->add_child(ss.str());
		}

		for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
			unit* u = *it;
			u->generate_adjust(mode, *cfg);
		}
	}
	
	return top;
}

std::vector<unit*> mkwin_controller::form_top_units() const
{
	std::vector<unit*> result;

	if (!current_unit_) {
		for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
			unit* u = dynamic_cast<unit*>(&*it);
			if (u->type() != unit::WIDGET) {
				continue;
			}
			result.push_back(u);
		}
	} else {
		result = top_.units;
	}

	return result;
}

void mkwin_controller::reload_map(int w, int h, bool redraw)
{
	map_ = gamemap(game_config_, generate_map_data(w, h));
	gui_->reload_map();
	units_.create_coor_map(map_.w(), map_.h());
	if (redraw) {
		gui_->redraw_everything();
		gui_->recalculate_minimap();
	}
}

bool same_parent(const unit* a, const unit* b)
{
	const unit* a_parent = a;
	const unit* b_parent = b;

	while (a_parent->parent()) {
		a_parent = a_parent->parent();
		if (a_parent == b) {
			return true;
		}
	}
	while (b_parent->parent()) {
		b_parent = b_parent->parent();
		if (b_parent == a) {
			return true;
		}
	}
	return false;
}

bool mkwin_controller::can_paste(const unit* u) const
{
	if (!u || !copied_unit_ || u == copied_unit_) {
		return false;
	}
	if (copied_unit_->type() == unit::ROW || copied_unit_->type() == unit::COLUMN) {
		return u->parent() == copied_unit_->parent() && u->type() == copied_unit_->type();
	}
	if (!u->is_spacer()) {
		return false;
	}
	return !same_parent(u, copied_unit_);
}

bool mkwin_controller::in_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);

	switch(command) {
	// idle section
	case tmkwin_theme::HOTKEY_RUN:
	case HOTKEY_ZOOM_IN:
	case HOTKEY_ZOOM_OUT:
	case HOTKEY_SYSTEM:
		return !selected_hex_.valid();

	case tmkwin_theme::HOTKEY_SETTING: // setting
		return u && !u->is_main_map();
	case tmkwin_theme::HOTKEY_SPECIAL_SETTING:
		return u && u->has_special_setting();
	case tmkwin_theme::HOTKEY_RECT_SETTING:
		return u && theme_ && u->type() == unit::WIDGET;

	case HOTKEY_COPY:
		return u && u != copied_unit_ && !u->is_spacer() && !u->is_main_map() && u->type() != unit::WINDOW;
	case HOTKEY_PASTE:
		return can_paste(u);

	// unit
	case tmkwin_theme::HOTKEY_ERASE: // erase
		return u && u->type() == unit::WIDGET && !u->is_spacer() && !u->is_main_map();

	// window
	case tmkwin_theme::HOTKEY_LINKED_GROUP:
		return !theme_ && u && u->type() == unit::WINDOW && !current_unit_;
	case tmkwin_theme::HOTKEY_MODE_SETTING:
		return in_theme_top() && u && u->type() == unit::WINDOW && !current_unit_;

	// row
	case tmkwin_theme::HOTKEY_INSERT_TOP:
	case tmkwin_theme::HOTKEY_INSERT_BOTTOM:
	case tmkwin_theme::HOTKEY_ERASE_ROW:
		return !in_theme_top() && u && u->type() == unit::ROW;

	// column
	case tmkwin_theme::HOTKEY_INSERT_LEFT:
	case tmkwin_theme::HOTKEY_INSERT_RIGHT:
	case tmkwin_theme::HOTKEY_ERASE_COLUMN:
		return !in_theme_top() && u && u->type() == unit::COLUMN;

	case tmkwin_theme::HOTKEY_INSERT_CHILD: // add a page
		return u && u->is_stacked_widget();
	case tmkwin_theme::HOTKEY_ERASE_CHILD: // erase this page
		return u && u->parent() && u->parent()->is_stacked_widget() && u->parent()->children().size() >= 2;

	default:
		return false;
	}

	return false;
}

bool mkwin_controller::actived_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);

	switch(command) {
	// row
	case tmkwin_theme::HOTKEY_ERASE_ROW:
		return map_.h() >= 3;

	// column
	case tmkwin_theme::HOTKEY_ERASE_COLUMN:
		return map_.w() >= 3;

	}
	return true;
}

bool mkwin_controller::verify_new_mode(const std::string& str)
{
	std::stringstream err;
	for (std::vector<tmode>::const_iterator it = modes_.begin(); it != modes_.end(); ++ it) {
		if (it->id == str) {
			err << dgettext("wesnoth-lib", "Duplicate mode!");
			gui2::show_message(gui_->video(), "", err.str());
			return false;
		}
	}
	return true;
}

void mkwin_controller::insert_mode(const std::string& id)
{
	modes_.push_back(tmode(id, 1024, 768));
	modes_.push_back(tmode(id, 800, 600));
	modes_.push_back(tmode(id, 640, 480));
	modes_.push_back(tmode(id, 480, 320));
}