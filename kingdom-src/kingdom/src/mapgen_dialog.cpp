/* $Id: mapgen_dialog.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "mapgen_dialog.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/map_generator.hpp"
#include "wml_exception.hpp"

static lg::log_domain log_engine("engine");
#define DBG_NG LOG_STREAM(debug, log_engine)

namespace {
	const size_t max_island = 10;
	const size_t max_coastal = 5;
}

default_map_generator::default_map_generator(const config &cfg) :
	default_width_(40),
	default_height_(40),
	width_(40),
	height_(40),
	island_size_(0),
	iterations_(1000),
	hill_size_(10),
	max_lakes_(20),
	nvillages_(25),
	castle_size_(9),
	nplayers_(2),
	link_castles_(true),
	economy_area_(),
	cfg_(cfg ? cfg : config())
{
	if (!cfg) return;

	int width = cfg["map_width"];
	if (width > 0)
		width_ = width;

	int height = cfg["map_height"];
	if (height > 0)
		height_ = height;

	default_width_ = width_;
	default_height_ = height_;

	int iterations = cfg["iterations"];
	if (iterations > 0)
		iterations_ = iterations;

	int hill_size = cfg["hill_size"];
	if (hill_size > 0)
		hill_size_ = hill_size;

	int max_lakes = cfg["max_lakes"];
	if (max_lakes > 0)
		max_lakes_ = max_lakes;

	int nvillages = cfg["villages"];
	if (nvillages > 0)
		nvillages_ = nvillages;

	int castle_size = cfg["castle_size"];
	if (castle_size > 0)
		castle_size_ = castle_size;

	int nplayers = cfg["players"];
	if (nplayers > 0)
		nplayers_ = nplayers;

	int island_size = cfg["island_size"];
	if (island_size > 0)
		island_size_ = island_size;
}

bool default_map_generator::allow_user_config() const { return true; }

bool default_map_generator::user_config(display& disp, const config& cfg, int max_players, int min_w, int max_w, int min_h, int max_h)
{
	int max = gamemap::MAX_PLAYERS;
	int factions = cfg.child_count("faction");
	if (factions && max > factions) {
		max = factions;
	}
	if (max_players != -1 && max > max_players) {
		max = max_players;
	}
	gui2::tmap_generator dlg(disp, *this, max, min_w, max_w, min_h, max_h);
	try {
		dlg.show(disp.video());
	} catch(twml_exception& e) {
		e.show(disp);
		return false;
	}
	if (dlg.get_retval() != gui2::twindow::OK || !dlg.changed()) {
		return false;
	}
	dlg.apply_change();

	return true;
}

std::string default_map_generator::name() const { return "default"; }

std::string default_map_generator::config_name() const
{
	if (const config &c = cfg_.child("scenario"))
		return c["name"];

	return std::string();
}

std::string default_map_generator::create_map(const std::vector<std::string>& args)
{
	return generate_map(args);
}

std::string default_map_generator::generate_map(const std::vector<std::string>& /*args*/, std::map<map_location,std::string>* labels)
{
	// the random generator thinks odd widths are nasty, so make them even
	if (is_odd(width_))
		++width_;

	size_t iterations = (iterations_*width_*height_)/(default_width_*default_height_);
	size_t island_size = 0;
	size_t island_off_center = 0;
	size_t max_lakes = max_lakes_;

	if(island_size_ >= max_coastal) {

		//islands look good with much fewer iterations than normal, and fewer lake
		iterations /= 10;
		max_lakes /= 9;

		//the radius of the island should be up to half the width of the map
		const size_t island_radius = 50 + ((max_island - island_size_)*50)/(max_island - max_coastal);
		island_size = (island_radius*(width_/2))/100;
	} else if(island_size_ > 0) {
		DBG_NG << "coastal...\n";
		//the radius of the island should be up to twice the width of the map
		const size_t island_radius = 40 + ((max_coastal - island_size_)*40)/max_coastal;
		island_size = (island_radius*width_*2)/100;
		island_off_center = std::min<size_t>(width_,height_);
		DBG_NG << "calculated coastal params...\n";
	}

	// A map generator can fail so try a few times to get a map before aborting.
	std::string map;
	// Keep a copy of labels as it can be written to by the map generator func
	std::map<map_location,std::string> labels_copy;
	std::string error_message;
	int tries = 10;
	do {
		if (labels) {
				labels_copy = *labels;
		}
		try{
			map = default_generate_map(width_, height_, island_size, island_off_center,
				iterations, hill_size_, max_lakes, (nvillages_ * width_ * height_) / 1000,
				castle_size_, nplayers_, link_castles_, &labels_copy, cfg_, economy_area_);
			error_message = "";
		}
		catch (mapgen_exception& exc){
			error_message = exc.message;
		}
		--tries;
	} while (tries && map.empty());
	if (labels) {
		labels->swap(labels_copy);
	}

	if (error_message != "")
		throw mapgen_exception(error_message);

	return map;
}

config default_map_generator::create_scenario(const std::vector<std::string>& args)
{
	DBG_NG << "creating scenario...\n";

	config res = cfg_.child_or_empty("scenario");

	DBG_NG << "got scenario data...\n";

	std::map<map_location,std::string> labels;
	DBG_NG << "generating map...\n";
	try{
		res["map_data"] = generate_map(args,&labels);
	}
	catch (mapgen_exception& exc){
		res["map_data"] = "";
		res["error_message"] = exc.message;
	}
	DBG_NG << "done generating map..\n";

	for(std::map<map_location,std::string>::const_iterator i =
			labels.begin(); i != labels.end(); ++i) {

		if(i->first.x >= 0 && i->first.y >= 0 &&
				i->first.x < static_cast<long>(width_) &&
				i->first.y < static_cast<long>(height_)) {

			config& label = res.add_child("label");
			label["text"] = i->second;
			i->first.write(label);
		}
	}

	res.merge_attributes(economy_area_);

	return res;
}
