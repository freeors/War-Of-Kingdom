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

#ifndef GUI_DIALOGS_INTERIOR_HPP_INCLUDED
#define GUI_DIALOGS_INTERIOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class game_display;
class team;
class unit_map;
class hero_map;
class hero;
class unit_type;
class artifical;
class department;

namespace gui2 {

class tlistbox;
class tstacked_widget;
class tbutton;

class tinterior : public tdialog
{
public:
	explicit tinterior(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city, bool browse);

	bool get_depute() const { return depute_; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {MIN_PAGE = 0, ABILITY_PAGE = MIN_PAGE, ADAPTABILITY_PAGE, RELATION_PAGE, MAX_PAGE = RELATION_PAGE};
	void catalog_page(twindow& window, int catalog, bool swap);
	void appoint(twindow& window);

	void type_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void refresh_tooltip(twindow& window);

	void hero_toggled(twidget* widget);

	std::pair<int, int> calculate_markets(int exploiture);
	std::pair<int, int> calculate_technologies(int exploiture);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	artifical& city_;
	bool browse_;
	team& current_team_;
	// fresh heros in city after sort by leadership
	std::vector<hero*> fresh_heros_;

	int type_index_;
	int checked_hero_;

	tbutton* appoint_;
	tstacked_widget* page_panel_;
	std::vector<tlistbox*> lists_;
	int current_page_;

	bool depute_;

	std::vector<department> departments_;
	std::map<const unit_type*, int> market_map_;
	std::map<const unit_type*, int> technology_map_;
	// int income_;
};

}

#endif

