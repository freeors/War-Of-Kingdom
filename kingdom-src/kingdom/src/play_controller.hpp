/* $Id: play_controller.hpp 47661 2010-11-21 13:59:25Z mordante $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef PLAY_CONTROLLER_H_INCLUDED
#define PLAY_CONTROLLER_H_INCLUDED

#include "actions.hpp"
#include "controller_base.hpp"
#include "game_end_exceptions.hpp"
#include "help.hpp"
#include "map.hpp"
#include "menu_events.hpp"
#include "mouse_events.hpp"
#include "persist_manager.hpp"
#include "statistics.hpp"
#include "tod_manager.hpp"
#include "hero.hpp"
#include "card.hpp"

#include <boost/scoped_ptr.hpp>

class game_display;
class game_state;
class team;
struct wml_menu_item;

namespace game_events {
	struct manager;
} // namespace game_events

namespace halo {
	struct manager;
} // namespace halo

namespace preferences {
	struct display_manager;
}

namespace soundsource {
	class manager;
} // namespace soundsource


namespace tooltips {
	struct manager;
} // namespace tooltips

class play_controller : public controller_base, public events::observer
{
public:
	play_controller(const config& level, game_state& state_of_game, hero_map& heros, hero_map& heros_start, 
		card_map& cards,
		int ticks, int num_turns, const config& game_config, CVideo& video, bool skip_replay, bool replaying);
	virtual ~play_controller();

	//event handler, overridden from observer
	//there is nothing to handle in this class actually but that might change in the future
	virtual void handle_generic_event(const std::string& /*name*/) {}

	//event handlers, overridden from command_executor
	virtual void objectives();
	virtual void show_statistics();

	virtual void show_rpg();
	virtual void rpg_detail();
	virtual void assemble_treasure();
	virtual void rpg_exchange(const std::vector<size_t>& human_p, size_t ai_p);
	virtual void rpg_independence(bool replaying = false);

	virtual void save_game();
	virtual void save_replay();
	virtual void save_map();
	virtual void load_game();
	virtual void preferences();
	virtual void show_chat_log();
	virtual void show_help();
	virtual void undo();
	virtual void redo();
	virtual void toggle_ellipses();
	virtual void toggle_grid();
	virtual void interior();
	virtual void technology_tree();
	virtual void final_battle(int side_num, int human_capital, int ai_capital);
	virtual void list();
	virtual void system();
	virtual void unit_detail();
	virtual void switch_list();

	virtual void do_init_side(const unsigned int team_index);
	virtual void play_side(const unsigned int team_num, bool save) = 0;

	virtual void force_end_turn() = 0;
	virtual void force_end_level(LEVEL_RESULT res) = 0;
	virtual void check_end_level() = 0;
	/**
	 * Asks the user whether to continue on an OOS error.
	 * @throw end_level_exception If the user wants to abort.
	 */
	virtual void process_oos(const std::string& msg) const;

	virtual void sync_undo();

	bool do_recruit(artifical* city, int cost_exponent, bool rpg_mode);
	bool generate_commoner(artifical& city, const unit_type* ut, bool replay);
	void rpg_update();

	void calculate_commoner_gotos(team& current_team, std::vector<std::pair<unit*, int> >& gotos);
	void do_commoner(team& current_team);
	void commoner_back(artifical* entered_city, artifical* belong_city, const map_location& src_loc);
	map_location move_unit(bool troop, team& current_team, const std::pair<unit*, int>& pair, map_location to, bool dst_must_reachable = true);

	void set_victory_when_enemies_defeated(bool e)
	{ victory_when_enemies_defeated_ = e; }
	end_level_data &get_end_level_data()
	{ return end_level_data_; }

	bool ally_all_ai(bool force = false);
	/**
	 * Checks to see if a side has won, and throws an end_level_exception.
	 * Will also remove control of villages from sides with dead leaders.
	 */
	void check_victory();

	size_t turn() const {return tod_manager_.turn();}

	int current_side() const { return player_number_; }

	void to_config(config& cfg) const;

	bool is_skipping_replay() const { return skip_replay_;}
	bool is_replaying() const { return replaying_; }
	bool all_ai_allied() const { return all_ai_allied_; }
	bool fallen_to_unstage() const { return fallen_to_unstage_; }
	bool card_mode() const { return card_mode_; }
	int duel() const { return duel_; }
	
	std::vector<int>& treasures() { return treasures_; }
	const std::vector<int>& treasures() const { return treasures_; }
	void erase_treasure(int pos);

	void set_final_capital(artifical* human, artifical* ai);
	std::pair<artifical*, artifical*>& final_capital() { return final_capital_; }
	const std::pair<artifical*, artifical*>& final_capital() const { return final_capital_; }

	team& current_team();
	const team& current_team() const;

	bool is_linger_mode() const { return linger_; }
	const undo_list& undo_stack() const { return undo_stack_; }

	events::mouse_handler& get_mouse_handler_base();
	events::menu_handler& get_menu_handler() { return menu_handler_; }

	std::map< std::string, std::vector<unit_animation> > animation_cache;

	void show_context_menu(theme::menu* m, display& gui);
	void refresh_city_buttons(const artifical& city) const;

	artifical* decide_ai_capital() const;
	void recommend();
	bool do_ally(bool alignment, int my_side, int to_ally_side, int emissary_number, int target_side, int strategy_index);

	void construct_road();
	const std::map<std::pair<int, int>, std::vector<map_location> >& roads() const { return roads_; }
	const std::vector<map_location>& road(int a, int b) const;
	const std::vector<map_location>& road(const unit& u) const;
	
	int human_players();
	int first_human_team() const { return first_human_team_; }
	const config& level() const { return level_; }

	card_map& cards() { return cards_; }

