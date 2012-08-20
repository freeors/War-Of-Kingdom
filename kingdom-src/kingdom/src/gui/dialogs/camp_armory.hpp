/* $Id: title_screen.hpp 47226 2010-10-26 21:37:36Z mordante $ */
/*
   Copyright (C) 2008 - 2010 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_CAMP_ARMORY_HPP_INCLUDED
#define GUI_DIALOGS_CAMP_ARMORY_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class unit;
class hero;
class artifical;

namespace gui2 {

class ttoggle_button;

class tcamp_armory : public tdialog
{
public:
	explicit tcamp_armory(std::vector<unit*>& candidate_troops, std::vector<hero*>& candidate_heros, int scenario = SCENARIO_CAMP, artifical* city = NULL);

	~tcamp_armory();

	/**
	 * Values for the menu-items of the main menu.
	 *
	 * @todo Evaluate the best place for these items.
	 */
	enum tscenario {SCENARIO_CAMP, SCENARIO_CITY};

private:

	/** Inherited from tdialog, implemented by REGISTER_WINDOW. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);
	void post_show(twindow& window);

	/**
	 * Updates the tip of day widget.
	 *
	 * @param window              The window being shown.
	 * @param previous            Show the previous tip, else shows the next
	 *                            one.
	 */
	void refresh_according_to_troop(twindow& window, const int curr = 0);
	void refresh_according_to_captain(twindow& window, const int curr = 0);
	void refresh_according_to_hero(twindow& window, const int curr = 0);
	void do_battle(twindow& window);

	void refresh_buttons_from_armory_cfg(twindow& window, bool first = false);
	void update_troop_ui(ttoggle_button* toggle, unit* u);
	void update_troop_ui2(ttoggle_button* toggle, unit* u);
	void update_hero_ui(ttoggle_button* toggle, hero* h, size_t index);
private:
	tscenario scenario_;
	artifical* city_;
	int prev_troop_;
	int prev_captain_;
	int prev_hero_;
	std::vector<unit*>& candidate_troops_;
	std::vector<hero*>& candidate_heros_;
	std::vector<int> hit_points_;

	int first_human_troop_;
	size_t reserve_heros_;
};

} // namespace gui2

#endif
