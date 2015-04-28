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

#ifndef GUI_DIALOGS_COMBO_BOX_HPP_INCLUDED
#define GUI_DIALOGS_COMBO_BOX_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class tlistbox;

class tcombo_box : public tdialog
{
public:
	enum {NONE, EXTRACT};

	explicit tcombo_box(const std::vector<tval_str>& items, int val, int type = NONE);

	/**
	 * Returns the selected item index after displaying.
	 * @return -1 if the dialog was cancelled.
	 */
	int selected_index() const { return index_; }
	int selected_val() const { return items_[index_].val; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void item_selected(twindow& window, tlistbox& list, const int type);

private:
	int index_;
	std::vector<tval_str> items_;
	int type_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
