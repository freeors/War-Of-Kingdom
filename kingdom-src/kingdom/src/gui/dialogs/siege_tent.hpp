/* $Id: hero_list.hpp 49605 2011-05-22 17:56:24Z mordante $ */
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

#ifndef GUI_DIALOGS_SIEGE_TENT_HPP_INCLUDED
#define GUI_DIALOGS_SIEGE_TENT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>
#include "unit_types.hpp"
#include "multiplayer.hpp"

class game_display;
class game_state;

namespace gui2 {

class tlabel;
class tlistbox;
class tbutton;

struct tsubcontinent_param {
	tsubcontinent_param(int defender, const std::map<int, http::membership>& members, const config& scenario, const map_location& aloc, const map_location& dloc, int attacker_city, int defender_city)
		: defender(defender)
		, members(members)
		, scenario(scenario)
		, aloc(aloc)
		, dloc(dloc)
		, attacker_city(attacker_city)
		, defender_city(defender_city)
	{}

	int defender;
	const std::map<int, http::membership>& members;
	const config& scenario;
	std::vector<std::vector<std::string> > defender_map_str;
	map_location aloc;
	map_location dloc;
	int attacker_city;
	int defender_city;
};

class tsiege_tent : public tdialog
{
public:
	explicit tsiege_tent(game_display& disp, hero_map& heros, game_state& state, const config& game_config, tsubcontinent_param* param);

	config& get_scenario() { return scenario_; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void genus(twindow& window);
	void save_map(twindow& window);
	void reinforce_attacker(twindow& window);
	void defender_username(twindow& window);
	void browse_stratagem(twindow& window);
	
	void set_defender(twindow& window, int uid, const std::string& username);
	void set_reinforce_attacker(twindow& window, int uid, const std::string& username);

	void genereate_roam_troops(config& side_cfg, tgroup& g, const map_location& loc, const std::string& event_name);

	void refresh_reinforce_defender_str(twindow& window);
	std::vector<tgroup::tassociate> defender_list() const;
	std::vector<tgroup::tassociate> reinforce_attacker_list() const;
	void set_save_map_status(twindow& window);
	void refresh_interior_ui(tlabel& label, const std::string& interior_str);
	std::vector<std::vector<std::string> > crop_map(std::vector<std::vector<std::string> >& full_map_str, const map_location& from_loc, const map_location& to_loc);
private:
	game_display& disp_;
	hero_map& heros_;
	game_state& state_;
	const config& game_config_;
	tsubcontinent_param* param_;
	bool local_only_;
	bool rflip_;

	int turns_;
	std::vector<std::vector<std::string> > attacker_map_str_;
	std::string defender_map_data_;
	
	std::map<int, http::membership> associate_members_;
	std::set<int> loaded_defender_;
	config scenario_;
	std::pair<int, std::string> defender_;
	std::pair<int, std::string> reinforce_attacker_;
	std::vector<tgroup::tassociate> reinforce_defender_;
	const ttechnology* attacker_stratagem_;
	const ttechnology* defender_stratagem_;

	tbutton* genus_;
};

}

#endif

