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

#ifndef GUI_DIALOGS_COLUMN_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_COLUMN_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include <vector>

class display;
class unit;

namespace gui2 {

class tcolumn_setting : public tsetting_dialog
{
public:
	explicit tcolumn_setting(display& disp, const unit& u);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void save(twindow& window, bool& handled, bool& halt);

private:
	display& disp_;
	const unit& u_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
