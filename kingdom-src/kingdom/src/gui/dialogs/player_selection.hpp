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

class display;
class hero_map;
class hero;
class card_map;

typedef struct {
	int hero_;
	int leader_;
	int city_;
	int stratum_;
} hero_row;

namespace gui2 {

class tlistbox;

class tplayer_selection : public tdialog
{
public:
	explicit tplayer_selection(display& gui, hero_map& heros, card_map& cards, const config& campaign_config, hero& player_hero);
	~tplayer_selection();

	const config player() const;
	const std::vector<bool>& checked_card() const { return checked_card_; }
	bool shroud() const { return shroud_; }
	bool fog() const { return fog_; }

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void player_city(twindow& window);

	void add_row_to_heros(tlistbox* list, int h, int leader, int city, int stratum);
	void player_selected(twindow& window);
	void card_toggled(twidget* widget);
private:
	display& gui_;
	hero_map& heros_;
	card_map& cards_;
	const config& campaign_config_;

	tlistbox* card_table_;
	hero_row* rows_mem_;
	int mem_vsize_;
	int player_;
	std::vector<bool> checked_card_;

	bool rpg_mode_;
	tfield_bool* chk_shroud_;
	tfield_bool* chk_fog_;

	hero* player_hero_;
	std::map<int, int> city_map_;;
	std::map<int, int> city_leader_map_;

	bool shroud_;
	bool fog_;
};

}

#endif

