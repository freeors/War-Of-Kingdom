/* $Id: multiplayer.hpp 47847 2010-12-05 21:12:23Z shadowmaster $ */
/*
   Copyright (C) 2005 - 2010 Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef MULTIPLAYER_HPP_INCLUDED
#define MULTIPLAYER_HPP_INCLUDED

#include "multiplayer_ui.hpp"

class config;
class game_display;
class hero_map;
class card_map;

namespace mp {

// max. length of a player name
const size_t max_login_size = 20;

/*
 * This is the main entry points of multiplayer mode.
 */

/** Starts a multiplayer game in single-user mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param default_controller The default controller type
 */
void start_local_game(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, mp::controller default_controller);

/** Starts a multiplayer game in client mode.
 *
 * @param disp        The global display
 * @param game_config The global, top-level WML configuration for the game
 * @param host        The host to connect to.
 */
void start_client(game_display& disp, const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, const std::string& host);


}
#endif
