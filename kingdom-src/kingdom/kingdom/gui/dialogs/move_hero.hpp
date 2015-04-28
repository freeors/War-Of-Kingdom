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

#ifndef GUI_DIALOGS_MOVE_HERO_HPP_INCLUDED
#define GUI_DIALOGS_MOVE_HERO_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class game_display;
class team;
class unit_map;
class hero_map;
class hero;
class artifical;

namespace gui2 {

class tlistbox;
class tstacked_widget;

class tmove_hero : public tdialog
{
public:
	explicit tmove_hero(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city);

	const std::set<size_t>& checked_heros() const { return checked_heros_; }
	artifical* dst_city() const;

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {ABILITY_PAGE, ADAPTABILITY_PAGE, PERSONAL_PAGE, RELATION_PAGE};
	void catalog_page(twindow& window, int catalog, bool swap);

	void type_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void refresh_tooltip(twindow& window);

	void hero_toggled(twidget* widget);

private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;

	team& current_team_;
	artifical& city_;
	// fresh heros in city after sort by leadership
	std::vector<hero*> fresh_heros_;

	std::vector<artifical*> candidate_cities_;
	int type_index_;
	std::set<size_t> checked_heros_;

	tstacked_widget* page_panel_;
	std::vector<tlistbox*> lists_;
	int current_page_;
	std::vector<std::string> recruits_;
};

}

#endif

