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

#ifndef GUI_DIALOGS_RECRUIT_HPP_INCLUDED
#define GUI_DIALOGS_RECRUIT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class game_display;
class team;
class unit_map;
class hero_map;
class game_state;
class hero;
class artifical;
class unit_type;

namespace gui2 {

class tlistbox;
class tstacked_widget;

class trecruit : public tdialog
{
public:
	explicit trecruit(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, artifical& city, int cost_exponent, bool rpg_mode);

	hero* master() const;
	hero* second() const;
	hero* third() const;

	const unit_type* unit_type_ptr() const;

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

	void type_selected2(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void type_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void refresh_tooltip(twindow& window);

	void hero_toggled(twidget* widget);

	void switch_type(twindow& window, bool next);
	void switch_type_internal(twindow& window);
private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	game_state& gamestate_;

	int max_level_;
	team& current_team_;
	artifical& city_;
	int cost_exponent_;
	// fresh heros in city after sort by leadership
	std::vector<hero*> fresh_heros_;

	std::vector<const unit_type*> unit_types_;
	int type_index_;
	std::set<int> checked_heros_;

	bool rpg_mode_;
	tstacked_widget* page_panel_;
	std::vector<tlistbox*> lists_;
	int current_page_;
};

}

#endif

