/* $Id: unit_map.cpp 37221 2009-07-24 21:48:38Z crab $ */
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

/** @file unit_map.cpp */

#include "unit.hpp"
#include "log.hpp"
#include "map.hpp"
#include "resources.hpp"
#include "game_preferences.hpp"
#include "team.hpp"
#include "game_display.hpp"
#include "pathutils.hpp"
#include "play_controller.hpp"
#include "artifical.hpp"
#include "wml_exception.hpp"

#include <functional>

#include "actions.hpp"

//
// city_map section
//
city_map::iterator city_map_iter_invalid = city_map::iterator(CITYS_INVALID_NUMBER, NULL);
city_map::const_iterator city_map_const_iter_invalid = city_map::const_iterator(CITYS_INVALID_NUMBER, NULL);

bool city_map::affect_team_ = true;

ai_plan::ai_plan() :
	mrs_()
{
}

void ai_plan::reset()
{
	mrs_.clear();
}


city_map::city_map() :
	map_(NULL),
	map_vsize_(0)
{
}

city_map::~city_map()
{
	clear_map();
}

void city_map::realloc_map(const size_t size)
{
	if (map_) {
		clear_map();
	}
	map_ = (artifical**)malloc(size * sizeof(artifical*));
	memset(map_, 0, size * sizeof(artifical*));
	map_vsize_ = size;
}

void city_map::clear_map()
{
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

city_map::iterator city_map::begin() 
{
	size_t i = 0;
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i]) {
			break;
		}
	}
	if (i != map_vsize_) {
		return iterator(i, this);
	}
	return city_map_iter_invalid;	
}

city_map::const_iterator city_map::begin() const 
{
	size_t i = 0;
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i]) {
			break;
		}
	}
	if (i != map_vsize_) {
		return const_iterator(i, this);
	}
	return city_map_const_iter_invalid;
}

city_map::iterator city_map::end()
{ 
	return city_map_iter_invalid; 
}

city_map::const_iterator city_map::end() const 
{ 
	return city_map_const_iter_invalid; 
}

void city_map::add(artifical* city)
{
	if (map_[city->cityno()]) {
		return;
	}
	// @city isn't in current city_map, add it
	map_[city->cityno()] = city;

	if (affect_team_) {
		team& holded_team = (*resources::teams)[city->side() - 1];
		holded_team.add_city(city);
	}
}

// @city: must be valid
void city_map::erase(const artifical* city)
{
	if (!map_[city->cityno()]) {
		return;
	}
	// @city is in current city_map, erase it
	map_[city->cityno()] = NULL;

	team& holded_team = (*resources::teams)[city->side() - 1];
	holded_team.erase_city(city);
}

artifical* city_map::city_from_cityno(int cityno)
{
	return map_[cityno];
}

//
// unit_map section
//
surface unit_map::desc_bg_[10] = {};
surface unit_map::desc_hot = surface();
surface unit_map::enemy_orb_ = surface();
surface unit_map::ally_orb_ = surface();
surface unit_map::moved_orb_ = surface();
surface unit_map::unmoved_orb_ = surface();
surface unit_map::partmoved_orb_ = surface();
surface unit_map::automatic_orb_ = surface();
surface unit_map::self_orb_ = surface();

surface unit_map::normal_food = surface();
surface unit_map::lack_food = surface();
surface unit_map::robber = surface();

std::string unit_map::bar_vtl_png;
std::string unit_map::bar_vtl_hot_png;

unit* unit_map::scout_unit_ = NULL;
std::map<std::pair<int, int>, size_t> unit_map::inter_city_move_cost_ = std::map<std::pair<int, int>, size_t>();

int unit_map::main_ticks = 0;
int unit_map::top_side = 0;

std::map<const map_location, int> unit_map::economy_areas_;

void unit_map::set_zoom()
{
	std::stringstream str;
	for (int i = 0; i < 10; i ++) {
		str.str("");
		str << "misc/unit-desc-" << i << ".png";
		desc_bg_[i] = image::get_image(str.str(), image::SCALED_TO_ZOOM);
		desc_bg_[i].assign(adjust_surface_alpha(desc_bg_[i], ftofxp(0.7), false));
		SDL_SetSurfaceRLE(desc_bg_[i].get(), 0);
	}
	desc_hot = image::get_image("misc/unit-desc-hot.png", image::SCALED_TO_ZOOM);
	enemy_orb_ = image::get_image("misc/orb-enemy.png", image::SCALED_TO_ZOOM);
	ally_orb_ = image::get_image("misc/orb-ally.png", image::SCALED_TO_ZOOM);
	moved_orb_ = image::get_image("misc/orb-moved.png", image::SCALED_TO_ZOOM);
	unmoved_orb_ = image::get_image("misc/orb-unmoved.png", image::SCALED_TO_ZOOM);
	partmoved_orb_ = image::get_image("misc/orb-partmoved.png", image::SCALED_TO_ZOOM);
	automatic_orb_ = image::get_image("misc/orb-auto.png", image::SCALED_TO_ZOOM);
	self_orb_ = image::get_image("misc/orb-self.png", image::SCALED_TO_ZOOM);

	normal_food = image::get_image("misc/food-status-normal.png", image::SCALED_TO_ZOOM);
	lack_food = image::get_image("misc/food-status-lack.png", image::SCALED_TO_ZOOM);
	robber = image::get_image("misc/robber.png", image::SCALED_TO_ZOOM);

	if (display::default_zoom_ == display::ZOOM_48) {
		bar_vtl_png = "misc/bar-vtl-72.png~CROP(0, 0, 48, 48)";
		bar_vtl_hot_png = "misc/bar-vtl-72-hot.png~CROP(0, 0, 48, 48)";
	} else if (display::default_zoom_ == display::ZOOM_56) {
		bar_vtl_png = "misc/bar-vtl-72.png~CROP(0, 0, 56, 56)";
		bar_vtl_hot_png = "misc/bar-vtl-72-hot.png~CROP(0, 0, 56, 56)";
	} else if (display::default_zoom_ == display::ZOOM_64) {
		bar_vtl_png = "misc/bar-vtl-72.png~CROP(0, 0, 64, 64)";
		bar_vtl_hot_png = "misc/bar-vtl-72-hot.png~CROP(0, 0, 64, 64)";
	} else {
		bar_vtl_png = "misc/bar-vtl-72.png";
		bar_vtl_hot_png = "misc/bar-vtl-72-hot.png";
	}
}

#define index(x, y)  (w_ * (y) + (x))

unit_map::unit_map() : 
	w_(0),
	h_(0),
	map_(NULL), 
	map_vsize_(0),
	coor_map_(NULL), 
	citys_(),
	expediting_(false)
{
	// set_zoom();

	inter_city_move_cost_.clear();
}

unit_map &unit_map::operator=(const unit_map &that)
{
	create_coor_map(that.w_, that.h_);

	expediting_ = that.expediting_;

	for (size_t i = 0; i < that.citys_.map_vsize_; i ++) {
		if (that.citys_.map_[i]) {
			add(that.citys_.map_[i]->get_location(), that.citys_.map_[i]);
			citys_.map_[i]->field_troops().clear();
			citys_.map_[i]->field_arts().clear();
		}
	}
	for (const_iterator i = that.begin(); i != that.end(); i++) {
		if (!i->is_artifical()) {
			add(i->get_location(), &*i);
		}
	}

	return *this;
}


unit_map::~unit_map()
{
	delete_all();
	if (scout_unit_) {
		delete scout_unit_;
		scout_unit_ = NULL;
	}
	economy_areas_.clear();

	unit::draw_desc_ = true;
	unit::ignore_pack = false;
}

void unit_map::extract_heros_number()
{
	for (size_t i = 0; i < map_vsize_; i ++) {
		map_[i]->second->extract_heros_number();
	}	
}

void unit_map::recalculate_heros_pointer()
{
	for (size_t i = 0; i < map_vsize_; i ++) {
		map_[i]->second->recalculate_heros_pointer();
	}	
}

