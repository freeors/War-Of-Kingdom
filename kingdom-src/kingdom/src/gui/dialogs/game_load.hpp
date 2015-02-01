/* $Id: game_load.hpp 49605 2011-05-22 17:56:24Z mordante $ */
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

#ifndef GUI_DIALOGS_LOAD_GAME_HPP_INCLUDED
#define GUI_DIALOGS_LOAD_GAME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/listbox.hpp"
#include "savegame.hpp"
#include "tstring.hpp"

class display;
class hero_map;

namespace gui2 {

class ttoggle_button;
class tscrollbar_panel;

class tgame_load : public tdialog
{
public:
	explicit tgame_load(display& disp, hero_map& heros, const config& cache_config, bool allow_network);

	const std::string& filename() const { return filename_; }

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	enum {NONE_PAGE, MIN_PAGE, LOCAL_PAGE = MIN_PAGE, MAX_PAGE, NETWORK_PAGE = MAX_PAGE};
	void sheet_toggled(twidget* widget);
	void swap_page(twindow& window, int page, bool swap);

	void fill_local(twindow& window);
	void fill_network(twindow& window);
private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void list_item_clicked(twindow& window);
	void delete_button_callback(twindow& window);
	void xmit_button_callback(twindow& window);

	void display_savegame(twindow& window);
	void evaluate_summary_string(std::stringstream& str, const config& cfg_summary);
	void fill_game_list(twindow& window, std::vector<savegame::save_info>& games);

private:
	display& disp_;
	hero_map& heros_;
	std::string filename_;

	std::vector<savegame::save_info> games_;
	std::vector<savegame::www_save_info> www_saves_;
	const config& cache_config_;
	bool allow_network_;

	int current_page_;
	std::map<int, ttoggle_button*> sheet_;
	tlistbox* savegame_list_;
};

}

#endif

