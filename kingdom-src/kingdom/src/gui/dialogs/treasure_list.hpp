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

#ifndef GUI_DIALOGS_TREASURE_LIST_HPP_INCLUDED
#define GUI_DIALOGS_TREASURE_LIST_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"

class team;
class unit_map;
class hero;
class hero_map;

namespace gui2 {

class tbutton;
class tlistbox;
class tgrid;

typedef struct {
	std::vector<team*> sides_;
	std::vector<hero*> heros_;
} ownership;

class ttreasure_list : public tdialog
{
public:
	explicit ttreasure_list(std::vector<team>& teams, unit_map& units, hero_map& heros, int side = -1);

	bool compare_row(tgrid& row1, tgrid& row2);
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {MIN_PAGE = 0, OWNERSHIP_PAGE = MIN_PAGE, MAX_PAGE = OWNERSHIP_PAGE};
	void catalog_page(twindow& window, int catalog, bool swap);	

	void hero_changed(twindow& window);
	void fill_table(int catalog);
	void fill_table_row(std::map<int, ownership>::iterator& p, int catalog);

	void sort_column(twindow& window, tbutton& widget);
private:
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	int side_;

	std::map<int, ownership> ownerships_;
	
	bool ascend_;
	int current_page_;
	std::map<int, std::vector<tbutton*> > sorting_widgets_;
	tbutton* sorting_widget_;

	tlistbox* hero_table_;
};

}

#endif