unit* unit_map::find_unit(const hero& h) const
{
	if (!hero::data_variable(h.number_)) {
		// many unit use same master-hero shared. if it, nul.
		return NULL;
	}

	const artifical* city = city_from_cityno(h.city_);
	if (!city) {
		if (h.status_ != hero_status_military) {
			// unstage hero
			return NULL;
		} else {
			std::vector<team>& teams = *resources::teams;
			team& t = teams[h.side_];
			const std::pair<unit**, size_t> p = t.field_troop();
			unit** troops = p.first;
			size_t troops_vsize = p.second;
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit& u = *troops[i];
				if (u.master().number_ == h.number_) {
					return &u;
				}
				if (u.second().valid() && u.second().number_ == h.number_) {
					return &u;
				} 
				if (u.third().valid() && u.third().number_ == h.number_) {
					return &u;
				}
			}
		}
		return NULL;
	}
	if (city->master().number_ == h.number_) {
		return (unit*)city;
	}

	if (h.status_ == hero_status_military) {
		// in troop
		const std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::const_iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
			unit& u = **itor;
			if (u.master().number_ == h.number_) {
				return &u;
			}
			if (u.second().valid() && u.second().number_ == h.number_) {
				return &u;
			} 
			if (u.third().valid() && u.third().number_ == h.number_) {
				return &u;
			}
		}
		const std::vector<unit*>& field_troops = city->field_troops();
		for (std::vector<unit*>::const_iterator itor = field_troops.begin(); itor != field_troops.end(); ++ itor) {
			unit& u = **itor;
			if (u.master().number_ == h.number_) {
				return &u;
			}
			if (u.second().valid() && u.second().number_ == h.number_) {
				return &u;
			} 
			if (u.third().valid() && u.third().number_ == h.number_) {
				return &u;
			}
		}
	}
	return (unit*)city;
}

unit* unit_map::find_unit(const map_location& loc) const
{
	if (!resources::game_map->on_board(loc)) return NULL;
	unit_map::node* node = coor_map_[index(loc.x, loc.y)].overlay;
	if (node) {
		return node->second;
	}
	node = coor_map_[index(loc.x, loc.y)].base;
	if (node) {
		return node->second;
	}
	return NULL;
}

void unit_map::create_coor_map(int w, int h)
{
	coor_map_ = (loc_cookie*)malloc(w * h * sizeof(loc_cookie));
	for (int i = 0; i < h; i ++) {
		int pitch = i * w;
		for (int j = 0; j < w; j ++) {
			coor_map_[pitch + j].base = NULL;
			coor_map_[pitch + j].overlay = NULL;
		}
	}

	map_ = (node**)malloc(w * h * sizeof(node*));
	map_vsize_ = 0;

	citys_.realloc_map(w * h);

	// remember this size
	w_ = w;
	h_ = h;
}

void unit_map::set_expediting(artifical* city, bool troop, int index)
{
	size_t i;
	node *ptr;
	
	if (!city) {
		if (expediting_city_) {
			// if necessary, do clean
			if (expediting_node_) {
				ptr = coor_map_[index(expediting_node_->first.x, expediting_node_->first.y)].overlay;
				// swap back
				coor_map_[index(expediting_node_->first.x, expediting_node_->first.y)].overlay = expediting_node_;
				
				delete ptr;
				expediting_node_ = NULL;
			}
		}
		// normal state
		expediting_city_ = NULL;
		expediting_ = false;
		return;
	} else if (expediting_) {
		posix_print_mb("unit_map::set_expediting, reenter set recalling to true");
		return;
	}

	const map_location& loc = city->get_location();

	// 1.construct pair
	if (troop) {
		expediting_node_ = new std::pair<map_location, unit*>(loc, city->reside_troops()[index]);
	} else {
		expediting_node_ = new std::pair<map_location, unit*>(loc, city->reside_commoners()[index]);
	}

	// 2.
	ptr = coor_map_[index(loc.x, loc.y)].overlay;
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i] == ptr) {
			break;
		}
	}
	if (i == map_vsize_) {
		return;
	}
	coor_map_[index(loc.x, loc.y)].overlay = expediting_node_;
	// swap
	expediting_node_ = ptr;

	// enter into expedit state.
	expediting_ = true;
	expediting_city_ = city;
	expediting_troop_ = troop;
	expediting_index_ = index;

	return;
}

bool unit_map::expediting(void) const
{
	return expediting_;
}

unit_map::node* unit_map::expediting_city_node(void) const
{
	return expediting_node_;
}

bool unit_map::last_expedite_troop(void) const
{
	return expediting_troop_;
}

int unit_map::last_expedite_index(void) const
{
	return expediting_index_;
}

unit_map::node* unit_map::get_cookie(const map_location& loc, bool overlay) const
{
	gamemap *game_map = resources::game_map;

	if (!game_map->on_board(loc)) {
		return NULL;
	}
	return overlay? coor_map_[index(loc.x, loc.y)].overlay: coor_map_[index(loc.x, loc.y)].base;
}

unit_map::iterator unit_map::find(const map_location &loc, bool overlay) 
{
	gamemap *game_map = resources::game_map;

	if (!game_map->on_board(loc)) {
		return iterator(map_vsize_, this);
	}
	if (overlay) {
		return iterator(coor_map_[index(loc.x, loc.y)].overlay, this);
	} else {
		return iterator(coor_map_[index(loc.x, loc.y)].base, this);
	}
}
unit_map::iterator unit_map::begin() 
{
	// This call just needs to go somewhere that is likely to be
	size_t i = 0;
	return iterator(i, this);
}

unit_map::const_iterator unit_map::begin() const 
{
	size_t i = 0;
	return const_iterator(i, this);
}

void unit_map::add(const map_location&l, const unit* u)
{
	if (u->is_artifical()) {
		insert(new std::pair<map_location, unit*>(l, new artifical(*(dynamic_cast<const artifical*>(u)))));
	} else {
		insert(new std::pair<map_location, unit*>(l, new unit(*u)));
	}
}

bool unit_map::move(const map_location &src, const map_location &dst) 
{
	bool can_undo = true;
	std::vector<team>& teams = *resources::teams;
	size_t i;
	std::pair<map_location, unit*> *p = NULL;
	artifical* cobj = city_from_loc(dst);
		
	p = extract(src);
	bool troop = !p->second->is_commoner();

	// !!!src is reference. it value may be from p->first
	p->first = dst;

	if (!cobj) {
		// destination: non-city
		if (expediting_) {
			// system must confirm: destination isn't city when expediting.
			insert(p);

			unit& u = *(p->second);
			artifical& city = *city_from_cityno(u.cityno());

			// modify troop/commoner reside/field list
			if (troop) {
				city.troop_go_out(u, false);
				u.set_food(u.max_food());
			} else {
				city.commoner_go_out(u, false);
			}

			place(expediting_node_); // expediting_node_ indicate expediting city.
			expediting_node_ = NULL;

			expediting_city_ = NULL;
			expediting_ = false;

			if (resources::screen) {
				game_display& disp = *resources::screen;
				disp.remove_expedite_city();
			}
		} else {
			place(p);
		}

		if (resources::controller->allow_intervene()) {
			// if who moved, remove from it from slot-u or slot-special.
			p->second->remove_from_slot_cache();
		}
		
	} else {
		// destination: city
		for (i = 0; i < map_vsize_; i ++) {
			if (map_[i] == p) {
				break;
			}
		}
		VALIDATE(i != map_vsize_, "unitmap::move, i == map_vsize, check code!");

		if (i < map_vsize_ - 1) {
			memcpy(&(map_[i]), &(map_[i + 1]), (map_vsize_ - i - 1) * sizeof(node*));
		}
		map_vsize_ --;

		if (!expediting_) {
			// troop come into city.
			if (citys_.map_[p->second->cityno()]) {
				if (troop) {
					citys_.map_[p->second->cityno()]->field_troops_erase(p->second);
				} else {
					citys_.map_[p->second->cityno()]->field_commoners_erase(p->second);
				}
			}
			teams[p->second->side() - 1].erase_troop(p->second);
		}

		// below statement will change cityno, place here
		cobj->troop_come_into(p->second);

		// once enter into city, don't allow undo!
		can_undo = false;

		// troop_come_into reuse memory of unit, so don't call delete p->second
		delete p;
	}
	return can_undo;
}

// pos: this unit result to should resort, it is valid.
void unit_map::resort_map(size_t pos)
{
	std::pair<map_location, unit*>* p = map_[pos];
	const unit* u = p->second;
	if (pos < map_vsize_ - 1) {
		memcpy(&(map_[pos]), &(map_[pos + 1]), (map_vsize_ - pos - 1) * sizeof(node*));
	}
	VALIDATE(u->consider_ticks(), "desired resort_map must be ticks's unit!");

	size_t i;
	for (i = 0; i < map_vsize_ - 1; i ++) {
		const unit* that = map_[i]->second;
		// must include non-ticks unit.
		if (!that->consider_ticks() || u->compare_action_order(*that)) {
			break;
		}
	}
	if (i < map_vsize_ - 1) {
		memmove(&(map_[i + 1]), &(map_[i]), (map_vsize_ - i - 1) * sizeof(node*));
		map_[i] = p;

	} else {
		map_[map_vsize_ - 1] = p;
	}
}

void unit_map::resort_map(const unit& u)
{
	size_t i;
	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i]->second == &u) {
			resort_map(i);
			return;
		}
	}
	VALIDATE(i < map_vsize_, "resort_map, cann't find u that result to resort!");
}

