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

#ifndef GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"

class display;
class unit;

namespace gui2 {

struct tlinked_group;

class tcontrol_setting : public tsetting_dialog
{
public:
	explicit tcontrol_setting(display& disp, const unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void save(twindow& window, bool& handled, bool& halt);

	void set_horizontal_layout(twindow& window);
	void set_vertical_layout(twindow& window);
	
	void set_horizontal_layout_label(twindow& window);
	void set_vertical_layout_label(twindow& window);

	void set_linked_group(twindow& window);
	void set_linked_group_label(twindow& window);

	void set_textdomain(twindow& window, bool label);
	void set_textdomain_label(twindow& window, bool label);

protected:
	display& disp_;
	const unit& u_;
	const std::vector<std::string>& textdomains_;
	const std::vector<tlinked_group>& linkeds_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
