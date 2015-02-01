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

#include "unit_map.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "gui/dialogs/mkwin_theme.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "game_config.hpp"

unit_map::unit_map(mkwin_controller& controller, const gamemap& gmap)
	: base_map(gmap)
	, controller_(controller)
{
}

void unit_map::create_coor_map(int w, int h)
{
	size_t orignal_map_vsize = map_vsize_;
	base_unit** orignal_map = NULL;
	if (orignal_map_vsize) {
		VALIDATE(w * h >= (int)orignal_map_vsize, "desire map size must not less than orignal nodes!"); 
		orignal_map = (base_unit**)malloc(map_vsize_ * sizeof(base_unit*));
		memcpy(orignal_map, map_, map_vsize_ * sizeof(base_unit*));
	}
	base_map::create_coor_map(w, h);
	if (orignal_map_vsize) {
		map_vsize_ = orignal_map_vsize;
		memcpy(map_, orignal_map, map_vsize_ * sizeof(base_unit*));

		for (size_t i = 0; i < map_vsize_; i ++) {
			base_unit* n = map_[i];
			const map_location& loc = n->get_location();
			int pitch = loc.y * w;
			if (n->base()) {
				coor_map_[pitch + loc.x].base = n;
			} else {
				coor_map_[pitch + loc.x].overlay = n;
			}
		}
	}
}

void unit_map::move_mass(bool horizontal, bool extend, int start, int stop)
{
	map_location loc;
	int inc_factor = extend? 1: -1;
	if (horizontal) {
		for (int y = 0; y < h_; y ++) {
			int pitch = y * w_;
			for (int x = start; x <= stop; x ++) {
				base_unit* n = coor_map_[pitch + x].overlay;
				loc = map_location(x + inc_factor, y);
				n->set_location(loc);
			}
		}
	} else {
		for (int y = start; y <= stop; y ++) {
			int pitch = y * w_;
			for (int x = 0; x < w_; x ++) {
				base_unit* n = coor_map_[pitch + x].overlay;
				loc = map_location(x, y + inc_factor);
				n->set_location(loc);
			}
		}
	}
}

void unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const unit* u = dynamic_cast<const unit*>(base_u);
	insert(loc, new unit(*u));
}

unit* unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<unit*>(find_base_unit(loc));
}

unit* unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<unit*>(find_base_unit(loc, overlay));
}