void unit_map::multi_resort_map(game_display* disp, const std::vector<unit*>& v, bool full)
{
	if (full) {
		for (std::vector<unit*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
			unit& u = **it;
			u.ticks_adjusting = true;
		}
	}

	for (std::vector<unit*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		unit& u = **it;
		u.ticks_adjusting = false;
		if (disp) {
			disp->resort_access_troops(u);
		} else {
			resort_map(u);
		}
	}
}

unit& unit_map::current_unit()
{
	size_t i;
	for (i = 0; i < map_vsize_; i ++) {
		unit& u = *(map_[i]->second);
		if (!u.consider_ticks()) {
			continue;
		}
		break;
	}
	return *(map_[i]->second);
}

void unit_map::do_escape_ticks_uh(const std::vector<team>& teams, game_display& disp, int escape, bool first_zero)
{
	if (escape == 0) {
		return;
	}

	unit_map::main_ticks += escape;

	int adjusted, side;
	std::vector<unit*> decreases;
	// 1th sort: normal or increase unit. these cannot goto behind i.
	const unit* last = NULL;
	for (size_t i = 0; i < map_vsize_; i ++) {
		unit& u = *(map_[i]->second);

		if (!u.consider_ticks()) {
			break;
		}
		if (&u == last) {
			// reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=22045&extra=page%3D1
			continue;
		}
		last = &u;
		side = u.side();
		adjusted = u.forward_ticks(escape);
		u.increase_ticks(-1 * adjusted, 0, false);
		// below refresh_access_troops will update "all" drawn_ticks, will update main_ticks early. 
		if (u.drawn_ticks() != HIDDEN_TICKS) {
			u.set_drawn_ticks(NONE_TICKS);
		}

		if (adjusted < escape || (!u.ticks() && !u.is_city())) {
			// why require !u.ticks(),  reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=22045&extra=page%3D1, 2
			decreases.push_back(&u);
			u.ticks_adjusting = true;

		} else if (adjusted > escape) {
			disp.resort_access_troops(u, i);

		}
	}

	// 2th sort: decreased unit
	multi_resort_map(&disp, decreases, false);

	disp.refresh_access_troops(-1, game_display::REFRESH_DRAW);
}

void unit_map::do_escape_ticks_bh(const std::vector<team>& teams, game_display& disp, int player_number)
{
	size_t i = 0;
	std::vector<unit*> v;

	if (!tent::turn_based) {
		if (actor_can_action(*this)) {
			VALIDATE(!unit::actor->ticks(), "unit::actor's ticks must be 0!");

			if (unit::actor->get_state(ustate_tag::EXPEDITED)) {
				// in siege mode, there is pre-layout troop.
				unit::actor->set_state(ustate_tag::EXPEDITED, false);
			}
			unit::actor->set_ticks(unit::actor->max_ticks());
			disp.resort_access_troops(*unit::actor);

			// skip all non-capital city
			if (unit::actor->is_city()) {
				for (i = 0; i < map_vsize_; i ++) {
					unit& u = *(map_[i]->second);
					if (u.is_city() && (!u.is_capital(teams) || u.side() == player_number)) {
						VALIDATE(!u.ticks(), "non-capital's ticks must be 0!");
						v.push_back(&u);
						u.set_ticks(u.max_ticks());
						continue;
					}
					break;
				}
			}
		}

		for (; i < map_vsize_; i ++) {
			unit& u = *(map_[i]->second);
			if (u.get_state(ustate_tag::EXPEDITED)) {
				VALIDATE(!u.ticks(), "expedited unit's ticks must be 0!");
				v.push_back(&u);
				u.set_state(ustate_tag::EXPEDITED, false);
				u.set_ticks(u.max_ticks());
				continue;
			}
			if (u.ticks()) {
				break;
			}
		}

	} else {
		for (i = 0; i < map_vsize_; i ++) {
			unit& u = *(map_[i]->second);
			if (u.side() != top_side) {
				break;
			}
			v.push_back(&u);
		}
		top_side = (top_side % teams.size()) + 1;
	}

	multi_resort_map(&disp, v, true);
}

bool unit_map::insert(std::pair<map_location, unit*>* p)
{
	if (!p->first.valid()) {
		posix_print_mb("Trying to add %s at an invalid location; Discarding.\n", p->second->name().c_str());
		delete p->second;
		delete p;
		return false;
	}
	unit* u = p->second;
	bool base = u->base();

	if ((base && coor_map_[index(p->first.x, p->first.y)].base) ||
		(!base && coor_map_[index(p->first.x, p->first.y)].overlay)) {
		posix_print_mb("unit_map::insert, trying to overwrite existing unit at (%i, %i)", p->first.x, p->first.y);
		delete u;
		delete p;
		return false;
	}

	if (unit_is_city(u)) {
		// to city, in order to city_from_loc valid, first set cookie valid!
		// if city has reside troop, set_location of reside troop need valid city cookie infromation.
		// of course this equal is duplicate and below touch_locs equals.
		coor_map_[index(p->first.x, p->first.y)].overlay = p;
	}

	u->set_location(p->first);

	// it is called after set_location directly, use touch_locs safely.
	const std::set<map_location>& touch_locs = u->get_touch_locations();
	for (std::set<map_location>::const_iterator itor = touch_locs.begin(); itor != touch_locs.end(); ++ itor) {
		if (base) {
			coor_map_[index(itor->x, itor->y)].base = p;
		} else {
			coor_map_[index(itor->x, itor->y)].overlay = p;
		}
	}

	// insert p into time-axis.*
	map_[map_vsize_ ++] = p;
	if (u->consider_ticks()) {
		resort_map(map_vsize_ - 1);
	}

	game_display* disp = resources::screen;

	if (unit_is_city(u)) {
		citys_.add(unit_2_artifical(u));
	} else if (!u->is_artifical()) {
		if (citys_.map_[u->cityno()]) {
			if (!u->is_commoner()) {
				citys_.map_[u->cityno()]->field_troops_add(u);
			} else {
				citys_.map_[u->cityno()]->field_commoners_add(u);
			}
		} else {
			if (disp) {
				disp->refresh_access_troops(u->side() - 1, game_display::REFRESH_INSERT, u);
			}
		}

	} else if (u->is_artifical() && citys_.map_[u->cityno()]) {
		citys_.map_[u->cityno()]->field_arts_add(unit_2_artifical(u));
	}
	if (city_map::affect_team_ && !unit_is_city(u)) {
		(*resources::teams)[u->side() - 1].add_troop(u);
	}

	if (disp && u->terrain() != t_translation::NONE_TERRAIN) {
		game_display& gui = *resources::screen;
		gui.set_terrain_dirty();

		const map_location& loc = u->get_location();
		if (point_in_rect_of_hexes(loc.x, loc.y, gui.draw_area())) {
			map_location locs[6];
			get_adjacent_tiles(loc, locs);
			for (size_t i = 0; i < 6; i ++) {
				gui.invalidate(locs[i]);
			}
		}
	}

	return true;
}

bool unit_map::valid(const map_location& loc, bool check_loc, bool overlay) const
{
	if (check_loc) {
		gamemap *game_map = resources::game_map;
		if (!game_map->on_board(loc)) {
			return false;
		}
	}
	if (overlay) {
		return coor_map_[index(loc.x, loc.y)].overlay? true: false;
	} else {
		return coor_map_[index(loc.x, loc.y)].base? true: false;
	}
}

size_t unit_map::count(const map_location& loc, bool overlay) const
{ 
	gamemap *game_map = resources::game_map;
	if (!game_map->on_board(loc)) {
		return 0;
	}
	if (overlay) {
		return coor_map_[index(loc.x, loc.y)].overlay? 1: 0; 
	} else {
		return coor_map_[index(loc.x, loc.y)].base? 1: 0; 
	}
}

void unit_map::replace(const map_location &l, const unit* u)
{
	//when 'l' is the reference to map_location that is part
	//of that unit iterator which is to be deleted by erase,
	// 'l' is invalidated by erase, too. Thus, 'add(l,u)' fails.
	// So, we need to make a copy of that map_location.
	const map_location loc = l;

	unit* replacing = u->base()? coor_map_[index(l.x, l.y)].base->second: coor_map_[index(l.x, l.y)].overlay->second;
	erase(replacing);
	add(loc, u);
}

void unit_map::delete_all()
{
	if (coor_map_) {
		free(coor_map_);
		coor_map_ = NULL;
	}
	for (size_t i = 0; i != map_vsize_; ++i) {
		delete map_[i]->second;
		delete(map_[i]);
	}
	free(map_);
	map_ = NULL;
	map_vsize_ = 0;
}

