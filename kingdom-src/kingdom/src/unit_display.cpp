/* $Id: unit_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#include "global.hpp"
#include "unit_display.hpp"

#include "game_preferences.hpp"
#include "game_events.hpp"
#include "mouse_events.hpp"
#include "resources.hpp"
#include "terrain_filter.hpp"
#include "artifical.hpp"
#include "play_controller.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/theme2.hpp"

#include "sound.hpp"
#include <boost/foreach.hpp>

static void teleport_unit_between( const map_location& a, const map_location& b, unit& temp_unit, bool force_scroll)
{
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || (disp->fogged(a) && disp->fogged(b))) {
		return;
	}
	disp->scroll_to_tiles(a,b,game_display::ONSCREEN,true,0.0, force_scroll);

	temp_unit.set_location(a);
	if (!disp->fogged(a)) { // teleport
		disp->invalidate(temp_unit.get_location());
		temp_unit.set_facing(a.get_relative_dir(b));
		unit_animator animator;
		animator.add_animation(&temp_unit,"pre_teleport",a);
		animator.start_animations();
		animator.wait_for_end();
	}

	temp_unit.set_location(b);
	if (!disp->fogged(b)) { // teleport
		disp->invalidate(temp_unit.get_location());
		temp_unit.set_facing(a.get_relative_dir(b));
		disp->scroll_to_tiles(b,a,game_display::ONSCREEN,true,0.0, force_scroll);
		unit_animator animator;
		animator.add_animation(&temp_unit,"post_teleport",b);
		animator.start_animations();
		animator.wait_for_end();
	}

	temp_unit.set_standing();
	disp->update_display();
	events::pump();
}

class pack_base_lock
{
public:
	pack_base_lock(const map_location& loc)
	{
		unit::pack_base_loc = loc;
	}

	~pack_base_lock()
	{
		unit::pack_base_loc = map_location();
	}
};

static void move_unit_between(const map_location& a, const map_location& b, unit& temp_unit,unsigned int step_num,unsigned int step_left, bool force_scroll)
{
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || (disp->fogged(a) && disp->fogged(b))) {
		return;
	}

	{
		// reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=21959&extra=page%3D1
		pack_base_lock lock(b);
		temp_unit.set_location(a);
	}

	disp->invalidate(temp_unit.get_location());
	temp_unit.set_facing(a.get_relative_dir(b));
	unit_animator animator;

	animator.replace_anim_if_invalid(&temp_unit,"movement",a,b,step_num,
			false, false, "", 0, unit_animation::INVALID, NULL, null_str, step_left);

	// animator.add_animation(&temp_unit, "pre_movement", a, b);

	animator.start_animations();
        animator.pause_animation();
	disp->scroll_to_tiles(a,b,game_display::ONSCREEN,true,0.0, force_scroll);	
        animator.restart_animation();

	// useless now, previous short draw() just did one
	// new_animation_frame();

	int target_time = animator.get_animation_time_potential();

		// target_time must be short to avoid jumpy move
		// std::cout << "target time: " << target_time << "\n";
	// we round it to the next multile of 200
	target_time += 200;
	target_time -= target_time%200;

	// This code causes backwards teleport because the time > 200 causes offset > 1.0
	// which will not match with the following -1.0
	// if(  target_time - animator.get_animation_time_potential() < 100 ) target_time +=200;

	// change wait_until to wait_for_end!
	// animator.wait_until(target_time);
	animator.wait_for_end();

		// debug code, see unit_frame::redraw()
		// std::cout << "   end\n";
	map_location arr[6];
	get_adjacent_tiles(a, arr);
	unsigned int i;
	for (i = 0; i < 6; ++i) {
		disp->invalidate(arr[i]);
	}
	get_adjacent_tiles(b, arr);
	for (i = 0; i < 6; ++i) {
		disp->invalidate(arr[i]);
	}
}

namespace unit_display
{
int player_number_;

bool cast_tactic_state;
unit_animator tactic_animator;

tactic_anim_lock::tactic_anim_lock(game_display& disp, const unit& u, bool scroll)
	: disp_(disp)
	, u_(u)
	, scroll_(scroll)
{
	cast_tactic_state = true;
}

tactic_anim_lock::~tactic_anim_lock()
{
	cast_tactic_state = false;

	bool require_anim = false;
	if (!scroll_) {
		const rect_of_hexes& draw_area = disp_.draw_area();
		if (point_in_rect_of_hexes(u_.get_location().x, u_.get_location().y, draw_area)) {
			require_anim = true;
		}
		if (!require_anim) {
			const std::vector<unit_animator::anim_elem>& animated_units = tactic_animator.animated_units();
			for (std::vector<unit_animator::anim_elem>::const_iterator it = animated_units.begin(); it != animated_units.end(); ++ it) {
				if (it->my_unit == &u_) {
					continue;
				}
				const unit& my_unit = *it->my_unit;
				const map_location& loc = my_unit.get_location();
				if (point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
					require_anim = true;
					break;
				}
			}
		}
	} else {
		require_anim = true;
	}

	if (require_anim && tactic_animator.get_end_time() != INT_MIN) {
		// This is all the pretty stuff.
		if (scroll_) {
			disp_.scroll_to_tile(u_.get_location(), game_display::ONSCREEN, true, true);
		}
	
		tactic_animator.start_animations();
		tactic_animator.wait_for_end();
		tactic_animator.set_all_standing();
	}
	tactic_animator.clear();
}

formation_anim_lock::formation_anim_lock(game_display& disp, tformation& formation, formation_anim_lock** relative)
	: disp_(disp)
	, formation_(formation)
	, relative_(relative)
	, defender_()
	, started_(false)
{
	cast_tactic_state = true;
	if (relative_) {
		*relative_ = this;
	}
}

formation_anim_lock::~formation_anim_lock()
{
	flush();
	cast_tactic_state = false;

	if (formation_.gold_) {
		unit_display::unit_income(*formation_.u_, formation_.gold_);
	}
	if (relative_) {
		*relative_ = NULL;
	}
}

void formation_anim_lock::push_defender(const unit* def)
{
	defender_.push_back(def);
}

void formation_anim_lock::flush()
{
	if (!started_) {
		if (tactic_animator.get_end_time() != INT_MIN) {
			disp_.scroll_to_tile(formation_.u_->get_location(), game_display::ONSCREEN, true, true);
			unit_display::formation_attack_start(formation_);

			started_ = true;
		}
	}

	if (tactic_animator.get_end_time() != INT_MIN) {
		// This is all the pretty stuff.
		disp_.scroll_to_tile(formation_.u_->get_location(), game_display::ONSCREEN, true, true);
	
		std::vector<unit_animator::anim_elem>& animated_units = tactic_animator.animated_units();
		// 1. erase that anim.animation is NULL.
		for (std::vector<unit_animator::anim_elem>::iterator it = animated_units.begin(); it != animated_units.end();) {
			unit_animator::anim_elem& anim = *it;
			if (!anim.animation) {
				it = animated_units.erase(it);
			} else {
				++ it;
			}
		}

		// 2. combine defender
		std::vector<bool> desire_erase;
		std::vector<unit*> attacker;
		std::map<const unit*, unit_animator::anim_elem*> defender_pos;
		desire_erase.resize(animated_units.size(), false);
		size_t index = 0;
		for (std::vector<unit_animator::anim_elem>::iterator it = animated_units.begin(); it != animated_units.end(); ++ it, index ++) {
			unit_animator::anim_elem& anim = *it;

			if (std::find(defender_.begin(), defender_.end(), anim.my_unit) != defender_.end()) {
				std::map<const unit*, unit_animator::anim_elem*>::const_iterator find = defender_pos.find(anim.my_unit);
				if (find != defender_pos.end()) {
					if (!anim.text.empty()) {
						if (!find->second->text.empty()) {
							find->second->text = find->second->text + "\n" + anim.text;
						} else {
							find->second->text = anim.text;
						}
					}
					desire_erase[index] = true;
				} else {
					defender_pos.insert(std::make_pair(anim.my_unit, &anim));
				}
			} else if (anim.animation->event0() == "attack") {
				attacker.push_back(anim.my_unit);
			}
		}
		// 3. erase conflicted anim
		index = 0;
		for (std::vector<unit_animator::anim_elem>::iterator it = animated_units.begin(); it != animated_units.end(); index ++) {
			unit_animator::anim_elem& anim = *it;
			std::string e = anim.animation->event0();
			if (e != "attack" && e != "defend") {
				if (std::find(attacker.begin(), attacker.end(), anim.my_unit) != attacker.end()) {
					desire_erase[index] = true;
				} else if (defender_pos.find(anim.my_unit) != defender_pos.end()) {
					desire_erase[index] = true;
				}
			}
			if (desire_erase[index]) {
				it = animated_units.erase(it);
			} else {
				++ it;
			}
		}

		tactic_animator.start_animations();

		for (index = 0; index < attacker.size(); index ++) {
			unit& u = *attacker[index];
			u.get_animation()->update_parameters(u.get_location(), defender_[index]->get_location());
		}

		tactic_animator.wait_for_end();
		tactic_animator.set_all_standing();
	}
	tactic_animator.clear();
	defender_.clear();
}

void move_unit(const std::vector<map_location>& path, unit& u,
		const std::vector<team>& teams, bool animate,
		map_location::DIRECTION dir)
{
	bool force_scroll = (!resources::controller->is_replaying() && u.human() && animate) || preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	assert(!path.empty());
	assert(disp);
	if(!disp || disp->video().update_locked())
		return;
	rect_of_hexes& draw_area = disp->draw_area();
	// One hex path (strange), nothing to do
	if (path.size() == 1)
		return;
	if (dir == map_location::NDIRECTIONS) {
		dir = path[path.size()-2].get_relative_dir(path.back());
	}
	// Don't animate, only set facing and redraw path ends
	// if (!animate || (!force_scroll && !point_in_rect_of_hexes(path[0].x, path[0].y, draw_area) && !point_in_rect_of_hexes(path[path.size() - 1].x, path[path.size() - 1].y, draw_area))) {
	if ((!force_scroll && !point_in_rect_of_hexes(path[0].x, path[0].y, draw_area) && !point_in_rect_of_hexes(path[path.size() - 1].x, path[path.size() - 1].y, draw_area))) {
		u.set_facing(dir);
		disp->invalidate(path.front());
		disp->invalidate(path.back());
		return;
	}

	bool invisible = teams[u.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
		u.invisible(path[0]);

	bool was_hidden = u.get_hidden();

	// if (force_scroll || point_in_rect_of_hexes(path[0].x, path[0].y, draw_area) || point_in_rect_of_hexes(path[path.size() - 1].x, path[path.size() - 1].y, draw_area)) {
		// Original unit is usually hidden (but still on map, so count is correct)
		unit temp_unit = u;
		u.set_hidden(true);
		temp_unit.set_formation_flag(unit::FORMATION_NONE);
		temp_unit.set_standing(false);
		temp_unit.set_hidden(false);
		disp->place_temporary_unit(&temp_unit);
		if(!invisible) {
			// Scroll to the path, but only if it fully fits on screen.
			// If it does not fit we might be able to do a better scroll later.
			disp->scroll_to_tiles(path, game_display::ONSCREEN, true, true,0.0,false);
		}
		// We need to clear big invalidation before the move and have a smooth animation
		// (mainly black stripes and invalidation after canceling atatck dialog)
		// Two draw calls are needed to also redraw the previously invalidated hexes
		// We use update=false because we don't need delay here (no time wasted)
		// and no screen refresh (will be done by last 3rd draw() and it optimizes
		// the double blitting done by these invalidations)
		disp->draw(false);
		disp->draw(false);

		// The last draw() was still slow, and its initial new_animation_frame() call
		// is now old, so we do another draw() to get a fresh one
		// TODO: replace that by a new_animation_frame() before starting anims
		//       don't forget to change the previous draw(false) to true
		disp->draw(true);

		// extra immobile mvt anim for take-off
		temp_unit.set_location(path[0]);
		disp->invalidate(temp_unit.get_location());
		temp_unit.set_facing(path[0].get_relative_dir(path[1]));
		unit_animator animator;
		animator.add_animation(&temp_unit,"pre_movement",path[0],path[1]);
		animator.start_animations();
		animator.wait_for_end();

		for(size_t i = 0; i+1 < path.size(); ++i) {

			invisible = teams[temp_unit.side()-1].is_enemy(int(disp->viewing_team()+1)) &&
					temp_unit.invisible(path[i]) &&
					temp_unit.invisible(path[i+1]);

			if(!invisible) {
				if (!disp->tile_fully_on_screen(path[i]) || !disp->tile_fully_on_screen(path[i+1])) {
					// prevent the unit from dissappearing if we scroll here with i == 0
					temp_unit.set_location(path[i]);
					disp->invalidate(temp_unit.get_location());
					// scroll in as much of the remaining path as possible
					std::vector<map_location> remaining_path;
					for(size_t j = i; j < path.size(); ++j) {
						remaining_path.push_back(path[j]);
					}

					// temp_unit.set_location() may invalidate temp_unit.anim_, must not call pause_animation()/restart_animation().

					// temp_unit.get_animation()->pause_animation();
					disp->scroll_to_tiles(remaining_path,
								game_display::ONSCREEN, true,false,0.0,false);
					// temp_unit.get_animation()->restart_animation();

					temp_unit.set_facing(path[i].get_relative_dir(path[i + 1]));
					animator.clear();
					animator.add_animation(&temp_unit,"pre_movement",path[i],path[i + 1]);
					animator.start_animations();
					animator.wait_for_end();
				}

				if(tiles_adjacent(path[i], path[i+1])) {
					move_unit_between(path[i],path[i+1],temp_unit,i,path.size()-2-i, force_scroll);
				} else if (path[i] != path[i+1]) {
					teleport_unit_between(path[i],path[i+1],temp_unit, force_scroll);
				} else {
					continue; // no move needed
				}
			}
		}
		temp_unit.set_location(path[path.size() - 1]);
		temp_unit.set_facing(path[path.size()-2].get_relative_dir(path[path.size()-1]));
		animator.clear();
		animator.add_animation(&temp_unit,"post_movement",path[path.size()-1],map_location::null_location);
		animator.start_animations();
		animator.wait_for_end();
		disp->remove_temporary_unit();
	// }

	u.set_facing(dir);
	u.set_hidden(was_hidden);

	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	if (mousehandler) {
		mousehandler->invalidate_reachmap();
	}

	disp->invalidate(path.front());
	disp->invalidate(path.back());
}

void reset_helpers(const unit *attacker, const unit *defender, bool stronger = false);

void unit_draw_weapon(const map_location& loc, unit& attacker,
		const attack_type* attack,const attack_type* secondary_attack, const map_location& defender_loc,unit* defender)
{
	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if(!disp ||disp->video().update_locked() || disp->fogged(loc) || preferences::show_combat() == false) {
		return;
	}
	rect_of_hexes& draw_area = disp->draw_area();
	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area) || point_in_rect_of_hexes(defender_loc.x, defender_loc.y, draw_area)) {
		unit_animator animator;
		animator.add_animation(&attacker,"draw_weapon",loc,defender_loc,0,false,false,"",0,unit_animation::HIT,attack);
		animator.add_animation(defender,"draw_weapon",defender_loc,loc,0,false,false,"",0,unit_animation::MISS,secondary_attack);
		animator.start_animations();
		animator.wait_for_end();
	}
}


void unit_sheath_weapon(const map_location& primary_loc, unit* primary_unit,
		const attack_type* primary_attack,const attack_type* secondary_attack, const map_location& secondary_loc,unit* secondary_unit)
{
	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if (!disp ||disp->video().update_locked() || disp->fogged(primary_loc) || preferences::show_combat() == false) {
		return;
	}
	rect_of_hexes& draw_area = disp->draw_area();
	if (force_scroll || point_in_rect_of_hexes(primary_loc.x, primary_loc.y, draw_area) || point_in_rect_of_hexes(secondary_loc.x, secondary_loc.y, draw_area)) {
		unit_animator animator;
		if(primary_unit) {
			animator.add_animation(primary_unit,"sheath_weapon",primary_loc,secondary_loc,0,false,false,"",0,unit_animation::INVALID,primary_attack);
		}
		if(secondary_unit) {
			animator.add_animation(secondary_unit,"sheath_weapon",secondary_loc,primary_loc,0,false,false,"",0,unit_animation::INVALID,secondary_attack);
		}

		if(primary_unit || secondary_unit) {
			animator.start_animations();
			animator.wait_for_end();
		}
		if(primary_unit) {
			primary_unit->set_standing();
		}
		if(secondary_unit) {
			secondary_unit->set_standing();
		}
		reset_helpers(primary_unit,secondary_unit);
	}
}

void city_die_mh(game_display& disp, unit& loser, unit_animator& animator)
{
	if (const animation* tpl = area_anim::anim(area_anim::HSCROLL_TEXT)) {
		int id = disp.insert_area_anim(*tpl);
		animation& anim = disp.area_anim(id);

		// std::stringstream strstr;
		utils::string_map symbols;

		// symbols["city"] = tintegrate::generate_format(loser.name(), "green");
		symbols["city"] = loser.name();
		anim.replace_static_text("__text", vgettext("wesnoth-lib", "Capture $city", symbols));

		new_animation_frame();
		anim.start_animation(0);

		animator.start_animations();
		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp.delay(10);
			finished = anim.animation_finished_potential();
			finished &= animator.would_end();
		}
		disp.erase_area_anim(id);
	}
}

void unit_die(const map_location& loc, unit& loser,
		const attack_type* attack,const attack_type* secondary_attack, const map_location& winner_loc,unit* winner)
{
	bool force_scroll = (!resources::controller->is_replaying() && loser.human()) || preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if (!disp ||disp->video().update_locked() || disp->fogged(loc) || preferences::show_combat() == false) {
		return;
	}
	rect_of_hexes& draw_area = disp->draw_area();

	// winner may be NULL
	if (force_scroll || point_in_rect_of_hexes(loser.get_location().x, loser.get_location().y, draw_area)) {
		unit_animator animator;
		// hide the hp/xp bars of the loser (useless and prevent bars around an erased unit)
		animator.add_animation(&loser,"death",loc,winner_loc,0,false,false,"",0,unit_animation::KILL,attack);

		if (!loser.is_city()) {
			// but show the bars of the winner (avoid blinking and show its xp gain)
			animator.add_animation(winner,"victory",winner_loc,loc,0,true,false,"",0,
					unit_animation::KILL,secondary_attack);
			animator.start_animations();
			animator.wait_for_end();
		} else {
			city_die_mh(*disp, loser, animator);
		}

		reset_helpers(winner,&loser);
		events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
		if (mousehandler) {
			mousehandler->invalidate_reachmap();
		}
	}
}

class attack_temporary_state_lock
{
public:
	attack_temporary_state_lock(unit* u, bool stronger)
		: u_(u)
		, stronger_(stronger)
	{
		if (u_) {
			u_->set_temporary_state(unit::BIT_ATTACKING, true);
			if (stronger_) {
				u_->set_temporary_state(unit::BIT_STRONGER, true);
			}
		}
	}

	~attack_temporary_state_lock() 
	{
		if (u_) {
			u_->set_temporary_state(unit::BIT_ATTACKING, false);
			if (stronger_) {
				u_->set_temporary_state(unit::BIT_STRONGER, false);
			}
		}
	}

private:
	unit* u_;
	bool stronger_;
};

class defend_temporary_state_lock
{
public:
	defend_temporary_state_lock(const std::vector<unit*>& uvec, bool formationed)
		: uvec_(uvec)
		, formationed_(formationed)
	{
		for (std::vector<unit*>::const_iterator it = uvec_.begin(); it != uvec_.end(); it ++) {
			unit& u = **it;
			u.set_temporary_state(unit::BIT_DEFENDING, true);
			if (formationed_ && it == uvec_.begin()) {
				u.set_temporary_state(unit::BIT_STRONGER, true);
			}
		}
	}

	~defend_temporary_state_lock() 
	{
		for (std::vector<unit*>::const_iterator it = uvec_.begin(); it != uvec_.end(); it ++) {
			unit& u = **it;
			u.set_temporary_state(unit::BIT_DEFENDING, false);
			if (formationed_ && it == uvec_.begin()) {
				u.set_temporary_state(unit::BIT_STRONGER, false);
			}
		}
	}
private:
	const std::vector<unit*>& uvec_;
	bool formationed_;
};

class bit_temporary_state_lock
{
public:
	bit_temporary_state_lock(unit& u, int bit)
		: u_(u)
		, bit_(bit)
	{
		u_.set_temporary_state(bit_, true);
	}

	~bit_temporary_state_lock() 
	{
		u_.set_temporary_state(bit_, false);
	}

private:
	unit& u_;
	int bit_;
};

class release_unit_animation_lock
{
public:
	release_unit_animation_lock()
		: anims_()
	{}
	~release_unit_animation_lock()
	{
		for (std::vector<const unit_animation*>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
			delete *it;
		}
	}
	void insert(const unit_animation* anim)
	{
		anims_.push_back(anim);
	}

private:
	std::vector<const unit_animation*> anims_;
};

// 1. b_vec, damagc_vec and hit_text_vec must be same size.
// 2. first element of b_vec/damagc_vec/hit_text_vec is "center" of defender.
void unit_attack(unit& attacker, std::vector<unit*>& def_ptr_vec, std::vector<int>& damage_vec,
	const attack_type* attack, const attack_type* secondary_attack,
	int swing, std::vector<std::string>& hit_text_vec, bool drain, bool stronger, 
	const std::string& att_text, artifical* effecting_tactic, const tformation& defender_formation)
{
	const map_location& a = attacker.get_location();
	std::vector<map_location> b_vec;
	for (std::vector<unit*>::const_iterator b = def_ptr_vec.begin(); b != def_ptr_vec.end(); ++ b) {
		b_vec.push_back((*b)->get_location());
	}
	bool force_scroll = (!resources::controller->is_replaying() && attacker.human() && attacker.task() == unit::TASK_NONE) || preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	std::vector<team>& teams = *resources::teams;
	if (!disp ||disp->video().update_locked() ||
			(disp->fogged(a) && disp->fogged(b_vec[0])) || preferences::show_combat() == false) {
		return;
	}
	rect_of_hexes& draw_area = disp->draw_area();

	unit_map& units = disp->get_units();

	const size_t def_size = b_vec.size();
	bool has_human_in_defs = false;
	bool has_eyeshot_in_defs = false;

	for (size_t i = 0; i < def_size; i ++) {
		unit* def = def_ptr_vec[i];
		map_location& b = b_vec[i];
		if (!has_human_in_defs) {
			if (def->human()) {
				has_human_in_defs = true;
			} else if (def->is_artifical()) {
				if (rpg::stratum == hero_stratum_leader) {
					if (def->side() == rpg::h->side_ + 1) {
						has_human_in_defs = true;
					}
				} else if (rpg::stratum == hero_stratum_mayor) {
					if (def->cityno() == rpg::h->city_) {
						has_human_in_defs = true;
					}
				}
			}
		}
		if (!has_eyeshot_in_defs && point_in_rect_of_hexes(b.x, b.y, draw_area)) {
			has_eyeshot_in_defs = true;
		}
	}

	if (!resources::controller->is_replaying() && has_human_in_defs) {
		force_scroll = true;
	}
	if (force_scroll || point_in_rect_of_hexes(attacker.get_location().x, attacker.get_location().y, draw_area) || has_eyeshot_in_defs) {
		// clear global draw_desc flag
		unit::draw_desc_ = false;
		const attack_temporary_state_lock lock1(&attacker, stronger);
		const defend_temporary_state_lock lock2(def_ptr_vec, defender_formation.valid(false));
		const attack_temporary_state_lock lock3(effecting_tactic, true);

		if (stronger) {
			sound::play_sound("stronger.wav");
		}

		disp->select_hex(map_location::null_location);

		// scroll such that there is at least half a hex spacing around fighters
		disp->scroll_to_tiles(a, b_vec[0], game_display::ONSCREEN, true, 0.5, force_scroll);

		// do a copy so we can change the caracteristics
		// unit::redraw_unit is different from artifcal::redraw_unit, left call "right" protol-member according type!
		std::vector<int> def_hitpoints_vec;
		for (size_t i = 0; i < def_size; i ++) {
			def_hitpoints_vec.push_back(def_ptr_vec[i]->hitpoints());
		}

		attacker.set_facing(a.get_relative_dir(b_vec[0]));
		for (size_t i = 0; i < def_size; i ++) {
			def_ptr_vec[i]->set_facing(b_vec[i].get_relative_dir(a));
		}

		unit_animator normal_animator;
		unit_animator* animator = &normal_animator;
		if (cast_tactic_state) {
			animator = &tactic_animator;
		}

		unit_ability_list leaders;
		unit_ability_list encouragers;
		if (attack) {
			leaders = attacker.get_abilities("leadership");
			if (stronger) {
				encouragers = attacker.get_abilities("encourage");
			}
		}
		unit_ability_list helpers = def_ptr_vec[0]->get_abilities("resistance");
		// helpers_to indicate which defender does resistance in helpers map to.
		std::vector<size_t> helpers_to;
		if (!helpers.empty()) {
			helpers_to.resize(helpers.cfgs.size());
			std::fill_n(helpers_to.begin(), helpers.cfgs.size(), 0);
		}
		for (size_t i = 1; i < def_size; i ++) {
			unit_ability_list helpers_i = def_ptr_vec[i]->get_abilities("resistance");
			if (!helpers_i.empty()) {
				size_t old_helpers_size = helpers.cfgs.size(); 
				helpers.merge(helpers_i);
				if (old_helpers_size != helpers.cfgs.size()) {
					helpers_to.resize(helpers.cfgs.size());
					std::fill_n(helpers_to.begin() + old_helpers_size, helpers.cfgs.size() - old_helpers_size, i);
				}
			}
		}

		std::vector<std::string> text_vec;
		for (size_t i = 0; i < def_size; i ++) {
			std::string text;
			if (damage_vec[i]) text = lexical_cast<std::string>(damage_vec[i]);
			if (!hit_text_vec[i].empty()) {
				text.insert(text.begin(), hit_text_vec[i].size()/2, ' ');
				text = text + "\n" + hit_text_vec[i];
			}
			text_vec.push_back(text);
		}

		std::string text_2 ;
		if (drain && damage_vec[0]) text_2 = lexical_cast<std::string>(std::min<int>(damage_vec[0], def_ptr_vec[0]->hitpoints()) / 4);
		if (!att_text.empty()) {
			text_2.insert(text_2.begin(), att_text.size()/2, ' ');
			text_2 = text_2 + "\n" + att_text;
		}

		std::set<const unit*> animated_units;
		std::vector<unit_animation::hit_type> hit_type_vec;
		for (size_t i = 0; i < def_size; i ++) {
			unit_animation::hit_type hit_type;
			if (damage_vec[i] >= def_ptr_vec[i]->hitpoints()) {
				hit_type = unit_animation::KILL;
			} else if (damage_vec[i] > 0) {
				hit_type = unit_animation::HIT;
			} else {
				hit_type = unit_animation::MISS;
			}
			hit_type_vec.push_back(hit_type);
		}
		animator->add_animation(&attacker, "attack", a,
			def_ptr_vec[0]->get_location(), damage_vec[0], true, false, text_2,
			display::rgb(0, 255, 0), hit_type_vec[0], attack, null_str,
			swing);
		animated_units.insert(&attacker);

		// note that we take an anim from the real unit, we'll use it later
		std::vector<const unit_animation*> defender_anim_vec;
		for (size_t i = 0; i < def_size; i ++) {
			const unit_animation* defender_anim = def_ptr_vec[i]->choose_animation(*disp,
				def_ptr_vec[i]->get_location(), "defend", a, damage_vec[0],
				hit_type_vec[i], secondary_attack, attack? attack->type(): null_str, swing);
			animator->add_animation(def_ptr_vec[i], defender_anim, def_ptr_vec[i]->get_location(),
				true, false, text_vec[i], display::rgb(255, 0, 0));
			defender_anim_vec.push_back(defender_anim);

			animated_units.insert(def_ptr_vec[i]);
		}

		if (defender_formation.valid(false)) {
			std::string text;
			std::vector<unit*>::iterator find = std::find(def_ptr_vec.begin(), def_ptr_vec.end(), defender_formation.u_);
			if (find != def_ptr_vec.end()) {
				text = text_vec[std::distance(def_ptr_vec.begin(), find)];
			}
			const unit_animation* heal_anim = unit_animation::share_anim(area_anim::FORMATION_DEFEND);
			if (heal_anim) {
				animator->add_animation(defender_formation.u_, heal_anim, defender_formation.u_->get_location(), 
					!text.empty(), false, text, display::rgb(255, 0, 0));
				animated_units.insert(defender_formation.u_);
			}
		}

		for (std::vector<std::pair<const config *, unit *> >::iterator itor = leaders.cfgs.begin(); itor != leaders.cfgs.end(); ++itor) {
			if (animated_units.find(itor->second) != animated_units.end()) {
				continue;
			}
			// if (itor->second == &attacker) continue;
			// if (std::find(def_ptr_vec.begin(), def_ptr_vec.end(), itor->second) != def_ptr_vec.end()) continue;
			unit* leader = itor->second;
			leader->set_facing(leader->get_location().get_relative_dir(a));
			animator->add_animation(leader, "leading", leader->get_location(),
				a, damage_vec[0], true, false, "", 0,
				hit_type_vec[0], attack, null_str, swing);

			animated_units.insert(leader);
		}
		size_t helper_cfgs_index = 0;
		for (std::vector<std::pair<const config *, unit *> >::iterator itor = helpers.cfgs.begin(); itor != helpers.cfgs.end(); ++itor, helper_cfgs_index ++) {
			if (animated_units.find(itor->second) != animated_units.end()) {
				continue;
			}
			// if (itor->second == &attacker) continue;
			// if (std::find(def_ptr_vec.begin(), def_ptr_vec.end(), itor->second) != def_ptr_vec.end()) continue;
			unit* helper = itor->second;
			size_t mapped_defend_index = helpers_to[helper_cfgs_index];
			helper->set_facing(helper->get_location().get_relative_dir(b_vec[mapped_defend_index]));
			animator->add_animation(helper, "resistance", helper->get_location(),
				def_ptr_vec[mapped_defend_index]->get_location(), damage_vec[mapped_defend_index], true, false, "", 0,
				hit_type_vec[mapped_defend_index], attack, null_str, swing);
			animated_units.insert(helper);
		}
		for (std::vector<std::pair<const config *, unit *> >::iterator itor = encouragers.cfgs.begin(); itor != encouragers.cfgs.end(); ++itor) {
			if (animated_units.find(itor->second) != animated_units.end()) {
				continue;
			}
			// if (itor->second == &attacker) continue;
			// if (std::find(def_ptr_vec.begin(), def_ptr_vec.end(), itor->second) != def_ptr_vec.end()) continue;
			unit* encourager = itor->second;
			encourager->set_facing(encourager->get_location().get_relative_dir(a));
			animator->add_animation(encourager, "leading", encourager->get_location(),
				a, damage_vec[0], true, false, "", 0,
				hit_type_vec[0], attack, null_str, swing);
			animated_units.insert(encourager);
		}

		if (!cast_tactic_state) {
			normal_animator.start_animations();
			// to attack animation, need update loc of dst. dst maybe distance one or more grid!
			attacker.get_animation()->update_parameters(a, b_vec[0]);

			normal_animator.wait_until(0);
			std::vector<int> damage_left = damage_vec;
			// why use this while? --make HP bare damage more smooth.
			while (damage_left[0] > 0 && !normal_animator.would_end()) {
				int step_left = (normal_animator.get_end_time() - normal_animator.get_animation_time() )/50;
				if (step_left < 1) step_left = 1;
				for (size_t i = 0; i < def_size; i ++) {
					int removed_hp =  damage_left[i] / step_left ;
					if (removed_hp < 1) removed_hp = 1;
					def_ptr_vec[i]->take_hit(removed_hp);
					damage_left[i] -= removed_hp;
				}
				normal_animator.wait_until(normal_animator.get_animation_time_potential() + 50);
			}

			normal_animator.wait_for_end();

			// pass the animation back to the real unit
			for (size_t i = 0; i < def_size; i ++) {
				// by far, I cannot understand why call start_animation
				// def_ptr_vec[i]->start_animation(animator.get_end_time(), defender_anim_vec[i], true);
				reset_helpers(i == 0? &attacker: NULL, def_ptr_vec[i], stronger);
				def_ptr_vec[i]->set_hitpoints(def_hitpoints_vec[i]);
				if (i) {
					def_ptr_vec[i]->set_standing();
				}
			}

			if (defender_formation.valid(false)) {
				defender_formation.u_->set_standing();
			}
		}

		// set global draw_desc flag
		unit::draw_desc_ = true;
	}
}

void unit_attack2(unit* attacker, const std::string& type, std::vector<unit*>& def_ptr_vec, std::vector<int>& damage_vec,
	 std::vector<std::string>& hit_text_vec)
{
	if (def_ptr_vec.empty()) return;

	const map_location& a = attacker? attacker->get_location(): map_location::null_location;
	std::vector<map_location> b_vec;
	for (std::vector<unit*>::const_iterator b = def_ptr_vec.begin(); b != def_ptr_vec.end(); ++ b) {
		b_vec.push_back((*b)->get_location());
	}
	bool force_scroll = (!resources::controller->is_replaying() && attacker && attacker->human() && attacker->task() == unit::TASK_NONE) || preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	std::vector<team>& teams = *resources::teams;
	if (!disp ||disp->video().update_locked() ||
			((!attacker || disp->fogged(a)) && disp->fogged(b_vec[0])) || preferences::show_combat() == false) {
		return;
	}
	rect_of_hexes& draw_area = disp->draw_area();

	unit_map& units = disp->get_units();

	const size_t def_size = b_vec.size();
	bool has_human_in_defs = false;
	bool has_eyeshot_in_defs = false;
	map_location scroll_to_loc = b_vec[0];

	for (size_t i = 0; i < def_size; i ++) {
		unit* def = def_ptr_vec[i];
		map_location& b = b_vec[i];
		if (!has_human_in_defs) {
			if (def->human()) {
				has_human_in_defs = true;
				scroll_to_loc = b;
			} else if (def->is_artifical()) {
				if (rpg::stratum == hero_stratum_leader) {
					if (def->side() == rpg::h->side_ + 1) {
						has_human_in_defs = true;
						scroll_to_loc = b;
					}
				} else if (rpg::stratum == hero_stratum_mayor) {
					if (def->cityno() == rpg::h->city_) {
						has_human_in_defs = true;
						scroll_to_loc = b;
					}
				}
			}
		}
		if (!has_eyeshot_in_defs && point_in_rect_of_hexes(b.x, b.y, draw_area)) {
			has_eyeshot_in_defs = true;
			scroll_to_loc = b;
		}
	}

	if (!resources::controller->is_replaying() && has_human_in_defs) {
		force_scroll = true;
	}
	bool attacker_in_eyeshot = false;
	if (attacker && point_in_rect_of_hexes(a.x, a.y, draw_area)) {
		attacker_in_eyeshot = true;
	}
	if (force_scroll || attacker_in_eyeshot || has_eyeshot_in_defs) {
		// clear global draw_desc flag
		unit::draw_desc_ = false;
		const attack_temporary_state_lock lock1(attacker, false);
		const defend_temporary_state_lock lock2(def_ptr_vec, false);

		disp->select_hex(map_location::null_location);

		// scroll such that there is at least half a hex spacing around fighters
		if (attacker) {
			disp->scroll_to_tiles(a, scroll_to_loc, game_display::ONSCREEN, true, 0.5, force_scroll);
		} else {
			disp->scroll_to_tile(scroll_to_loc, game_display::ONSCREEN, true, force_scroll);
		}

		// do a copy so we can change the caracteristics
		// unit::redraw_unit is different from artifcal::redraw_unit, left call "right" protol-member according type!
		std::vector<int> def_hitpoints_vec;
		for (size_t i = 0; i < def_size; i ++) {
			def_hitpoints_vec.push_back(def_ptr_vec[i]->hitpoints());
		}

		if (attacker) {
			attacker->set_facing(a.get_relative_dir(b_vec[0]));
			for (size_t i = 0; i < def_size; i ++) {
				def_ptr_vec[i]->set_facing(b_vec[i].get_relative_dir(a));
			}
		}

		unit_animator animator;

		unit_ability_list helpers = def_ptr_vec[0]->get_abilities("resistance");
		// helpers_to indicate which defender does resistance in helpers map to.
		std::vector<size_t> helpers_to;
		if (!helpers.empty()) {
			helpers_to.resize(helpers.cfgs.size());
			std::fill_n(helpers_to.begin(), helpers.cfgs.size(), 0);
		}
		for (size_t i = 1; i < def_size; i ++) {
			unit_ability_list helpers_i = def_ptr_vec[i]->get_abilities("resistance");
			if (!helpers_i.empty()) {
				size_t old_helpers_size = helpers.cfgs.size(); 
				helpers.merge(helpers_i);
				if (old_helpers_size != helpers.cfgs.size()) {
					helpers_to.resize(helpers.cfgs.size());
					std::fill_n(helpers_to.begin() + old_helpers_size, helpers.cfgs.size() - old_helpers_size, i);
				}
			}
		}

		std::vector<std::string> text_vec;
		for (size_t i = 0; i < def_size; i ++) {
			std::string text;
			if (damage_vec[i]) text = lexical_cast<std::string>(damage_vec[i]);
			if (!hit_text_vec[i].empty()) {
				text.insert(text.begin(), hit_text_vec[i].size()/2, ' ');
				text = text + "\n" + hit_text_vec[i];
			}
			text_vec.push_back(text);
		}

		std::set<unit*> animated_units;
		std::vector<unit_animation::hit_type> hit_type_vec;
		for (size_t i = 0; i < def_size; i ++) {
			unit_animation::hit_type hit_type;
			if (damage_vec[i] >= def_ptr_vec[i]->hitpoints()) {
				hit_type = unit_animation::KILL;
			} else if (damage_vec[i] > 0) {
				hit_type = unit_animation::HIT;
			} else {
				hit_type = unit_animation::MISS;
			}
			hit_type_vec.push_back(hit_type);
		}

		if (attacker) {
			const unit_animation* attacker_anim = unit_animation::share_anim(area_anim::MAGIC);
			if (attacker_anim) {
				animator.add_animation(attacker, attacker_anim);
				animated_units.insert(attacker);
			}
		}

		// note that we take an anim from the real unit, we'll use it later
		std::vector<const unit_animation*> defender_anim_vec;
		const unit_animation* defender_anim = NULL;
		if (type == "blade" || type == "pierce" || type == "impact") {
			defender_anim = unit_animation::share_anim(area_anim::BLADE);
		} else {
			defender_anim = unit_animation::share_anim(area_anim::FIRE);
		}
		for (size_t i = 0; i < def_size; i ++) {
			unit* defender = def_ptr_vec[i];
			if (!defender_anim) {
				defender_anim = defender->choose_animation(*disp,
					defender->get_location(), "defend", a, damage_vec[0],
					hit_type_vec[i], NULL, type);
			}

			animator.add_animation(defender, defender_anim, defender->get_location(),
				true, false, text_vec[i], display::rgb(255, 0, 0));
			defender_anim_vec.push_back(defender_anim);

			animated_units.insert(defender);
		}

		size_t helper_cfgs_index = 0;
		for (std::vector<std::pair<const config *, unit *> >::iterator itor = helpers.cfgs.begin(); itor != helpers.cfgs.end(); ++itor, helper_cfgs_index ++) {
			if (animated_units.find(itor->second) != animated_units.end()) {
				continue;
			}
			unit* helper = itor->second;
			size_t mapped_defend_index = helpers_to[helper_cfgs_index];
			helper->set_facing(helper->get_location().get_relative_dir(b_vec[mapped_defend_index]));
			animator.add_animation(helper, "resistance", helper->get_location(),
				def_ptr_vec[mapped_defend_index]->get_location(), damage_vec[mapped_defend_index], true, false, "", 0,
				hit_type_vec[mapped_defend_index], NULL);
			animated_units.insert(helper);
		}

		animator.start_animations();
		if (attacker) {
			// to attack animation, need update loc of dst. dst maybe distance one or more grid!
			attacker->get_animation()->update_parameters(a, b_vec[0]);
		}

		animator.wait_until(0);
		std::vector<int> damage_left = damage_vec;
		// why use this while? --make HP bare damage more smooth.
		while (damage_left[0] > 0 && !animator.would_end()) {
			int step_left = (animator.get_end_time() - animator.get_animation_time() )/50;
			if (step_left < 1) step_left = 1;
			for (size_t i = 0; i < def_size; i ++) {
				int removed_hp =  damage_left[i] / step_left ;
				if (removed_hp < 1) removed_hp = 1;
				def_ptr_vec[i]->take_hit(removed_hp);
				damage_left[i] -= removed_hp;
			}
			animator.wait_until(animator.get_animation_time_potential() + 50);
		}

		animator.wait_for_end();

		// pass the animation back to the real unit
		for (std::set<unit*>::const_iterator it = animated_units.begin(); it != animated_units.end(); ++ it) {
			unit& u = **it;
			u.set_standing();
		}

		// remember! recove hit_point of defender!
		for (size_t i = 0; i < def_size; i ++) {
			// by far, I cannot understand why call start_animation
			def_ptr_vec[i]->set_hitpoints(def_hitpoints_vec[i]);
		}

		// set global draw_desc flag
		unit::draw_desc_ = true;
	}
}

void unit_heal2(std::vector<team>& teams, unit_map& units, unit* doctor, unit& patient, int healing)
{
	if (!healing) {
		return;
	}
	const map_location& doctor_loc = doctor? doctor->get_location(): map_location::null_location;
	const map_location& patient_loc = patient.get_location();
	
	bool force_scroll = preferences::scroll_to_action();
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked()) {
		return;
	}
	const rect_of_hexes& draw_area = disp->draw_area();
	bool toucher_in_eyeshot = point_in_rect_of_hexes(doctor_loc.x, doctor_loc.y, draw_area) || point_in_rect_of_hexes(patient_loc.x, patient_loc.y, draw_area);

	if (force_scroll || toucher_in_eyeshot) {
		// clear global draw_desc flag
		unit::draw_desc_ = false;
		const attack_temporary_state_lock lock1(&patient, false);

		disp->select_hex(map_location::null_location);

		// scroll such that there is at least half a hex spacing around fighters
		if (force_scroll) {
			disp->scroll_to_tile(patient_loc, game_display::ONSCREEN, true, true);
		}

		std::set<unit*> animated_units;
		unit_animator animator;

		const unit_animation* heal_anim = unit_animation::share_anim(area_anim::HEAL);
		if (heal_anim) {
			animator.add_animation(&patient, heal_anim, patient_loc, false, false, lexical_cast<std::string>(healing), display::rgb(0, 255, 0));
			animated_units.insert(&patient);
		}

		animator.start_animations();
		if (heal_anim) {
			patient.get_animation()->update_parameters(patient_loc, doctor_loc);
		}

		animator.wait_for_end();

		// pass the animation back to the real unit
		for (std::set<unit*>::const_iterator it = animated_units.begin(); it != animated_units.end(); ++ it) {
			unit& u = **it;
			u.set_standing();
		}

		// set global draw_desc flag
		unit::draw_desc_ = true;
	}
}

void unit_destruct(std::vector<team>& teams, unit_map& units, unit* attacker, artifical& defender, const std::string& text, artifical* effecting_tactic)
{
	const map_location& attacker_loc = attacker? attacker->get_location(): map_location::null_location;
	const map_location& defender_loc = defender.get_location();
	
	bool force_scroll = preferences::scroll_to_action();
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked()) {
		return;
	}
	const rect_of_hexes& draw_area = disp->draw_area();
	bool toucher_in_eyeshot = point_in_rect_of_hexes(attacker_loc.x, attacker_loc.y, draw_area) || point_in_rect_of_hexes(defender_loc.x, defender_loc.y, draw_area);

	if (force_scroll || toucher_in_eyeshot) {
		// clear global draw_desc flag
		unit::draw_desc_ = false;
		const attack_temporary_state_lock lock1(effecting_tactic, true);

		disp->select_hex(map_location::null_location);

		// scroll such that there is at least half a hex spacing around fighters
		if (force_scroll) {
			disp->scroll_to_tile(defender_loc, game_display::ONSCREEN, true, true);
		}

		std::set<unit*> animated_units;
		unit_animator animator;

		const unit_animation* destruct_anim = unit_animation::share_anim(area_anim::DESTRUCT);
		if (destruct_anim) {
			animator.add_animation(&defender, destruct_anim, defender.get_location(), false, false, text, display::rgb(255, 0, 0));
			animated_units.insert(&defender);
		}

		animator.start_animations();
		if (destruct_anim) {
			defender.get_animation()->update_parameters(defender_loc, attacker_loc);
		}

		animator.wait_for_end();

		// pass the animation back to the real unit
		for (std::set<unit*>::const_iterator it = animated_units.begin(); it != animated_units.end(); ++ it) {
			unit& u = **it;
			u.set_standing();
		}

		// set global draw_desc flag
		unit::draw_desc_ = true;
	}
}

// private helper function, set all helpers to default position
void reset_helpers(const unit *attacker,const unit *defender, bool stronger)
{
	game_display* disp = game_display::get_singleton();
	unit_map& units = disp->get_units();
	if (attacker) {
		unit_ability_list leaders = attacker->get_abilities("leadership");
		for (std::vector<std::pair<const config *, unit *> >::iterator itor = leaders.cfgs.begin(); itor != leaders.cfgs.end(); ++itor) {
			unit* leader = itor->second;
			leader->set_standing();
		}
		if (stronger) {
			unit_ability_list encouragers = attacker->get_abilities("encourage");
			for (std::vector<std::pair<const config *, unit *> >::iterator itor = encouragers.cfgs.begin(); itor != encouragers.cfgs.end(); ++itor) {
				unit* encourager = itor->second;
				encourager->set_standing();
			}
		}
	}

	if(defender) {
		unit_ability_list helpers = defender->get_abilities("resistance");
		for (std::vector<std::pair<const config *, unit *> >::iterator itor = helpers.cfgs.begin(); itor != helpers.cfgs.end(); ++itor) {
			unit* helper = itor->second;
			helper->set_standing();
		}
	}
}

void unit_recruited(const map_location& loc,const map_location& leader_loc)
{
	const events::command_disabler disable_commands;

	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(loc)) return;
	unit_map::iterator u = disp->get_units().find(loc);
	if(u == disp->get_units().end()) return;
	u->set_hidden(true);

	rect_of_hexes& draw_area = disp->draw_area();
	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area) || point_in_rect_of_hexes(leader_loc.x, leader_loc.y, draw_area)) {
		unit_animator animator;
		if(leader_loc != map_location::null_location) {
			unit_map::iterator leader = disp->get_units().find(leader_loc);
			if(leader == disp->get_units().end()) return;
			disp->scroll_to_tiles(loc,leader_loc,game_display::ONSCREEN,true,0.0,false);
			leader->set_facing(leader_loc.get_relative_dir(loc));
			animator.add_animation(&*leader, "recruiting", leader_loc, loc, 0, true);
		} else {
			disp->scroll_to_tile(loc, game_display::ONSCREEN, true, true);
		}

		disp->draw();
		u->set_hidden(false);
		u->set_facing(static_cast<map_location::DIRECTION>(rand()%map_location::NDIRECTIONS));
		animator.add_animation(&*u, "recruited", loc, leader_loc);
		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
		if (loc==disp->mouseover_hex()) disp->invalidate_unit();
	}
}

void unit_healing(unit& healed, const map_location& healed_loc, const std::vector<unit *> &healers, int healing)
{

	const events::command_disabler disable_commands;

	if (healing == 0) {
		return;
	}
	// To human player, I think it isn't necessary to display all heal animation.
	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if(!disp || disp->video().update_locked() || disp->fogged(healed_loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();
	if (force_scroll || point_in_rect_of_hexes(healed_loc.x, healed_loc.y, draw_area)) {
		// This is all the pretty stuff.
		disp->scroll_to_tile(healed_loc, game_display::ONSCREEN, true, true);
		disp->display_unit_hex(healed_loc);
		unit_animator animator;
		// const bit_temporary_state_lock lock(healed, unit::BIT_STRONGER);

		BOOST_FOREACH (unit *h, healers) {
			h->set_facing(h->get_location().get_relative_dir(healed_loc));
			animator.add_animation(h, "healing", h->get_location(),
				healed_loc, healing);
		}
		if (healing < 0) {
			animator.add_animation(&healed,"poisoned",healed_loc,map_location::null_location,-healing,false,false,lexical_cast<std::string>(-healing), display::rgb(255,0,0));
		} else {
			animator.add_animation(&healed,"healed",healed_loc,map_location::null_location,healing,false,false,lexical_cast<std::string>(healing), display::rgb(0,255,0));
		}
		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
	}
}

void unit_build(artifical& art)
{
	const map_location& art_loc = art.get_location();

	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || disp->fogged(art_loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();

	if (force_scroll || point_in_rect_of_hexes(art_loc.x, art_loc.y, draw_area)) {
		// This is all the pretty stuff.
		disp->scroll_to_tile(art_loc, game_display::ONSCREEN, true, true);
		disp->display_unit_hex(art_loc);
		unit_animator animator;

		animator.add_animation(&art, "build", art_loc, map_location::null_location, 0, false, false, null_str, display::rgb(0, 255, 0));

		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
	}
}

void unit_repair(artifical& art, int healing)
{
	if (healing <= 0) return;
	// To human player, I think it isn't necessary to display all heal animation.
	const map_location& art_loc = art.get_location();

	bool force_scroll = preferences::scroll_to_action()? true: false;
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || disp->fogged(art_loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();

	if (force_scroll || point_in_rect_of_hexes(art_loc.x, art_loc.y, draw_area)) {
		// This is all the pretty stuff.
		disp->scroll_to_tile(art_loc, game_display::ONSCREEN, true, true);
		disp->display_unit_hex(art_loc);
		unit_animator animator;

		animator.add_animation(&art, "repair", art_loc, map_location::null_location, healing, false, false, lexical_cast<std::string>(healing), display::rgb(0,255,0));

		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
	}
}

void wml_animation_internal(unit_animator &animator, const vconfig &cfg, const map_location &default_location = map_location::null_location);

void wml_animation(const vconfig &cfg, const map_location &default_location)
{
	game_display &disp = *resources::screen;
	if (disp.video().update_locked()) return;
	unit_animator animator;
	wml_animation_internal(animator, cfg, default_location);
	animator.start_animations();
	animator.wait_for_end();
	animator.set_all_standing();
}

void wml_animation_internal(unit_animator &animator, const vconfig &cfg, const map_location &default_location)
{
	unit_map::iterator u = resources::units->find(default_location);

	// Search for a valid unit filter,
	// and if we have one, look for the matching unit
	vconfig filter = cfg.child("filter");
	if(!filter.null()) {
		for (u = resources::units->begin(); u != resources::units->end(); ++u) {
			if (game_events::unit_matches_filter(*u, filter))
				break;
		}
	}

	// We have found a unit that matches the filter
	if (u.valid() && !resources::screen->fogged(u->get_location()))
	{
		attack_type *primary = NULL;
		attack_type *secondary = NULL;
		Uint32 text_color;
		unit_animation::hit_type hits=  unit_animation::INVALID;
		std::vector<attack_type> attacks = u->attacks();
		std::vector<attack_type>::iterator itor;

		filter = cfg.child("primary_attack");
		if(!filter.null()) {
			for(itor = attacks.begin(); itor != attacks.end(); ++itor){
				if(itor->matches_filter(filter.get_parsed_config())) {
					primary = &*itor;
					break;
				}
			}
		}

		filter = cfg.child("secondary_attack");
		if(!filter.null()) {
			for(itor = attacks.begin(); itor != attacks.end(); ++itor){
				if(itor->matches_filter(filter.get_parsed_config())) {
					secondary = &*itor;
					break;
				}
			}
		}

		if(cfg["hits"] == "yes" || cfg["hits"] == "hit") {
			hits = unit_animation::HIT;
		}
		if(cfg["hits"] == "no" || cfg["hits"] == "miss") {
			hits = unit_animation::MISS;
		}
		if( cfg["hits"] == "kill" ) {
			hits = unit_animation::KILL;
		}
		if(cfg["red"].empty() && cfg["green"].empty() && cfg["blue"].empty()) {
			text_color = display::rgb(0xff,0xff,0xff);
		} else {
			text_color = display::rgb(cfg["red"], cfg["green"], cfg["blue"]);
		}
		resources::screen->scroll_to_tile(u->get_location(), game_display::ONSCREEN, true, false);
		vconfig t_filter = cfg.child("facing");
		map_location secondary_loc = map_location::null_location;
		if(!t_filter.empty()) {
			terrain_filter filter(t_filter, *resources::units);
			std::set<map_location> locs;
			filter.get_locations(locs);
			if (!locs.empty() && u->get_location() != *locs.begin()) {
				map_location::DIRECTION dir =u->get_location().get_relative_dir(*locs.begin());
				u->set_facing(dir);
				secondary_loc = u->get_location().get_direction(dir);
			}
		}
		animator.add_animation(&*u, cfg["flag"], u->get_location(),
			secondary_loc, cfg["value"], cfg["with_bars"].to_bool(), false,
			cfg["text"], text_color, hits, primary, null_str,
			cfg["value_second"]);
	}
	const vconfig::child_list sub_anims = cfg.get_children("animate");
	vconfig::child_list::const_iterator anim_itor;
	for(anim_itor = sub_anims.begin(); anim_itor != sub_anims.end();++anim_itor) {
		wml_animation_internal(animator, *anim_itor);
	}

}

void card_start(card_map& cards, const card& c)
{
	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(area_anim::CARD)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);

		anim.replace_image_name("__id.png", c.image());
		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

void tactic_start(hero& h)
{

	const ttactic& t = unit_types.tactic(h.tactic_);

	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(area_anim::TACTIC)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);

		anim.replace_image_name("__bg.png", t.bg_image());
		anim.replace_image_name("__id.png", h.image(true));
		anim.replace_static_text("__hero", h.name());
		anim.replace_static_text("__tactic", t.name());
		anim.replace_static_text("__description", t.description());

		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

void formation_attack_start(const tformation& formation)
{

	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(area_anim::FORMATION_ATTACK)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);
		std::stringstream strstr;

		const tformation_profile& profile = *formation.profile_;
		
		anim.replace_image_name("__bg.png", profile.bg_image());
		anim.replace_image_name("__id.png", formation.u_->master().image(true));
		anim.replace_static_text("__formation_name", profile.name());
		anim.replace_static_text("__formation_description", profile.description());
		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

void global_anim_2(int type, const std::string& id1, const std::string& id2, const std::string& text, const SDL_Color& color)
{

	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(type)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);

		anim.replace_image_name("__id1.png", id1);
		anim.replace_image_name("__id2.png", id2);
		anim.replace_static_text("__text", text);
		if (color != font::GOOD_COLOR) {
			anim.replace_int("text_color", display::rgb(0, 255, 0), display::rgb(color.r, color.g, color.b));
		}
		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

void perfect_anim()
{
	const events::command_disabler disable_commands;

	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(area_anim::PERFECT)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);
		std::stringstream strstr;

		anim.replace_static_text("__perfect", dsgettext("wesnoth-lib", "perfect^Perfect"));

		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

int start_title_screen_anim()
{
	game_display* disp = game_display::get_singleton();
	int id = -1;
	if (const animation* tpl = area_anim::anim(area_anim::TITLE_SCREEN)) {
		id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);
		std::stringstream strstr;

		// anim.replace_static_text("__perfect", dsgettext("wesnoth-lib", "perfect^Perfect"));

		new_animation_frame();
		anim.start_animation(0);
	}
	return id;
}

void stratagem_anim(const ttechnology& stratagem, const std::string& image, bool up)
{
	const events::command_disabler disable_commands;

	game_display* disp = game_display::get_singleton();
	if (const animation* tpl = area_anim::anim(up? area_anim::STRATAGEM_UP: area_anim::STRATAGEM_DOWN)) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);
		std::stringstream strstr;

		anim.replace_static_text("__name", stratagem.name());
		anim.replace_image_name("__image", image);

		new_animation_frame();
		anim.start_animation(0);

		// wait_until
		bool finished = false;
		while (!finished) {
			resources::controller->play_slice(false);
			disp->delay(10);
			finished = anim.animation_finished_potential();
		}
		disp->erase_area_anim(id);
	}
}

void unit_touching(unit& u, std::vector<unit *>& touchers, int touching, const std::string& prefix)
{

	if (touching == 0) return;
	bool force_scroll = preferences::scroll_to_action();
	const map_location& loc = u.get_location();
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || disp->fogged(loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();
	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
		// This is all the pretty stuff.
		disp->scroll_to_tile(loc, game_display::ONSCREEN, true, true);
		disp->display_unit_hex(loc);
		unit_animator animator;

		std::stringstream str;
		BOOST_FOREACH (unit *h, touchers) {
			str.str("");
			str << prefix;
			if (touching < 0) {
				str << touching;
				animator.add_animation(h, "poisoned", h->get_location(), map_location::null_location, -touching, false, false, str.str(), display::rgb(255,0,0));
			} else {
				str << "+" << touching;
				animator.add_animation(h, "healed", h->get_location(), map_location::null_location, touching, false, false, str.str(), display::rgb(0,255,0));
			}
		}
		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
	}
}

void unit_text(unit& u, bool poisoned, const std::string& text)
{

	const events::command_disabler disable_commands;

	bool force_scroll = preferences::scroll_to_action();
	const map_location& loc = u.get_location();
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || disp->fogged(loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();

	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
		// This is all the pretty stuff.
		if (!cast_tactic_state) {
			disp->scroll_to_tile(loc, game_display::ONSCREEN, true, true);
			disp->display_unit_hex(loc);
			unit_animator animator;

			if (poisoned) {
				animator.add_animation(&u, "poisoned", loc, map_location::null_location, 10, false, false, text, display::rgb(255,0,0));
			} else {
				animator.add_animation(&u, "healed", loc, map_location::null_location, 5, false, false, text, display::rgb(0,255,0));
			}

			animator.start_animations();
			animator.wait_for_end();
			animator.set_all_standing();
		} else {
			if (poisoned) {
				tactic_animator.add_animation(&u, "poisoned", loc, map_location::null_location, 10, false, false, text, display::rgb(255,0,0));
			} else {
				tactic_animator.add_animation(&u, "healed", loc, map_location::null_location, 5, false, false, text, display::rgb(0,255,0));
			}
		}
	}
}

void unit_income(unit& u, int income)
{
	const events::command_disabler disable_commands;

	const unit_animation* income_anim_tpl = unit_animation::share_anim(area_anim::INCOME);
	if (!income_anim_tpl) {
		return;
	}

	unit_animation income_anim = *income_anim_tpl;

	bool force_scroll = u.human() || preferences::scroll_to_action()? true: false;
	const map_location& loc = u.get_location();
	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked() || disp->fogged(loc)) return;
	rect_of_hexes& draw_area = disp->draw_area();

	const gui2::twidget* widget = disp->get_theme_report(gui2::tgame_theme::GOLD);
	const SDL_Rect& rect = widget->get_rect();

	int xoffset = rect.x - disp->get_location_x(loc);
	int yoffset = rect.y - disp->get_location_y(loc);
	std::stringstream strstr;
	strstr.str("");
	strstr << "0~" << xoffset;
	income_anim.replace_progressive("x", "5555", strstr.str());
	income_anim.replace_progressive("x", "6666", str_cast(xoffset));
	strstr.str("");
	strstr << "-60~" << yoffset;
	income_anim.replace_progressive("y", "5555", strstr.str());
	income_anim.replace_progressive("y", "6666", str_cast(yoffset));

	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
		// This is all the pretty stuff.
		std::string text = str_cast(income);
		Uint32 color = income >= 0? display::rgb(0, 255, 0): display::rgb(255, 0, 0);
		
		disp->scroll_to_tile(loc, game_display::ONSCREEN, true, true);
		disp->display_unit_hex(loc);
		unit_animator animator;

		animator.add_animation(&u, &income_anim, loc, !text.empty(), false, text, color);

		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();

		disp->invalidate_game_status();
	}
}

void location_text(const std::vector<tlocation_anim>& locs)
{
	const animation* tpl = area_anim::anim(area_anim::LOCATION);
	if (!tpl || locs.empty()) {
		return;
	}

	game_display* disp = game_display::get_singleton();
	if (!disp || disp->video().update_locked()) {
		return;
	}
	
	std::map<int, const animation*> wait;
	for (std::vector<tlocation_anim>::const_iterator it = locs.begin(); it != locs.end(); ++ it) {
		int id = disp->insert_area_anim(*tpl);
		animation& anim = disp->area_anim(id);

		anim.replace_progressive("offset_x", "5555", str_cast(1.0 * it->xoffset / 100));
		anim.replace_progressive("offset_y", "5555", str_cast(1.0 * it->yoffset / 100));

		map_location src = map_location::null_location;
		if (!it->text.empty()) {
			int xoffset1 = it->xoffset;
			int yoffset1 = it->yoffset;
			if (it->xoffset < 0) {
				xoffset1 = 0;
			} else if (it->xoffset >= 100) {
				xoffset1 = 99;
			}
			if (it->yoffset < 0) {
				yoffset1 = 0;
			} else if (it->yoffset >= 100) {
				yoffset1 = 99;
			}
			const SDL_Rect& map_area = disp->map_area();
			int x = map_area.x + xoffset1 * map_area.w / 100;
			int y = map_area.y + yoffset1 * map_area.h / 100;
			src = disp->hex_clicked_on(x, y);
		}
		if (it == locs.begin()) {
			new_animation_frame();
		}
		anim.start_animation(0, src, map_location::null_location, false, it->text, it->color);
		wait.insert(std::make_pair(id, &anim));
	}

	// wait_until
	bool finished = false;

	while (!finished) {
		finished = true;

		resources::controller->play_slice(false);
		disp->delay(10);
		for (std::map<int, const animation*>::const_iterator it = wait.begin(); it != wait.end(); ++ it) {
			const animation& anim = *it->second;
			if (!anim.animation_finished_potential()) {
				finished = false;
			}
		}
	}

	for (std::map<int, const animation*>::const_iterator it = wait.begin(); it != wait.end(); ++ it) {
		disp->erase_area_anim(it->first);
	}
}

void location_text(int xoffset, int yoffset, const std::string& text, Uint32 color)
{
	std::vector<tlocation_anim> locs;
	locs.push_back(tlocation_anim(xoffset, yoffset, text, color));

	location_text(locs);
}

} // end unit_display namespace
