/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#ifndef GUI_DIALOGS_SUBCONTINENT_HPP_INCLUDED
#define GUI_DIALOGS_SUBCONTINENT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"
#include "map_location.hpp"
#include "multiplayer.hpp"

namespace gui2 {

class thexmap;
class tbutton;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tsubcontinent : public tdialog
{
public:
	struct tcity
	{
		tcity(const config& cfg);

		int cityno;
		int number;
		map_location loc;
		int ownership;
		int endurance;
		int times;
	};

	struct tparam
	{
		tparam(const config& main_cfg);

		tcity& find_city(int cityno);

		std::string id;
		std::string name;
		int npc;
		config scenario;
		std::vector<tcity> cities;
		http::tsubcontinent_record http_subcontinent;

		bool valid;
	};

	enum tresult { SIEGE = 1, SCENARIO};

	tsubcontinent(game_display& disp, hero_map& heros, game_state& state, const config& game_config);

	~tsubcontinent();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/**
	 * Updates the tip of day widget.
	 *
	 * @param window              The window being shown.
	 * @param previous            Show the previous tip, else shows the next
	 *                            one.
	 */
	void size_change(thexmap* widget);
	void attack(twindow& window);
	void repair(twindow& window);
	void discard(twindow& window);
	
	void city_selected(twindow& window);

	void switch_to_subcontinent(twindow& window, tparam& param);
	void switch_to_city(twindow& window, tcity& city);

	void refresh_title_flag(twindow& window) const;
	void refresh_city_table(twindow& window, const tparam& param, int cursel) const;
	void refresh_action_button(twindow& window, tcity* city);

	void execute_bh(twindow& window, int tag);
	std::vector<std::vector<std::string> > crop_map(std::vector<std::vector<std::string> >& full_map_str, const map_location& from_loc, const map_location& to_loc);
private:
	game_display& disp_;
	hero_map& heros_;
	game_state& state_;
	const config& game_config_;

	std::vector<tparam> params_;
	std::map<int, http::membership> associate_members_;
	tparam* cursel_;
	tcity* curcity_;
	tcity* my_city_;
	const config* npc_side_;
	bool local_only_;
	tbutton* attack_;
	tbutton* repair_;
	tbutton* discard_;
	bool rflip_;
};

} // namespace gui2

#endif