// extract/place pair only use in overlay layer.
std::pair<map_location,unit*>* unit_map::extract(const map_location &loc)
{
	node* ptr;

	if (!loc.valid()) {
		return NULL;
	}

	ptr = coor_map_[index(loc.x, loc.y)].overlay;
	if (!ptr) {
		return NULL;
	}
	coor_map_[index(loc.x, loc.y)].overlay = NULL;

	const std::set<map_location::DIRECTION>& touch_dirs = ptr->second->get_touch_dirs();
	if (!touch_dirs.empty()) {
		const map_location* center = &loc;
		if (loc != ptr->second->get_location()) {
			center = &(ptr->second->get_location());
		}
		for (std::set<map_location::DIRECTION>::const_iterator itor = touch_dirs.begin(); itor != touch_dirs.end(); ++ itor) {
			map_location offset = center->get_direction(*itor);
			coor_map_[index(offset.x, offset.y)].overlay = NULL;
		}
	}

	return ptr;
}

void unit_map::place(std::pair<map_location, unit*>* p)
{
	p->second->set_location(p->first);
	// update cookies on map
	coor_map_[index(p->first.x, p->first.y)].overlay = p;
	const std::set<map_location::DIRECTION>& touch_dirs = p->second->get_touch_dirs();
	for (std::set<map_location::DIRECTION>::const_iterator itor = touch_dirs.begin(); itor != touch_dirs.end(); ++ itor) {
		map_location offset = p->first.get_direction(*itor);
		coor_map_[index(offset.x, offset.y)].overlay = p;
	}
}

// Notice: caller must make sure loc of parameter is centor location of desired erase unit!
bool unit_map::erase(const map_location& loc, bool overlay)
{
	if (!loc.valid()) {
		return false;
	}
	node* n = overlay? coor_map_[index(loc.x, loc.y)].overlay: coor_map_[index(loc.x, loc.y)].base;
	if (!n) {
		return false;
	}
	return erase(n->second);
}

bool unit_map::erase(unit* u, bool delete_unit)
{
	size_t i;
	bool base = u->base();
	const map_location& loc = u->get_location();
	node* ptr = base? coor_map_[index(loc.x, loc.y)].base: coor_map_[index(loc.x, loc.y)].overlay;

	for (i = 0; i < map_vsize_; i ++) {
		if (map_[i] == ptr) {
			break;
		}
	}

	VALIDATE(i != map_vsize_, "unitmap::erase, i == map_vsize, check code!");

	if (i < map_vsize_ - 1) {
		memcpy(&(map_[i]), &(map_[i + 1]), (map_vsize_ - i - 1) * sizeof(node*));
	}
	map_vsize_ --;

	game_display* disp = resources::screen;
	if (unit_is_city(u)) {
		citys_.erase(unit_2_artifical(u));
	} else if (!u->is_artifical()) {
		if (citys_.map_[u->cityno()]) {
			// field troop of city.
			if (!u->is_commoner()) {
				citys_.map_[u->cityno()]->field_troops_erase(u);
			} else {
				citys_.map_[u->cityno()]->field_commoners_erase(u);
			}
		} else if (disp) {
			// roam troop
			disp->refresh_access_troops(u->side() - 1, game_display::REFRESH_ERASE, const_cast<unit*>(u));
		}
	} else if (u->is_artifical() && citys_.map_[u->cityno()]) {
		citys_.map_[u->cityno()]->field_arts_erase(unit_2_artifical(u));
	}
	if (!unit_is_city(u)) {
		(*resources::teams)[u->side() - 1].erase_troop(u);
	}

	std::set<map_location> invalid_locs;
	invalid_locs.insert(loc);
	if (base) {
		coor_map_[index(loc.x, loc.y)].base = NULL;
	} else {
		coor_map_[index(loc.x, loc.y)].overlay = NULL;
	}
	const std::set<map_location::DIRECTION>& touch_dirs = ptr->second->get_touch_dirs();
	for (std::set<map_location::DIRECTION>::const_iterator itor = touch_dirs.begin(); itor != touch_dirs.end(); ++ itor) {
		map_location offset = loc.get_direction(*itor);
		if (base) {
			coor_map_[index(offset.x, offset.y)].base = NULL;
		} else {
			coor_map_[index(offset.x, offset.y)].overlay = NULL;
		}
		invalid_locs.insert(offset);
	}

	for (size_t i = 0; i < u->adjacent_size_; i ++) {
		invalid_locs.insert(u->adjacent_[i]);
	}

	if (disp) {
		if (u->terrain() != t_translation::NONE_TERRAIN) {
			disp->set_terrain_dirty();
		}
		disp->invalidate(invalid_locs);
	}

	if (delete_unit) {
		delete ptr->second;
	}
	delete ptr;

	return true;
}

void unit_map::clear()
{
	delete_all();
}

unit_map::iterator unit_map::find_leader(int side)
{
	unit_map::iterator i = begin(), i_end = end();
	return i_end;
}

artifical* unit_map::city_from_cityno(int cityno) const
{
	return citys_.map_[cityno];
}

artifical* unit_map::city_from_loc(const map_location& loc) const
{
	gamemap *game_map = resources::game_map;

	if (!game_map->on_board(loc)) {
		return NULL;
	}
	if (!coor_map_[index(loc.x, loc.y)].overlay) {
		return NULL;
	}
	if (unit_is_city(coor_map_[index(loc.x, loc.y)].overlay->second)) {
		return unit_2_artifical(coor_map_[index(loc.x, loc.y)].overlay->second);
	}
	return NULL;
}

// @seed: ramdon value. but can recover within replay.
artifical* unit_map::city_from_seed(size_t seed)
{
	std::vector<artifical*> citys;
	for (size_t i = 0; i < (*resources::teams).size(); i ++) {
		std::vector<artifical*>& side_citys = (*resources::teams)[i].holden_cities();
		citys.insert(citys.end(), side_citys.begin(), side_citys.end());
	}
	return citys[seed % citys.size()];
}

const artifical* unit_map::city_from_seed(size_t seed) const
{
	std::vector<artifical*> citys;
	for (size_t i = 0; i < (*resources::teams).size(); i ++) {
		std::vector<artifical*>& side_citys = (*resources::teams)[i].holden_cities();
		citys.insert(citys.end(), side_citys.begin(), side_citys.end());
	}
	return citys[seed % citys.size()];
}

bool unit_map::side_survived(int side, int* residuals) const
{
	int residuals_internal = 0;
	for (size_t i = 0; i < map_vsize_; i ++) {
		if (map_[i]->second->side() == side) {
			if (residuals) {
				residuals_internal ++;		
			} else {
				return true;
			}
		}
	}
	if (residuals) {
		*residuals = residuals_internal;
	}
	return residuals_internal? true: false;
}

// @draw_area_unit[OUT]: units_from_rect fill valid units to it.
// return value
//  size of filled units
// remark:
//  1)unit overlapped multi-grid, return center loc only
size_t unit_map::units_from_rect(unit** draw_area_unit, const rect_of_hexes& draw_area_rect)
{
	gamemap* game_map = resources::game_map;

	size_t draw_area_unit_size = 0;

	int draw_area_min_x = std::max(0, draw_area_rect.left);
	int draw_area_max_x = std::min(game_map->w() - 1, draw_area_rect.right);
	int draw_area_min_y[2], draw_area_max_y[2];
	draw_area_min_y[0] = std::max(0, draw_area_rect.top[0]);
	draw_area_min_y[1] = std::max(0, draw_area_rect.top[1]);
	draw_area_max_y[0] = std::min(game_map->h() - 1, draw_area_rect.bottom[0]);
	draw_area_max_y[1] = std::min(game_map->h() - 1, draw_area_rect.bottom[1]);

	for (int x = draw_area_min_x; x <= draw_area_max_x; x ++) {
		for (int y = draw_area_min_y[x & 1]; y <= draw_area_max_y[x & 1]; y ++) {
			node* node = coor_map_[index(x, y)].base;
			if (node) {
				draw_area_unit[draw_area_unit_size ++] = node->second;
			}
			node = coor_map_[index(x, y)].overlay;
			if (node && (node->first.x == x && node->first.y == y)) {
				draw_area_unit[draw_area_unit_size ++] = node->second;
			}
			
		}
	}

	return draw_area_unit_size;
}

void unit_map::ally_terminate_adjust(team& adjusting_team, const SDL_Rect& rect)
{
	int adjusting_side = adjusting_team.side();

	for (int y = rect.y; y < rect.y + rect.h; y ++) {
		for (int x = rect.x; x < rect.x + rect.w; x ++) {
			node* node = coor_map_[index(x, y)].overlay;
			if (!node || node->first != map_location(x, y) || node->second->side() != adjusting_side) {
				continue;
			}
			unit& u = *node->second;
			if (u.is_artifical()) {
				continue;
			}
			u.set_state(ustate_tag::SLOWED, true);
			u.set_state(ustate_tag::BROKEN, true);
		}
	}
}