void unit_map::save_map_to(unit::tchild& child)
{
	child.clear(false);

	child.window = dynamic_cast<unit*>(map_[0]);
	for (int x = 1; x < w_; x ++) {
		child.cols.push_back(dynamic_cast<unit*>(map_[x]));
	}
	for (int y = 1; y < h_; y ++) {
		int pitch = y * w_;
		for (int x = 0; x < w_; x ++) {
			if (x) {
				child.units.push_back(dynamic_cast<unit*>(map_[pitch + x]));
			} else {
				child.rows.push_back(dynamic_cast<unit*>(map_[pitch + x]));
			}
		}
	}

	// clear, except unit.
	if (coor_map_) {
		free(coor_map_);
		coor_map_ = NULL;
	}
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

void unit_map::restore_map_from(const unit::tchild& child)
{
	VALIDATE(!map_vsize_, "map_vsize_ must be 0.");

	insert(map_location(0, 0), child.window);
	for (int x = 1; x < w_; x ++) {
		insert(map_location(x, 0), child.cols[x - 1]);
	}
	int uindex = 0;
	for (int y = 1; y < h_; y ++) {
		int pitch = y * w_;
		for (int x = 0; x < w_; x ++) {
			if (x) {
				insert(map_location(x, y), child.units[uindex ++]);
			} else {
				insert(map_location(x, y), child.rows[y - 1]);
			}
		}
	}
}

void unit_map::move_line(bool horizontal, const int index)
{
	int threshold = horizontal? h_: w_;
	if (threshold <= 2) {
		return;
	}
	if (!index || index >= threshold) {
		return;
	}

	std::vector<base_unit*> caches;
	if (horizontal) {
		int pitch = index * w_;
		for (int x = 0; x < w_; x ++) {
			caches.push_back(coor_map_[pitch + x].overlay);
		}
		for (int y = index + h_ - 1; y > index; y --) {
			if (y == h_) {
				continue;
			}
			int from = y % h_;
			int to = (y + 1) % h_;
			if (!to) {
				to ++;
			}

			int from_pitch = from * w_;
			int to_pitch = to * w_;
			for (int x = 0; x < w_; x ++) {
				base_unit* from_u = coor_map_[from_pitch + x].overlay;
				from_u->set_location(map_location(x, to));
				coor_map_[to_pitch + x].overlay = from_u;

				sort_map2(*from_u);
			}
		}

		int to = (index + 1 != h_)? index + 1: 1;
		pitch = to * w_;
		for (int x = 0; x < w_; x ++) {
			base_unit* from_u = caches[x];
			from_u->set_location(map_location(x, to));
			coor_map_[pitch + x].overlay = from_u;

			sort_map2(*from_u);
		}
	} else {
		for (int y = 0; y < h_; y ++) {
			int pitch = y * w_;
			caches.push_back(coor_map_[pitch + index].overlay);
		}
		for (int x = index + w_ - 1; x > index; x --) {
			if (x == w_) {
				continue;
			}
			int from = x % w_;
			int to = (x + 1) % w_;
			if (!to) {
				to ++;
			}

			for (int y = 0; y < h_; y ++) {
				int pitch = y * w_;
				base_unit* from_u = coor_map_[pitch + from].overlay;
				from_u->set_location(map_location(to, y));
				coor_map_[pitch + to].overlay = from_u;

				sort_map2(*from_u);
			}
		}

		int to = (index + 1 != w_)? index + 1: 1;
		for (int y = 0; y < h_; y ++) {
			int pitch = y * w_;
			base_unit* from_u = caches[y];
			from_u->set_location(map_location(to, y));
			coor_map_[pitch + to].overlay = from_u;

			sort_map2(*from_u);
		}
	}
}

void unit_map::swap_line(bool horizontal, const int index)
{
	int threshold = horizontal? h_: w_;
	if (threshold <= 2) {
		return;
	}
	if (!index || index >= threshold) {
		return;
	}
	int next = index + 1;
	if (next == threshold) {
		next = 1;
	}

	std::vector<base_unit*> caches;
	if (horizontal) {
		for (int x = 0; x < w_; x ++) {
			int from_pitch = index * w_;
			int next_pitch = next * w_;
			
			base_unit* from = coor_map_[from_pitch + x].overlay;
			base_unit* to = coor_map_[next_pitch + x].overlay;

			from->set_location(map_location(x, next));
			sort_map2(*from);
			to->set_location(map_location(x, index));
			sort_map2(*to);

			coor_map_[from_pitch + x].overlay = to;
			coor_map_[next_pitch + x].overlay = from;
		}
	} else {
		for (int y = 0; y < h_; y ++) {
			int pitch = y * w_;
			
			base_unit* from = coor_map_[pitch + index].overlay;
			base_unit* to = coor_map_[pitch + next].overlay;

			from->set_location(map_location(next, y));
			sort_map2(*from);
			to->set_location(map_location(index, y));
			sort_map2(*to);

			coor_map_[pitch + index].overlay = to;
			coor_map_[pitch + next].overlay = from;
		}
	}
}

bool unit_map::line_is_spacer(bool row, int index) const
{
	if (row) {
		int pitch = index * w_;
		for (int x = 1; x < w_; x ++) {
			const unit* u = dynamic_cast<unit*>(coor_map_[pitch + x].overlay);
			if (!u->is_spacer()) {
				return false;
			}
		}
	} else {
		for (int y = 1; y < h_; y ++) {
			int pitch = y * w_;
			const unit* u = dynamic_cast<unit*>(coor_map_[pitch + index].overlay);
			if (!u->is_spacer()) {
				return false;
			}
		}
	}
	return true;
}
