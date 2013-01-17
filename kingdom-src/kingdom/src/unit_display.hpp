/* $Id: unit_display.hpp 47623 2010-11-21 13:57:27Z mordante $ */
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

/**
 *  @file
 *  Display units performing various actions: moving, attacking, and dying.
 */

#ifndef UNIT_DISPLAY_HPP_INCLUDED
#define UNIT_DISPLAY_HPP_INCLUDED

#include "unit_map.hpp"
#include "gamestatus.hpp"

class attack_type;
class team;
class unit;
class card;
class hero;

/**
 *  Contains a number of free functions which display units
 *
 *  performing various on-screen actions - moving, attacking, and dying.
 */
namespace unit_display
{
/**
 * global animations
 */
enum GLOBAL_ANIM {ANIM_REINFORCE = 0, ANIM_ENCOURAGE, ANIM_TACTIC, 
	ANIM_BLADE, ANIM_PIERCE, ANIM_IMPACT, ANIM_ARCHERY, ANIM_COLLAPSE, ANIM_ARCANE, ANIM_FIRE, ANIM_COLD,
	ANIM_STRIKE, ANIM_MAGIC
};

void load_global_animations(const config& cfg);

void global_anim_2(int type, const std::string& id1, const std::string& id2);


extern int player_number_;

/**
 * Display a unit moving along a given path.
 *
 * Note: Hide the unit in its current location,
 * but don't actually remove it until the move is done,
 * so that while the unit is moving status etc.
 * will still display the correct number of units.
 *
 * @param path
 * @param u
 * @param teams
 * @param animate If set to false, only side-effects of move
 *        are applied (correct unit facing, path hexes redrawing).
 * @param dir Unit will be set facing this direction after move.
 *        If nothing passed, direction will be set based on path.
 */
void move_unit(const std::vector<map_location>& path, unit& u,
	const std::vector<team>& teams, bool animate=true,
	map_location::DIRECTION dir=map_location::NDIRECTIONS);

/**
 * Play a pre-fight animation
 * First unit is the attacker, second unit the defender
 */
void unit_draw_weapon( const map_location& loc, unit& u, const attack_type* attack=NULL, const attack_type*secondary_attack=NULL,const map_location& defender_loc = map_location::null_location, unit * defender=NULL);

/**
 * Play a post-fight animation
 * Both unit can be set to null, only valid units will play their animation
 */
void unit_sheath_weapon( const map_location& loc, unit* u=NULL, const attack_type* attack=NULL, const attack_type*secondary_attack=NULL,const map_location& defender_loc = map_location::null_location, unit * defender=NULL);

/**
 * Show a unit fading out.
 *
 * Note: this only shows the effect, it doesn't actually kill the unit.
 */
 void unit_die( const map_location& loc, unit& u,
 	const attack_type* attack=NULL, const attack_type* secondary_attack=NULL,
 	const map_location& winner_loc=map_location::null_location,
 	unit* winner=NULL);


/**
 *  Make the unit on tile 'a' attack the unit on tile 'b'.
 *
 *  The 'damage' will be subtracted from the unit's hitpoints,
 *  and a die effect will be displayed if the unit dies.
 *
 *  @retval	true                  if the defending unit is dead, should be
 *                                removed from the playing field.
 */
void unit_attack(unit& attacker, std::vector<unit*>& def_ptr_vec, std::vector<int>& damage_vec,
	const attack_type* attack, const attack_type* secondary_attack,
	int swing, std::vector<std::string>& hit_text_vec, bool drain, bool stronger, const std::string& att_text);

void unit_attack2(unit& attacker, const std::string& type, std::vector<unit*>& def_ptr_vec, std::vector<int>& damage_vec,
	 std::vector<std::string>& hit_text_vec);

void unit_recruited(const map_location& loc,
	const map_location& leader_loc=map_location::null_location);

/**
 * This will use a poisoning anim if healing<0.
 */
void unit_healing(unit &healed, const map_location &healed_loc,
	const std::vector<unit *> &healers, int healing);

/**
 *  Set taxer_loc to an invalid location if there are no taxers.
 *
 *  This will use a poisoning anim if taxing<0.
 */
void unit_taxing(unit& taxed, map_location& taxed_loc, std::vector<unit_map::iterator> taxers, int taxing);

/**
 * Parse a standard WML for animations and play the corresponding animation.
 * Returns once animation is played.
 *
 * This is used for the animate_unit action, but can easily be generalized if
 * other wml-decribed animations are needed.
 */
void wml_animation(const vconfig &cfg,
	const map_location& default_location=map_location::null_location);

void card_start(card_map& cards, card& c);

void tactic_start(hero& h);

void unit_touching(const map_location& loc, std::vector<unit *>& touchers, int touching, const std::string& prefix);

void unit_text(unit& u, bool poisoned, const std::string& text);

}

#endif
