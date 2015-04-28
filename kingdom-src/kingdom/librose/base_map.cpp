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
#include "controller_base.hpp"

#define index(x, y)  (w_ * (y) + (x))

base_map::base_map(controller_base& controller, const gamemap& gmap, bool consistent) 
	: controller_(controller)
	, gmap_(gmap)
	, w_(0)
	, h_(0)
	, map_(NULL)
	, map_vsize_(0)
	, coor_map_(NULL)
	, consistent_(consistent)
	, place_unsort_(false)
{}

base_map &base_map::operator=(const base_map &that)
{
	consistent_ = that.consistent_;
	create_coor_map(that.w_, that.h_);
	return *this;
}


base_map::~base_map()
{
	clear();
}

base_unit* base_map::find_base_unit(const map_location& loc, bool overlay) const
{
	if (loc.x < 0 || loc.x >= w_ || loc.y < 0 || loc.y >= h_) {
		return NULL;
	}
	return overlay? coor_map_[index(loc.x, loc.y)].overlay: coor_map_[index(loc.x, loc.y)].base;
}

base_unit* base_map::find_base_unit(const map_location& loc) const
{
	if (loc.x < 0 || loc.x >= w_ || loc.y < 0 || loc.y >= h_) {
		return NULL;
	}
	base_unit* u = coor_map_[index(loc.x, loc.y)].overlay;
	if (u) {
		return u;
	}
	return coor_map_[index(loc.x, loc.y)].base;
}

void base_map::create_coor_map(int w, int h)
{
	VALIDATE(!map_ && !map_vsize_ && !coor_map_, "Error state!");
	map_ = (base_unit**)malloc(w * h * sizeof(base_unit*));
	memset(map_, 0, w * h * sizeof(base_unit*));
	map_vsize_ = 0;

	coor_map_ = (loc_cookie*)malloc(w * h * sizeof(loc_cookie));
	memset(coor_map_, 0, w * h * sizeof(loc_cookie));

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
void base_map::sort_map(const base_unit& u2)
{
	base_unit* u = map_[u2.map_index_];

	int i;
	for (i = 0; i < map_vsize_; i ++) {
		const base_unit* that = map_[i];
		if (that == u) {
			continue;
		}

		if (u->sort_compare(*that)) {
			break;
		}
	}
	if (u->map_index_ == i - 1) {
		return;
	}

	if (i > u->map_index_) {
		// if (i == map_vsize_) {
			i --;
		// }
		for (int i2 = u->map_index_; i2 < i; i2 ++) {
			map_[i2] = map_[i2 + 1];
			map_[i2]->map_index_ = i2;

		}
	} else {
		for (int i2 = u->map_index_; i2 > i; i2 --) {
			map_[i2] = map_[i2 - 1];
			map_[i2]->map_index_ = i2;

		}
	}

	map_[i] = u;
	u->map_index_ = i;
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

	// some application maybe require coor_map_ valid before set_location.
	// but touch_locs is got after set_location. only valid on center location.
	if (base) {
		coor_map_[index(loc.x, loc.y)].base = u;
	} else {
		coor_map_[index(loc.x, loc.y)].overlay = u;
	}

	u->set_location(loc);

	// it is called after set_location directly, use touch_locs safely.
	const std::set<map_location>& touch_locs = u->get_touch_locations();
	if (touch_locs.size() != 1) {
		for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
			if (base) {
				coor_map_[index(itor->x, itor->y)].base = u;
			} else {
				coor_map_[index(itor->x, itor->y)].overlay = u;
			}
		}
	}

	// insert p into time-axis.*
	u->map_index_ = map_vsize_;
	map_[map_vsize_ ++] = u;
	if (u->require_sort()) {
		sort_map(*u);
	}
}

void base_map::expand_coor_map(int w)
{
	VALIDATE(w > w_, null_str);

	// map_ must point to size w_ * h_!
	base_unit** tmp = (base_unit**)malloc(w * h_ * sizeof(base_unit*));
	memset(tmp, 0, w * h_ * sizeof(base_unit*));
	if (map_vsize_) {
		memcpy(tmp, map_, map_vsize_ * sizeof(base_unit*));
	}
	map_ = tmp;

	loc_cookie* tmp2 = (loc_cookie*)malloc(w * h_ * sizeof(loc_cookie));
	memset(tmp2, 0, w * h_ * sizeof(loc_cookie));

	for (int y = 0; y < h_; y ++) {
		memcpy(tmp2 + (y * w), coor_map_ + (y * w_), w_ * sizeof(loc_cookie));
	}
	free(coor_map_);

	coor_map_ = tmp2;
	w_ = w;
}

