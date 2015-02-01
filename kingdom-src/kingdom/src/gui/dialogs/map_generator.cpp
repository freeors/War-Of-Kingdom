/* $Id: mp_connect.cpp 49307 2011-04-25 07:19:14Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/map_generator.hpp"

#include "gettext.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "game_display.hpp"
#include "mapgen_dialog.hpp"
#include "map.hpp"

#include <boost/bind.hpp>

namespace gui2 {

namespace {
	const int min_players = 2;

	const int min_width = 20;
	const int max_width = 100;
	const int max_height = 100;

	const int min_iterations = 10;
	const int max_iterations = 3000;

	const int min_hillsize = 1;
	const int max_hillsize = 50;

	const int min_villages = 0;
	const int max_villages = 50;

	const int min_castlesize = 2;
	const int max_castlesize = 14;

	const size_t max_island = 10;
	const size_t max_coastal = 5;
}

REGISTER_DIALOG(map_generator)

tmap_generator::tmap_generator(display& gui, default_map_generator& generator, int max_players, int min_w, int max_w, int min_h, int max_h)
	: gui_(gui)
	, generator_(generator)
	, max_players_(max_players)
	, min_width_(min_w != -1? min_w: min_width)
	, max_width_(max_w != -1? max_w: max_width)
	, min_height_(min_h != -1? min_h: min_width)
	, max_height_(max_h != -1? max_h: max_height)
	, val_nplayers_(generator.nplayers_)
	, val_width_(generator.width_)
	, val_height_(generator.height_)
	, val_iterations_(generator.iterations_)
	, val_hill_size_(generator.hill_size_)
	, val_nvillages_(generator.nvillages_)
	, val_castle_size_(generator.castle_size_)
	, val_island_size_(generator.island_size_)
	, val_link_castles_(generator.link_castles_)
{
}

std::string players_str(int players)
{
	std::stringstream strstr;
	strstr << players;
	return strstr.str();
}

std::string width_str(int width)
{
	std::stringstream strstr;
	strstr << width;
	return strstr.str();
}

std::string height_str(int height)
{
	std::stringstream strstr;
	strstr << height;
	return strstr.str();
}

std::string iterations_str(int iterations)
{
	std::stringstream strstr;
	strstr << iterations;
	return strstr.str();
}

std::string hillsize_str(int hillsize)
{
	std::stringstream strstr;
	strstr << hillsize;
	return strstr.str();
}

std::string villages_str(int villages)
{
	std::stringstream strstr;
	strstr << villages << _("/1000 tiles");
	return strstr.str();
}

std::string castlesize_str(int castlesize)
{
	std::stringstream strstr;
	strstr << castlesize;
	return strstr.str();
}

std::string landform_str(int island_size)
{
	std::stringstream strstr;
	if (island_size == 0) {
		strstr << _("Inland");
	} else if (island_size < max_coastal) {
		strstr << _("Coastal");
	} else {
		strstr << _("Island");
	}
	strstr << "(" << island_size << ")";
	return strstr.str();
}

void tmap_generator::pre_show(CVideo& /*video*/, twindow& window)
{
	players_ = find_widget<tbutton>(&window, "players", false, true);
	connect_signal_mouse_left_click(
		*players_
		, boost::bind(
			&tmap_generator::players
			, this
			, boost::ref(window)));
	players_->set_label(players_str(val_nplayers_));

	width_ = find_widget<tbutton>(&window, "width", false, true);
	connect_signal_mouse_left_click(
		*width_
		, boost::bind(
			&tmap_generator::width
			, this
			, boost::ref(window)));
	width_->set_label(width_str(val_width_));

	height_ = find_widget<tbutton>(&window, "height", false, true);
	connect_signal_mouse_left_click(
		*height_
		, boost::bind(
			&tmap_generator::height
			, this
			, boost::ref(window)));
	height_->set_label(height_str(val_height_));

	iterations_ = find_widget<tbutton>(&window, "iterations", false, true);
	connect_signal_mouse_left_click(
		*iterations_
		, boost::bind(
			&tmap_generator::iterations
			, this
			, boost::ref(window)));
	iterations_->set_label(iterations_str(val_iterations_));

	hillsize_ = find_widget<tbutton>(&window, "hillsize", false, true);
	connect_signal_mouse_left_click(
		*hillsize_
		, boost::bind(
			&tmap_generator::hillsize
			, this
			, boost::ref(window)));
	hillsize_->set_label(hillsize_str(val_hill_size_));

	villages_ = find_widget<tbutton>(&window, "villages", false, true);
	connect_signal_mouse_left_click(
		*villages_
		, boost::bind(
			&tmap_generator::villages
			, this
			, boost::ref(window)));
	villages_->set_label(villages_str(val_nvillages_));

	castlesize_ = find_widget<tbutton>(&window, "castlesize", false, true);
	connect_signal_mouse_left_click(
		*castlesize_
		, boost::bind(
			&tmap_generator::castlesize
			, this
			, boost::ref(window)));
	castlesize_->set_label(castlesize_str(val_castle_size_));

	landform_ = find_widget<tbutton>(&window, "landform", false, true);
	connect_signal_mouse_left_click(
		*landform_
		, boost::bind(
			&tmap_generator::landform
			, this
			, boost::ref(window)));
	landform_->set_label(landform_str(val_island_size_));

	link_castles_ = find_widget<ttoggle_button>(&window, "link_castles", false, true);
	link_castles_->set_value(val_link_castles_);
}