protected:
	void slice_before_scroll();

	game_display& get_display();
	bool have_keyboard_focus();
	void process_focus_keydown_event(const SDL_Event& event);
	void process_keydown_event(const SDL_Event& event);
	void process_keyup_event(const SDL_Event& event);
	void post_mouse_press(const SDL_Event& event);

	virtual std::string get_action_image(hotkey::HOTKEY_COMMAND, int index) const;
	virtual hotkey::ACTION_STATE get_action_state(hotkey::HOTKEY_COMMAND command, int index) const;
	/** Check if a command can be executed. */
	virtual bool can_execute_command(hotkey::HOTKEY_COMMAND command, int index=-1) const;
	virtual bool execute_command(hotkey::HOTKEY_COMMAND command, int index=-1, std::string str = "");
	void show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu);
	
	/**
	 *  Determines whether the command should be in the context menu or not.
	 *  Independent of whether or not we can actually execute the command.
	 */
	bool in_context_menu(hotkey::HOTKEY_COMMAND command) const;
	bool enable_context_menu(hotkey::HOTKEY_COMMAND command, const map_location& loc) const;

	void init_managers();
	void fire_prestart(bool execute);
	void fire_start(bool execute);
	virtual void init_gui();
	virtual void init_side(const unsigned int team_index, bool is_replay = false, bool need_record = true);
	void place_sides_in_preferred_locations();
	virtual void finish_side_turn();
	void finish_turn();
	bool clear_shroud();
	bool enemies_visible() const;

	void enter_textbox();
	void tab();
	
	/** Find a human team (ie one we own) starting backwards from 'team_num'. */
	int find_human_team_before(const size_t team) const;

	//managers
	boost::scoped_ptr<preferences::display_manager> prefs_disp_manager_;
	boost::scoped_ptr<tooltips::manager> tooltips_manager_;
	boost::scoped_ptr<game_events::manager> events_manager_;
	boost::scoped_ptr<halo::manager> halo_manager_;
	font::floating_label_context labels_manager_;
	help::help_manager help_manager_;
	events::mouse_handler mouse_handler_;
	events::menu_handler menu_handler_;
	boost::scoped_ptr<soundsource::manager> soundsources_manager_;
	tod_manager tod_manager_;
	persist_manager persist_;

	//other objects
	boost::scoped_ptr<game_display> gui_;
	const statistics::scenario_context statistics_context_;
	const config& level_;
	std::vector<team> teams_;
	game_state& gamestate_;
	hero_map& heros_;
	hero_map& heros_start_;
	gamemap map_;
	unit_map units_;
	undo_list undo_stack_;
	card_map& cards_;

	const unit_type::experience_accelerator xp_mod_;
	//if a team is specified whose turn it is, it means we're loading a game
	//instead of starting a fresh one. Gets reset to false after init_side
	bool loading_game_;

	int first_human_team_;
	int player_number_;
	int first_player_;
	unsigned int start_turn_;
	bool is_host_;
	bool skip_replay_;
	bool replaying_;
	bool linger_;
	unsigned int previous_turn_;
	bool all_ai_allied_;
	std::pair<artifical*, artifical*> final_capital_;
	bool fallen_to_unstage_;
	bool card_mode_;
	int duel_;
	std::vector<int> treasures_;

	const std::string& select_victory_music() const;
	const std::string& select_defeat_music()  const;

	void set_victory_music_list(const std::string& list);
	void set_defeat_music_list(const std::string& list);

private:
	void init(CVideo &video);

	// Expand AUTOSAVES in the menu items, setting the real savenames.
	void expand_autosaves(std::vector<std::string>& items);
	std::vector<std::string> savenames_;

	void expand_wml_commands(std::vector<std::string>& items);
	std::vector<wml_menu_item *> wml_commands_;
	static const size_t MAX_WML_COMMANDS = 7;

	int human_players_;
	bool rpging_;

	unit** troops_cache_;
	size_t troops_cache_vsize_;

	std::map<std::pair<int, int>, std::vector<map_location> > roads_;
	
	bool victory_when_enemies_defeated_;
	end_level_data end_level_data_;
	std::vector<std::string> victory_music_;
	std::vector<std::string> defeat_music_;
};


#endif