const map_location& unit_map::center_loc(const map_location& loc) const
{
	gamemap *game_map = resources::game_map;

	if (!game_map->on_board(loc)) {
		return map_location::null_location;
	}
	node* param = coor_map_[index(loc.x, loc.y)].overlay;
	if (!param) {
		return map_location::null_location;
	}
	return param->first;
}

class sort_front_cities_func
{
public:
	sort_front_cities_func(void* caller, const mr_data& mr, bool (*callback)(void*, const mr_data& mr, artifical&, artifical&))
	  : caller_(caller)
	  , callback_(callback)
	  , mr_(mr)
	{}

	bool operator()(artifical* a, artifical* b) const
	{
		return callback_(caller_, mr_, *a, *b);
	}

private:
	void* caller_;
	const mr_data& mr_;
	bool (*callback_)(void*, const mr_data&, artifical&, artifical&);
};
/*
bool unit_map::compare_front_cities(const mr_data& mr, artifical& a, artifical& b)
{
	const mr_data::enemy_data& a_data = mr.own_cities.find(&a)->second;
	size_t a_cities = a_data.cities.size();
	const mr_data::enemy_data& b_data = mr.own_cities.find(&b)->second;
	size_t b_cities = b_data.cities.size();

	if (!a_cities && !b_cities) {
		return a_data.troops.size() >= b_data.troops.size();
	} else if (!a_cities) {
		return false;
	} else if (!b_cities) {
		return true;
	} else if (a_cities != b_cities) {
		return a_cities < b_cities;
	} else {
		// Distance to center of map.
		gamemap& game_map = *resources::game_map;
		map_location center(game_map.w() / 2, game_map.h() / 2);
		size_t a_distance = distance_between(center, a.get_location());
		size_t b_distance = distance_between(center, b.get_location());
		return a_distance <= b_distance;
	}
}
*/
bool unit_map::compare_enemy_cities(const mr_data& mr, artifical& a, artifical& b)
{
	artifical& center_city = *mr.center_city;
	gamemap& map = *resources::game_map;
	unit_map& units_ = *resources::units;
	std::vector<team>& teams_ = *resources::teams;
	team& my_team = teams_[center_city.side() - 1];
	team& a_team = teams_[a.side() - 1];
	team& b_team = teams_[b.side() - 1];

	// first, select strategy city.
	if (mr.aggressing_strategy->impletement_turns_) {
		if (mr.aggressing_strategy->target_ == a.cityno()) {
			return true;
		} else if (mr.aggressing_strategy->target_ == b.cityno()) {
			return false;
		}
	}

	// condition: a_side == b_side, compare move cost
	int center_cityno = center_city.cityno();
	int a_cityno = a.cityno();
	int b_cityno = b.cityno();
	int first_cityno, second_cityno, a_cost, b_cost;
	std::map<std::pair<int, int>, size_t>::const_iterator itor;
	
	// a_cost
	if (center_cityno > a_cityno) {
		first_cityno = a_cityno;
		second_cityno = center_cityno;
	} else {
		first_cityno = center_cityno;
		second_cityno = a_cityno;
	}

	itor = unit_map::inter_city_move_cost_.find(std::make_pair(first_cityno, second_cityno));
	if (itor != unit_map::inter_city_move_cost_.end()) {
		a_cost = itor->second;
	} else {
		// const pathfind::shortest_path_calculator calc(*unit_map::scout_unit_, my_team, units_, teams_, map, true);
		const pathfind::emergency_path_calculator calc(*unit_map::scout_unit_, map);
		double stop_at = 10000.0;
		//allowed teleports
		std::set<map_location> allowed_teleports;
		// NOTICE! although start and end are same, but rank filp, result maybe differenrt! so use cityno to make sure same.
		pathfind::plain_route route = a_star_search(city_from_cityno(first_cityno)->get_location(), city_from_cityno(second_cityno)->get_location(), stop_at, &calc, map.w(), map.h(), &allowed_teleports);
		if (route.steps.empty()) {
			route.move_cost = 10000;
		}
		unit_map::inter_city_move_cost_.insert(std::make_pair(std::make_pair(first_cityno, second_cityno), route.move_cost));
		a_cost = route.move_cost;
	}

	// b_cost
	if (center_cityno > b_cityno) {
		first_cityno = b_cityno;
		second_cityno = center_cityno;
	} else {
		first_cityno = center_cityno;
		second_cityno = b_cityno;
	}

	itor = unit_map::inter_city_move_cost_.find(std::make_pair(first_cityno, second_cityno));
	if (itor != unit_map::inter_city_move_cost_.end()) {
		b_cost = itor->second;
	} else {
		// const pathfind::shortest_path_calculator calc(*unit_map::scout_unit_, my_team, units_, teams_, map, true);
		const pathfind::emergency_path_calculator calc(*unit_map::scout_unit_, map);
		double stop_at = 10000.0;
		//allowed teleports
		std::set<map_location> allowed_teleports;
		// NOTICE! although start and end are same, but rank filp, result maybe differenrt! so use cityno to make sure same.
		pathfind::plain_route route = a_star_search(city_from_cityno(first_cityno)->get_location(), city_from_cityno(second_cityno)->get_location(), stop_at, &calc, map.w(), map.h(), &allowed_teleports);
		if (route.steps.empty()) {
			route.move_cost = 10000;
		}
		unit_map::inter_city_move_cost_.insert(std::make_pair(std::make_pair(first_cityno, second_cityno), route.move_cost));
		b_cost = route.move_cost;
	}

	if (a_team.side() == b_team.side()) {
		return (a_cost < b_cost) || (a_cost == b_cost && a_cityno < b_cityno);
	}

	// condition: a_side != b_side, compare cost.
	if (a_cost > b_cost + 10) {
		return false;
	} else if (a_cost + 10 < b_cost) {
		return true;
	}

	// condition: a_side != b_side, compare power, location.
	size_t a_power, b_power;
	std::vector<artifical*>& a_cities = a_team.holden_cities();
	a_power = 0;
	for (std::vector<artifical*>::const_iterator i = a_cities.begin(); i != a_cities.end(); ++ i) {
		artifical& city = **i;
		a_power += city.fresh_heros().size() + city.finish_heros().size() + (city.reside_troops().size() + city.field_troops().size()) * 2;
	}
	std::vector<artifical*>& b_cities = b_team.holden_cities();
	b_power = 0;
	for (std::vector<artifical*>::const_iterator i = b_cities.begin(); i != b_cities.end(); ++ i) {
		artifical& city = **i;
		b_power += city.fresh_heros().size() + city.finish_heros().size() + (city.reside_troops().size() + city.field_troops().size()) * 2;
	}
	double a_power_ratio = 1.0 * a_power / (a_power + b_power);
	double b_power_ratio = 1.0 * b_power / (a_power + b_power);

	double a_x_offset = 1.0 * a.get_location().x / map.w() - 0.5;
	if (a_x_offset < 0) {
		a_x_offset *= -1;
	}
	double a_y_offset = 1.0 * a.get_location().y / map.h() - 0.5;
	if (a_y_offset < 0) {
		a_y_offset *= -1;
	}
	double b_x_offset = 1.0 * b.get_location().x / map.w() - 0.5;
	if (b_x_offset < 0) {
		b_x_offset *= -1;
	}
	double b_y_offset = 1.0 * b.get_location().y / map.h() - 0.5;
	if (b_y_offset < 0) {
		b_y_offset *= -1;
	}
	// power ratio is inverse proportion.
	if (b_power_ratio + a_x_offset + a_y_offset > a_power_ratio + b_x_offset + b_y_offset) {
		double a_double = b_power_ratio + a_x_offset + a_y_offset;
		double b_double = a_power_ratio + b_x_offset + b_y_offset;
		return true;
	} else if ((b_power_ratio + a_x_offset + a_y_offset == a_power_ratio + b_x_offset + b_y_offset) && a_cityno < b_cityno) {
		return true;
	}
	return false;
}	
/*
static bool callback_compare_front_cities(void* caller, const mr_data& mr, artifical& a, artifical& b)
{
	return reinterpret_cast<unit_map*>(caller)->compare_front_cities(mr, a, b);
}
*/
static bool callback_compare_enemy_cities(void* caller, const mr_data& mr, artifical& a, artifical& b)
{
	return reinterpret_cast<unit_map*>(caller)->compare_enemy_cities(mr, a, b);
}

