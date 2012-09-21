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

#ifndef GUI_DIALOGS_TROOP_SELECTION_HPP_INCLUDED
#define GUI_DIALOGS_TROOP_SELECTION_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class team;
class unit;
class unit_map;
class hero;
class hero_map;

namespace gui2 {

class tlistbox;

class ttroop_selection : public tdialog
{
public:
	explicit ttroop_selection(std::vector<team>* teams, unit_map* units, hero_map& heros, std::vector<std::pair<int, unit*> >& pairs, int side, const std::string& disable_str);

	const std::set<size_t>& checked_pairs() const { return checked_pairs_; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {MIN_PAGE = 0, STATUS_PAGE = MIN_PAGE, MERIT_PAGE, MAX_PAGE = MERIT_PAGE};
	void catalog_page(twindow& window, int catalog, bool swap);	

	void hero_changed(twindow& window);
	void fill_table(int catalog);

	void hero_toggled(twidget* widget);
private:
	std::vector<team>* teams_;
	unit_map* units_;
	hero_map& heros_;

	int side_;
	std::vector<std::pair<int, unit*> >& pairs_;
	std::set<size_t> checked_pairs_;
	std::vector<std::string> disables_;
	size_t max_checked_size_;

	tlistbox* hero_table_;
};

}

#endif