map_location base_map::conflict_calculate_loc(const base_unit& u)
{
	const display& disp = controller_.get_display();
	const SDL_Rect& rect = u.get_rect();
	map_location loc = disp.pixel_position_to_hex(rect.x, rect.y);
	int pitch = w_ * loc.y;
	if (coor_map_[pitch + loc.x].overlay) {
		int w = gmap_.w();
		for (int x = w; x < w_; x ++) {
			if (!coor_map_[pitch + x].overlay) {
				loc.x = x;
				break;
			}
		}
		if (loc.x < w) {
			// requrie extend.
			const int expand_factor = 2;
			expand_coor_map(w_ + expand_factor);
			loc.x = w_ - expand_factor;
		}
	}
	return loc;
}

void base_map::insert2(const display& disp, base_unit* u)
{
	VALIDATE(!consistent_ && !u->consistent(), null_str);
	map_location loc = conflict_calculate_loc(*u);

	insert(loc, u);
}

base_unit* base_map::unit_clicked_on(const int xclick, const int yclick, const map_location& mloc) const
{
	const int w = gmap_.w();
	const display& disp = get_controller().get_display();
	int xmap = xclick, ymap = yclick;
	disp.pixel_screen_to_map(xmap, ymap);

	base_unit* u;
	for (int y = mloc.y; y >= 0; y --) {
		int pitch = y * w_;
		int max_x = 0;
		int min_y = INT_MAX;
		for (int x = mloc.x; x >= 0; x --) {
			u = coor_map_[pitch + x].overlay;
			if (u && !u->hidden_) {
				const SDL_Rect& rect = u->get_rect();
				if (point_in_rect(xmap, ymap, rect)) {
					return u;
				}
				if (max_x < rect.x + rect.w) {
					max_x = rect.x + rect.w;
				}
				if (min_y > rect.y) {
					min_y = rect.y;
				}
			}
			if (min_y <= ymap) {
				break;
			}
		}
		for (int x1 = w; x1 < w_; x1 ++) {
			u = coor_map_[pitch + x1].overlay;
			if (u && !u->hidden_) {
				const SDL_Rect& rect = u->get_rect();
				if (point_in_rect(xmap, ymap, rect)) {
					return u;
				}
				if (max_x < rect.x + rect.w) {
					max_x = rect.x + rect.w;
				}
			}
		}
		if (y != mloc.y && max_x >= xmap) {
			break;
		}
	}
	return NULL;
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
	if (map_) {
		free(map_);
		map_ = NULL;
	}
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

	if (!place_unsort_) {
		VALIDATE(u->get_map_index() != UNIT_NO_INDEX, null_str);
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

	if (!place_unsort_) {
		VALIDATE(u->get_map_index() != UNIT_NO_INDEX, null_str);
		if (u->require_sort()) {
			sort_map(*u);
		}
		verify_map_index();
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

void base_map::verify_map_index() const
{
	for (int i = 0; i < map_vsize_; i ++) {
		VALIDATE(map_[i]->map_index_ == i, null_str);
	}
}

bool base_map::erase2(base_unit* u, bool delete_unit)
{
	VALIDATE(u->map_index_ != UNIT_NO_INDEX, "unit must be in map_!");

	map_vsize_ --;
	for (int i = u->map_index_; i < map_vsize_; i ++) {
		map_[i] = map_[i + 1];
		map_[i]->map_index_ = i;
	}
	map_[map_vsize_] = NULL;

	bool base = u->base();
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
		disp->invalidate(u->get_draw_locations());
	}

	if (delete_unit) {
		delete u;
	} else {
		u->map_index_ = UNIT_NO_INDEX;
	}

	verify_map_index();

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

	if (consistent_) {
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
	} else {
		display& disp = controller_.get_display();
		int zoom = disp.hex_size();
		SDL_Rect rect = create_rect(draw_area_min_x * zoom, draw_area_min_y[0] * zoom,
			(draw_area_max_x - draw_area_min_x + 1) * zoom,
			(draw_area_max_y[0] - draw_area_min_y[0] + 1) * zoom);
		for (int i = 0; i < map_vsize_; i ++) {
			base_unit* u = map_[i];
			if (rects_overlap(u->get_rect(), rect)) {
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