void unit_map::calculate_mr_rects_from_city_rect(std::vector<team>& teams, gamemap& map, std::vector<mr_data>& mrs, int side)
{
	team& curr_team = teams[side - 1];

	// now only support one military_region, so set military_region_size to a imposibble maximum value.
	int military_region_size = 300;
	SDL_Rect temp_rect, unallocated = curr_team.city_rect();

	while (unallocated.w / military_region_size > 1) {
		while (unallocated.h / military_region_size > 1) {
			temp_rect.x = unallocated.x;
			temp_rect.y = unallocated.y;
			temp_rect.w = temp_rect.h = military_region_size;
			mrs.push_back(mr_data(map, temp_rect));
			unallocated.y += military_region_size;
			unallocated.h -= military_region_size;
		}
		temp_rect.x = unallocated.x;
		temp_rect.y = unallocated.y;
		temp_rect.w = military_region_size;
		temp_rect.h = unallocated.h;
		mrs.push_back(mr_data(map, temp_rect));

		unallocated.x += military_region_size;
		unallocated.y = curr_team.city_rect().y;
		unallocated.w -= military_region_size;
		unallocated.h = curr_team.city_rect().h;
	}

	// last collum, use unallocated.w instead of military_region_size
	while (unallocated.h / military_region_size > 1) {
		temp_rect.x = unallocated.x;
		temp_rect.y = unallocated.y;
		temp_rect.w = unallocated.w;
		temp_rect.h = military_region_size;
		mrs.push_back(mr_data(map, temp_rect));
		unallocated.y += military_region_size;
		unallocated.h -= military_region_size;
	}
	// use unallocated.w instead of military_region_size
	temp_rect.x = unallocated.x;
	temp_rect.y = unallocated.y;
	temp_rect.w = unallocated.w;
	temp_rect.h = unallocated.h;
	mrs.push_back(mr_data(map, temp_rect));
}

// 1. To each mr, calculate kinds of data
// 2. Form mr's target.
void unit_map::calculate_mrs_data(game_state& state, std::vector<mr_data>& mrs, int side, bool action)
{
	std::vector<team>& teams = *resources::teams;
	team& current_team = teams[side - 1];
	gamemap& game_map = *resources::game_map;
	hero_map& heros = *resources::heros;
	play_controller& controller = *resources::controller;

	const hero& leader = *current_team.leader();

	calculate_mr_rects_from_city_rect(*resources::teams, game_map, mrs, side);

	int min_field_arts = 3;
	
	const std::set<const unit_type*>& can_build = current_team.builds();
	if (tent::mode == mode_tag::SIEGE || can_build.find(unit_types.find_wall()) == can_build.end()) {
		min_field_arts = 0;
	}
	std::set<const unit_type*> can_build_ea;
	for (std::set<const unit_type*>::const_iterator it = can_build.begin(); it != can_build.end(); ++ it) {
		const unit_type* ut = *it;
		if (ut == unit_types.find_market() || ut == unit_types.find_technology() || ut == unit_types.find_tactic()) {
			can_build_ea.insert(ut);
		}
	}

	for (std::vector<mr_data>::iterator mr_itor = mrs.begin(); mr_itor != mrs.end(); ++ mr_itor) {
		mr_data& mr = *mr_itor;

		mr.enemy_troops.clear();
		mr.enemy_cities.clear();
		mr.own_heros = 0;
		mr.own_front_cities.clear();
		mr.own_back_cities.clear();
		mr.own_cities.clear();
		mr.encountered_sides.clear();

		std::vector<artifical*>& holden_cities = current_team.holden_cities();
		for (std::vector<artifical*>::iterator city_itor = holden_cities.begin(); city_itor != holden_cities.end(); ++ city_itor) {
			artifical& city = **city_itor;
			const map_location& loc = city.get_location();
			if (point_in_rect(loc.x, loc.y, mr.consider_rect)) {
				mr.own_cities[city.cityno()] = mr_data::enemy_data();
				mr.own_heros += city.fresh_heros().size() + city.finish_heros().size() + (city.reside_troops().size() + city.field_troops().size()) * 2;
			}
		}

		// process low loyalty hero.
		for (std::map<int, mr_data::enemy_data>::iterator city_itor = mr.own_cities.begin(); action && city_itor != mr.own_cities.end(); ++ city_itor) {
			if (tent::tower_mode()) {
				continue;
			}
			artifical& city = *city_from_cityno(city_itor->first);
			if (rpg::stratum == hero_stratum_mayor && city.mayor() == rpg::h) {
				continue;
			}
			city.demolish_ea(can_build_ea);

			if (tent::mode != mode_tag::RPG) {
				continue;
			}
			std::vector<std::pair<unit*, std::vector<hero*> > > new_low_loyalty_troops;
			std::vector<hero*> original_captains;
			std::vector<hero*> new_captains;
					
			std::vector<unit*>& resides = city.reside_troops();
			std::vector<hero*>& freshes = city.fresh_heros();
			for (std::vector<unit*>::iterator it = resides.begin(); it != resides.end(); ++ it) {
				unit& u = **it;
				if (u.human() || !u.consider_loyalty()) {
					continue;
				}
				original_captains.clear();
				new_captains.clear();

				int l = u.master().loyalty(leader);
				original_captains.push_back(&u.master());
				if (l < game_config::ai_keep_loyalty_threshold) {
					new_captains.push_back(&u.master());
				}
				if (u.second().valid()) {
					l = u.second().loyalty(leader);
					original_captains.push_back(&u.second());
					if (l < game_config::ai_keep_loyalty_threshold) {
						new_captains.push_back(&u.second());
					}
				}
				if (u.third().valid()) {
					l = u.third().loyalty(leader);
					original_captains.push_back(&u.third());
					if (l < game_config::ai_keep_loyalty_threshold) {
						new_captains.push_back(&u.third());
					}
				}
				// always don't disband this unit.
				if (new_captains.empty()) {
					// all hero don't less than ai_keep_loyalty_threshold, do nothing.
					continue;
				}
				if (original_captains.size() != new_captains.size()) {
					u.replace_captains(new_captains);
					std::vector<hero*>::iterator find_it;
					for (std::vector<hero*>::iterator h_it = original_captains.begin(); h_it != original_captains.end(); ++ h_it) {
						if (std::find(new_captains.begin(), new_captains.end(), *h_it) == new_captains.end()) {
							city.fresh_into(**h_it);
						}
					}
					new_low_loyalty_troops.push_back(std::make_pair(&u, new_captains));
				} else {
					new_low_loyalty_troops.push_back(std::make_pair(&u, original_captains));
				}
			}

			// low loyalty hero section
			std::vector<hero*> low_loyalty_heros;
			for (std::vector<hero*>::iterator it = freshes.begin(); it != freshes.end(); ++ it) {
				hero& h = **it;
				if (&h == rpg::h) {
					continue;
				}
				int l = h.loyalty(leader);
				if (l < game_config::ai_keep_loyalty_threshold) {
					low_loyalty_heros.push_back(&h);
				}
			}
			if (low_loyalty_heros.empty()) {
				continue;
			}
			for (std::vector<std::pair<unit*, std::vector<hero*> > >::iterator it = new_low_loyalty_troops.begin(); it != new_low_loyalty_troops.end() && !low_loyalty_heros.empty(); ++ it) {
				if (it->second.size() == 3) {
					continue;
				}
				while (it->second.size() < 3 && !low_loyalty_heros.empty()) {
					hero* h = low_loyalty_heros.front();
					it->second.push_back(h);
					low_loyalty_heros.erase(low_loyalty_heros.begin());
					freshes.erase(std::find(freshes.begin(), freshes.end(), h));
				}
				it->first->replace_captains(it->second);
			}
			if (low_loyalty_heros.empty()) {
				continue;
			}
			const unit_type* ut = city.recruits(game_config::default_ai_level)[0];
			while (!low_loyalty_heros.empty()) {
				std::vector<const hero*> v;
				do {
					hero* h = low_loyalty_heros.front();
					v.push_back(h);
					low_loyalty_heros.erase(low_loyalty_heros.begin());
				} while (v.size() < 3 && !low_loyalty_heros.empty());
				do_recruit(*this, heros, teams, current_team, ut, v, city, ut->cost() * current_team.cost_exponent() / 100, false, false, state);
			}
		}
		
		mr.center_city = mr.calculate_center_city(map_location(mr.consider_rect.x + mr.consider_rect.w / 2, mr.consider_rect.y + mr.consider_rect.h / 2));

		// enemy troop/city
		for (int y = mr.consider_rect.y; y < mr.consider_rect.y + mr.consider_rect.h; y ++) {
			for (int x = mr.consider_rect.x; x < mr.consider_rect.x + mr.consider_rect.w; x ++) {
				node* node = coor_map_[index(x, y)].overlay;
				if (node && (node->first == map_location(x, y))) {
					if (current_team.is_enemy(node->second->side())) {
						for (std::map<int, mr_data::enemy_data>::iterator city_itor = mr.own_cities.begin(); city_itor != mr.own_cities.end(); ++ city_itor) {
							// in order to judge back/front city, calculate city's enemy troops. 
							artifical& city = *city_from_cityno(city_itor->first);
							const map_location& loc = node->first;
							if (point_in_rect(loc.x, loc.y, city.alert_rect())) {
								mr_data::enemy_data& data = city_itor->second;
								if (unit_is_city(node->second)) {
									data.cities.push_back(unit_2_artifical(node->second));
								} else {
									data.troops.push_back(node->second);
								}
							} 
						}
						if (unit_is_city(node->second)) {
							mr.enemy_cities.push_back(unit_2_artifical(node->second));
							mr.encountered_sides.insert(node->second->side());
						} else if (!node->second->is_artifical()) {
							mr.enemy_troops.push_back(node->second);
							mr.encountered_sides.insert(node->second->side());
						}
					}
				}
			}
		}

		if (mr.enemy_cities.empty()) {
			// There is no enemy city in alert rectangle.
			mr.consider_rect.x = mr.consider_rect.y = 0;
			mr.consider_rect.w = game_map.w();
			mr.consider_rect.h = game_map.h();

			for (city_map::iterator c_itor = citys_.begin(); c_itor != citys_.end(); ++ c_itor) {
				if (current_team.is_enemy(c_itor->side())) {
					mr.enemy_cities.push_back(&*c_itor);
				}
			}
		}

		mr.aggressing_strategy = &current_team.get_strategy(0, strategy::AGGRESS);

		// sort enemy_cities.
		artifical* aggressing = NULL;
		if (!mr.enemy_cities.empty() && !mr.own_cities.empty()) {
			std::sort(mr.enemy_cities.begin(), mr.enemy_cities.end(), sort_front_cities_func(this, mr, callback_compare_enemy_cities));
			artifical* capital_of_ai = controller.final_capital().second;
			if (!capital_of_ai || mr.own_cities.find(capital_of_ai->cityno()) == mr.own_cities.end()) {
				aggressing = mr.calculate_center_city(mr.enemy_cities[0]->get_location());
			} else {
				aggressing = capital_of_ai;				
			}
			if (action) {
				if (mr.aggressing_strategy->target_ != mr.enemy_cities[0]->cityno()) {
					if (mr.aggressing_strategy->target_) {
						current_team.erase_strategy(mr.aggressing_strategy->target_);
					}
					current_team.add_strategy(strategy(mr.enemy_cities[0]->cityno(), strategy::AGGRESS, strategy::default_impletement_turns));
				} else if (!mr.aggressing_strategy->impletement_turns_) {
					mr.aggressing_strategy->impletement_turns_ = strategy::default_impletement_turns;
				}
			}
		}

		size_t field_arts = 0;
		for (std::map<int, mr_data::enemy_data>::iterator city_itor = mr.own_cities.begin(); city_itor != mr.own_cities.end(); ++ city_itor) {
			artifical* city = city_from_cityno(city_itor->first);
			field_arts += city->field_arts().size();
			if (!aggressing || city != aggressing) {
				if ((int)city_itor->second.cities.size() + (int)city_itor->second.troops.size() >= mr_data::minimum_enemy_in_front) {
					if (action) {
						city->inching_fronts(true);
					}
					mr.own_front_cities.push_back(city);
				} else {
					if (action) {
						city->inching_fronts(false);
					}
					mr.own_back_cities.push_back(city);
				}
			}
		}
		if (aggressing) {
			if (action) {
				aggressing->inching_fronts(true);
			}
			mr.own_front_cities.insert(mr.own_front_cities.begin(), aggressing);
		}

		if (action) {
			for (std::vector<artifical*>::iterator it = mr.own_front_cities.begin(); it != mr.own_front_cities.end(); ++ it) {
				artifical* city = *it;
				const std::vector<unit*>& enemy_troops = mr.own_cities[city->cityno()].troops;
				if (city->fronts() == mr_data::max_fronts && (int)enemy_troops.size() > mr_data::minimum_enemy_in_front) {
					strategy& defend_strategy = current_team.get_strategy(city->cityno());
					if (!defend_strategy.target_) {
						current_team.add_strategy(strategy(city->cityno(), strategy::DEFEND, strategy::default_impletement_turns));
					} else if (!defend_strategy.impletement_turns_) {
						defend_strategy.impletement_turns_ = strategy::default_impletement_turns;
					}
				}
			}
		}

		// calculate target.
		if (mr.own_cities.empty() && !mr.enemy_cities.empty()) {
			mr.target = mr_data::TARGET_CHAOTIC;
		} else if (mr.own_front_cities.empty()) {
			mr.target = mr_data::TARGET_INTERIOR;
		} else if ((field_arts >= mr.own_cities.size() * min_field_arts) && !mr.enemy_cities.empty() && mr.own_heros > mr_data::min_interior_requirement * mr.own_back_cities.size() + mr_data::min_front_requirement * mr.own_front_cities.size()) {
			mr.target = mr_data::TARGET_AGGRESS;

			if (!tent::tower_mode() && mr.own_cities.size() == 1) {
				// Alert! Be back to guard!
				std::map<int, mr_data::enemy_data>::iterator it_p = mr.own_cities.begin();
				artifical* only = city_from_cityno(it_p->first);
				const map_location& loc = only->get_location();
				artifical* aggressed = mr.enemy_cities[0];
				int my_ratio = only->hitpoints() * 100 / only->max_hitpoints();
				int aggressed_ratio = aggressed->hitpoints() * 100 / aggressed->max_hitpoints();

				if (aggressed_ratio >= 10) {
					size_t guard_troops = 0;
					for (std::vector<unit*>::const_iterator it = only->field_troops().begin(); it != only->field_troops().end(); ++ it) {
						unit& u = **it;
						if ((int)distance_between(loc, u.get_location()) < u.total_movement() * 2 / 3) {
							guard_troops ++;
						}
					}
					size_t troops = it_p->second.troops.size();
					if (troops >= 2 && troops >= only->fresh_heros().size() + only->reside_troops().size() + guard_troops) {
						if (troops >= 4 && action && !current_team.is_human()) {
							// don't set human troop.
							for (std::vector<unit*>::const_iterator it = only->field_troops().begin(); it != only->field_troops().end(); ++ it) {
								unit& u = **it;
								if (!u.provoked_turns() && (int)distance_between(loc, u.get_location()) > u.total_movement() * 2) {
									// please be back to guard. must back!
									u.set_attacks(0);
								}
							}
						}
						mr.target = mr_data::TARGET_GUARD;
					}
				}
			}
		} else {
			mr.target = mr_data::TARGET_GUARD;
		}
	}
}

