/* $Id: playcampaign.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003-2005 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2010 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef PLAYCAMPAIGN_H_INCLUDED
#define PLAYCAMPAIGN_H_INCLUDED

#include "game_end_exceptions.hpp"

class game_display;
class game_state;
class config;
class hero_map;
class card_map;
class CVideo;

enum io_type_t {
	IO_NONE,
	IO_SERVER,
	IO_CLIENT
};

LEVEL_RESULT play_game(game_display& disp, game_state& state,
		const config& game_config, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		io_type_t io_type=IO_NONE,
		bool skip_replay = false);

void play_replay(game_display& disp, game_state& state,
		const config& game_config, hero_map& heros, hero_map& heros_start, 
		card_map& cards, CVideo& video);

#endif // PLAYCAMPAIGN_H_INCLUDED

