/* $Id: mp_connect.hpp 48784 2011-03-06 14:14:07Z mordante $ */
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

#ifndef GUI_DIALOGS_MAP_GENERATOR_HPP_INCLUDED
#define GUI_DIALOGS_MAP_GENERATOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"

class display;
class default_map_generator;

namespace gui2 {

class tbutton;
class ttoggle_button;

class tmap_generator : public tdialog
{
public:
	explicit tmap_generator(display& gui, default_map_generator& generator, int max_players, int min_w, int max_w, int min_h, int max_h);

	bool changed() const;
	void apply_change();
private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void players(twindow& window);
	void width(twindow& window);
	void height(twindow& window);
	void iterations(twindow& window);
	void hillsize(twindow& window);
	void villages(twindow& window);
	void castlesize(twindow& window);
	void landform(twindow& window);

private:
	display& gui_;
	default_map_generator& generator_;
	int max_players_;
	int min_width_;
	int max_width_;
	int min_height_;
	int max_height_;

	size_t val_nplayers_;
	size_t val_width_;
	size_t val_height_;
	size_t val_iterations_;
	size_t val_hill_size_;
	size_t val_nvillages_;
	size_t val_castle_size_;
	size_t val_island_size_;
	bool val_link_castles_;

	tbutton*  players_;
	tbutton*  width_;
	tbutton*  height_;
	tbutton*  iterations_;
	tbutton*  hillsize_;
	tbutton*  villages_;
	tbutton*  castlesize_;
	tbutton*  landform_;
	ttoggle_button* link_castles_;
};

} // namespace gui2

#endif

