/* $Id: base_map.cpp 37221 2009-07-24 21:48:38Z crab $ */
/*
   Copyright (C) 2006 - 2009 by Rusty Russell <rusty@rustcorp.com.au>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file base_map.cpp */

#include "base_map.hpp"
#include "map.hpp"
#include "display.hpp"
#include "wml_exception.hpp"

#define index(x, y)  (w_ * (y) + (x))

base_map::base_map(const gamemap& gmap) 
	: gmap_(gmap)
	, w_(0)
	, h_(0)
	, map_(NULL)
	, map_vsize_(0)
	, coor_map_(NULL)
{}

base_map &base_map::operator=(const base_map &that)
{
	create_coor_map(that.w_, that.h_);
	return *this;
}


base_map::~base_map()
{
	clear();
}

base_unit* base_map::find_base_unit(const map_location& loc, bool overlay) const
{
	if (!gmap_.on_board(loc)) return NULL;

	return overlay? coor_map_[index(loc.x, loc.y)].overlay: coor_map_[index(loc.x, loc.y)].base;
}

base_unit* base_map::find_base_unit(const map_location& loc) const
{
	if (!gmap_.on_board(loc)) return NULL;
	base_unit* u = coor_map_[index(loc.x, loc.y)].overlay;
	if (u) {
		return u;
	}
	return coor_map_[index(loc.x, loc.y)].base;
}

void base_map::create_coor_map(int w, int h)
{
	coor_map_ = (loc_cookie*)malloc(w * h * sizeof(loc_cookie));
	for (int i = 0; i < h; i ++) {
		int pitch = i * w;
		for (int j = 0; j < w; j ++) {
			coor_map_[pitch + j].base = NULL;
			coor_map_[pitch + j].overlay = NULL;
		}
	}

	map_ = (base_unit**)malloc(w * h * sizeof(base_unit*));
	map_vsize_ = 0;

	// remember this size
	w_ = w;
	h_ = h;
}

base_map::iterator base_map::begin() 
{
	// This call just needs to go somewhere that is likely to be
	size_t i = 0;
	return iterator(i, this);
}

base_map::const_iterator base_map::begin() const 
{
	size_t i = 0;
	return const_iterator(i, this);
}

void base_map::move(const map_location src, const map_location& dst) 
{
	base_unit* u = extract(src);
	place(dst, u);
}

// pos: this unit result to should resort, it is valid.
void base_map::sort_map(size_t pos)
{
	base_unit* u = map_[pos];
	const map_location& u_loc = u->get_location();
	if (pos < map_vsize_ - 1) {
		memcpy(&(map_[pos]), &(map_[pos + 1]), (map_vsize_ - pos - 1) * sizeof(base_unit*));
	}

	size_t i;
	for (i = 0; i < map_vsize_ - 1; i ++) {
		const base_unit* that = map_[i];
		const map_location& that_loc = that->get_location();
		// must include non-ticks unit.
		if (u->sort_compare(*that)) {
			break;
		}
	}
	if (i < map_vsize_ - 1) {
		memmove(&(map_[i + 1]), &(map_[i]), (map_vsize_ - i - 1) * sizeof(base_unit*));
		map_[i] = u;

	} else {
		map_[map_vsize_ - 1] = u;
	}
}

void base_map::sort_map2(const base_unit& u)
{
	size_t i;
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i] == &u) {
			sort_map(i);
			return;
		}
	}
	VALIDATE(i < map_vsize_, "sort_map2, cann't find u that result to sort!");
}

void base_map::insert(const map_location loc, base_unit* u)
{
	std::stringstream err;
	if (!loc.valid()) {
		err << "Trying to add " << u->name() << " at an invalid location; Discarding.";
		delete u;
		VALIDATE(false, err.str());
	}
	bool base = u->base();

	if ((base && coor_map_[index(loc.x, loc.y)].base) ||
		(!base && coor_map_[index(loc.x, loc.y)].overlay)) {
		err << "trying to overwrite existing unit at (" << loc.x << ", " << loc.y << ")";
		delete u;
		VALIDATE(false, err.str());
	}

	u->set_location(loc);

	// it is called after set_location directly, use touch_locs safely.
	const std::set<map_location>& touch_locs = u->get_touch_locations();
	for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
		if (base) {
			coor_map_[index(itor->x, itor->y)].base = u;
		} else {
			coor_map_[index(itor->x, itor->y)].overlay = u;
		}
	}

	// insert p into time-axis.*
	map_[map_vsize_ ++] = u;
	if (u->require_sort()) {
		sort_map(map_vsize_ - 1);
	}
}

bool base_map::valid2(const map_location& loc, bool overlay) const
{
	if (!gmap_.on_board(loc)) {
		return false;
	}
	if (overlay) {
		return coor_map_[index(loc.x, loc.y)].overlay? true: false;
	} else {
		return coor_map_[index(loc.x, loc.y)].base? true: false;
	}
}