void unit_map::ai_capture_aggressed(artifical& aggressed, int side, bool to_recorder)
{
	team& current_team = (*resources::teams)[side - 1];
	gamemap& game_map = *resources::game_map;

	const map_location& aggressed_loc = aggressed.get_location();
	SDL_Rect effect_rect = extend_rectangle(game_map, aggressed_loc.x, aggressed_loc.y, 10);
	for (int y = effect_rect.y; y < effect_rect.y + effect_rect.h; y ++) {
		for (int x = effect_rect.x; x < effect_rect.x + effect_rect.w; x ++) {
			node* node = coor_map_[index(x, y)].overlay;
			if (node && !node->second->is_artifical()) {
				unit* troop= node->second;
				if (!troop->human() && !troop->is_commoner() && troop->side() == side && troop->cityno() != aggressed.cityno()) {
					aggressed.unit_belong_to(troop, false, to_recorder);
					troop->set_goto(aggressed_loc);
				}
			}
		}
	}
}

int mr_data::min_interior_requirement = 3;
int mr_data::max_interior_requirement = 4;
int mr_data::min_front_requirement = 4;
int mr_data::mid_front_requirement = 8;
int mr_data::max_front_requirement = 12;
int mr_data::min_alert_fronts = 2;
int mr_data::max_fronts = 8;
int mr_data::min_interior_lack_ = 5;
int mr_data::aggress_requirement = 18;

int mr_data::ally_forbid_turns = 10;
int mr_data::minimum_enemy_in_front = 3;
int mr_data::consider_rect_radius = 30;

mr_data::mr_data(gamemap& map, SDL_Rect& city_rect)
	: target(mr_data::TARGET_INTERIOR)
	, own_heros(0)
	, own_front_cities()
	, own_back_cities()
	, own_cities()
	, enemy_cities()
	, encountered_sides()
	, center_city(NULL)
{
	// calculate consider-rectangle according to city-rectangle
	if (city_rect.x != -1) {
		consider_rect = extend_rectangle(map, city_rect, consider_rect_radius);
	} else {
		consider_rect.x = consider_rect.y = 0;
		consider_rect.w = map.w();
		consider_rect.h = map.h();
	}
}

