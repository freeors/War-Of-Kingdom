/* $Id: player_selection.hpp 49605 2011-05-22 17:56:24Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_BOOK2_HPP_INCLUDED
#define GUI_DIALOGS_BOOK2_HPP_INCLUDED

#include "gui/dialogs/book.hpp"

#include "config.hpp"
#include <set>
#include <list>
#include "help.hpp"

class display;
class hero_map;

namespace gui2 {

class tbook2 : public tbook
{
public:
	explicit tbook2(display& disp, gamemap* map, hero_map& heros, const config& game_config, const std::string& tag);

	std::vector<help::topic> generate_topics(const bool sort_generated, const std::string &generator);
	void generate_sections(const config* help_cfg, const std::string &generator, help::section &sec, int level);

private:
	std::vector<help::topic> generate_ability_topics(const bool sort_generated);
	std::vector<help::topic> generate_weapon_special_topics(const bool sort_generated);
	std::vector<help::topic> generate_faction_topics(const bool sort_generated);
	std::vector<help::topic> generate_unit_topics(const bool sort_generated, const std::string& race);

	void generate_races_sections(const config *help_cfg, help::section &sec, int level);

};

}

#endif

