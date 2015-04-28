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

#ifndef GUI_DIALOGS_PLAY_CARD_HPP_INCLUDED
#define GUI_DIALOGS_PLAY_CARD_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>

class game_display;
class team;
class unit_map;
class hero_map;
class hero;
class card_map;
class artifical;
class unit_type;
class game_state;

namespace gui2 {

class twindow;
class tlistbox;

class tplay_card : public tdialog
{
public:
	explicit tplay_card(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, card_map& cards, game_state& gamestate, int side, button_action* discard);

	int card_index() const { return card_index_; }

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void discard(bool& handled, bool& halt, int index);
	void contex(twindow& window);

	void card_selected(twindow& window, tlistbox& list, const int type = twidget::drag_none);
	void refresh_tooltip(twindow& window);

private:
	game_display& gui_;
	std::vector<team>& teams_;
	unit_map& units_;
	hero_map& heros_;
	card_map& cards_;
	game_state& gamestate_;

	team& current_team_;

	button_action* discard_;
	int card_index_;

	tlistbox* hero_table_;
	twindow* window_;
};

}

#endif

