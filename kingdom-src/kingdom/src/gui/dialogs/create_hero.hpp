/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_CREATE_HERO_HPP_INCLUDED
#define GUI_DIALOGS_CREATE_HERO_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include <hero.hpp>

class game_display;
class hero_map;
class config;

namespace gui2 {

/** Do we wish to show the button for the debug clock. */
/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tcreate_hero : public tdialog
{
public:
	explicit tcreate_hero(game_display& gui, hero_map& heros, hero& player_hero);

	~tcreate_hero();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void post_show(twindow& window);

	void regenerate(twindow& window);
	void change_avatar(twindow& window, bool gender);
	void adaptability(twindow& window, int type, int index);
	void feature(twindow& window, bool side);
	void tactic(twindow& window);
	void character(twindow& window);
	void catalog(twindow& window);

	void account(twindow& window);
	void synchronize(twindow& window);
	bool create(twindow& window, bool close);

	std::string text_box_str(twindow& window, const std::string& id, const std::string& name, bool allow_empty = false);

	void set_text_box_int(twindow& window, const std::string& id, int value, int maximum_length = -1);
	bool text_box_int(twindow& window, const std::string& id, const std::string& name, int& value, int min = 1, int max = 127);
private:
	game_display& gui_;
	hero_map& heros_;
	hero h_;
	hero& player_hero_;

	int male_number_;
	int female_number_;
};

} // namespace gui2

#endif
