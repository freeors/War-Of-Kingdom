/* $Id: unit_attack.hpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_UNIT_ATTACK_HPP_INCLUDED
#define GUI_DIALOGS_UNIT_ATTACK_HPP_INCLUDED

#include "actions.hpp"
#include "gui/dialogs/dialog.hpp"
#include "unit_map.hpp"

class game_display;
class unit_map;

namespace gui2 {

class tunit_attack
	: public tdialog
{
public:
	tunit_attack(game_display& gui, unit_map& units, unit& attacker, unit& defender, std::vector<battle_context>& bc_vector, unsigned int best);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	int get_selected_weapon() const { return selected_weapon_; }

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void set_weapon_info(twindow& window, const std::vector<battle_context>& bc_vector, const int best_weapon);
private:
	game_display& gui_;
	unit_map& units_;
	/** The index of the selected weapon. */
	int selected_weapon_;

	/** Iterator pointing to the attacker. */
	unit& attacker_;

	/** Iterator pointing to the defender. */
	unit& defender_;

	std::vector<battle_context> bc_vector_; 
	unsigned int best_;
};

} // namespace gui2

#endif

