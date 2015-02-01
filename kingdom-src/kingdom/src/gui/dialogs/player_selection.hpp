/* $Id: player_selection.hpp 49605 2011-05-22 17:56:24Z mordante $ */
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

#ifndef GUI_DIALOGS_PLAYER_SELECTION_HPP_INCLUDED
#define GUI_DIALOGS_PLAYER_SELECTION_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include "tent.hpp"

namespace gui2 {

class tlistbox;
class tbutton;

class tplayer_selection : public ttent
{
public:
	explicit tplayer_selection(display& gui, hero_map& heros, card_map& cards, const config& cfg, const config& campaign_config, hero& player_hero);
	~tplayer_selection();

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	void player_selected(twindow& window);
	display& gui() { return gui_; }
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const;

	void player_city(twindow& window);
private:
	display& gui_;
};

}

#endif

