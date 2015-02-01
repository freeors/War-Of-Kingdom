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

#ifndef GUI_DIALOGS_SYSTEM_HPP_INCLUDED
#define GUI_DIALOGS_SYSTEM_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class tsystem : public tdialog
{
public:
	struct titem {
		explicit titem(const std::string& _name, bool _enable = true)
			: name(_name)
			, enable(_enable)
		{}

		std::string name;
		bool enable;
	};

	explicit tsystem(const std::vector<titem>& items);

	int get_retval() const { return retval_; }
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void set_retval(twindow& window, int val);
private:
	const std::vector<titem>& items_;

	int retval_;
};


}


#endif /* ! GUI_DIALOGS_MENU_HPP_INCLUDED */
