/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_MP_SIDE_CREATOR_HPP_INCLUDED
#define GUI_DIALOGS_MP_SIDE_CREATOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gamestatus.hpp"
#include "multiplayer_ui.hpp"
#include "network.hpp"
#include "gui/dialogs/lobby/lobby_info.hpp"

class hero_map;
class config;
class game_display;
class gamemap;

namespace ai {
	struct description;
}

namespace gui2 {

class tlabel;
class tbutton;
class tlistbox;

class tplayer_list_side_creator: public tplayer_list
{
public:
	void init(twindow& w);

public:
	tsub_player_list active_game;
};

class tmp_side_creator : public tdialog, public lobby_base
{
public:
	struct connected_user {
		connected_user(const std::string& name, controller controller__,
				network::connection connection) :
			name(name), controller_(controller__), connection(connection)
		{};
		std::string name;
		controller controller_;
		network::connection connection;
		operator std::string() const
		{
			return name;
		}
	};

	typedef std::vector<connected_user> connected_user_list;

	class side 
	{
	public:
		side(tlistbox* sides_table, tmp_side_creator& parent, const config& cfg, int index);

		side(const side& a);

		void player(twindow& window);
		void faction(twindow& window);
		void feature(twindow& window);

		/**
		 * Gets a config object representing this side.
		 *
		 * If include_leader is set to true, the config objects include the
		 * "type=" defining the leader type, else it does not.
		 */
		config get_config() const;

		/**
		 * Returns true if this side is waiting for a network player and
		 * players allowed.
		 */
		bool available(const std::string& name = "") const;

		/** Returns true, if the player has chosen his leader and this side is ready for the game to start */
		bool ready_for_start() const;

		/** Return true if players are allowed to take this side. */
		bool allow_player() const;

		/** Sets the controller of a side. */
		void set_controller(controller controller);
		controller get_controller() const;

		/** Adds an user to the user list combo. */
		void update_player_id();

		const std::string& get_player_id() const;

		/** Sets the username of this side. */
		void set_player_id(const std::string& player_id);

		/** Sets if the joining player has chosen his leader. */
		void set_ready_for_start(bool ready_for_start);

		/**
		 * Imports data from the network into this side, and updates the UI
		 * accordingly.
		 */
		void import_network_user(const config& data);

		/** Resets this side to its default state, and updates the UI accordingly. */
		void reset(controller controller_);

		/** Resolves the random leader / factions. */
		void resolve_random();

		int faction_;
	private:
		void update_controller_ui();
		void update_ui();

		/**
		 * The mp::connect widget owning this mp::connect::side.
		 *
		 * Used in the constructor, must be first.
		 */
		tmp_side_creator* parent_;

		/**
		 * A non-const config. Be careful not to insert keys when only reading.
		 *
		 * (Use cfg_.get_attribute().)
		 */
		config cfg_;

		// Configurable variables
		int index_;
		std::string id_;
		std::string player_id_;
		std::string save_id_;
		controller controller_;
		int team_;
		int color_;
		int gold_;
		int income_;
		std::string ai_algorithm_;
		bool ready_for_start_;

		bool allow_player_;
		bool allow_changes_;
		bool enabled_;
		bool changed_;

		int selected_feature_;

		tbutton* player_button_;
		tbutton* faction_button_;
		tbutton* feature_button_;
		tbutton* team_button_;
		tbutton* color_button_;
	};

	friend class side;

	typedef std::vector<side> side_list;

	game_display& gui_;
	hero_map& heros_;
	gamemap& gmap_;
	const config& game_config_;

	int get_faction(int side) const;
	std::vector<int> get_candidate_factions(int side) const;
	void set_faction(int side, int faction);

	explicit tmp_side_creator(hero_map& heros, game_display& gui, gamemap& gmap, const config& game_config,
			config& gamelist, const mp_game_settings& params, const int num_turns,
			mp::controller default_controller, bool local_players_only = false);

	~tmp_side_creator();

	enum legacy_result { CONTINUE, JOIN, OBSERVE, CREATE, PREFERENCES, PLAY, QUIT };

	legacy_result get_legacy_result() const { return legacy_result_; }

	void take_reserved_side(tmp_side_creator::side& side, const config& data);

	/**
	 * Returns the game state, which contains all information about the current
	 * scenario.
	 */
	const game_state& get_state();

	/**
	 * Updates the current game state, resolves random factions, and sends a
	 * "start game" message to the network.
	 */
	void start_game();
private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void launch(twindow& window);
	void cancel(twindow& window);

	void player(twindow& window, int side);
	void faction(twindow& window, int side);
	void feature(twindow& window, int side);

	void update_playerlist();
private:
	// Those 2 functions are actually the steps of the (complex)
	// construction of this class.

	/**
	 * Called by the constructor to initialize the game from a
	 * create::parameters structure.
	 */
	void load_game(twindow& window);
	void lists_init();

	/** Convenience function. */
	config* current_config();

	/** Updates the level_ variable to reflect the sides in the sides_ vector. */
	void update_level();

	/** Updates the level, and send a diff to the clients. */
	void update_and_send_diff(bool update_time_of_day = false);

	/** Returns true if there still are sides available for this game. */
	bool sides_available() const;

	/** Returns true if all sides are ready to start the game. */
	bool sides_ready() const;

	/**
	 * Validates whether the game can be started.
	 *
	 * returns                       Can the game be started?
	 */
	bool can_start_game() const;

	/**
	 * Updates the state of the player list, the launch button and of the start
	 * game label, to reflect the actual state.
	 */
	void update_playerlist_state(bool silent=true);

	/** Returns the index of a player, from its id, or -1 if the player was not found. */
	connected_user_list::iterator find_player(const std::string& id);

	/** Returns the side which is taken by a given player, or -1 if none was found. */
	int find_player_side(const std::string& id) const;

	/** Adds a player. */
	void update_user_combos();

	/**
	 * Network polling callback
	 */
	void process_network_data(const config& data, const network::connection sock);
	void process_network_error(network::error& error);

	bool local_only_;

	config level_;

	command_pool replay_data_;

	/** This is the "game state" object which is created by this dialog. */
	game_state state_;
	mp_game_settings params_;
	int num_turns_;

	config* era_cfg_;
	/** The list of available sides for the current era. */
	std::vector<const config *> era_sides_;
	/** The list of available forces. */
	std::vector<std::pair<const config *, int> > factions_;

	// Lists used for combos
	std::vector<std::pair<std::string, int> >player_types_;
	std::vector<std::pair<std::string, int> >player_xtypes_;
	std::vector<std::string> player_factions_;
	std::vector<std::string> side_features_;
	std::vector<std::string> player_teams_;
	std::vector<std::string> player_colors_;

	// team_name list and "Team" prefix
	std::vector<std::string> team_names_;
	std::vector<std::string> user_team_names_;
	const std::string team_prefix_;

	side_list sides_;
	connected_user_list users_;

	tlistbox* sides_table_;
	tlabel* waiting_;
	tbutton* launch_;
	
	controller default_controller_;

	/** Sets the user list */
	virtual void gamelist_updated(bool silent=true) {};
	// multiplayer_ui.hpp
	config& gamelist_;

	/**
	 * Result flag for interfacing with other MP dialogs
	 */
	legacy_result legacy_result_;

	tplayer_list_side_creator player_list_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
