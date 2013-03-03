/* $Id: replay.hpp 40489 2010-01-01 13:16:49Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file replay.hpp
 *  Replay control code.
 */

#ifndef REPLAY_H_INCLUDED
#define REPLAY_H_INCLUDED

#include "config.hpp"
#include "map_location.hpp"
#include "rng.hpp"

#include <deque>

class game_display;
class terrain_label;
class unit_map;
class play_controller;
class unit_type;
struct time_of_day;

class replay: public rand_rng::rng, public command_pool
{
public:
	replay();
	explicit replay(const command_pool& pool);

	void append(const config& cfg);

	void set_skip(bool skip);
	bool is_skipping() const;

	void add_start();
	void add_recruit(const std::string& type, const map_location& loc, std::vector<const hero*>& troop_heros, int cost, bool human);
	void add_expedite(bool troop, int index, const std::vector<map_location>& steps);
	void add_disband(int unit_index, const map_location& loc, int income = 0);
	void add_armory(const map_location& loc, const std::vector<size_t>& diff);
	void add_move_heros(const map_location& src, const map_location& dst, std::set<size_t>& heros);
	void add_belong_to(const unit* u, const artifical* to, bool loyalty);
	void add_build_begin(const std::string& type);
	void add_build(const unit_type* type, const map_location& city_loc, const map_location& builder_loc, const map_location& art_loc, int cost);
	void add_cast_tactic(const unit& tactician, const hero& h, const unit* special);
	void add_build_cancel();
	void add_countdown_update(int value,int team);
	void add_movement(const std::vector<map_location>& steps);
	void add_attack(const unit& a, const unit& b,
		int att_weapon, int def_weapon, const std::string& attacker_type_id,
		const std::string& defender_type_id, int attacker_lvl,
		int defender_lvl, const size_t turn, const time_of_day t, bool move = true);
	void add_duel(const hero& left, const hero& right, int percentage);
	void add_seed(const char* child_name, rand_rng::seed_t seed);
	void user_input(const std::string &, const config &);
	void choose_option(int index);
	void text_input(std::string input);
	void set_random_value(const std::string& choice);
	void add_label(const terrain_label*);
	void clear_labels(const std::string&);
	void add_rename(const std::string& name, const map_location& loc);
	void add_diplomatism(const std::string& type, int first_side = -1, int second_side = -1, bool alignment = true, int emissary = 0, int target_side = -1, int strategy = -1);
	void add_final_battle(artifical* human, artifical* ai);
	void add_card(int side, size_t number, bool dialog);
	void erase_card(int side, int index, const map_location& loc, std::vector<std::pair<int, unit*> >& touched, bool dialog);
	void add_interior(const artifical& city);
	void add_hero_field(int number, int side_feature);
	void init_side();
	void end_turn();
	void init_ai();
	void do_commoner();
	void add_inching_block(const unit& u, bool increase);
	enum EVENT_TYPE {EVENT_ENCOURAGE, EVENT_RPG_INDEPENDENCE, EVENT_FIND_TREASURE};
	void add_event(int type, const map_location& loc);
	void add_assemble_treasure(const std::map<int, int>& diff);
	void add_rpg_exchange(const std::set<size_t>& checked_human, size_t checked_ai);
	void add_ing_technology(const std::string& id);

	void add_log_data(const std::string &key, const std::string &var);
	void add_log_data(const std::string &category, const std::string &key, const std::string &var);
	void add_log_data(const std::string &category, const std::string &key, const config& c);

	/**
	 * Mark an expected advancement adding it to the queue
	 */
	void add_expected_advancement(unit* u);

	/**
	 * Access to the expected advancements queue.
	 */
	const std::deque<unit*>& expected_advancements() const;

	/**
	 * Remove the front expected advancement from the queue
	 */
	void pop_expected_advancement();

	void add_chat_message_location();
	void speak(const config& cfg);
	std::string build_chat_log();

	//get data range will get a range of moves from the replay system.
	//if data_type is 'ALL_DATA' then it will return all data in this range
	//except for undoable data that has already been sent. If data_type is
	//NON_UNDO_DATA, then it will only retrieve undoable data, and will mark
	//it as already sent.
	//undoable data includes moves such as placing a label or speaking, which is
	//ignored by the undo system.
	enum DATA_TYPE { ALL_DATA, NON_UNDO_DATA };
	config get_data_range(int cmd_start, int cmd_end, DATA_TYPE data_type=ALL_DATA);
	config get_last_turn(int num_turns=1);
	void get_replay_data(command_pool& that);

