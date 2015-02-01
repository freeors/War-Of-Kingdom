/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_LINKED_GROUP_HPP_INCLUDED
#define GUI_DIALOGS_LINKED_GROUP_HPP_INCLUDED

#include "gui/auxiliary/window_builder.hpp"
#include "gui/dialogs/dialog.hpp"
#include <vector>

class display;

namespace gui2 {

class tlinked_group2: public tdialog
{
public:
	static const int min_id_len = 1;
	static const int max_id_len = 24;

	explicit tlinked_group2(display& disp, const std::vector<tlinked_group>& linked_groups);

	const std::vector<tlinked_group>& linked_groups() const { return linked_groups_; }
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void reload_linked_group_table(twindow& window, int cursel);
	void map_linked_group_to(twindow& window, int index);

	void item_selected(twindow& window);
	void append(twindow& window);
	void modify(twindow& window);
	void erase(twindow& window);

	std::string get_id(twindow& window, int exclude);
	bool verify_other_param(bool width, bool height);

private:
	display& disp_;

	const std::vector<tlinked_group>& orignal_;
	std::vector<tlinked_group> linked_groups_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
