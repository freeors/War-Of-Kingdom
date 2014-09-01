/* $Id: game_save.hpp 48937 2011-03-19 21:04:08Z mordante $ */
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

#ifndef GUI_DIALOGS_GIVE_HPP_INCLUDED
#define GUI_DIALOGS_GIVE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "multiplayer.hpp"

namespace gui2 {

class tgive : public tdialog
{
public:

	tgive(game_display& disp, hero_map& heros, http::membership& target);

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

private:
	void refresh_summary(twindow& window) const;
	void do_give(twindow& window);

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	game_display& disp_;
	hero_map& heros_;
	http::membership& target_;
};

}

#endif