void tmap_generator::post_show(twindow& window)
{
	val_link_castles_ = link_castles_->get_value();
}

bool tmap_generator::changed() const
{
	return generator_.nplayers_ != val_nplayers_ ||
		generator_.width_ != val_width_ ||
		generator_.height_ != val_height_ ||
		generator_.iterations_ != val_iterations_ ||
		generator_.hill_size_ != val_hill_size_ ||
		generator_.nvillages_ != val_nvillages_ ||
		generator_.castle_size_ != val_castle_size_ ||
		generator_.island_size_ != val_island_size_ ||
		generator_.link_castles_ != val_link_castles_;
}

void tmap_generator::apply_change()
{
	generator_.nplayers_ = val_nplayers_;
	generator_.width_ = val_width_;
	generator_.height_ = val_height_;
	generator_.iterations_ = val_iterations_;
	generator_.hill_size_ = val_hill_size_;
	generator_.nvillages_ = val_nvillages_;
	generator_.castle_size_ = val_castle_size_;
	generator_.island_size_ = val_island_size_;
	generator_.link_castles_ = val_link_castles_;
}

void tmap_generator::players(twindow& window)
{
	std::vector<tval_str> items;
	
	for (int i = min_players; i <= max_players_; i ++) {
		items.push_back(tval_str(i, players_str(i)));
	}

	gui2::tcombo_box dlg(items, val_nplayers_);
	dlg.show(gui_.video());

	val_nplayers_ = dlg.selected_val();
	players_->set_label(items[dlg.selected_index()].str);
}

void tmap_generator::width(twindow& window)
{
	std::vector<tval_str> items;
	
	int step = (max_width_ - min_width_) / 5;
	if (!step) {
		step = 1;
	}
	for (int i = min_width_; i <= max_width_; i += step) {
		items.push_back(tval_str(i, height_str(i)));
	}

	gui2::tcombo_box dlg(items, val_width_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_width_ = items[selected].val;

	width_->set_label(items[selected].str);
}

void tmap_generator::height(twindow& window)
{
	std::vector<tval_str> items;
	
	int step = (max_height_ - min_height_) / 5;
	if (!step) {
		step = 1;
	}
	for (int i = min_height_; i <= max_height_; i += step) {
		items.push_back(tval_str(i, height_str(i)));
	}

	gui2::tcombo_box dlg(items, val_height_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_height_ = items[selected].val;

	height_->set_label(items[selected].str);
}

void tmap_generator::iterations(twindow& window)
{
	std::vector<tval_str> items;
	
	for (int i = min_iterations; i <= max_iterations; i += (max_iterations - min_iterations) / 10) {
		items.push_back(tval_str(i, iterations_str(i)));
	}

	gui2::tcombo_box dlg(items, val_iterations_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_iterations_ = items[selected].val;

	iterations_->set_label(items[selected].str);
}

void tmap_generator::hillsize(twindow& window)
{
	std::vector<tval_str> items;
		
	int max = max_hillsize;
	int step = (max_hillsize - min_hillsize) / 10;
	if (max_width_ < 10 || max_height_ < 10) {
		max = 3;
		step = 1;
	} else if (max_width_ < 15 || max_height_ < 15) {
		max = 4;
		step = 1;
	} else if (max_width_ < 20 || max_height_ < 20) {
		max = 5;
		step = 1;
	} else if (max_width_ < 30 || max_height_ < 30) {
		max = 10;
		step = 2;
	}
	for (int i = min_hillsize; i <= max; i += step) {
		items.push_back(tval_str(i, hillsize_str(i)));
	}

	gui2::tcombo_box dlg(items, val_hill_size_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_hill_size_ = items[selected].val;

	hillsize_->set_label(items[selected].str);
}

void tmap_generator::villages(twindow& window)
{
	std::vector<tval_str> items;
	
	for (int i = min_villages; i <= max_villages; i += (max_villages - min_villages) / 5) {
		items.push_back(tval_str(i, villages_str(i)));
	}

	gui2::tcombo_box dlg(items, val_nvillages_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_nvillages_ = items[selected].val;

	villages_->set_label(items[selected].str);
}

void tmap_generator::castlesize(twindow& window)
{
	std::vector<tval_str> items;
	
	items.push_back(tval_str(2, castlesize_str(2)));
	items.push_back(tval_str(4, castlesize_str(4)));
	items.push_back(tval_str(6, castlesize_str(6)));
	items.push_back(tval_str(8, castlesize_str(8)));
	items.push_back(tval_str(10, castlesize_str(10)));
	items.push_back(tval_str(12, castlesize_str(12)));
	items.push_back(tval_str(14, castlesize_str(14)));

	gui2::tcombo_box dlg(items, val_castle_size_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_castle_size_ = items[selected].val;

	castlesize_->set_label(items[selected].str);
}

void tmap_generator::landform(twindow& window)
{
	std::vector<tval_str> items;
	
	items.push_back(tval_str(0, landform_str(0)));
	items.push_back(tval_str(2, landform_str(2)));
	items.push_back(tval_str(4, landform_str(4)));
	items.push_back(tval_str(6, landform_str(6)));
	items.push_back(tval_str(8, landform_str(8)));
	items.push_back(tval_str(10, landform_str(10)));

	gui2::tcombo_box dlg(items, val_island_size_);
	dlg.show(gui_.video());

	int selected = dlg.selected_index();
	val_island_size_ = items[selected].val;

	landform_->set_label(items[selected].str);
}

} // namespace


