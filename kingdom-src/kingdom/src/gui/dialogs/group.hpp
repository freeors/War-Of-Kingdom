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

#ifndef GUI_DIALOGS_GROUP_HPP_INCLUDED
#define GUI_DIALOGS_GROUP_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>
#include <hero.hpp>
#include "multiplayer.hpp"
#include "animation.hpp"

class game_display;
class unit_type;

namespace gui2 {

class tlistbox;
class tbutton;
class ttoggle_button;
class tscrollbar_panel;

class tgroup2 : public tdialog
{
public:
	explicit tgroup2(game_display& disp, hero_map& heros, const config& game_config, tgroup& group, bool browse = false);

	enum tresult {LAYOUT = 1, START_MAP_EDITOR};
	std::string get_selected_map_data() const;

protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void set_retval(twindow& window, int retval);

	void player_city(twindow& window);
	void update_map(twindow& window);
	void layout(twindow& window);
	void edit_map(twindow& window);
	
	void personnel(twindow& window);

	void type_selected(twindow& window);
	void fill_2_least(twindow& window);
	void draw(twindow& window);
	void upgrade(twindow& window);
	void detail(twindow& window);
	void exile(twindow& window);
	void discard_member(twindow& window);
	void employ_employee(twindow& window);

	void exchange(twindow& window, bool score_2_coin);
	void refresh_title_flag(twindow& window) const;

	enum {NONE_PAGE, MIN_PAGE, BASE_PAGE = MIN_PAGE, CITY_PAGE, FIX_PAGE, ROAM_PAGE, ASSOCIATE_PAGE, MAX_PAGE = ASSOCIATE_PAGE};
	void sheet_toggled(twidget* widget);
	void swap_page(twindow& window, int page, bool swap);

	void fill_base(twindow& window);
	void fill_city(twindow& window);
	void fill_fix(twindow& window);
	void fill_employee(twindow& window);
	void fill_associate(twindow& window);

	void fill_member_table(twindow& window, const std::vector<tgroup::tmember*>& members, int cursel = 0);
	void refresh_employee_other(twindow& window);
	void employee_selected(twindow& window);
	void fill_employee_table(twindow& window, int cursel);
	
	void associate_selected(twindow& window);
	void fill_associate_table(twindow& window, int cursel = 0);
	void refresh_button_according_associate(tbutton& button, const tgroup::tassociate& a);
	void insert_associate(twindow& window);
	void do_agreement(twindow& window);
	void remove_associate(twindow& window);
	void refresh_associate(twindow& window);
	void detail_associate(twindow& window);
	void give_score(twindow& window);

	bool do_draw(int lack, int coin, int score);
	bool text_box_int(twindow& window, const std::string& id, const std::string& name, int& value, int min, int max);
	void refresh_leader_noble(twindow& window);
	void fill_other(twindow& window);

private:
	void start_upgrade_anim(hero& h, const std::string& description);
	void upgrade_anim_finish(animation* anim);
	void enable_window_ui(twindow& window, bool enable);

private:
	game_display& disp_;
	hero_map& heros_;
	const config& game_config_;
	tgroup& group_;
	bool browse_;

	int selected_number_;
	int selected_associate_;
	std::vector<http::membership> associate_members_;
	std::map<int, http::temployee> employees_;

	std::string map_data_;

	int current_page_;
	std::map<int, ttoggle_button*> sheet_;
	tscrollbar_panel* page_panel_;
};

}

#endif

