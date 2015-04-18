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

#ifndef GUI_DIALOGS_MODE_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_MODE_SETTING_HPP_INCLUDED

#include "gui/dialogs/mode_navigate.hpp"

class unit_map;

namespace gui2 {

class tbutton;
class tlistbox;
class ttext_box;

class tmode_setting: public tdialog, public tmode_navigate
{
public:
	explicit tmode_setting(mkwin_controller& controller, display& disp, unit_map& units);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void item_selected(twindow& window);

	void fill_change_list(twindow& window);
	void toggle_tabbar(twidget* widget);
	std::string form_tab_label(int at) const;

	int calculate_change_count(int at) const;
	void delete_mode(twindow& window);
	void modify_mode(twindow& window);

private:
	unit_map& units_;
	tbutton* ok_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
