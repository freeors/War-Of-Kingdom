/* $Id: lobby_player_info.hpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_AUTO_SCROLL_HPP_INCLUDED
#define GUI_DIALOGS_AUTO_SCROLL_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"

namespace gui2 {

class tauto_scroll : public tdialog
{
public:
	tauto_scroll(const std::string& txt);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	std::string txt_;
};

} //end namespace gui2

#endif
