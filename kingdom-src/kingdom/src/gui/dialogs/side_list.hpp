/* $Id: hero_list.hpp 49605 2011-05-22 17:56:24Z mordante $ */
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

#ifndef GUI_DIALOGS_SIDE_LIST_HPP_INCLUDED
#define GUI_DIALOGS_SIDE_LIST_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"

class game_display;
class team;
class unit_map;
class hero;
class hero_map;
class game_state;

namespace gui2 {

class tlistbox;

class tside_list : public tdialog
{
public:
	explicit tside_list(game_display& disp, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, const config& game_config);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {MIN_PAGE = 0, STATUS_PAGE = MIN_PAGE, MILITARY_PAGE, DIPLOMATISM_PAGE, GOLD_PAGE, FEATURE_PAGE, TECHNOLOGY_PAGE, ARTIFICAL_PAGE, PLAN_PAGE, MAX_PAGE = PLAN_PAGE};
	void catalog_page(twindow& window, int catalog, bool swap);	

	void hero_changed(twindow& window);
	void fill_table(int catalog);
	void fill_table_row(team& t, int catalog);

	void noble_list(int n);
	void browse_group(int n);
	void technology_tree(int n);
private:
	game_display& disp_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	game_state& gamestate_;
	const config& game_config_;

	int side_;

	tlistbox* hero_table_;
};

}

#endif

