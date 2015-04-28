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

#ifndef GUI_DIALOGS_EXCHANGE_HPP_INCLUDED
#define GUI_DIALOGS_EXCHANGE_HPP_INCLUDED

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

class texchange : public tdialog
{
public:
	explicit texchange(std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<std::pair<size_t, unit*> >& human_pairs, std::vector<std::pair<size_t, unit*> >& ai_pairs);

	const std::set<size_t>& checked_human_pairs() const { return checked_human_pairs_; }
	const size_t checked_ai_pairs() const { return *checked_ai_pairs_.begin(); }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void human_hero_toggled(twidget* widget);
	void ai_hero_toggled(twidget* widget);
private:
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;

	std::vector<std::pair<size_t, unit*> >& human_pairs_;
	std::vector<std::pair<size_t, unit*> >& ai_pairs_;
	std::set<size_t> checked_human_pairs_;
	std::set<size_t> checked_ai_pairs_;
	size_t max_checked_human_size_;
	size_t max_checked_ai_size_;

	tlistbox* human_list_;
	tlistbox* ai_list_;
};

}

#endif