void base_map::replace(const map_location &l, const base_unit* u)
{
	//when 'l' is the reference to map_location that is part
	//of that unit iterator which is to be deleted by erase,
	// 'l' is invalidated by erase, too. Thus, 'add(l,u)' fails.
	// So, we need to make a copy of that map_location.
	const map_location loc = l;

	base_unit* replacing = u->base()? coor_map_[index(l.x, l.y)].base: coor_map_[index(l.x, l.y)].overlay;
	erase2(replacing);
	add(loc, u);
}

void base_map::clear()
{
	if (coor_map_) {
		free(coor_map_);
		coor_map_ = NULL;
	}
	for (size_t i = 0; i != map_vsize_; ++i) {
		delete map_[i];
	}
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

// extract/place pair only use in overlay layer.
base_unit* base_map::extract(const map_location &loc)
{
	if (!loc.valid()) {
		return NULL;
	}

	base_unit* u = coor_map_[index(loc.x, loc.y)].overlay;
	if (!u) {
		return NULL;
	}

	const std::set<map_location>& touch_locs = u->get_touch_locations();
	for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
		coor_map_[index(itor->x, itor->y)].overlay = NULL;
	}
	return u;
}

void base_map::place(const map_location loc, base_unit* u)
{
	u->set_location(loc);
	// update cookies on map
	const std::set<map_location>& touch_locs = u->get_touch_locations();
	for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
		coor_map_[index(itor->x, itor->y)].overlay = u;
	}
}

// Notice: caller must make sure loc of parameter is centor location of desired erase unit!
bool base_map::erase(const map_location& loc, bool overlay)
{
	if (!loc.valid()) {
		return false;
	}
	base_unit* u = overlay? coor_map_[index(loc.x, loc.y)].overlay: coor_map_[index(loc.x, loc.y)].base;
	if (!u) {
		return false;
	}
	return erase2(u);
}

bool base_map::erase2(base_unit* u, bool delete_unit)
{
	size_t i;
	bool base = u->base();
	const map_location& loc = u->get_location();
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i] == u) {
			break;
		}
	}

	VALIDATE(i != map_vsize_, "unitmap::erase, i == map_vsize, check code!");

	if (i < map_vsize_ - 1) {
		memcpy(&(map_[i]), &(map_[i + 1]), (map_vsize_ - i - 1) * sizeof(base_unit*));
	}
	map_vsize_ --;

	std::set<map_location> invalid_locs;
	const std::set<map_location>& touch_locs = u->get_touch_locations();
	for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
		const map_location& loc = *itor;
		if (base) {
			coor_map_[index(loc.x, loc.y)].base = NULL;
		} else {
			coor_map_[index(loc.x, loc.y)].overlay = NULL;
		}
		invalid_locs.insert(loc);
	}

	display* disp = display::get_singleton();
	if (disp) {
		disp->invalidate(invalid_locs);
	}

	if (delete_unit) {
		delete u;
	}

	return true;
}

// @draw_area_unit[OUT]: units_from_rect fill valid units to it.
// return value
//  size of filled units
// remark:
//  1)unit overlapped multi-grid, return center loc only
size_t base_map::units_from_rect(base_unit** draw_area_unit, const rect_of_hexes& draw_area_rect)
{
	size_t draw_area_unit_size = 0;

	int draw_area_min_x = std::max(0, draw_area_rect.left);
	int draw_area_max_x = std::min(gmap_.w() - 1, draw_area_rect.right);
	int draw_area_min_y[2], draw_area_max_y[2];
	draw_area_min_y[0] = std::max(0, draw_area_rect.top[0]);
	draw_area_min_y[1] = std::max(0, draw_area_rect.top[1]);
	draw_area_max_y[0] = std::min(gmap_.h() - 1, draw_area_rect.bottom[0]);
	draw_area_max_y[1] = std::min(gmap_.h() - 1, draw_area_rect.bottom[1]);

	std::set<const base_unit*> calculated;
	for (int x = draw_area_min_x; x <= draw_area_max_x; x ++) {
		for (int y = draw_area_min_y[x & 1]; y <= draw_area_max_y[x & 1]; y ++) {
			base_unit* u = coor_map_[index(x, y)].base;
			if (u && !calculated.count(u)) {
				calculated.insert(u);
				draw_area_unit[draw_area_unit_size ++] = u;
			}
			u = coor_map_[index(x, y)].overlay;
			if (u && !calculated.count(u)) {
				calculated.insert(u);
				draw_area_unit[draw_area_unit_size ++] = u;
			}
			
		}
	}

	return draw_area_unit_size;
}

const map_location& base_map::center_loc(const map_location& loc) const
{
	if (!gmap_.on_board(loc)) {
		return map_location::null_location;
	}
	base_unit* param = coor_map_[index(loc.x, loc.y)].overlay;
	if (!param) {
		return map_location::null_location;
	}
	return param->get_location();
}