	config& get_current_cfg() { return cfg_; }
	const config& get_current_cfg() const { return cfg_; }

	config& get_build_cfg() { return build_cfg_; }
	const config& get_build_cfg() const { return build_cfg_; }


	void undo();

	void start_replay();
	void revert_action();
	config* get_next_action();
	void pre_replay();

	bool at_end() const;
	void set_to_end();

	void clear();
	bool empty();

	enum MARK_SENT { MARK_AS_UNSENT, MARK_AS_SENT };
	void add_config(const config& cfg, MARK_SENT mark=MARK_AS_UNSENT);

	void remove_command(int);

	int ncommands();

	static void throw_error(const std::string& msg);

	struct error {
		error(const std::string& msg) : message(msg) {}
		std::string message;
	};

	void pool_2_config(config& cfg);

	static std::string last_replay_error;

	static void process_error(const std::string& msg);
private:
	//generic for add_movement and add_attack
	void add_pos(const std::string& type,
	             const map_location& a, const map_location& b);

	void add_value(const std::string& type, int value);

	void add_chat_log_entry(const config &speak, std::ostream &str) const;

	/** Adds a new empty command to the command list.
	 *
	 * @param update_random_context  If set to false, do not update the
	 *           random context variables: all random generation will take
	 *           place in the previous random context. Used for commands
	 *           for which "random context" is pointless, and which can be
	 *           issued while some other commands are still taking place,
	 *           like, for example, messages during combats.
	 *
	 * @return a pointer to the added command
	 */
	config* add_command(bool update_random_context = true);
	void config_2_command(const config& cfg, command* cmd);
	void command_2_config(command* cmd, config& cfg);

	config cfg_;
	int pos_;

	config* current_;

	bool skip_;

	config build_cfg_;

	std::vector<int> message_locations;

	/**
	 * A queue of units (locations) that are supposed to advance but the
	 * relevant advance (choice) message has not yet been received
	 */
	std::deque<unit*> expected_advancements_;
};

replay& get_replay_source();

extern replay recorder;

//replays up to one turn from the recorder object
//returns true if it got to the end of the turn without data running out
bool do_replay(int side_num, replay *obj = NULL);

bool do_replay_handle(int side_num, const std::string &do_untill);

//an object which can be made to undo a recorded move
//unless the transaction is confirmed
struct replay_undo
{
	replay_undo(replay& obj) : obj_(&obj) {}
	~replay_undo() { if(obj_) obj_->undo(); }
	void confirm_transaction() { obj_ = NULL; }

private:
	replay* obj_;
};

class replay_network_sender
{
public:
	replay_network_sender(replay& obj);
	~replay_network_sender();

	void sync_non_undoable();
	void commit_and_sync();
private:
	replay& obj_;
	int upto_;
};

namespace mp_sync {

/**
 * Interface for querying local choices.
 * It has to support querying the user and making a random choice from a
 * preseeded random generator.
 */
struct user_choice
{
	virtual ~user_choice() {}
	virtual config query_user() const = 0;
	virtual config random_choice(rand_rng::simple_rng &) const = 0;
};

/**
 * Performs a choice for WML events.
 *
 * The choice is synchronized across all the multiplayer clients and
 * stored into the replay. The function object is called if the local
 * client is responsible for making the choice.
 * @param name Tag used for storing the choice into the replay.
 * @param side The number of the side responsible for making the choice.
 *             If zero, it defaults to the currently active side.
 * @param force_sp If true, user choice will happen in prestart and start
 *                 events too. But if used for these events in multiplayer,
 *                 an exception will be thrown instead.
 *
 * @note In order to prevent issues with sync, crash, or infinite loop, a
 *       number of precautions must be taken when getting a choice from a
 *       specific side.
 *       - The calling function must enter a loop to wait for network sync
 *         if the side is non-local. This loop must end when a response is
 *         received in the replay.
 *       - The server must recognize @name replay commands as legal from
 *         non-active players. Preferably the server should be notified
 *         about which player the data is expected from, and discard data
 *         from unexpected players.
 *       - do_replay_handle must ignore the @name replay command when the
 *         originating player's turn is reached.
 */
config get_user_choice(const std::string &name, const user_choice &uch,
	int side = 0, bool force_sp = false);

}
#endif