artifical* mr_data::calculate_center_city(const map_location& center)
{
	unit_map& units_ = *resources::units;
	if (own_cities.empty()) {
		return NULL;
	}
	std::map<int, mr_data::enemy_data>::iterator it = own_cities.begin();
	if (own_cities.size() == 1) {
		return units_.city_from_cityno(it->first);
	}

	std::map<int, mr_data::enemy_data>::iterator choice = it;

	size_t min_distance = -1;
	artifical* first = units_.city_from_cityno(it->first);
	if (first->mayor() != rpg::h) {
		min_distance = distance_between(center, first->get_location());
	}
	for (++ it; it != own_cities.end(); ++ it) {
		size_t distance = -1;
		first = units_.city_from_cityno(it->first);
		if (first->mayor() != rpg::h) {
			distance = distance_between(center, first->get_location());
		}
		if (distance < min_distance) {
			min_distance = distance;
			choice = it;
		// } else if (distance == min_distance && first->cityno() < choice->first->cityno()) {
		} else if (distance == min_distance) {
			// own_cities is std::map, sort by pointer of city.
			// pointer of city is random, so when distance == min_distance, it is necessary stonger estimate (using cityno).
			min_distance = distance;
			choice = it;
		}
	}
	return units_.city_from_cityno(choice->first);
}

int mess_estimate_radius = 7;
void mr_data::calculate_mass(const unit_map& units, const team& current_team)
{
	messes.clear();

	tmess_data::tadjacent_data adjacent;
	std::set<const unit*> calculated_base, calculated_overlay;
	std::vector<const unit*> field_arts;

	int side = current_team.side();
	
	const std::pair<unit**, size_t> p = current_team.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		if (troops[i]->is_artifical()) {
			field_arts.push_back(troops[i]);
			continue;
		}

		unit& u_analysing = *troops[i];
		
		tmess_data* mess = NULL;
		const map_location& loc_analysing = u_analysing.get_location();
		// center
		for (std::vector<tmess_data>::iterator it = messes.begin(); it != messes.end(); ++ it) {
			const map_location center(it->cumulate_x / it->selfs.size(), it->cumulate_y / it->selfs.size());
			int d = distance_between(center, loc_analysing);
			if (d <= mess_estimate_radius) {
				mess = &*it;
			}
		}
		if (!mess) {
			messes.push_back(tmess_data());
			mess = &messes.back();
		}
		mess->cumulate_x += loc_analysing.x;
		mess->cumulate_y += loc_analysing.y;

		adjacent.clear();

		const map_location* tiles = u_analysing.adjacent_;
		size_t adjance_size = u_analysing.adjacent_size_;
		for (int step = 0; step < 2; step ++) {
			if (step == 1) {
				tiles = u_analysing.adjacent_2_;
				adjance_size = u_analysing.adjacent_size_2_;
			}

			for (size_t adj = 0; adj < adjance_size; adj ++) {
				const map_location& loc = tiles[adj];
				// overlay
				unit_map::node* curr_node = reinterpret_cast<unit_map::node*>(units.get_cookie(loc));
				if (curr_node) {
					const unit* u = curr_node->second;
					if (u->side() != side) {
						bool calculated = calculated_overlay.find(u) != calculated_overlay.end();
						if (current_team.is_enemy(u->side())) {
							if (u->is_artifical()) {
								if (!calculated) {
									mess->enemy_arts ++;
								}
							} else if (!u->is_commoner()) {
								adjacent.enemies ++;
								if (!calculated) {
									mess->enemies ++;
								}
							}
						} else {
							if (u->is_artifical()) {
								if (!calculated) {
									mess->friend_arts ++;
								}
							} else if (!u->is_commoner()) {
								adjacent.friends ++;
								if (!u->hide_turns()) {
									adjacent.unhides ++;
								}
								if (u->uncleared()) {
									adjacent.unnormals ++;
								}
								if (u->healable()) {
									adjacent.healables ++;
								}
								if (!u->provoked_turns()) {
									adjacent.unprovoks ++;
								}
								if (!calculated) {
									mess->allys ++;
								}
							}
						}
						if (!calculated) {
							calculated_overlay.insert(u);
						}
					} else if (!u->is_artifical()) {
						adjacent.friends ++;
						if (!u->hide_turns()) {
							adjacent.unhides ++;
						}
						if (u->uncleared()) {
							adjacent.unnormals ++;
						}
						if (u->healable()) {
							adjacent.healables ++;
						}
						if (!u->provoked_turns()) {
							adjacent.unprovoks ++;
						}
					}
				} 
				// base
				curr_node = reinterpret_cast<unit_map::node*>(units.get_cookie(loc, false));
				if (curr_node) {
					const unit* u = curr_node->second;
					if (u->side() != side) {
						bool calculated = calculated_base.find(u) != calculated_base.end();
						if (current_team.is_enemy(u->side())) {
							if (u->is_artifical()) {
								if (!calculated) {
									mess->enemy_arts ++;
								}
							} else {
								adjacent.enemies ++;
								if (!calculated) {
									mess->enemies ++;
								}
							}
						} else {
							if (u->is_artifical()) {
								if (!calculated) {
									mess->friend_arts ++;
								}
							} else {
								adjacent.friends ++;
								if (!u->hide_turns()) {
									adjacent.unhides ++;
								}
								if (u->uncleared()) {
									adjacent.unnormals ++;
								}
								if (u->healable()) {
									adjacent.healables ++;
								}
								if (!u->provoked_turns()) {
									adjacent.unprovoks ++;
								}
								if (!calculated) {
									mess->allys ++;
								}
							}
						}
						if (!calculated) {
							calculated_base.insert(u);
						}
					} else if (!u->is_artifical()) {
						adjacent.friends ++;
						if (!u->hide_turns()) {
							adjacent.unhides ++;
						}
						if (u->uncleared()) {
							adjacent.unnormals ++;
						}
						if (u->healable()) {
							adjacent.healables ++;
						}
						if (!u->provoked_turns()) {
							adjacent.unprovoks ++;
						}
					}
				}
			}
		}
		mess->selfs[&u_analysing] = adjacent;
	}

	// combine if necessary
	bool combined = true;
	while (combined) {
		combined = false;
		for (std::vector<tmess_data>::iterator it = messes.begin(); it != messes.end(); ++ it) {
			const map_location center(it->cumulate_x / it->selfs.size(), it->cumulate_y / it->selfs.size());
			for (std::vector<tmess_data>::iterator it2 = messes.begin(); it2 != messes.end(); ++ it2) {
				if (it->cumulate_x == it2->cumulate_x && it->cumulate_y == it2->cumulate_y) {
					continue;
				}
				const map_location center2(it2->cumulate_x / it2->selfs.size(), it2->cumulate_y / it2->selfs.size());
				int d = distance_between(center, center2);
				if (d > mess_estimate_radius) {
					continue;
				}
				// combined this
				it->combine(*it2);
				messes.erase(it2);

				combined = true;
				break;
			}
			if (combined) {
				break;
			}
		}
	}

	for (std::vector<const unit*>::iterator it = field_arts.begin(); it != field_arts.end(); ++ it) {
		const artifical* art = const_unit_2_artifical(*it);
		if (!art->wall()) {
			continue;
		}
		for (std::vector<tmess_data>::iterator it2 = messes.begin(); it2 != messes.end(); ++ it2) {
			const map_location center(it2->cumulate_x / it2->selfs.size(), it2->cumulate_y / it2->selfs.size());
			int d = distance_between(center, art->get_location());
			if (d <= mess_estimate_radius) {
				it2->friend_arts ++;
				// one wall only belong to a mess.
				break;
			}
		}
	}
}

void tmess_data::combine(const tmess_data& that)
{
	cumulate_x += that.cumulate_x;
	cumulate_y += that.cumulate_y;
	for (std::map<unit*, tadjacent_data>::const_iterator it = that.selfs.begin(); it != that.selfs.end(); ++ it) {
		selfs[it->first] = it->second;
	}
	allys += that.allys;
	friend_arts += that.friend_arts;
	enemies += that.enemies;
	enemy_arts += that.enemy_arts;
}

bool cb_terrain_matches(const map_location& loc, const t_translation::t_match& terrain_types_match)
{
	unit_map& units = *resources::units;
	unit_map::const_iterator u_itor = units.find(loc, false);
	t_translation::t_terrain terrain = t_translation::NONE_TERRAIN;

	if (!u_itor.valid()) {
		u_itor = units.find(loc);
	}
	if (u_itor.valid()) {
		terrain = u_itor->terrain();
	}
	// In despite of terrain is t_translation::NONE_TERRAIN, terrain_matches maybe return false.
	// when overlay isn't 0xffffffff, for example *^_fme.
	return terrain_matches(terrain, terrain_types_match);
}

void cb_build_terrains(std::map<t_translation::t_terrain, std::vector<map_location> >& terrain_by_type)
{
	if (!resources::units) {
		return;
	}

	unit_map& units = *resources::units;
	for (unit_map::const_iterator u_itor = units.begin(); u_itor != units.end(); u_itor ++) {
		const t_translation::t_terrain& t = u_itor->terrain();
		if (t != t_translation::NONE_TERRAIN) {
			terrain_by_type[t].push_back(u_itor->get_location());
		}
	}
}