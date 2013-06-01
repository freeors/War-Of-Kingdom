/* $Id: mp_create_game.hpp 48926 2011-03-17 19:41:11Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_MP_CREATE_GAME_HPP_INCLUDED
#define GUI_DIALOGS_MP_CREATE_GAME_HPP_INCLUDED

#include "random_map.hpp"
#include "gui/dialogs/lobby/lobby_info.hpp"

class config;
class game_display;

namespace gui2 {

class tbutton;
class tlabel;
class ttext_box;

class tmp_create_game : public tdialog, public lobby_base, public trandom_map
{
public:

	explicit tmp_create_game(game_display& gui, const config& cfg);

	int num_turns() const { return num_turns_; }

protected:
	void update_map_settings(twindow& window);
	void post_update_map(twindow& window, int select);

	game_display& gui() { return gui_; }
private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	/**
	 * Network polling callback
	 */
	void process_network_data(const config& data, const network::connection sock);

	/**
	 * All fields are also in the normal field vector, but they need to be
	 * manually controled as well so add the pointers here as well.
	 */

	tfield_bool
		*use_map_settings_,
		*fog_,
		*observers_,
		*shroud_,
		*time_limit,
		*start_time_;

	tfield_integer
		*turns_,
		*gold_,
		*experience_;

public:

	void era(twindow& window);
	void password(twindow& window);
	void maximal_defeated_activity(twindow& window);

private:
	game_display& gui_;
	int num_turns_;
	int era_index_;

	ttext_box* name_entry_;

	tbutton* era_;
	tbutton* launch_game_;

	tbutton* maximal_defeated_activity_;
};

} // namespace gui2

#endif

