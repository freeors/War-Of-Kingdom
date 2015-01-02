/* $Id: play_controller.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/**
 *  @file
 *  Handle input via mouse & keyboard, events, schedule commands.
 */

#include "play_controller.hpp"
#include "dialogs.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "halo.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "terrain_filter.hpp"
#include "save_blocker.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "soundsource.hpp"
#include "tooltips.hpp"
#include "game_preferences.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "ai/manager.hpp"
#include "scripting/lua.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "playcampaign.hpp"
#include "multiplayer.hpp"
#include "version.hpp"

#include "gui/dialogs/system.hpp"

#include "gui/dialogs/employ.hpp"
#include "gui/dialogs/camp_armory.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/recruit.hpp"
#include "gui/dialogs/rpg_detail.hpp"
#include "gui/dialogs/treasure.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "gui/dialogs/final_battle.hpp"
#include "gui/dialogs/exchange.hpp"
#include "gui/dialogs/independence.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/noble.hpp"
#include "gui/dialogs/square.hpp"
#include "gui/dialogs/prelude.hpp"
#include "gui/dialogs/book.hpp"
#include "gui/dialogs/side_report.hpp"
#include "gui/widgets/window.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)
#define DBG_NG LOG_STREAM(debug, log_engine)

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)

namespace null_val {
	const std::vector<map_location> vector_location;
}

void show_tick_infromation(game_display& disp, const unit_map& units, const play_controller& controller)
{
	std::stringstream info;

	info << (controller.is_replaying()? "[Replay]": "[Runtime]") << "  ";
	info << "main_ticks: " << unit_map::main_ticks << "    turn: " << controller.turn() << "\n";

	size_t index = 0;
	for (unit_map::const_iterator it = units.begin(); it != units.end(); ++ it) {
		const unit& u = *it;
		if (!u.consider_ticks()) {
			// continue;
		}
		info << "[" << index << "](" << u.master().name() << "(" << u.get_location() << "),T:" << u.ticks() << " INC:" << u.ticks_increase() << ";  ";
		index ++;
	}
	gui2::show_message(disp.video(), "", info.str());
}

static void clear_resources()
{
	resources::game_map = NULL;
	resources::units = NULL;
	resources::heros = NULL;
	resources::teams = NULL;
	resources::state_of_game = NULL;
	resources::controller = NULL;
	resources::screen = NULL;
	resources::soundsources = NULL;
	resources::tod_manager = NULL;
	resources::persist = NULL;
}

play_controller::play_controller(const config& level, game_state& state_of_game, hero_map& heros, hero_map& heros_start,
		card_map& cards,
		int ticks, int num_turns, const config& game_config, CVideo& video,
		bool skip_replay, bool replaying) :
	controller_base(ticks, game_config, video),
	observer(),
	animation_cache(),
	prefs_disp_manager_(),
	tooltips_manager_(),
	events_manager_(),
	halo_manager_(),
	mouse_handler_(*this, NULL, teams_, units_, heros, map_, tod_manager_, state_of_game, undo_stack_),
	menu_handler_(*this, NULL, units_, heros, cards, teams_, level, map_, game_config, tod_manager_, state_of_game, undo_stack_),
	soundsources_manager_(),
	tod_manager_(level, num_turns, &state_of_game),
	persist_(),
	gui_(),
	statistics_context_(level["name"]),
	level_(level),
	teams_(),
	gamestate_(state_of_game),
	map_(game_config, level["map_data"]),
	units_(),
	heros_(heros),
	heros_start_(heros_start),
	cards_(cards),
	undo_stack_(),
	xp_mod_(level["experience_modifier"].to_int(100)),
	loading_game_(level["playing_team"].empty() == false),
	human_team_(-1),
	player_number_(1),
	first_player_(level_["playing_team"].to_int() + 1),
	has_network_player_(false),
	start_turn_(tod_manager_.turn()), // tod_manager_ constructed above
	is_host_(true),
	skip_replay_(skip_replay),
	replaying_(replaying),
	linger_(false),
	end_turn_(false),
	previous_turn_(0),
	savenames_(),
	wml_commands_(),
	victory_when_enemy_no_city_(true),
	end_level_data_(),
	victory_music_(),
	defeat_music_(),
	all_ai_allied_(level_["ai_allied"].to_bool()),
	final_capital_(std::make_pair(reinterpret_cast<artifical*>(NULL), reinterpret_cast<artifical*>(NULL))),
	rpging_(false),
	allow_intervene_(false),
	allow_active_(false),
	troops_cache_(NULL),
	troops_cache_vsize_(0),
	roads_(),
	renames_(),
	fallen_to_unstage_(level_["fallen_to_unstage"].to_bool()),
	card_mode_(level_["card_mode"].to_bool()),
	duel_(RANDOM_DUEL),
	vip_(false),
	more_card_(false),
	tactic_slot_(false),
	difficulty_level_(0),
	pause_when_human_(true),
	treasures_(), 
	emploies_(),
	autosave_ticks_(0)
{
	resources::game_map = &map_;
	resources::units = &units_;
	resources::heros = &heros_;
	resources::teams = &teams_;
	resources::state_of_game = &gamestate_;
	resources::controller = this;
	resources::tod_manager = &tod_manager_;
	resources::persist = &persist_;
	persist_.start_transaction();

	unit_map::top_side = first_player_;

	troops_cache_vsize_ = 40;
	troops_cache_ = (unit**)malloc(troops_cache_vsize_ * sizeof(unit*));

	// Setup victory and defeat music
	set_victory_music_list(level_["victory_music"]);
	set_defeat_music_list(level_["defeat_music"]);

	game_config::add_color_info(level);
	hotkey::deactivate_all_scopes();
	hotkey::set_scope_active(hotkey::SCOPE_GENERAL);
	hotkey::set_scope_active(hotkey::SCOPE_GAME);
	try {
		init(video);
	
		// Setup treasures
		std::vector<std::string> vstr = utils::split(level["treasures"].str());
		const std::vector<ttreasure>& treasures = unit_types.treasures();
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			int t = lexical_cast_default<int>(*it, -1);
			if (t < 0 || t >= (int)treasures.size()) {
				throw game::load_game_failed("treasures");
			}
			treasures_.push_back(t);
		}

		// emploies
		vstr = utils::split(level["emploies"]);
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			int number = lexical_cast_default<int>(*it, HEROS_INVALID_NUMBER);
			if (number == HEROS_INVALID_NUMBER || number >= (int)heros_.size()) {
				throw game::load_game_failed("emploies, invalid number!");
			}
			hero& h = heros_[number];
			if (std::find(emploies_.begin(), emploies_.end(), &h) != emploies_.end()) {
				throw game::load_game_failed("emploies, duplicate number!");
			}
			emploies_.push_back(&h);
		}

		if (!level["final_capital"].empty()) {
			const std::vector<std::string> capitals = utils::split(level_["final_capital"]);
			final_capital_.first = units_.city_from_cityno(lexical_cast_default<int>(capitals[0]));
			final_capital_.second = units_.city_from_cityno(lexical_cast_default<int>(capitals[1]));
		}
		
		vstr = utils::split(level["renames"].str());
		for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
			int number = lexical_cast_default<int>(*it, HEROS_INVALID_NUMBER);
			++ it;
			insert_rename(number, *it);
		}

		// if sceanrio is set, derive it. keep runtime synthnize with replay.
		if (!level["maximal_defeated_activity"].empty()) {
			game_config::maximal_defeated_activity = level["maximal_defeated_activity"].to_int(100);
		} else {
			// FIXME: this will result game_config::maximal_defeated_activity set became invalid!
			game_config::maximal_defeated_activity = 0;
		}
		if (tent::duel == -1) {
			if (level.has_attribute("duel")) {
				const std::string& duel = level["duel"];
				if (duel == "no") {
					duel_ = NO_DUEL;
				} else if (duel == "always") {
					duel_ = ALWAYS_DUEL;
				} else {
					duel_ = RANDOM_DUEL;
				}
			}
		} else {
			duel_ = tent::duel;
			tent::duel = -1;
		}
				
	} catch (...) {
		clear_resources();
		throw;
	}
}

std::string vector_2_str(const std::vector<size_t>& vec)
{
	std::stringstream str;
	if (!vec.empty()) {
		bool first = true;
		str.str("");
		for (std::vector<size_t>::const_iterator i = vec.begin(); i != vec.end(); ++ i) {
			if (!first) {
				str << ",";
			} else {
				first = false;
			}
			str << *i;
		}
	}
	return str.str();
}

play_controller::~play_controller()
{
	// update player variables from human team.
	if (!gamestate_.get_variables().empty()) {
		// when load-game use menu command, gamestate_.get_variables() is empty. in this case don't update player variable.
		for (size_t side = 0; side != teams_.size(); side ++) {
			if (teams_[side].is_human()) {
				team& t = teams_[side];
				const std::vector<size_t>& holded_cards = t.holded_cards();
				gamestate_.set_variable("player.holded_cards", vector_2_str(holded_cards));

				gamestate_.set_variable("player.navigation", lexical_cast<std::string>(t.navigation()));
				gamestate_.set_variable("player.stratum", lexical_cast<std::string>(rpg::stratum));

				// gamestate_.write_armory_2_variable(t);
				break;
			}
		}
	}
	
	clear_resources();

	if (troops_cache_) {
		free(troops_cache_);
	}
}

struct tlayout_context {
	tlayout_context(int t, bool mirror, bool stratagem, bool wall, bool station, bool spend, bool fresh_to)
		: t(t)
		, mirror(mirror)
		, stratagem(stratagem)
		, wall(wall)
		, station(station)
		, spend(spend)
		, fresh_to(fresh_to)
	{}

	int t;
	bool mirror;
	bool stratagem;
	bool wall;
	bool station;
	bool spend;
	bool fresh_to;
};

void play_controller::recover_layout(int random)
{
	std::vector<tlayout_context> desire_layout;
	if (tent::mode == mode_tag::LAYOUT) {
		desire_layout.push_back(tlayout_context(human_team_, false, true, true, false, false, false));

	} else if (tent::mode == mode_tag::SIEGE) {
		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			if (t.side() > 2) {
				break;
			}
			if (t.is_human()) {
				desire_layout.push_back(tlayout_context(t.side() - 1, true, false, false, false, true, true));

			} else if (t.is_ai()) {
				desire_layout.push_back(tlayout_context(t.side() - 1, false, true, true, true, false, true));
			}
		}
	}
	for (std::vector<tlayout_context>::const_iterator it = desire_layout.begin(); it != desire_layout.end(); ++ it) {
		const tlayout_context& l = *it;
		team& current_team = teams_[l.t];
		tgroup& g = runtime_groups::get(current_team.leader()->number_);
		g.layout_to_team(*gui_, units_, heros_, teams_, gamestate_, current_team, l.mirror, l.stratagem, l.wall);

		if (it->fresh_to) {
			current_team.fresh_to_field_troop(random);
		}
		std::pair<unit**, size_t> fields = current_team.field_troop();
		unit** troops = fields.first;
		size_t troops_vsize = fields.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			unit& u = *troops[i];

			if (!u.is_artifical()) {
				if (l.spend) {
					current_team.spend_gold(u.cost());
				}
				if (l.station) {
					u.set_task(unit::TASK_STATION);
				}
			}
		}
		if (current_team.gold() < 0) {
			current_team.set_gold(0);
		}
	}
}

surface generate_rpg_surface(hero& h)
{
	surface genus_surf = image::get_image(unit_types.genus(tent::turn_based? tgenus::TURN_BASED: tgenus::HALF_REALTIME).icon());
	surface hero_surf = image::get_image(h.image());
	surface masked_surf = mask_surface(hero_surf, image::get_image("buttons/photo-mask.png"));
	blit_surface(genus_surf, NULL, masked_surf, NULL);
	return masked_surf;
}

void play_controller::init(CVideo& video)
{
	provoke_cache_ready = false;
	slot_cache::ready = false;
	guard_cache::ready = false;
	uint32_t start = SDL_GetTicks();

	util::scoped_resource<loadscreen::global_loadscreen_manager*, util::delete_item> scoped_loadscreen_manager;
	loadscreen::global_loadscreen_manager* loadscreen_manager = loadscreen::global_loadscreen_manager::get();
	if (!loadscreen_manager) {
		scoped_loadscreen_manager.assign(new loadscreen::global_loadscreen_manager(video));
		loadscreen_manager = scoped_loadscreen_manager.get();
	}

	loadscreen::start_stage("load level");
	// If the recorder has no event, adds an "game start" event
	// to the recorder, whose only goal is to initialize the RNG
	bool tent_valid = false;
	if (recorder.empty()) {
		recorder.add_start();
		tent_valid = true;
	} else {
		recorder.pre_replay();
	}
	recorder.set_skip(false);

	bool snapshot = level_["snapshot"].to_bool();

	BOOST_FOREACH (const config &t, level_.child_range("time_area")) {
		tod_manager_.add_time_area(t);
	}

	// en-clean unit_map
	units_.create_coor_map(resources::game_map->w(), resources::game_map->h());

	while (!replaying_ && !level_["tent"].to_bool()) {
		config& player_cfg = gamestate_.get_variables().child("player");
		if (!player_cfg) {
			break;
		}
		player_cfg.clear_children("armory");
		break;

		config& armory_cfg = player_cfg.child("armory");
		if (!armory_cfg) {
			break;
		}
		// armory_cfg.remove_attribute("heros");
		// armory_cfg.clear_children("unit");
		break;
	}

	if (!replaying_ && level_.child("prelude")) {
		game_display& disp = *game_display::get_singleton();
		gui2::tprelude dlg(disp, heros_, game_config_, level_);
		dlg.show(disp.video());
	}

/*
	while (!replaying_ && level_.child_count("camp")) {
		std::vector<unit*> candidate_troops;
		std::vector<hero*> candidate_heros;

		config& player_cfg = gamestate_.get_variables().child("player");
		if (!player_cfg) {
			break;
		}
		tent::leader = &heros_[player_cfg["leader"].to_int()];

		config& armory_cfg = player_cfg.child("armory");
		if (!armory_cfg) {
			break;
		}
		// troops of armory
		BOOST_FOREACH (config &i, armory_cfg.child_range("unit")) {
			// i["cityno"] = 0;
			unit* u = new unit(units_, heros_, teams_, i); // ???
			u->new_turn();
			// u->heal_all();
			u->set_state(ustate_tag::SLOWED, false);
			u->set_state(ustate_tag::BROKEN, false);
			u->set_state(ustate_tag::POISONED, false);
			u->set_state(ustate_tag::PETRIFIED, false);
			if (u->packed()) {
				u->pack_to(NULL);
			}
			candidate_troops.push_back(u);
		}

		// heros of armory
		const std::vector<std::string> armory_heros = utils::split(armory_cfg["heros"]);
		for (std::vector<std::string>::const_iterator itor = armory_heros.begin(); itor != armory_heros.end(); ++ itor) {
			candidate_heros.push_back(&heros_[lexical_cast_default<int>(*itor)]);
		}

		gui2::tcamp_armory dlg(candidate_troops, candidate_heros);
		dlg.show(video);
		int res = dlg.get_retval();
		if (res == gui2::twindow::CANCEL) {
			// release resource
			for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
				delete *itor;
			}
			candidate_troops.clear();
			candidate_heros.clear();
			throw game::load_game_failed("init");
		}
		// save result to [player].[armory]
		std::stringstream armory_heros_str;

		armory_cfg.clear_children("unit");

		for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
			config& u = armory_cfg.add_child("unit");
			(*itor)->get_location().write(u);
			(*itor)->write(u);
		}

		for (std::vector<hero*>::iterator h = candidate_heros.begin(); h != candidate_heros.end(); ++ h) {
			if (!armory_heros_str.str().empty()) {
				armory_heros_str << ", ";
			}
			armory_heros_str << lexical_cast<std::string>((*h)->number_);
		}
		armory_cfg["heros"] = armory_heros_str.str();

		// (support replay)save result to starting_pos. it is sure that there is armory block in starting_pos(reference play_game in playcampaign.cpp).
		// gui2::tcamp_armory may change [armory], write changed [armory] to starting_pos.
		config& player_cfg_dst = gamestate_.starting_pos.child("variables").child("player");
		player_cfg_dst.clear_children("armory");

		player_cfg_dst.add_child("armory", armory_cfg);

		// release resource
		for (std::vector<unit*>::iterator itor = candidate_troops.begin(); itor != candidate_troops.end(); ++ itor) {
			delete *itor;
		}
		candidate_troops.clear();
		candidate_heros.clear();
		break;
	}
*/
	// initialized teams...
	loadscreen::start_stage("init teams");

	// This *needs* to be created before the show_intro and show_map_scene
	// as that functions use the manager state_of_game
	// Has to be done before registering any events!
	events_manager_.reset(new game_events::manager(level_));

	// if restart, rpg::h's memory maybe change. reset.
	rpg::h = &hero_invalid;
	rpg::stratum = hero_stratum_wander;
	team::empty_side = -1;

	tent::mode = mode_tag::find(gamestate_.classification().mode);
	VALIDATE(tent::mode != mode_tag::NONE, "predefined mode is invalid!");
	
	if (!tent::tower_mode()) {
		increase_xp::ability_per_xp = 1;
		increase_xp::arms_per_xp = 3;
		increase_xp::skill_per_xp = 16;
		increase_xp::meritorious_per_xp = 5;
		increase_xp::navigation_per_xp = 2;
	} else {
		increase_xp::ability_per_xp = 4;
		increase_xp::arms_per_xp = 12;
		increase_xp::skill_per_xp = 64;
		increase_xp::meritorious_per_xp = 20;
		increase_xp::navigation_per_xp = 8;
	}
	if (level_["modify_placing"].to_bool()) {
		// modifying placing...
		place_sides_in_preferred_locations();
	}

	// game genus
	VALIDATE(level_.has_attribute("turn_based"), "level must exist turn_based!");
	tent::turn_based = level_["turn_based"].to_bool();

	// main_ticks
	unit_map::main_ticks = level_["main_ticks"].to_int();

	// main_ticks
	autosave_ticks_ = level_["autosave_ticks"].to_int(-1);

	// main_ticks
	unit::global_signature = level_["global_signature"].to_int();

	if (level_.has_attribute("subcontinent")) {
		tent::subcontient_from_str(level_["subcontinent"]);
	}

	config& player_cfg = gamestate_.get_variables().child("player");
	if (!player_cfg || !player_cfg.has_attribute("hero")) {
		rpg::stratum = hero_stratum_leader;
	} else {
		rpg::stratum = player_cfg["stratum"].to_int();
	}

	std::stringstream strstr;
	int random = -1;
	int team_num = 0;
	size_t team_size = 0;
	config side_cfg;
	if (level_.child_count("side")) {
		if (!replaying_) {
			runtime_groups::gs.clear();
			runtime_groups::insert(group);
			for (std::map<int, tgroup>::iterator it = other_group.begin(); it != other_group.end(); ++ it) {
				runtime_groups::insert(it->second);
			}
			// set group invalid.
			group.reset();
		}

		// calculate count of team.
		BOOST_FOREACH (const config &side, level_.child_range("side")) {
			if (side["controller"].str() == "null" && side["leader"].to_int() == hero::number_empty_leader) {
				team_size ++;
			}
		}
		VALIDATE(team_size <= 1, "play_controller::init, there is at most one null-empty_leader side in scenario");

		if (team_size) {
			team_size = level_.child_count("side");
		} else {
			team_size = level_.child_count("side") + 1;
		}
		if (tent::mode == mode_tag::RPG) {
			team_size ++;
		}

		// [side] ---> class team
		BOOST_FOREACH (const config &side, level_.child_range("side")) {
			side_cfg = side;
			game_events::wml_expand_if(side_cfg);
			gamestate_.build_team(side_cfg, teams_, level_, map_, units_, heros_, cards_, snapshot, team_size);

			++ team_num;
		}
		// create autonomy team if necessary.
		if (team::empty_side == -1) {
			side_cfg.clear();
			side_cfg["side"] = team_num + 1;
			side_cfg["controller"] = "null";
			gamestate_.build_team(side_cfg, teams_, level_, map_, units_, heros_, cards_, snapshot, team_size);
			++ team_num;
		}
		// if select rpg, create 
		if (tent::mode == mode_tag::RPG) {
			if (gamestate_.classification().campaign.find("subcontinent_") == 0) {
				// browse continent mode. verify city location.
				int total_siege_map_w = game_config::siege_map_w + 2;
				int total_siege_map_h = game_config::siege_map_h + 2;
				const city_map& citys = units_.get_city_map();
				SDL_Point lt, rb;
				lt.x = 1;
				lt.y = 3;
				rb.x = map_.w() - (game_config::siege_map_w - lt.x);
				rb.y = map_.h() - (game_config::siege_map_h - lt.y);
				std::stringstream err;
				utils::string_map symbols;
				for (city_map::const_iterator it = citys.begin(); it != citys.end(); ++ it) {
					const artifical& city = *it;
					const map_location& loc = city.get_location();
					// 1) x-axis, city place to left.
					// 2) y-axis, above and bottom.
					if (loc.x < lt.x || loc.x > rb.x || loc.y < lt.y || loc.y > rb.y) {
						symbols["name"] = tintegrate::generate_format(city.name(), "yellow");
						symbols["x"] = tintegrate::generate_format(loc.x + 1, "red");
						symbols["y"] = tintegrate::generate_format(loc.y + 1, "red");
						err << vgettext("wesnoth-lib", "Map is for subcontinent. Find improper city location: $name($x, $y).", symbols);
						break;
					}
				}
				if (!err.str().empty()) {
					err << "\n";
					symbols["x1"] = str_cast(lt.x + 1);
					symbols["y1"] = str_cast(lt.y + 1);
					symbols["x2"] = str_cast(rb.x + 1);
					symbols["y2"] = str_cast(rb.y + 1);
					err << vgettext("wesnoth-lib", "Location.x of city must be in [$x1, $x2], y in [$y1, $y2]!", symbols);
					throw game::load_game_failed(err.str());
				}
			}
			side_cfg.clear();
			side_cfg["side"] = (int)teams_.size() + 1;
			int leader = player_cfg["leader"].to_int();
			if (leader != hero_map::map_size_from_dat) {
				side_cfg["controller"] = "null";
				leader = player_cfg["hero"].to_int();

			} else {
				side_cfg["controller"] = "human";
				side_cfg["gold"] = "200";
				
				side_cfg["candidate_cards"] = "$player.candidate_cards";

				strstr.str("");
				strstr << unit_types.find_wall()->id();
				strstr << ", " << unit_types.find_market()->id();
				strstr << ", " << unit_types.find_technology()->id();
				strstr << ", " << unit_types.find_tactic()->id();
				// strstr << ", " << unit_types.find_tower()->id();
				side_cfg["build"] = strstr.str();
			}
			side_cfg["leader"] = leader;
			gamestate_.build_team(side_cfg, teams_, level_, map_, units_, heros_, cards_, snapshot, team_size);

			team& t = teams_.back();
			if (leader == hero_map::map_size_from_dat && rpg::stratum == hero_stratum_leader) {
				if (random == -1) {
					random = gamestate_.rng().get_next_random();
				}
				hero& city_h = heros_[player_cfg["city"].to_int()];
				VALIDATE(city_h.city_ != HEROS_ROAM_CITY, std::string("play_controller::init, not set valid city of player hero: ") + city_h.name());
				artifical* city = units_.city_from_cityno(city_h.city_);

				team& from_team = teams_[city->side() - 1];
				from_team.erase_city(city);

				city->set_side(t.side());
				t.add_city(city);
				city->adjust();
				// change side, require resort. by far, game_display is invalid, so use resort_map direct.
				units_.resort_map(*city);

				// wander all hero to other city, and erase it.
				std::vector<artifical*> citys;
				for (size_t i = 0; i < teams_.size(); i ++) {
					if (i == t.side() - 1) {
						continue;
					}
					std::vector<artifical*>& side_citys = teams_[i].holden_cities();
					citys.insert(citys.end(), side_citys.begin(), side_citys.end());
				}
				std::vector<hero*>& wanders = city->wander_heros();
				for (std::vector<hero*>::iterator it = wanders.begin(); it != wanders.end();) {
					hero& h = **it;
					artifical* selected = citys[(random + h.number_) % citys.size()];
					selected->wander_into(h, false);
					it = wanders.erase(it);
				}

				tgroup& g = runtime_groups::get(leader);
				if (!replaying_) {
					side_cfg.clear();
					config& sub_s = side_cfg.add_child("artifical");
					g.to_side_cfg(side_cfg, random, game_config::rpg_fix_members);
					const std::vector<std::string> vstr = utils::split(sub_s["service_heros"]);
					for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
						city->fresh_into(heros_[lexical_cast_default<int>(*it)]);
					}
				} else {
					const std::vector<tgroup::tmember>& members = g.members();
					for (std::vector<tgroup::tmember>::const_iterator it = members.begin(); it != members.end(); ++ it) {
						const tgroup::tmember& m = *it;
						city->fresh_into(*m.h);
					}
					const std::vector<tgroup::tmember>& exiles = g.exiles();
					for (std::vector<tgroup::tmember>::const_iterator it = exiles.begin(); it != exiles.end(); ++ it) {
						const tgroup::tmember& m = *it;
						city->fresh_into(*m.h);
					}
				}
			} else if (leader == hero_map::map_size_from_dat) {
				t.leader()->official_ = HEROS_NO_OFFICIAL;
			}
		}
		runtime_groups::remove_unstage_member();
	} else {
		unit_segment2* sides = (unit_segment2*)game_config::savegame_cache;
		int offset = sizeof(unit_segment2);
		for (uint32_t i = 0; i < sides->count_; i ++) {
			gamestate_.build_team(game_config::savegame_cache + offset, teams_, level_, map_, units_, heros_, cards_, snapshot, sides->count_);
			offset += ((unit_segment2*)(game_config::savegame_cache + offset))->size_;

			++ team_num;
		}
	}
	tent::lobby_side_params.clear();

	// adjust controller according to controller and current_player
	if (tent::io_type == IO_NONE) {
		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			if (t.controller() == controller_tag::NETWORK) {
				// this is multiplay save, and load by local. will play in single-player mode.
				// reset all human troop
				t.change_controller(controller_tag::AI);
			}
		}
	} else {
		// IO_SERVER/IO_CLIENT
		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			if (t.current_player() == preferences::login()) {
				if (t.controller() != controller_tag::HUMAN) {
					if (t.controller() != controller_tag::NETWORK) {
						t.change_controller(controller_tag::HUMAN);
					} else {
						t.make_human();
					}
				}
			} else if (!t.current_player().empty()) {
				if (t.controller() != controller_tag::NETWORK) {
					// this is multiplay save, and load by lobby, and change local to network.
					t.make_network();
				}

			} else if (t.controller() == controller_tag::HUMAN || t.controller() == controller_tag::NETWORK) {
				// this is multiplay save, and load by lobby, and change network to ai.
				// reset all human troop
				t.change_controller(controller_tag::AI);

			} else if (tent::io_type == IO_CLIENT && t.controller() == controller_tag::AI) {
				/// @todo Fix logic to use network_ai controller
				// if it is networked ai
				t.make_network();
			}
		}
	}

	for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
		team& t = *it;
		if (t.is_human()) {
			VALIDATE(human_team_ == -1, "Only allow define one human team.");
			human_team_ = t.side() - 1;
		}
		if (t.is_network()) {
			has_network_player_ = true;
		}
	}
	VALIDATE(human_team_ >= 0, "Must define one human team at least.");

	if (!player_cfg || !player_cfg.has_attribute("hero")) {
		team& human = teams_[human_team_];
		rpg::h = &heros_[human.leader()->number_];
		
	} else {
		rpg::h = &heros_[player_cfg["hero"].to_int()];
		if (rpg::h->status_ == hero_status_unstage) {
			// this is player hero. allocat it to proper city.
			// HEROS_NO_OFFICIAL
			hero& city_h = heros_[player_cfg["city"].to_int()];
			VALIDATE(city_h.city_ != HEROS_ROAM_CITY, std::string("play_controller::init, not set valid city of player hero: ") + city_h.name());
			artifical* city = units_.city_from_cityno(city_h.city_);
			city->fresh_into(*rpg::h);
		}
	}

	if (level_.child_count("side")) {
		// if necessary, set begin fields troop to human.
		team& current_team = teams_[rpg::h->side_];

		const std::pair<unit**, size_t> p = current_team.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (troops[i]->is_artifical()) {
				continue;
			}
			if (rpg::stratum == hero_stratum_leader) {
				troops[i]->set_human(true);
			} else if (rpg::stratum == hero_stratum_mayor) {
				if (troops[i]->cityno() == rpg::h->city_) {
					troops[i]->set_human(true);
				}
			} else if (rpg::stratum == hero_stratum_citizen) {
				if (&troops[i]->master() == rpg::h || &troops[i]->second() == rpg::h || &troops[i]->third() == rpg::h) {
					troops[i]->set_human(true);
				}
			}
		}

		// generate fort from villages
		const std::vector<map_location>& villages = map_.villages();
		const unit_type* ut = unit_types.find_fort();
		for (std::vector<map_location>::const_iterator it = villages.begin(); it != villages.end(); ++ it) {
			const map_location& vill_loc = *it;

			std::vector<const hero*> v;
			v.push_back(&heros_[ut->master()]);

			type_heros_pair pair(ut, v);
			artifical new_art(units_, heros_, teams_, gamestate_, pair, 0, true);
			units_.add(vill_loc, &new_art);
		}

	}

	if (level_.has_attribute("employ_count") || level_.has_attribute("ai_count")) {
		if (random == -1) {
			random = gamestate_.rng().get_next_random();
		}
		std::set<int> candidate;
		for (hero_map::iterator it = heros_.begin(); it != heros_.end(); ++ it) {
			hero& h = *it;
			if (h.number_ >= hero_map::map_size_from_dat) {
				break;
			}
			if (hero::is_system(h.number_) || hero::is_soldier(h.number_) || hero::is_commoner(h.number_)) {
				continue;
			}
			if (h.get_flag(hero_flag_robber)) {
				continue;
			}
			if (h.status_ == hero_status_unstage && h.gender_ != hero_gender_neutral) {
				candidate.insert(h.number_);
			}
		}
		int employ_count = level_["employ_count"].to_int();
		if (employ_count) {
			fill_employ_hero(*rpg::h, candidate, employ_count, random);
		}
	
		int ai_count = level_["ai_count"].to_int();
		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			if (!t.is_ai() && !t.is_human()) continue;
			std::vector<artifical*>& holded = t.holden_cities();
			for (std::vector<artifical*>::iterator it2 = holded.begin(); it2 != holded.end(); ++ it2) {
				artifical& c = **it2;
				std::vector<hero*>& freshes = c.fresh_heros();
				std::vector<hero*>& wanders = c.wander_heros();
				if (t.is_ai()) {
					c.fill_ai_hero(candidate, ai_count, random);
				} else if (tent::mode == mode_tag::TOWER) {
					const int min_fix_wanders = 12;
					int fill_freshes = (int)freshes.size() >= game_config::tower_fix_heros? 0: game_config::tower_fix_heros - freshes.size();
					int fill_wanders = (int)wanders.size() >= min_fix_wanders? 0: min_fix_wanders - wanders.size();
					if (fill_freshes || fill_wanders) {
						c.fill_human_hero(*t.leader(), candidate, fill_freshes, fill_wanders, random);
					}
				}
				if (t.leader()->number_ == hero::number_empty_leader && !freshes.empty()) {
					hero* l = c.select_leader(random);
					if (l) {
						t.set_leader(*l);
					}
				}
			}
		}
	}

	// mouse_handler expects at least one team for linger mode to work.
	if (teams_.empty()) end_level_data_.linger_mode = false;

	// loading units...
	loadscreen::start_stage("load units");
	preferences::encounter_recruitable_units(teams_);
	preferences::encounter_start_units(units_);
	preferences::encounter_recallable_units(teams_);
	preferences::encounter_map_terrain(map_);

	// initializing theme...
	loadscreen::start_stage("init theme");
	const config &theme_cfg = get_theme(game_config_, level_["theme"]);

	// end scenario-outer animation

	// building terrain rules...
	loadscreen::start_stage("build terrain");
	gui_.reset(new game_display(units_, heros_, this, video, map_, tod_manager_, teams_, theme_cfg, level_));
	
	gui_->widget_set_surface("rpg", generate_rpg_surface(*rpg::h));

	for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
		team& t = *it;
		t.calculate_strategy_according_to_mode();
		allow_intervene_ |= t.allow_intervene;
		allow_active_ |= t.allow_active;
	}

	VALIDATE(!allow_intervene_ || !allow_active_, "intervent and active must not same be true!");

	if (level_.child_count("side")) {
		calculate_difficulty_level();

		gamestate_.classification().create = (long)time(NULL);
		if (random == -1) {
			random = gamestate_.rng().get_next_random();
		}
		gamestate_.classification().hash = random % 0xffff;

		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			t.select_leader_noble(false);
			if (t.is_empty()) {
				continue;
			}
			const hero& leader = *t.leader();
			if (leader.city_ != HEROS_ROAM_CITY) {
				t.set_capital(units_.city_from_cityno(leader.city_));
			}
		}

		recover_layout(random);

		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			t.fill_normal_noble(true, false);

			// check ally config
			const std::map<int, diplomatism_data>& diplomatisms = t.diplomatisms();
			for (size_t i = 1; i <= teams_.size(); ++ i) {
				if (i == t.side()) {
					continue;
				}
				if (t.is_enemy(i) != teams_[i - 1].is_enemy(t.side())) {
					std::stringstream err;
					err << "ally error, " << t.name() << " and " << teams_[i - 1].name();
					VALIDATE(false, err.str());
				}
			}
		}
	} else {
		difficulty_level_ = level_["difficulty_level"].to_int();
	}

	gamestate_.set_session_start(time(NULL));
	
	str_2_provoke_cache(units_, heros_, level_["provoke_cache"]);
	slot_cache::str_2_cache(units_, heros_, level_["tactic_cache"]);
	guard_cache::str_2_cache(units_, heros_, level_["guard_cache"]);

	loadscreen::start_stage("init display");
	mouse_handler_.set_gui(gui_.get());
	menu_handler_.set_gui(gui_.get());
	resources::screen = gui_.get();
	theme::set_known_themes(&game_config_);

	// done initializing display...
	if (human_team_ != -1) {
		gui_->set_team(human_team_);
		if (tent::tower_mode()) {
			std::vector<artifical*>& cities = teams_[human_team_].holden_cities();
			for (std::vector<artifical*>::iterator it = cities.begin(); it != cities.end(); ++ it) {
				artifical& city = **it;
				if (!city.mayor()->valid()) {
					city.select_mayor(NULL, false);
				}
			}
		}

	} else if (is_observer()) {
		// Find first team that is allowed to be observered.
		// If not set here observer would be without fog untill
		// the first turn of observerable side
		size_t i;
		for (i=0;i < teams_.size();++i) {
			if (!teams_[i].get_disallow_observers()) {
				gui_->set_team(i);
			}
		}
	}

	browse_ = true;

	init_managers();
	// add era events for MP game
	if (const config &era_cfg = level_.child("era")) {
		game_events::add_events(era_cfg.child_range("event"), "era_events");
	}

	loadscreen::global_loadscreen->start_stage("draw theme");

	// construct roads
	construct_road();

	uint32_t stop = SDL_GetTicks();
	posix_print("play_controller::init, used time: %u ms\n", stop - start);
}

void play_controller::adjust_according_to_group_interior()
{
	// if human_leader is player, modify group's according to group interior.
	if (rpg::stratum != hero_stratum_leader || (rpg::h->number_ != hero_map::map_size_from_dat && !has_network_player_)) {
		return;
	}
	std::vector<std::pair<int, int> > offsets;
/*
	offsets.push_back(std::make_pair(20, 50));
	offsets.push_back(std::make_pair(80, 50));
	offsets.push_back(std::make_pair(20, 95));
	offsets.push_back(std::make_pair(80, 95));
*/
	offsets.push_back(std::make_pair(50, 60));
	offsets.push_back(std::make_pair(50, 60));
	offsets.push_back(std::make_pair(50, 60));
	offsets.push_back(std::make_pair(50, 60));

	std::vector<tlocation_anim> anims;
	std::map<int, int> fields;
	std::vector<size_t> cards;
	std::pair<int, int> offset;
	size_t pos = 0;
	int random = -1;
	const int items_per_line = 2;
	for (size_t n = 0; n < teams_.size() && pos < offsets.size(); n ++) {
		team& t = teams_[n];
		tgroup& g = runtime_groups::get(t.leader()->number_);
		if (!g.valid()) {
			continue;
		}
		if (tent::mode == mode_tag::SIEGE) {
			if (n == 0) {
				offset = offsets[1];
			} else if (n == 1) {
				offset = offsets[0];
			} else if (t.is_enemy(2)) {
				// reinforce of attacker
				offset = offsets[3];
			} else {
				offset = offsets[2];
			}
		} else {
			offset = offsets[pos];
		}
		std::stringstream text;
		std::string img;
		img = std::string(t.leader()->image()) + "~SCALE(32, 40)";
		text << tintegrate::generate_img(img);
		text << tintegrate::generate_format(t.leader()->name(), "yellow") << "\n";
		text << tintegrate::generate_img("misc/tintegrate-split-line.png");


		fields = g.adjust_hero_field(hero_field_leadership);
		for (std::map<int, int>::iterator it = fields.begin(); it != fields.end(); ++ it) {
			hero& h = heros_[it->first];
			img = std::string(h.image()) + "~SCALE(32, 40)";
			if (std::distance(fields.begin(), it) % items_per_line == 0) {
				text << "\n";
			} else {
				text << "  ";
			}
			text << tintegrate::generate_img(img);
			text << tintegrate::generate_img("misc/leadership.png");
			text << tintegrate::generate_format(it->second, "green");
		}
		if (!fields.empty()) {
			text << "\n";
		}

		fields = g.adjust_hero_field(hero_field_charm);
		for (std::map<int, int>::iterator it = fields.begin(); it != fields.end(); ++ it) {
			hero& h = heros_[it->first];
			img = std::string(h.image()) + "~SCALE(32, 40)";
			if (std::distance(fields.begin(), it) % items_per_line == 0) {
				text << "\n";
			} else {
				text << "  ";
			}
			text << tintegrate::generate_img(img);
			text << tintegrate::generate_img("misc/charm.png");
			text << tintegrate::generate_format(it->second, "green");
		}
		if (!fields.empty()) {
			text << "\n";
		}

		// card
		cards = g.increase_card(t, tent::mode);
		for (std::vector<size_t>::iterator it = cards.begin(); it != cards.end(); ++ it) {
			const card& c = cards_[*it];
			if (it == cards.begin()) {
				text << "\n";
				text << tintegrate::generate_img("misc/card.png");

			} else if (std::distance(cards.begin(), it) % items_per_line == 0) {
				text << "\n";
			} else {
				text << ", ";
			}
			text << tintegrate::generate_format(c.name(), "yellow");
		}
		if (!cards.empty()) {
			text << "\n";
		}

		// technology
		if (random == -1) {
			random = gamestate_.rng().get_next_random();
		}
		std::vector<const ttechnology*> techs = g.calculate_allocatable_technology(tent::mode, t.holded_technologies(), random);
		for (std::vector<const ttechnology*>::iterator it = techs.begin(); it != techs.end(); ++ it) {
			const ttechnology& tech = **it;
			if (it == techs.begin()) {
				text << "\n";
				text << tintegrate::generate_img("misc/technology.png");
			} else if (std::distance(techs.begin(), it) % items_per_line == 0) {
				text << "\n";
			} else {
				text << ", ";
			}
			text << tintegrate::generate_format(tech.name(), "yellow");

			t.insert_technology(tech);
		}

		if (n == human_team_) {
			anims.push_back(tlocation_anim(offset.first, offset.second, text.str(), display::rgb(255, 255, 255)));
		}
		pos ++;
	}
	if (!anims.empty()) {
		unit_display::location_text(anims);
		// gui2::show_square_message(gui_->video(), "", anims.front().text);
	}
}

void play_controller::init_managers()
{
	LOG_NG << "initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
	prefs_disp_manager_.reset(new preferences::display_manager(gui_.get()));
	tooltips_manager_.reset(new tooltips::manager(gui_->video()));
	soundsources_manager_.reset(new soundsource::manager(*gui_));

	resources::soundsources = soundsources_manager_.get();

	halo_manager_.reset(new halo::manager(*gui_));
	LOG_NG << "done initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
}

static int placing_score(int mode, const config& side, const gamemap& map, const map_location& pos)
{
	int positions = 0, liked = 0;
	const t_translation::t_list terrain = t_translation::read_list(side["terrain_liked"]);

	for(int i = pos.x-8; i != pos.x+8; ++i) {
		for(int j = pos.y-8; j != pos.y+8; ++j) {
			const map_location pos(i,j);
			if(map.on_board(pos)) {
				++positions;
				if(std::count(terrain.begin(),terrain.end(),map[pos])) {
					++liked;
				}
			}
		}
	}

	int mode_bonus = 0;
	if (mode == mode_tag::TOWER || mode == mode_tag::SIEGE) {
		// ensure side#1 to right, side#2 to left
		int s = side["side"].to_int();
		if (s == 1) {
			mode_bonus = pos.x * 100;
		} else {
			mode_bonus = (map.w() - pos.x) * 100;
		}
	}

	return (100*liked)/positions + mode_bonus;
}

struct placing_info {

	placing_info() :
		side(0),
		score(0),
		pos()
	{
	}

	int side, score;
	map_location pos;
};

static bool operator<(const placing_info& a, const placing_info& b) { return a.score > b.score; }

void play_controller::place_sides_in_preferred_locations()
{
	std::vector<placing_info> placings;

	int num_pos = map_.num_valid_starting_positions();

	int side_num = 1;
	BOOST_FOREACH (const config &side, level_.child_range("side"))
	{
		for(int p = 1; p <= num_pos; ++p) {
			const map_location& pos = map_.starting_position(p);
			int score = placing_score(tent::mode, side, map_, pos);
			placing_info obj;
			obj.side = side_num;
			obj.score = score;
			obj.pos = pos;
			placings.push_back(obj);
		}
		++side_num;
	}

	std::sort(placings.begin(),placings.end());
	std::set<int> placed;
	std::set<map_location> positions_taken;

	for (std::vector<placing_info>::const_iterator i = placings.begin(); i != placings.end() && int(placed.size()) != side_num - 1; ++i) {
		if(placed.count(i->side) == 0 && positions_taken.count(i->pos) == 0) {
			placed.insert(i->side);
			positions_taken.insert(i->pos);
			map_.set_starting_position(i->side,i->pos);
			LOG_NG << "placing side " << i->side << " at " << i->pos << '\n';
		}
	}
}

void play_controller::objectives(){
	menu_handler_.objectives(gui_->viewing_team()+1);
}

void play_controller::show_statistics()
{
	menu_handler_.show_statistics(gui_->viewing_team() + 1);
}

void play_controller::show_rpg()
{
	if (tent::tower_mode() || rpg::h->status_ == hero_status_wander) {
		if (!replaying_) {
			game_config::hide_tactic_slot = !game_config::hide_tactic_slot;
			current_team().refresh_tactic_slots(*gui_);
		}
		
	} else {
		rpging_ = true;
		gui_->goto_main_context_menu();
		rpging_ = false;
	}
}

void play_controller::rpg_detail() 
{
	gui2::trpg_detail dlg(*gui_, teams_, units_, heros_);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
}

void play_controller::assemble_treasure()
{
	std::vector<std::pair<int, unit*> > human_pairs = form_human_pairs(teams_, troop_can_assamble_treasure, this);

	std::vector<int> original_holded;
	for (std::vector<std::pair<int, unit*> >::iterator it = human_pairs.begin(); it != human_pairs.end(); ++ it) {
		const hero& h = heros_[it->first];
		original_holded.push_back(h.treasure_);
	}
	
	std::map<int, int> diff;
	std::set<unit*> diff_troops;
	{
		gui2::ttreasure dlg(*gui_, teams_, units_, heros_, human_pairs, replaying_);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		int index = 0;
		for (std::vector<std::pair<int, unit*> >::iterator it = human_pairs.begin(); it != human_pairs.end(); ++ it, index ++) {
			const int orig = original_holded[index];
			const hero& h = heros_[it->first];

			if (orig != h.treasure_) {
				diff[h.number_] = h.treasure_;
				if (!it->second->is_artifical()) {
					diff_troops.insert(it->second);
				}
			}
		}
	}

	for (std::set<unit*>::iterator it = diff_troops.begin(); it != diff_troops.end(); ++ it) {
		unit& u = **it;
		u.adjust();
	}

	if (!diff.empty()) {
		menu_handler_.clear_undo_stack(player_number_);

		recorder.add_assemble_treasure(diff);
	}
}

void play_controller::appoint_noble()
{
	std::vector<std::pair<int, unit*> > human_pairs = form_human_pairs(teams_, troop_can_appoint_noble, this);

	std::vector<int> original_holden;
	for (std::vector<std::pair<int, unit*> >::iterator it = human_pairs.begin(); it != human_pairs.end(); ++ it) {
		const hero& h = heros_[it->first];
		original_holden.push_back(h.noble_);
	}
	
	std::map<int, int> diff;
	std::set<unit*> diff_troops;
	{
		gui2::tnoble2 dlg(teams_, units_, heros_, human_pairs, replaying_);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}

		int index = 0;
		for (std::vector<std::pair<int, unit*> >::iterator it = human_pairs.begin(); it != human_pairs.end(); ++ it, index ++) {
			const int orig = original_holden[index];
			const hero& h = heros_[it->first];

			if (orig != h.noble_) {
				diff[h.number_] = h.noble_;
				if (!it->second->is_artifical()) {
					diff_troops.insert(it->second);
				}
			}
		}
	}

	for (std::set<unit*>::iterator it = diff_troops.begin(); it != diff_troops.end(); ++ it) {
		unit& u = **it;
		u.adjust();
	}

	if (!diff.empty()) {
		gui_->refresh_access_troops(player_number_ - 1, game_display::REFRESH_CLEAR);
		gui_->refresh_access_troops(player_number_ - 1, game_display::REFRESH_RELOAD);

		gui_->refresh_access_heros(player_number_ - 1, game_display::REFRESH_CLEAR);
		gui_->refresh_access_heros(player_number_ - 1, game_display::REFRESH_RELOAD);
		menu_handler_.clear_undo_stack(player_number_);

		recorder.add_appoint_noble(diff);
	}
}

void play_controller::rpg_exchange(const std::vector<size_t>& human_p, size_t ai_p) 
{
	team& rpg_team = teams_[rpg::h->side_];
	std::vector<std::pair<size_t, unit*> > human_pairs;
	std::vector<std::pair<size_t, unit*> > ai_pairs;
	std::vector<std::pair<size_t, unit*> >* tmp_pairs;

	std::vector<artifical*>& holden_cities = rpg_team.holden_cities();
	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical* city = *it;

		std::vector<unit*>& resides = city->reside_troops();
		for (std::vector<unit*>::iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
			unit* u = *it2;
			if (u->human()) {
				tmp_pairs = &human_pairs;
			} else {
				tmp_pairs = &ai_pairs;
			}
			tmp_pairs->push_back(std::make_pair(u->master().number_, u));
			if (u->second().valid()) {
				tmp_pairs->push_back(std::make_pair(u->second().number_, u));
			}
			if (u->third().valid()) {
				tmp_pairs->push_back(std::make_pair(u->third().number_, u));
			}
		}
		std::vector<unit*>& fields = city->field_troops();
		for (std::vector<unit*>::iterator it2 = fields.begin(); it2 != fields.end(); ++ it2) {
			unit* u = *it2;
			if (u->human()) {
				tmp_pairs = &human_pairs;
			} else {
				tmp_pairs = &ai_pairs;
			}
			tmp_pairs->push_back(std::make_pair(u->master().number_, u));
			if (u->second().valid()) {
				tmp_pairs->push_back(std::make_pair(u->second().number_, u));
			}
			if (u->third().valid()) {
				tmp_pairs->push_back(std::make_pair(u->third().number_, u));
			}
		}
		if (city->mayor() == rpg::h) {
			tmp_pairs = &human_pairs;
		} else {
			tmp_pairs = &ai_pairs;
		}
		std::vector<hero*>& freshes = city->fresh_heros();
		for (std::vector<hero*>::iterator it2 = freshes.begin(); it2 != freshes.end(); ++ it2) {
			hero& h = **it2;
			tmp_pairs->push_back(std::make_pair(h.number_, city));
		}
		std::vector<hero*>& finishes = city->finish_heros();
		for (std::vector<hero*>::iterator it2 = finishes.begin(); it2 != finishes.end(); ++ it2) {
			hero& h = **it2;
			tmp_pairs->push_back(std::make_pair(h.number_, city));
		}
	}

	std::set<size_t> checked_human;
	size_t checked_ai;
	if (human_p.empty()) {
		gui2::texchange dlg(teams_, units_, heros_, human_pairs, ai_pairs);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		checked_human = dlg.checked_human_pairs();
		checked_ai = dlg.checked_ai_pairs();
	} else {
		for (std::vector<size_t>::const_iterator it = human_p.begin(); it != human_p.end(); ++ it) {
			checked_human.insert(*it);
		}
		checked_ai = ai_p;
	}

	std::vector<hero*> human_heros;
	hero* ai_hero = NULL;
	std::vector<size_t> checked;
	for (std::set<size_t>::const_iterator it = checked_human.begin(); it != checked_human.end(); ++ it) {
		checked.push_back(*it);
	}
	for (int i = 0; i < 4; i ++) {
		std::pair<size_t, unit*>* pair;
		if (i < 3) {
			pair = &human_pairs[checked[i]];
		} else {
			pair = &ai_pairs[checked_ai];
		}
		unit* u = pair->second;
		hero& h = heros_[pair->first];
		if (i < 3) {
			human_heros.push_back(&h);
		} else {
			ai_hero = &h;
		}

		std::vector<hero*> captains;
		if (artifical* city = units_.city_from_loc(u->get_location())) {
			// fresh/finish/reside troop
			if (u->is_city()) {
				for (int type = 0; type < 3; type ++) { 
					std::vector<hero*>* list;
					if (type == 0) {
						list = &city->fresh_heros();
					} else if (type == 1) {
						list = &city->finish_heros();
					} else {
						list = &city->wander_heros();
					}
					std::vector<hero*>::iterator i2 = std::find(list->begin(), list->end(), &h);
					if (i2 != list->end()) {
						list->erase(i2);
						break;
					}
				}
			} else {
				std::vector<unit*>& reside_troops = city->reside_troops();
				int index = 0;
				for (std::vector<unit*>::iterator i2 = reside_troops.begin(); i2 != reside_troops.end(); ++ i2, index ++) {
					if (*i2 != u) {
						continue;
					}
					u->replace_captains_internal(h, captains);
					if (captains.empty()) {
						city->troop_go_out(*u);
					} else {
						u->replace_captains(captains);
					}
					break;
				}
			}
		} else {
			// field troop
			u->replace_captains_internal(h, captains);
			if (captains.empty()) {
				units_.erase(u);
			} else {
				u->replace_captains(captains);
			}
		}
	}

	std::stringstream str;
	utils::string_map symbols;
	for (std::vector<hero*>::iterator it = human_heros.begin(); it != human_heros.end(); ++ it) {
		hero& h = **it;
		if (it != human_heros.begin()) {
			str << ", ";
		}
		str << tintegrate::generate_format(h.name(), tintegrate::hero_color);
	}
	symbols["human"] = str.str();
	symbols["ai"] = tintegrate::generate_format(ai_hero->name(), tintegrate::hero_color);
	game_events::show_hero_message(rpg::h, NULL, vgettext("You exchanged $ai using $human.", symbols), game_events::INCIDENT_INVALID);


	// ai to human city
	artifical* city = units_.city_from_cityno(rpg::h->city_);
	city->move_into(*ai_hero);

	// human to ai city
	if (!human_heros.empty()) {
		std::vector<mr_data> mrs;
		units_.calculate_mrs_data(gamestate_, mrs, rpg::h->side_ + 1, false);
		artifical* aggressing = mrs[0].own_front_cities[0];

		for (std::vector<hero*>::iterator it = human_heros.begin(); it != human_heros.end(); ++ it) {
			hero& h = **it;
			aggressing->move_into(h);
		}
	}

	if (human_p.empty()) {
		recorder.add_rpg_exchange(checked_human, checked_ai);
	}
}

void play_controller::rpg_independence(bool replaying) 
{
	int from_side = rpg::h->side_ + 1;
	std::vector<mr_data> mrs;
	units_.calculate_mrs_data(gamestate_, mrs, from_side, false);
	artifical* aggressing = mrs[0].own_front_cities[0];

	if (!replaying) {
		int side_num = gui_->viewing_team() + 1;

		gui2::tindependence dlg(*gui_, teams_, units_, heros_, aggressing);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
	}

	artifical* rpg_city = units_.city_from_cityno(rpg::h->city_);
	team& from_team = teams_[rpg::h->side_];
	std::string message;
	utils::string_map symbols;
	
	symbols["first"] = tintegrate::generate_format(rpg::h->name(), tintegrate::hero_color);
	symbols["second"] = tintegrate::generate_format(rpg_city->name(), tintegrate::hero_color);
	game_events::show_hero_message(rpg::h, rpg_city, vgettext("$first declared independence.", symbols), game_events::INCIDENT_INDEPENDENCE);

	if (from_team.defeat_vote()) {
		aggressing = NULL;

		symbols["first"] = tintegrate::generate_format(from_team.name(), tintegrate::hero_color);
		game_events::show_hero_message(&heros_[hero::number_scout], NULL, vgettext("$first is defeated.", symbols), game_events::INCIDENT_DEFEAT);
	}
	hero* from_leader = from_team.leader();
	unit* from_leader_unit = units_.find_unit(*from_leader);
	std::map<int, hero*> from_appointed_nobles = from_team.appointed_nobles();
	for (std::map<int, hero*>::const_iterator it = from_appointed_nobles.begin(); it != from_appointed_nobles.end(); ++ it) {
		from_team.appoint_noble(*it->second, HEROS_NO_NOBLE, false);
	}

	// if from_lead and rpg::h is in same troop, disband this troop.
	if (from_leader_unit && aggressing) {
		bool same = false;
		if (&from_leader_unit->master() == rpg::h) {
			same = true;
		}
		if (!same && from_leader_unit->second().valid() && &from_leader_unit->second() == rpg::h) {
			same = true;
		}
		if (!same && from_leader_unit->third().valid() && &from_leader_unit->third() == rpg::h) {
			same = true;
		}
		if (same) {
			std::vector<hero*> v;
			v.push_back(&from_leader_unit->master());
			if (from_leader_unit->second().valid()) {
				v.push_back(&from_leader_unit->second());
			}
			if (from_leader_unit->third().valid()) {
				v.push_back(&from_leader_unit->third());
			}
			if (rpg_city->get_location() == from_leader_unit->get_location()) {
				// reside troop
				rpg_city->troop_go_out(*from_leader_unit);

			} else {
				// field troop
				units_.erase(from_leader_unit);
			}
			for (std::vector<hero*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
				rpg_city->fresh_into(**it);
			}
		}
	}

	// change in from_side
	from_team.rpg_changed(true);
	from_team.set_base_income(150);

	// change in to_side
	team& to_team = teams_.back();
	int to_side = to_team.side();
	// reset float_catalog equal to base_catalog_
	rpg::h->float_catalog_ = ftofxp8(rpg::h->base_catalog_);

	// select_mayor will change rpg stratum, first callit.
	rpg_city->select_mayor(&hero_invalid);
	// change rpg stratum to hero_stratum_leader.
	rpg::stratum = hero_stratum_leader;
	// artifical::troop_come_into/unit_belong_to need correct rpg::h->side_ to decide set_human.
	rpg::h->side_ = to_side - 1;
	rpg::h->official_ = hero_official_leader;
	if (!aggressing) {
		from_leader->official_ = HEROS_NO_OFFICIAL;
	}

	std::vector<artifical*> holden_cities = from_team.holden_cities();
	std::set<int> independenced_cities, undependenced_cities;
	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical* city = *it;
		bool independenced = aggressing? false: true;
		if (!independenced) {
			independenced = city->independence_vote(aggressing);
		}
		if (independenced) {
			independenced_cities.insert(city->cityno());
		} else {
			undependenced_cities.insert(city->cityno());
		}
	}

	// navigation
	int navigation = from_team.navigation();
	to_team.set_navigation(navigation * independenced_cities.size() / (independenced_cities.size() + undependenced_cities.size()));

	// gold
	int gold = from_team.gold();
	if (gold > to_team.gold()) {
		to_team.set_gold(gold * independenced_cities.size() / (independenced_cities.size() + undependenced_cities.size()));
	}

	for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		artifical* city = *it;
		bool independenced = aggressing? false: true;
		if (!independenced) {
			independenced = independenced_cities.find(city->cityno()) != independenced_cities.end();
		}
		city->independence(independenced, to_team, rpg_city, from_team, aggressing, from_leader, from_leader_unit);
		gui_->resort_access_troops(*city);
		if (independenced && city != rpg_city) {
			symbols["first"] = tintegrate::generate_format(city->name(), tintegrate::object_color);
			symbols["second"] = tintegrate::generate_format(rpg::h->name(), tintegrate::hero_color);
			game_events::show_hero_message(&heros_[hero::number_scout], city, vgettext("$first declared belonging to $second.", symbols), game_events::INCIDENT_INVALID);
		}
	}

	// change controller maybe change human-flag, place after process troop.
	if (aggressing) {
		from_team.change_controller(controller_tag::AI);
	} else {
		from_team.change_controller(controller_tag::EMPTY);
	}
	to_team.change_controller(controller_tag::HUMAN);

	// capital
	to_team.set_capital(units_.city_from_cityno(rpg::h->city_));
	// want to set human side in acess list.
	gui_->refresh_access_troops(rpg::h->side_, game_display::REFRESH_CLEAR);
	gui_->refresh_access_troops(rpg::h->side_, game_display::REFRESH_RELOAD);

	std::set<std::string> type_ids;
	const std::set<const unit_type*>& can_build = from_team.builds();
	for (std::set<const unit_type*>::const_iterator cr = can_build.begin(); cr != can_build.end(); ++ cr) {
		type_ids.insert((*cr)->id());
	}
	to_team.set_builds(type_ids);

	// shourd/fog
	if (from_team.uses_shroud()) {
		from_team.set_shroud(false);
		to_team.set_shroud(true);
	}
	if (from_team.uses_fog()) {
		from_team.set_fog(false);
		to_team.set_fog(true);
	}

	// card
	std::vector<size_t>& candidate_cards = to_team.candidate_cards(); 
	candidate_cards = from_team.candidate_cards();
	from_team.candidate_cards().clear();
	std::vector<size_t>& holded_cards = to_team.holded_cards();
	holded_cards = from_team.holded_cards();
	from_team.holded_cards().clear();

	// treasure
	to_team.holded_treasures() = from_team.holded_treasures();
	from_team.holded_treasures().clear();

	// technology
	to_team.holded_technologies() = from_team.holded_technologies();
	to_team.half_technologies() = from_team.half_technologies();
	to_team.apply_holded_technologies_finish();
	to_team.readjust_all_unit();

	// statistic
	to_team.cause_damage_ = from_team.cause_damage_;
	to_team.been_damage_ = from_team.been_damage_;
	to_team.defeat_units_ = from_team.defeat_units_;
	to_team.perfect_turns_ = from_team.perfect_turns_;
	to_team.defeat_units_one_turn_ = from_team.defeat_units_one_turn_;

	// nobles
	to_team.select_leader_noble(true);
	const std::set<int>& unappoint_nobles = to_team.unappoint_nobles();
	for (std::map<int, hero*>::const_iterator it = from_appointed_nobles.begin(); it != from_appointed_nobles.end(); ++ it) {
		if (it->second == rpg::h) {
			// rpg hero maybe is noble of previous team. now he is leader.
			continue;
		}
		if (it->second->side_ == to_side - 1 && unappoint_nobles.find(it->first) != unappoint_nobles.end()) {
			to_team.appoint_noble(*it->second, it->first, false);
		}
	}
	to_team.fill_normal_noble(true, false);
	if (aggressing) {
		from_team.fill_normal_noble(true, false);
	}

	// troop belong to rpg team maybe attacked, it should reset to no attack use end_turn.
	// calculate_end_turn(teams_, to_side);

	if (!replaying) {
		recorder.add_event(replay::EVENT_RPG_INDEPENDENCE, map_location());

		gui_->hide_context_menu();

		if (unit::actor->is_city()) {
			// end_turn();
		}
	}
}

void play_controller::interior()
{
	menu_handler_.interior(gui_->viewing_team() + 1);
}

void play_controller::technology_tree()
{
	team& t = teams_[gui_->viewing_team()];
	int retval;
	std::vector<gui2::tsystem::titem> items;

	{
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Assemble Treasure")));
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Appoint Noble")));
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Technology tree")));

		if (tent::mode == mode_tag::LAYOUT) {
			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Stratagem")));
		}

		gui2::tsystem dlg(items);
		try {
			dlg.show(gui_->video());
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}

	if (retval == 0) {
		assemble_treasure();

	} else if (retval == 1) {
		appoint_noble();

	} else if (retval == 2) {
		menu_handler_.technology_tree(t.side());

	} else if (retval == 3) {
		int tag = t.current_stratagem();
		if (tag == stratagem_tag::none) {
			tag = stratagem_tag::min;
		}
		
		t.insert_stratagem(browse_stratagem(*gui_, tag), true);
	}
}

void play_controller::employ()
{
	if (emploies_.empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There is no employable hero."));
		return;
	}
	double ratio = 2.0;
	gui2::temploy dlg(*gui_, teams_, units_, heros_, *this, emploies_, gui_->viewing_team() + 1, ratio, replaying_);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	team& current_team = teams_[gui_->viewing_team()];
	hero& h = *emploies_[dlg.get_cursel()];
	int cost = h.cost_ * ratio;
	do_employ(*this, units_, current_team, h, cost, false);

	gui_->show_context_menu();
}

void play_controller::start_pass_scenario_anim(LEVEL_RESULT result) const
{
	std::pair<int, int> ret = calculate_score(result);
	int coin = ret.first;
	int score = ret.second;
	bool capture = calculate_capture();
	const team& t = teams_[human_team_];

	if (gui_->pass_scenario_anim_id() != -1) {
		gui_->erase_area_anim(gui_->pass_scenario_anim_id());
	}
	if (const animation* tpl = area_anim::anim(area_anim::PASS_SCENARIO)) {
		animation& anim = gui_->insert_pass_scenario_anim(*tpl);

		std::stringstream strstr;

		anim.replace_static_text("__difficulty", dsgettext("wesnoth-lib", "scenario^Difficulty"));
		strstr.str("");
		strstr << "~CROP(0, 0, " << (48 * difficulty_level_) << ", 48)";
		anim.replace_image_mod("__diffmod", strstr.str());
		anim.replace_progressive("x", "5555", str_cast(48 * difficulty_level_ / 2));

		anim.replace_static_text("__turns", dsgettext("wesnoth-lib", "scenario^Turns"));
		anim.replace_static_text("__turns_count", str_cast(turn()));

		if (tent::tower_mode()) {
			anim.replace_static_text("__line3title", dsgettext("wesnoth-lib", "scenario^Capture fort"));
			anim.replace_image_name("__line3image", capture? "misc/ok.png": "misc/delete.png");

		} else {
			anim.replace_static_text("__line3title", "");
			anim.replace_image_name("__line3image", "");
		}
		anim.replace_static_text("__line4title", dsgettext("wesnoth-lib", "scenario^Perfect turns"));
		anim.replace_static_text("__line4text", str_cast(t.perfect_turns_));


		anim.replace_static_text("__score", str_cast(score));
		anim.replace_static_text("__coin", str_cast(coin));

		anim.start_animation(0);
	}
}

std::vector<int> group_employee(hero_map& heros, const tgroup& g)
{
	const hero& leader = g.leader();
	std::vector<int> ret;

	for (int number = hero_map::map_size_from_dat; number < (int)heros.size(); number ++) {
		const hero& h = heros[number];
		if (h.get_flag(hero_flag_employee) && h.player_ == leader.number_) {
			ret.push_back(g.member(h).base->number_);
		}
	}
	return ret;
}

tgroup& play_controller::human_team_group()
{
	tgroup* g = &runtime_groups::get(teams_[human_team_].leader()->number_);
	if (!g->valid()) {
		// leader isn't username, for example multi-scenario. in the case, there is only one group in gs.
		g = &runtime_groups::gs.begin()->second;
	}
	return *g;
}

void play_controller::upload()
{
	std::stringstream strstr;
	utils::string_map symbols;
	std::pair<int, int> ret = calculate_score(end_level_data_.result);
	int coin = ret.first;
	int score = ret.second;
	time_t now = time(NULL);

	std::pair<int, std::string> employee = std::make_pair(HEROS_INVALID_NUMBER, null_str);
	http::membership m;
	if (tent::mode != mode_tag::SIEGE) {
		if (!coin && !score) {
			gui2::show_transient_message(gui_->video(), "", dsgettext("wesnoth-lib", "No coin and score, cannot upload!"));
			return;
		}
		http::pass_statistic stat;
		stat.username = preferences::login();
		stat.create_time = gamestate_.classification().create;
		stat.duration = gamestate_.duration(now);
		stat.version = version_info(gamestate_.classification().version).transfer_format().first;
		stat.score = score;
		stat.coin = coin;
		stat.type = tent::mode;
		stat.hash = gamestate_.classification().hash;
		stat.subcontinent = tent::subcontinent;

		m = http::upload_pass(*gui_, heros_, stat);
	} else {
		http::tsiege_record rec;
		for (std::vector<team>::iterator it = teams_.begin(); it != teams_.end(); ++ it) {
			team& t = *it;
			tgroup& g = runtime_groups::get(t.leader()->number_);
			if (t.side() == human_team_ + 1) {
				rec.attacker = g.leader().uid();
			} else if (t.side() <= 2) {
				// don't use hold ciyte judge, allay my captrue attacker's city.
				// defender must be first tow side.
				rec.defender = g.leader().uid();
				if (tent::subcontinent.first.empty()) {
					std::vector<int> ret = group_employee(heros_, g);
					if (!ret.empty() && end_level_data_.result == VICTORY) {
						rec.employee = ret[rand() % ret.size()];
						employee = std::make_pair(rec.employee, g.leader().name());
					}
				}
			} else if (t.is_ai()) {
				if (t.is_enemy(human_team_ + 1)) {
					rec.def_reinforce = g.leader().uid();
				} else {
					rec.atk_reinforce = g.leader().uid();
				}
			}
		}
		if (!coin && !score && rec.employee == HEROS_INVALID_NUMBER) {
			gui2::show_transient_message(gui_->video(), "", dsgettext("wesnoth-lib", "No coin and score, cannot upload!"));
			return;
		}
		if (rec.attacker == rec.defender) {
			gui2::show_transient_message(gui_->video(), "", dsgettext("wesnoth-lib", "There is in test mode, cannot upload!"));
			return;
		}

		const int siege_upload_threshold = 12 * 3600;
		if (now < gamestate_.classification().create || now - gamestate_.classification().create > siege_upload_threshold) {
			symbols["threshold"] = tintegrate::generate_format(siege_upload_threshold / 3600, "yellow");
			strstr << vgettext("wesnoth-lib", "Save is more than $threshold hour, cannot upload!", symbols);

			gui2::show_transient_message(gui_->video(), "", strstr.str());
			return;
		}

		rec.score = score;
		rec.create_time = gamestate_.classification().create;
		rec.hash = gamestate_.classification().hash;
		rec.subcontinent = tent::subcontinent;
		if (end_level_data_.result == VICTORY) {
			rec.result = http::siege_result_victory;
		} else if (!teams_[human_team_].holden_cities().empty()) {
			rec.result = http::siege_result_defeat;
		} else {
			rec.result = http::siege_result_fallen;
		}
		m = http::upload_siege(*gui_, heros_, rec);
	}

	if (m.uid >= 0) {
		// group is invalid, use it's clone.
		tgroup& g = human_team_group();
		g.from_local_membership(*gui_, heros_, m, false);

		strstr.str("");
		if (employee.first != HEROS_INVALID_NUMBER) {
			hero& h = heros_[employee.first];

			symbols["username"] = tintegrate::generate_format(employee.second, "yellow");
			symbols["name"] = tintegrate::generate_format(h.name(), "red");
			strstr << vgettext("wesnoth-lib", "$username lose $name!", symbols) << "\n\n";
		}
		strstr << dsgettext("wesnoth-lib", "Upload successfully!");
		gui2::show_transient_message(gui_->video(), "", strstr.str());
	}
}

void play_controller::final_battle(int side_num, int human_capital, int ai_capital)
{
	// menu_handler_.final_battle(gui_->viewing_team()+1, human_capital, ai_capital);
	if (side_num == -1) {
		side_num = gui_->viewing_team() + 1;
	}
	artifical* capital_of_human;
	artifical* capital_of_ai;

	if (final_capital().first) {
		utils::string_map symbols;
		symbols["first"] = final_capital().first->master().name();
		symbols["second"] = final_capital().second? final_capital().second->master().name(): dgettext("wesnoth-lib", "Void");
		gui2::show_transient_message(gui_->video(), _("final battle"), vgettext("human capital: $first\n\n\n\nai capital: $second", symbols));
		return;
	}

	if (human_capital < 0) {
		// popup dialog, let player select capital
		std::vector<artifical*>& human_holded_cities = teams_[side_num - 1].holden_cities();
		if (human_holded_cities.size() >= 1) {

			gui2::tfinal_battle dlg(*gui_, teams_, units_, heros_, side_num);
			try {
				dlg.show(gui_->video());
			} catch(twml_exception& e) {
				e.show(*gui_);
				return;
			}
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			capital_of_human = dlg.dst_city();

		} else if (human_holded_cities.size() == 1) {
			capital_of_human = human_holded_cities[0];
		} else {
			return;
		}
	} else {
		capital_of_human = units_.city_from_cityno(human_capital);
	}

	if (!all_ai_allied()) {
		ally_all_ai(true);
	}

	if (ai_capital < 0) {
		capital_of_ai = decide_ai_capital();
	} else {
		capital_of_ai = units_.city_from_cityno(ai_capital);
	}
	set_final_capital(capital_of_human, capital_of_ai);

	utils::string_map symbols;
	symbols["first"] = final_capital().first->master().name();
	symbols["second"] = final_capital().second? final_capital().second->master().name(): dgettext("wesnoth-lib", "Void");
	gui2::show_transient_message(gui_->video(), _("final battle"), vgettext("human capital: $first\n\n\n\nai capital: $second", symbols));

	if (human_capital < 0) {
		recorder.add_final_battle(capital_of_human, capital_of_ai);
	}
}

void play_controller::list()
{
	menu_handler_.list(gui_->viewing_team() + 1);
}

void play_controller::system()
{
	int side_num = gui_->viewing_team() + 1;
	enum {_SAVE, _SAVEREPLAY, _SAVEMAP, _LOAD, _PREFERENCES, _HELP, _QUIT};

	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	int commands_disabled_thro = is_replaying()? 1: 0;
	bool disable_exit = events::commands_disabled > commands_disabled_thro || mousehandler->in_multistep_state();

	std::string str, map_data;
	if (tent::mode == mode_tag::LAYOUT) {
		const config& player_cfg = gamestate_.get_variables().child("player");
		map_data = player_cfg["map_data"].str();
		map_data = utils::strip(map_data);
	}

	team& t = teams_[side_num - 1];
	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;
	bool layout_str_dirty = false, map_str_dirty = false, stratagem_dirty = false;
	{
		if (tent::mode != mode_tag::LAYOUT) {
			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Save Game"), !disable_exit && !is_replaying()));
			rets.push_back(_SAVE);

			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Save Replay"), !disable_exit && !is_replaying()));
			rets.push_back(_SAVEREPLAY);

			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Load Game"), !disable_exit));
			rets.push_back(_LOAD);
		} else {
			str = tintegrate::generate_format(dgettext("wesnoth-lib", "Save Layout"), "blue");
			tgroup& g = runtime_groups::get(t.leader()->number_);

			map_str_dirty = g.map() != map_data;
			stratagem_dirty = stratagem_tag::find(g.stratagem_from_layout_str().id()) != t.current_stratagem();
			layout_str_dirty = preferences::layout() != tgroup::layout_from_team_internal(t, g);
			
			items.push_back(gui2::tsystem::titem(str, map_str_dirty || stratagem_dirty || layout_str_dirty));
			rets.push_back(_SAVE);

			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Load Game"), !disable_exit));
			rets.push_back(_LOAD);
		}

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Preferences")));
		rets.push_back(_PREFERENCES);

		if (!game_config::tiny_gui) {
			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Help")));
			rets.push_back(_HELP);
		}
		
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Quit"), !disable_exit));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(items);
		try {
			dlg.show(gui_->video());
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}
	if (rets[retval] == _SAVE) {
		if (tent::mode != mode_tag::LAYOUT) {
			save_game();
		} else {
			tgroup& g = runtime_groups::get(t.leader()->number_);
			http::membership m = http::upload_layout(*gui_, heros_, tgroup::layout_from_team_internal(t, g), map_data);
			if (m.uid >= 0) {
				g.from_local_membership(*gui_, heros_, m, true);
				if (t.current_stratagem() == stratagem_tag::none) {
					t.insert_stratagem(g.stratagem_from_layout_str(), false);
				}
			}
		}

	} else if (rets[retval] == _SAVEREPLAY) {
		save_replay();

	} else if (rets[retval] == _SAVEMAP) {
		save_map();

	} else if (rets[retval] == _LOAD) {
		load_game();

	} else if (rets[retval] == _PREFERENCES) {
		preferences();

	} else if (rets[retval] == _HELP) {
		show_help();

	} else if (rets[retval] == _QUIT) {
		if (gui_->in_game()) {
			std::stringstream strstr;
			utils::string_map symbols;
			
			if (map_str_dirty) {
				symbols["name"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "Map"), "red");
			} else if (stratagem_dirty) {
				symbols["name"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "Stratagem"), "red");
			} else if (layout_str_dirty) {
				symbols["name"] = tintegrate::generate_format(dsgettext("wesnoth-lib", "group^Layout"), "red");
			}
			if (!symbols.empty()) {
				strstr << vgettext("wesnoth-lib", "$name is dirty, but not save.", symbols) << " ";
			}

			strstr << dgettext("wesnoth-lib", "Do you really want to quit?");
			const int res = gui2::show_message(gui_->video(), _("Quit"), strstr.str(), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
			if (res != gui2::twindow::CANCEL) {
				throw end_level_exception(QUIT);
			}
		}
	}
}

void play_controller::remove_active_tactic(int s)
{
	if (s < 0 || s >= (int)slot_cache::cache.size()) {
		return;
	}
	const slot_cache::tslot& slot = slot_cache::cache[s];
	if (slot.type == slot_cache::tslot::ACTIVE_TACTIC) {
		do_remove_active_tactic(*slot.u, true);
	} else {
		slot.u->remove_from_slot_cache(s);
	}
}

void play_controller::bomb()
{
	team& current_team = teams_[gui_->viewing_team()];
	const int move_cost = 250;
	utils::string_map symbols;
	std::string message;
	symbols["cost"] = tintegrate::generate_format(move_cost, "red");

	if (current_team.bomb_turns() >= game_config::max_bomb_turns) {
		mouse_handler_.set_card_playing(current_team, -1);
	} else if (current_team.gold() >= move_cost) {
		message = vgettext("Do you want to spend $cost gold to purchase move?", symbols);

		if (gui2::show_message(gui_->video(), "", message, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK) {
			do_purchase(*gui_, current_team, PURCHASE_MOVE, move_cost, false);
			
		}
	} else {
		message = vgettext("If have $cost gold, you can purchase move.", symbols);
		gui2::show_message(gui_->video(), "", message);
	}
}

void play_controller::switch_list()
{
	menu_handler_.switch_list(gui_->viewing_team() + 1);
}

void play_controller::unit_detail()
{
	unit_map::iterator u_itor = find_visible_unit(mouse_handler_.get_selected_hex(), teams_[gui_->viewing_team()]);
	if (u_itor.valid()) {
		menu_handler_.unit_detail(&*u_itor);
	}
}

void play_controller::save_game()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		config snapshot;
		to_config(snapshot);
		savegame::game_savegame save(heros_, heros_start_, gamestate_, *gui_, snapshot);
		save.save_game_interactive(gui_->video(), "", gui::OK_CANCEL);
	} else {
		save_blocker::on_unblock(this,&play_controller::save_game);
	}
}

void play_controller::save_replay()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		savegame::replay_savegame save(heros_, heros_start_, gamestate_);
		save.save_game_interactive(gui_->video(), "", gui::OK_CANCEL);
	} else {
		save_blocker::on_unblock(this,&play_controller::save_replay);
	}
}

void play_controller::save_map()
{
	if(save_blocker::try_block()) {
		save_blocker::save_unblocker unblocker;
		menu_handler_.save_map();
	} else {
		save_blocker::on_unblock(this,&play_controller::save_map);
	}
}

void play_controller::sync_undo()
{
	menu_handler_.clear_undo_stack(gui_->viewing_team() + 1);
}

void play_controller::load_game()
{
	// 这里用了偷懒办法，真正正确应该是在玩家确实已经加载了一个存档文件之后再清空
	gamestate_.start_scenario_ss.str("");
	// gamestate_.start_hero_ss.str("");
	gamestate_.clear_start_hero_data();

	game_state state_copy;
	savegame::loadgame load(*gui_, heros_, game_config_, state_copy);
	load.load_game();
}

void play_controller::preferences()
{
	menu_handler_.preferences();
}

void play_controller::show_chat_log(){
	menu_handler_.show_chat_log();
}

void play_controller::show_help()
{
	gui2::tbook dlg(*gui_, &map_, heros_, game_config_, "book_help");
	dlg.show(gui_->video());
}

void play_controller::undo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.undo(player_number_);
}

void play_controller::redo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.redo(player_number_);
}

void play_controller::toggle_grid(){
	menu_handler_.toggle_grid();
}

void play_controller::fire_prestart(bool execute)
{
	// Run initialization scripts, even if loading from a snapshot.
	resources::state_of_game->set_phase(game_state::PRELOAD);
	resources::lua_kernel->initialize();
	uint32_t start = SDL_GetTicks();
	game_events::fire("preload");
	uint32_t end_fire_preload = SDL_GetTicks();
	posix_print("play_controller::fire_prestart, fire preload, used time: %u ms\n", end_fire_preload - start);

	// pre-start events must be executed before any GUI operation,
	// as those may cause the display to be refreshed.
	if (execute){
		update_locker lock_display(gui_->video());
		resources::state_of_game->set_phase(game_state::PRESTART);

		uint32_t end_set_phase = SDL_GetTicks();
		posix_print("play_controller::fire_prestart, set_phase, used time: %u ms\n", end_set_phase - end_fire_preload);

		game_events::fire("prestart");
		uint32_t end_fire_prestart = SDL_GetTicks();
		posix_print("play_controller::fire_prestart, fire prestart, used time: %u ms\n", end_fire_prestart - end_set_phase);

		check_end_level();
		// prestart event may modify start turn with WML, reflect any changes.
		start_turn_ = turn();
	}
}

void play_controller::fire_start(bool execute)
{
	if (execute) {
		adjust_according_to_group_interior();
	}
	if (!has_network_player_) {
		// when multiplay, keep vip false.
		// and solve multiplayer send replay command at the same time.
		bool vip_from_cfg = level_["vip"].to_bool();
		if (!replaying_) {
			bool vip_runtime = preferences::vip2();
			if (vip_runtime != vip_from_cfg) {
				tscenario_env env(duel_, game_config::maximal_defeated_activity, vip_runtime);
				do_scenario_env(env, true);
			} else {
				set_vip(vip_runtime);
			}
		} else {
			set_vip(vip_from_cfg);
		}
	}

	if (execute) {
		resources::state_of_game->set_phase(game_state::START);
		game_events::fire("start");
		check_end_level();
		// start event may modify start turn with WML, reflect any changes.
		start_turn_ = turn();
		gamestate_.get_variable("turn_number") = int(start_turn_);
	} else {
		previous_turn_ = turn();
	}
	resources::state_of_game->set_phase(game_state::PLAY);
}

void play_controller::init_gui()
{
	gui_->begin_game();
	gui_->adjust_colors(0,0,0);

	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		::clear_shroud(t - teams_.begin() + 1);
	}
}

// @team_index: base 0
void play_controller::init_side(const unsigned int team_index, bool new_side)
{
	team& current_team = teams_[team_index];

	mouse_handler_.set_side(team_index + 1);

	// If we are observers we move to watch next team if it is allowed
	if (is_observer()
		&& !current_team.get_disallow_observers()) {
		gui_->set_team(size_t(team_index));
	}
	gui_->set_playing_team(size_t(team_index));

	gamestate_.get_variable("side_number") = player_number_;
	gamestate_.last_selected = map_location::null_location;

	if (current_team.is_human()) {
		gui_->set_team(player_number_ - 1);
		gui_->recalculate_minimap();
		gui_->invalidate_all();
		gui_->draw(true,true);
	}

	const time_of_day &tod = tod_manager_.get_time_of_day();

	if (int(team_index) + 1 == first_player_)
		sound::play_sound(tod.sounds, sound::SOUND_SOURCES);

	if (!recorder.is_skipping()) {
		::clear_shroud(team_index + 1);
		gui_->invalidate_all();
	}

	/**
	 * We do this only for local side when we are not replaying.
	 * For all other sides it is recorded in replay and replay handler has to handle calling do_init_side()
	 * functions.
	 **/
/*
	if (!current_team.is_local() || is_replay) {
		return;
	}
*/
	if (new_side) {
		do_init_side(team_index);
	}
}

void play_controller::do_init_turn()
{
	game_events::fire("turn " + str_cast(turn()));
	game_events::fire("new turn");

	int random = get_random();
	recommend(random);
	if (ally_all_ai()) {
		game_events::show_hero_message(&heros_[hero::number_scout], NULL, _("lips death and teeth cold, all ai sides concluded treaty of alliance."), game_events::INCIDENT_ALLY);

		if (rpg::stratum != hero_stratum_leader) {
			// enter to final battle automaticly
			if (!final_capital().first) {
				artifical* capital_of_human = units_.city_from_cityno(rpg::h->city_);
				artifical* capital_of_ai = decide_ai_capital();

				set_final_capital(capital_of_human, capital_of_ai);
				final_battle(capital_of_human->side(), -1, -1);
			}
		}
	}

	previous_turn_ = turn();
}

/**
 * Called by replay handler or init_side() to do actual work for turn change.
 */
void play_controller::do_init_side(const unsigned int team_index)
{
	team& t = teams_[team_index];

	// If this is right after loading a game we don't need to fire events and such. It was already done before saving.
	// calculate interior exploiture
	std::vector<artifical*>& holden = t.holden_cities();
	for (std::vector<artifical*>::iterator it = holden.begin(); it != holden.end(); ++ it) {
		artifical& city = **it;
		city.active_exploiture();
	}		

	// first turn should begin to reserach technology.
	if (!t.ing_technology()) {
		t.select_ing_technology();
		if (t.ing_technology()) {
			utils::string_map symbols;
			symbols["begin"] = tintegrate::generate_format(t.ing_technology()->name(), "blue");
			symbols["side"] = tintegrate::generate_format(t.name(), "green");
			artifical* city = units_.city_from_cityno(t.leader()->city_);
			if (city && !tent::tower_mode()) {
				game_events::show_hero_message(&heros_[hero::number_civilian], city, 
					vgettext("$side begin to research $begin!", symbols), game_events::INCIDENT_TECHNOLOGY);
			}
		}
	}

	int random = get_random(); // when loading_game_ is true, random is unused.
	if (more_card_) {
		more_card(t, turn());
	}

	// We want to work out if units for this player should get healed,
	// and the player should get income now.
	// Healing/income happen if it's not the first turn of processing,
	// or if we are loading a game.
	if (turn() > 1) {
		t.new_turn(random);

		const std::pair<unit**, size_t> p = t.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			unit& u = *(troops[i]);
			if (!u.is_artifical() && !u.is_robber() && u.food() > 0 && u.consider_food()) {
				u.increase_food(-1);
			}
		}
	}

	t.previous_turn = turn();
}

bool play_controller::do_prefix_unit(int end_ticks, bool replay, bool record_at_end)
{
	// get first unit to current unit.
	unit::actor = &units_.current_unit();
	if (!tent::turn_based) {
		VALIDATE(!unit::actor->ticks(), "unit::actor's ticks must be 0!");
	}

	if (!replay) {
		bool new_turn = turn() != 1 && turn() != previous_turn_;
		recorder.add_prefix_unit(unit::actor->side(), new_turn, end_ticks);
	}

	if (turn() != previous_turn_) {
		do_init_turn();
	}

	unit_map::top_side = player_number_ = unit::actor->side();
	team& t = teams_[player_number_ - 1];
	bool new_side = !loading_game_ && t.previous_turn != turn();

	init_side(player_number_ - 1, new_side);

	if (loading_game_) {
		return new_side;
	}

	int random = get_random();
	// new turn: 1)team, 2)city, 3)troop/commoner/artifcal
	// 2) city
	if (tent::turn_based || unit::actor->is_city()) {
		t.spend_gold(-1 * t.total_income());
		t.do_technology_income(t.total_technology_income());

		for (std::vector<artifical*>::const_iterator it = t.holden_cities().begin(); it != t.holden_cities().end(); ++ it) {
			artifical& city = **it;

			city.get_experience(increase_xp::turn_ublock(city), 3);
			city.new_turn(*this, random);
			
			std::vector<unit*>& commoners = city.field_commoners();
			for (std::vector<unit*>::const_iterator it = commoners.begin(); it != commoners.end(); ++ it) {
				unit& u = **it;
				u.new_turn(*this, random);
			}

			std::vector<artifical*>& arts = city.field_arts();
			for (std::vector<artifical*>::const_iterator it = arts.begin(); it != arts.end(); ++ it) {
				artifical& art = **it;
				art.new_turn(*this, random);
			}
		}

		// gold, income changed, refresh top panel.
		gui_->invalidate_game_status();

	} else {
		if (unit::actor->new_turn(*this, random)) {
			units_.erase(unit::actor);
		}
	}

	if (!tent::turn_based) {
		if (actor_can_continue_action(units_, player_number_)) {
			// If the expense is less than the number of villages owned,
			// then we don't have to pay anything at all
			int expense = unit::actor->upkeep();
			if (expense > 0) {
				t.spend_gold(expense);
			}

			{
				// cast active tactics before calculate healing.
				// some tactic maybe clear poision state, so that can heal.
				const events::command_disabler disable_commands;

				// cast active tactic
				std::vector<unit*> tactics = current_team().active_tactics();
				if (std::find(tactics.begin(), tactics.end(), unit::actor) != tactics.end()) {
					t.save_last_active_tactic(tactics);
					hero& h = *unit::actor->tactic_hero();
					cast_tactic(teams_, units_, *unit::actor, h, NULL, false, false);
				}

				calculate_healing(teams_, *gui_, unit::actor);

				calculate_supplying(teams_, units_, *unit::actor);

				reset_resting(teams_, unit::actor);

				road_guarding(units_, teams_, gui_->road_locs(), unit::actor);
			}

			if (tent::mode == mode_tag::SIEGE) {
				int counterattack_turn = tod_manager_.number_of_turns() * 2 / 3;
				if (counterattack_turn < 1) {
					counterattack_turn = 1;
				}
				if (unit::actor->task() == unit::TASK_STATION && counterattack_turn == tod_manager_.turn()) {
					unit::actor->set_task(unit::TASK_NONE);
				}
			}	
			if (t.restrict_movement || unit::actor->task() == unit::TASK_STATION) {
				unit::actor->set_movement(0);
			}
		}

	} else {
		const std::pair<unit**, size_t> p = t.field_troop();
		if (p.second) {
			size_t troops_vsize = p.second;
			if (troops_cache_vsize_ < troops_vsize) {
				free(troops_cache_);
				troops_cache_vsize_ = troops_vsize;
				troops_cache_ = (unit**)malloc(troops_cache_vsize_ * sizeof(unit*));
			}
			memcpy(troops_cache_, p.first, troops_vsize * sizeof(unit*));
			for (size_t i = 0; i < troops_vsize; i ++) {
				unit* u = troops_cache_[i];
				if (u->new_turn(*this, random)) {
					units_.erase(u);
				} else if (t.restrict_movement || u->task() == unit::TASK_STATION) {
					u->set_movement(0);
				}
			}
		}

		// If the expense is less than the number of villages owned,
		// then we don't have to pay anything at all
		int expense = t.side_upkeep();
		if (expense > 0) {
			t.spend_gold(expense);
		}

		{
			// cast active tactics before calculate healing.
			// some tactic maybe clear poision state, so that can heal.
			const events::command_disabler disable_commands;

			// cast active tactic
			std::vector<unit*> tactics = t.active_tactics();
			t.save_last_active_tactic(tactics);
			for (std::vector<unit*>::const_iterator it = tactics.begin(); it != tactics.end(); ++ it) {
				unit& u = **it;
				hero& h = *u.tactic_hero();
				// why below tow statement need notice order. see remark#31
				do_remove_active_tactic(u, false);
				cast_tactic(teams_, units_, u, h, NULL, false, false);
			}

			calculate_healing2(teams_, *gui_, player_number_);

			calculate_supplying2(teams_, units_, player_number_);

			reset_resting(teams_, NULL);

			road_guarding(units_, teams_, gui_->road_locs(), NULL);
		}

		if (tent::mode == mode_tag::SIEGE) {
			int counterattack_turn = tod_manager_.number_of_turns() * 2 / 3;
			if (counterattack_turn < 1) {
				counterattack_turn = 1;
			}
			if (counterattack_turn == tod_manager_.turn()) {
				const std::pair<unit**, size_t> p = t.field_troop();
				for (size_t i = 0; i < p.second; i ++) {
					unit* u = p.first[i];
					if (u->task() == unit::TASK_STATION) {
						u->set_task(unit::TASK_NONE);
					}
				}
			}
		}
	}

	return new_side;
}

void play_controller::do_post_unit(bool replay)
{
	if (!replay) {
		recorder.add_post_unit();
	}
	finish_side_turn();

	units_.do_escape_ticks_bh(teams_, *gui_, player_number_);
	unit::actor = NULL;
}

void play_controller::more_card(team& current_team, int turn)
{
	if (!turn || turn % 25 || !current_team.is_human()) {
		return;
	}
	if (has_network_player()) {
		return;
	}
	bool can_wander = false;
	bool can_office = false;
	const std::vector<size_t>& candidate = current_team.candidate_cards();
	for (std::vector<size_t>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		const card& c = cards_[*it];
		if (c.number_ == cards_.wander().number_) {
			can_wander = true;
		} else if (c.number_ == cards_.office().number_) {
			can_office = true;
		}
	}
	if (!can_wander && !can_office) {
		return;
	}

	const std::vector<size_t>& holden = current_team.holded_cards();
	int wanders = 0;
	int offices = 0;
	for (std::vector<size_t>::const_iterator it = holden.begin(); it != holden.end(); ++ it) {
		const card& c = cards_[*it];
		if (c.number_ == cards_.wander().number_) {
			wanders ++;
		} else if (c.number_ == cards_.office().number_) {
			offices ++;
		}
	}
	if (!can_wander) {
		wanders = offices + 1;
	} else if (!can_office) {
		offices = wanders + 1;
	}

	card& add = (wanders <= offices)? cards_.wander(): cards_.office();
	if (current_team.add_card(add.number_, true)) {
		refresh_card_button(current_team, *gui_);
		// card
		utils::string_map symbols;
		symbols["first"] = tintegrate::generate_format(current_team.holded_card(current_team.holded_cards().size() - 1).name(), tintegrate::object_color);
		game_events::show_hero_message(&heros_[hero::number_scout], NULL, vgettext("Get card: $first.", symbols), game_events::INCIDENT_CARD);
	}
}

//builds the snapshot config from its members and their configs respectively
void play_controller::to_config(config& cfg) const
{
	// config cfg;
	cfg.clear();

	cfg.merge_attributes(level_);

	cfg.remove_attribute("employ_count");
	cfg.remove_attribute("ai_count");
	cfg.remove_attribute("modify_placing");

	cfg["turn_based"] = tent::turn_based;
	cfg["main_ticks"] = unit_map::main_ticks;
	cfg["autosave_ticks"] = autosave_ticks_;
	cfg["global_signature"] = unit::global_signature;
	cfg["subcontinent"] = tent::subcontient_to_str();

	std::stringstream strstr;
	strstr.str("");
	for (std::vector<int>::const_iterator it = treasures_.begin(); it != treasures_.end(); ++ it) {
		if (it != treasures_.begin()) {
			strstr << ", ";
		}
		strstr << *it;
	}
	cfg["treasures"] = strstr.str();

	// emploies
	strstr.str("");
	for (std::vector<hero*>::const_iterator it = emploies_.begin(); it != emploies_.end(); ++ it) {
		int number = (*it)->number_;
		if (it != emploies_.begin()) {
			strstr << ", ";
		}
		strstr << number;
	}
	cfg["emploies"] = strstr.str();

	if (final_capital_.first && !cfg.has_attribute("final_capital")) {
		strstr.str("");
		strstr << final_capital_.first->cityno() << "," << (final_capital_.second? final_capital_.second->cityno(): 0);
		cfg["final_capital"] = strstr.str();
	}

	// scenario environment
	cfg["maximal_defeated_activity"] = game_config::maximal_defeated_activity;
	if (duel_ == NO_DUEL) {
		cfg["duel"] = "no";
	} else if (duel_ == ALWAYS_DUEL) {
		cfg["duel"] = "always";
	} else {
		cfg.remove_attribute("duel");
	}
	cfg["vip"] = vip_;

	strstr.str("");
	for (std::map<int, std::string>::const_iterator it = renames_.begin(); it != renames_.end(); ++ it) {
		if (it != renames_.begin()) {
			strstr << ", ";
		}
		strstr << it->first << ", " << it->second;
	}
	cfg["renames"] = strstr.str();

	if (difficulty_level_) {
		cfg["difficulty_level"] = difficulty_level_;
	}
	
	cfg["provoke_cache"] = provoke_cache_2_str();
	cfg["tactic_cache"] = slot_cache::cache_2_str();
	cfg["guard_cache"] = guard_cache::cache_2_str();

	if (all_ai_allied_) {
		cfg["ai_allied"] = all_ai_allied_;
	}
	cfg["runtime"] = true;

	unit_segment2* top = (unit_segment2*)game_config::savegame_cache;
	top->count_ = teams_.size();
	int offset = sizeof(unit_segment2);
	
	for (std::vector<team>::const_iterator t = teams_.begin(); t != teams_.end(); ++t) {
		const std::vector<artifical*>& holden_cities = t->holden_cities();
		const std::pair<unit**, size_t> p = t->field_troop();

		unit_segment2* side = (unit_segment2*)(game_config::savegame_cache + offset);
		side->count_ = holden_cities.size() + p.second;
		side->size_ = offset;
		offset += sizeof(unit_segment2);

		t->write(game_config::savegame_cache + offset);
		offset += ((team_fields_t*)(game_config::savegame_cache + offset))->size_;

		// current visible units
		for (std::vector<artifical*>::const_iterator i = holden_cities.begin(); i != holden_cities.end(); ++ i) {
			artifical* city = *i;
			// this city
			// Attention: Config of city must be written prior to other artifical. When loading, any artifical added to city, this city must be evalued.
			city->write(game_config::savegame_cache + offset);
			unit_fields_t* fields = (unit_fields_t*)(game_config::savegame_cache + offset);
			offset += fields->size_;
		}
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			if (troops[i]->is_artifical()) {
				// field artifcals
				troops[i]->write(game_config::savegame_cache + offset);
				unit_fields_t* fields = (unit_fields_t*)(game_config::savegame_cache + offset);
				offset += fields->size_;
			} else {
				// field troops
				troops[i]->write(game_config::savegame_cache + offset);
				unit_fields_t* fields = (unit_fields_t*)(game_config::savegame_cache + offset);
				offset += fields->size_;
			}
		}
		side->size_ = offset - side->size_;
	}
	top->size_ = offset;

	cfg.merge_with(tod_manager_.to_config());

	// Write terrain_graphics data in snapshot, too
	BOOST_FOREACH (const config &tg, level_.child_range("terrain_graphics")) {
		cfg.add_child("terrain_graphics", tg);
	}

	//write out the current state of the map
	cfg["map_data"] = map_.write();
}

// whether execute from recover command
bool play_controller::is_recovering(int side) const
{
	return replaying_ || teams_[side - 1].is_network_human();
}

bool play_controller::scenario_env_changed(const tscenario_env& env) const
{
	if (env.duel != duel_) return true;
	if (env.maximal_defeated_activity != game_config::maximal_defeated_activity) return true;
	if (env.vip != vip_) return true;
	return false;
}

void play_controller::do_scenario_env(const tscenario_env& env, bool to_recorder)
{
	if (to_recorder) {
		recorder.add_scenario_env(env);
	}

	set_duel(env.duel);
	set_vip(env.vip);
	game_config::maximal_defeated_activity = env.maximal_defeated_activity;
}

void play_controller::set_vip(bool vip)
{
	vip_ = vip;

	more_card_ = vip;
	tactic_slot_ = vip;
	game_config::active_tactic_slots = game_config::fixed_tactic_slots + (tactic_slot_? 1: 0);
}

void play_controller::erase_treasure(int pos)
{
	treasures_.erase(treasures_.begin() + pos);
}

void play_controller::erase_employ(hero& h)
{
	std::vector<hero*>::iterator find = std::find(emploies_.begin(), emploies_.end(), &h);
	emploies_.erase(find);
}

void play_controller::finish_side_turn()
{
	if (!tent::turn_based) {
		if (actor_can_action(units_)) {
			if (unit::actor->is_artifical()) {
				const team& t = current_team();
				for (std::vector<artifical*>::const_iterator it = t.holden_cities().begin(); it != t.holden_cities().end(); ++ it) {
					artifical& city = **it;
					city.end_turn(true);
				}
			} else {
				unit::actor->end_turn(true);
			}
		}

		// expedited, revivaled
		const std::pair<unit**, size_t> p = current_team().field_troop();
		for (size_t i = 0; i < p.second; i ++) {
			unit& u = *p.first[i];
			if (u.get_state(ustate_tag::EXPEDITED) || u.get_state(ustate_tag::REVIVALED)) {
				u.end_turn(true);
			}
		}

	} else {
		calculate_end_turn(teams_, player_number_);
	}

	// This implements "delayed map sharing."
	// It is meant as an alternative to shared vision.
	if (unit::actor && teams_[unit::actor->side() - 1].copy_ally_shroud()) {
		gui_->recalculate_minimap();
		gui_->invalidate_all();
	}

	if (!allow_intervene_) {
		mouse_handler_.deselect_hex();
	}
	game_events::pump();
}

void play_controller::finish_turn()
{
  	const std::string turn_num = str_cast(turn() - 1);
	const std::string side_num = str_cast(player_number_);
	game_events::fire("turn end");
	game_events::fire("turn " + turn_num + " end");

	LOG_NG << "turn event..." << (recorder.is_skipping() ? "skipping" : "no skip") << '\n';
	update_locker lock_display(gui_->video(),recorder.is_skipping());
	gamestate_.get_variable("turn_number") = int(turn());
}

bool play_controller::enemies_visible() const
{
	// If we aren't using fog/shroud, this is easy :)
	if(current_team().uses_fog() == false && current_team().uses_shroud() == false)
		return true;

	// See if any enemies are visible
	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u)
		if (current_team().is_enemy(u->side()) && !gui_->fogged(u->get_location()))
			return true;

	return false;
}

bool play_controller::execute_command(hotkey::HOTKEY_COMMAND command, int index, std::string str)
{
	return command_executor::execute_command(command, index, str);
}

bool play_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	if (index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if ((i < savenames_.size() && !savenames_[i].empty()) || (i < wml_commands_.size() && wml_commands_[i] != NULL)) {
			return true;
		}
	}
	switch (command) {

	// Commands we can always do:
	case hotkey::HOTKEY_ZOOM_IN:
	case hotkey::HOTKEY_ZOOM_OUT:
	case hotkey::HOTKEY_ZOOM_DEFAULT:
	case hotkey::HOTKEY_SCREENSHOT:
	case hotkey::HOTKEY_MAP_SCREENSHOT:
	case hotkey::HOTKEY_TOGGLE_GRID:
	case hotkey::HOTKEY_MOUSE_SCROLL:
	case hotkey::HOTKEY_ANIMATE_MAP:
	case hotkey::HOTKEY_MUTE:
	case hotkey::HOTKEY_PREFERENCES:
	case hotkey::HOTKEY_OBJECTIVES:
	case hotkey::HOTKEY_STATISTICS:
	case hotkey::HOTKEY_QUIT_GAME:
	case hotkey::HOTKEY_CLEAR_MSG:
	case hotkey::HOTKEY_UNIT_DETAIL:
	case hotkey::HOTKEY_RPG:
		return true;

	case hotkey::HOTKEY_HELP:
		return game_config::tiny_gui? false: true;

	case hotkey::HOTKEY_CHAT_LOG:
		return network::nconnections() > 0;

	case hotkey::HOTKEY_UNDO:
		return !linger_ && !undo_stack_.empty() && !events::commands_disabled && !browse_;

	default:
		return false;
	}
}

void play_controller::enter_textbox()
{
	if(menu_handler_.get_textbox().active() == false) {
		return;
	}

	const std::string str = menu_handler_.get_textbox().box()->text();
	const unsigned int team_num = player_number_;
	events::mouse_handler& mousehandler = mouse_handler_;

	switch(menu_handler_.get_textbox().mode()) {
	case gui::TEXTBOX_MESSAGE:
		menu_handler_.do_speak();
		menu_handler_.get_textbox().close(*gui_);  //need to close that one after executing do_speak() !
		break;
	default:
		menu_handler_.get_textbox().close(*gui_);
		ERR_DP << "unknown textbox mode\n";
	}

}

void play_controller::tab()
{
	gui::TEXTBOX_MODE mode = menu_handler_.get_textbox().mode();

	std::set<std::string> dictionary;
	switch(mode) {
	case gui::TEXTBOX_MESSAGE:
	{
		BOOST_FOREACH (const team& t, teams_) {
			if(!t.is_empty())
				dictionary.insert(t.current_player());
		}

		// Add observers
		BOOST_FOREACH (const std::string& o, gui_->observers()){
			dictionary.insert(o);
		}
		//Exclude own nick from tab-completion.
		//NOTE why ?
		dictionary.erase(preferences::login());

		break;
	}
	default:
		ERR_DP << "unknown textbox mode\n";
	} //switch(mode)

	menu_handler_.get_textbox().tab(dictionary);
}


team& play_controller::current_team()
{
	assert(player_number_ > 0 && player_number_ <= int(teams_.size()));
	return teams_[player_number_-1];
}

const team& play_controller::current_team() const
{
	assert(player_number_ > 0 && player_number_ <= int(teams_.size()));
	return teams_[player_number_-1];
}

int play_controller::find_human_team_before(const size_t team_num) const
{
	if (team_num > teams_.size())
		return -2;

	int human_side = -2;
	for (int i = team_num-2; i > -1; --i) {
		if (teams_[i].is_human()) {
			human_side = i;
			break;
		}
	}
	if (human_side == -2) {
		for (size_t i = teams_.size()-1; i > team_num-1; --i) {
			if (teams_[i].is_human()) {
				human_side = i;
				break;
			}
		}
	}
	return human_side+1;
}

void play_controller::fill_employ_hero(const hero& leader, std::set<int>& candidate, int count, int& random)
{
	// 1/3: wander
	size_t loops = 0;
	while (count && !candidate.empty() && loops < candidate.size() * 4) {
		int pos = random % candidate.size();
		std::set<int>::iterator it = candidate.begin();
		std::advance(it, pos);
		hero& h = heros_[*it];

		if (!runtime_groups::exist_member(h.number_, leader) && (h.leadership_ >= ftofxp9(85) || h.charm_ >= ftofxp9(85))) {
			h.status_ = hero_status_employable;
			emploies_.push_back(&h);

			candidate.erase(it);
			count --;
		}
		// avoid deadlook.
		loops ++;
		if (loops ++ % candidate.size() == 0) {
			random ++;
		}

		random = random * 1103515245 + 12345;
		random = (static_cast<unsigned>(random / 65536) % 32768);
	}
}

void play_controller::calculate_difficulty_level()
{
	const int min_level = 1, max_level = 5;

	team& t = teams_[human_team_];
	if (tent::tower_mode()) {
		std::vector<hero*> service, wander;
		for (std::vector<artifical*>::const_iterator it = t.holden_cities().begin(); it != t.holden_cities().end(); ++ it) {
			artifical& city = **it;
			std::copy(city.fresh_heros().begin(), city.fresh_heros().end(), std::back_inserter(service));
			std::copy(city.wander_heros().begin(), city.wander_heros().end(), std::back_inserter(wander));
		}
		int service_cost = 0, great_150 = 0, employ_cost = 0;
		for (std::vector<hero*>::const_iterator it = service.begin(); it != service.end(); ++ it) {
			hero& h = **it;
			service_cost += h.cost_;
		}
		for (std::vector<hero*>::const_iterator it = wander.begin(); it != wander.end(); ++ it) {
			hero& h = **it;
			if (h.cost_ >= 150) {
				great_150 ++;
			}
		}
		for (std::vector<hero*>::const_iterator it = emploies_.begin(); it != emploies_.end(); ++ it) {
			hero& h = **it;
			employ_cost += h.cost_;
		}

		// assume 170 is maximum cost
		int easy_percent = service_cost / service.size() - 110;  // ==> 50%
		easy_percent += 40 * great_150 / 8; // ==> 40%
		if (!emploies_.empty()) {
			easy_percent += employ_cost / emploies_.size() - 145; // ==> 10%
		}

		int enemy_hero_count = 0;
		for (city_map::const_iterator it = units_.get_city_map().begin(); it != units_.get_city_map().end(); ++ it) {
			if (!t.is_enemy(it->side())){
				continue;
			}
			enemy_hero_count += it->fresh_heros().size() + it->wander_heros().size();
		}
		if (enemy_hero_count > 30) {
			easy_percent -= (enemy_hero_count - 30) * 2;
		}

		if (easy_percent > 100) {
			easy_percent = 100;
		}
		difficulty_level_ = (100 - easy_percent + 15) / 20;
		
	} else {
		// other mode fix to most easy.
		difficulty_level_ = min_level;
	}

	if (difficulty_level_ < min_level) {
		difficulty_level_ = min_level;

	} else if (difficulty_level_ > max_level) {
		difficulty_level_ = max_level;
	}
}

bool play_controller::calculate_capture() const
{
	const team& t = teams_[human_team_];
	bool capture = true;
	for (std::vector<team>::const_iterator it = teams_.begin(); it != teams_.end(); ++ it) {
		if (!t.is_enemy(it->side())) {
			continue;
		}
		if (!it->holden_cities().empty()) {
			capture = false;
			break;
		}
	}
	return capture;
}

// pre_kingdom_pass on mysql save score filed use tinyint(3) type, maximum value is 127.
const int max_pass_score = 127;
const int none_score = 10;
const int siege_score = 10;

std::pair<int, int> play_controller::calculate_score(LEVEL_RESULT result) const
{
	bool capture = calculate_capture();
	const team& t = teams_[human_team_];

	int score = 0, coin = 0;
	if (tent::mode == mode_tag::TOWER) {
		if (result == VICTORY) {
			score = turn() > 10? 16: 0;
			score += difficulty_level_ * 8;
			score += capture? 8: 0;
			score += 8 * t.perfect_turns_;

			int divisor;
			if (turn() < 20) {
				divisor = 10;	
			} else {
				divisor = 10 + (turn() - 20 ) / 2;
			}
			score = score * 10 / divisor;
			if (score > max_pass_score) {
				score = max_pass_score;
			}
		}

	} else if (tent::mode == mode_tag::SIEGE) {
		if (result == VICTORY) {
			int defender_team_index = -1;
			for (std::vector<team>::const_iterator it = teams_.begin(); it != teams_.end(); ++ it) {
				const team& t = *it;
				if (t.side() == human_team_ + 1) {
					
				} else if (t.side() <= 2) {
					defender_team_index = t.side() - 1;
				} else if (t.is_ai()) {
					if (t.is_enemy(human_team_ + 1)) {
						
					} else {
						
					}
				}
			}
			const team& defender_team = teams_[defender_team_index];
			tgroup& g = runtime_groups::get(defender_team.leader()->number_);
			if (!tent::subcontinent.first.empty()) {
				score = siege_score;
			} else if ((int)g.members().size() < (game_config::least_fix_members + game_config::least_roam_members) || g.layout().empty()) {
				score = 0;
			} else {
				score = siege_score;
			}

		} else {
			score = siege_score;
		}

	} else if (tent::mode == mode_tag::RPG) {
		if (result == VICTORY) {
			if (turn() >= 20 && teams_.size() > 12) {
				score = 8 * t.perfect_turns_;

				if (score > max_pass_score) {
					score = max_pass_score;
				}

				coin = 1;
			} else {
				score = none_score;
			}
		}

	} else if (tent::mode != mode_tag::LAYOUT) {
		if (turn() >= 6) {
			score = none_score;
		}
	}

	return std::make_pair(coin, score);
}

void play_controller::clear_slot_cache_selected()
{
	mouse_handler_.do_right_click(false);
}

void play_controller::do_delay_call(bool clear)
{
	if (!allow_intervene_) {
		return;
	}

	if (!events::commands_disabled) {
		// Notice
		// 1: when do slot action, maybe add to slot.
		// 2: replay don't execute do_delay_call, so keep synchronism.
		std::vector<slot_cache::tslot> calls = slot_cache::cache;
		while (!slot_cache::cache.empty()) {
			const slot_cache::tslot call = slot_cache::cache.front();
			
			call.u->remove_from_slot_cache(0);
			team& tm = teams_[call.u->side() - 1];
			int cost = call.cost();

			if (call.type == slot_cache::tslot::CAST_TACTIC) {
				const ttactic& t = unit_types.tactic(call.tactic.h->tactic_);
				if (call.u->tactic_degree() >= t.point() * game_config::tactic_degree_per_point || call.u->hot_turns()) {
					cost = 0;
				}
				if (tm.gold() < cost) {
					continue;
				}
				bool consume = !cost;
				::cast_tactic(teams_, units_, *call.u, *call.tactic.h, call.tactic.special, true, consume, cost);
				
			} else if (call.type == slot_cache::tslot::CLEAR_FORMATIONED) {
				if (!cost) {
					continue;
				}
				if (tm.gold() < cost) {
					continue;
				}
				do_clear_formationed(*gui_, teams_, units_, *call.u, cost, true);

			} else if (call.type == slot_cache::tslot::MOVE) {
				if (tm.gold() < cost) {
					continue;
				}
				const unit* target = units_.find_unit(call.move.to);
				if (target && (!target->base() || tm.is_enemy(target->side()))) {
					continue;
				}
				do_direct_move(teams_, units_, map_, *call.u, call.move.to, cost, true);
			}
		}
	} else if (clear) {
		slot_cache::cache.clear();
	}
}

void play_controller::slice_before_scroll() 
{
	do_delay_call(false);

	soundsources_manager_->update();
}

events::mouse_handler& play_controller::get_mouse_handler_base() {
	return mouse_handler_;
}

game_display& play_controller::get_display() {
	return *gui_;
}

bool play_controller::have_keyboard_focus()
{
	return !menu_handler_.get_textbox().active();
}

void play_controller::process_focus_keydown_event(const SDL_Event& event)
{
	if(event.key.keysym.sym == SDLK_ESCAPE) {
		menu_handler_.get_textbox().close(*gui_);
	} else if(event.key.keysym.sym == SDLK_TAB) {
		tab();
	} else if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
		enter_textbox();
	}
}

void play_controller::process_keydown_event(const SDL_Event& event) {
}

void play_controller::process_keyup_event(const SDL_Event& event) {
	// If the user has pressed 1 through 9, we want to show
	// how far the unit can move in that many turns
	if(event.key.keysym.sym >= '1' && event.key.keysym.sym <= '7') {
		const int new_path_turns = (event.type == SDL_KEYDOWN) ?
		                           event.key.keysym.sym - '1' : 0;

		if(new_path_turns != mouse_handler_.get_path_turns()) {
			mouse_handler_.set_path_turns(new_path_turns);

			const unit_map::iterator u = mouse_handler_.selected_unit();

			if(u != units_.end()) {
				bool teleport = u->get_ability_bool("teleport");

				// if it's not the unit's turn, we reset its moves
				unit_movement_resetter move_reset(*u, u->side() != player_number_);

				mouse_handler_.set_current_paths(pathfind::paths(map_, units_, u->get_location(),
				                       teams_,false,teleport, teams_[gui_->viewing_team()],
				                       mouse_handler_.get_path_turns()));

				gui_->highlight_reach(mouse_handler_.current_paths());
			}
		}
	}
}

void play_controller::post_mouse_press(const SDL_Event& /*event*/)
{
	if (mouse_handler_.get_undo()) {
		mouse_handler_.set_undo(false);
		menu_handler_.undo(player_number_);
	}
}

static void trim_items(std::vector<std::string>& newitems) {
	if (newitems.size() > 5) {
		std::vector<std::string> subitems;
		subitems.push_back(newitems[0]);
		subitems.push_back(newitems[1]);
		subitems.push_back(newitems[newitems.size() / 3]);
		subitems.push_back(newitems[newitems.size() * 2 / 3]);
		subitems.push_back(newitems.back());
		newitems = subitems;
	}
}

void play_controller::expand_autosaves(std::vector<std::string>& items)
{
}

void play_controller::expand_wml_commands(std::vector<std::string>& items)
{
	wml_commands_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "wml") {
			items.erase(items.begin() + i);
			std::map<std::string, wml_menu_item*>& gs_wmi = gamestate_.wml_menu_items;
			if(gs_wmi.empty())
				break;
			std::vector<std::string> newitems;

			const map_location& hex = mouse_handler_.get_last_hex();
			gamestate_.get_variable("x1") = hex.x + 1;
			gamestate_.get_variable("y1") = hex.y + 1;
			scoped_xy_unit highlighted_unit("unit", hex.x, hex.y, units_);

			std::map<std::string, wml_menu_item*>::iterator itor;
			for (itor = gs_wmi.begin(); itor != gs_wmi.end()
				&& newitems.size() < MAX_WML_COMMANDS; ++itor) {
				config& show_if = itor->second->show_if;
				config filter_location = itor->second->filter_location;
				if ((show_if.empty()
					|| game_events::conditional_passed(vconfig(show_if)))
				&& (filter_location.empty()
					|| terrain_filter(vconfig(filter_location), units_)(hex))
				&& (!itor->second->needs_select
					|| gamestate_.last_selected.valid()))
				{
					wml_commands_.push_back(itor->second);
					std::string newitem = itor->second->description;

					// Prevent accidental hotkey binding by appending a space
					newitem.push_back(' ');
					newitems.push_back(newitem);
				}
			}
			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			break;
		}
		wml_commands_.push_back(NULL);
	}
}

bool play_controller::in_build_menu(const std::string& minor) const
{
	const team& t = current_team();
	const std::set<const unit_type*>& can_build = t.builds();

	const unit_type* ut = unit_types.id_type(minor);
	if (!ut) {
		return false;
	} 
	return can_build.find(ut) != can_build.end();
}

bool play_controller::actived_build_menu(const std::string& minor) const
{
	const team& t = current_team();
	const std::set<const unit_type*>& can_build = t.builds();

	const unit_type* ut = unit_types.id_type(minor);
	if (!ut) {
		return false;
	} 
	if (ut->master() == hero::number_wall && !t.may_build_wall_count()) {
		return false;
	}
	return true;
}

bool play_controller::in_context_menu(const std::string& id) const
{
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	hotkey::HOTKEY_COMMAND command = hotkey::get_hotkey(item.first).get_id();

	// current side must human player
	const team& current_team = teams_[player_number_ - 1];

	if (command != hotkey::HOTKEY_LIST && command != hotkey::HOTKEY_SYSTEM && !replaying_ && !current_team.is_human()) {
		return false;
	}
	if (linger_) {
		if (command == hotkey::HOTKEY_LIST || command == hotkey::HOTKEY_SYSTEM) {
			return true;
		} else {
			return command == hotkey::HOTKEY_UPLOAD && !replaying_;
		}
	}
	if (replaying_ && (command == hotkey::HOTKEY_TECHNOLOGY_TREE || command == hotkey::HOTKEY_FINAL_BATTLE)) {
		// current don't support, remark them.
		return false;
	}

	const artifical* city = NULL;
	const map_location& loc = mouse_handler_.get_selected_hex();
	unit_map::const_iterator itor;
	if (!replaying_) {
		itor = find_visible_unit(loc, current_team);
	}

	switch(command) {
	// Only display these if the mouse is over a castle or keep tile
	case hotkey::HOTKEY_RECRUIT:
	case hotkey::HOTKEY_EXPEDITE:
	case hotkey::HOTKEY_ARMORY:
	case hotkey::HOTKEY_MOVE:
		if (rpging_ || tent::tower_mode()) {
			return false;
		}
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (!unit::actor) {
			return false;
		}
		if (!actor_can_continue_action(units_, player_number_) || !unit::actor->is_city()) {
			return false;
		}
		// loc is in myself city.
		city = units_.city_from_loc(loc);
		if (city && unit::actor->side() == city->side()) {
			return true;
		}
		break;

	case hotkey::HOTKEY_BUILD_M:
		if (current_team.builds().empty()) {
			return false;
		}
	case hotkey::HOTKEY_BUILD:
		if (rpging_) {
			return false;
		}
		if (tent::mode == mode_tag::SIEGE) {
			return false;
		}
		if (!itor.valid() || itor->is_artifical() || !itor->human()) {
			return false;
		}
		if (command == hotkey::HOTKEY_BUILD && !in_build_menu(item.second)) {
			return false;
		}
		if (tent::tower_mode()) {
			return true;
		}
		break;

	case hotkey::HOTKEY_GUARD:
		if (tent::tower_mode()) {
			return false;
		}
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (itor.valid() && !itor->is_artifical() && (itor->side() == player_number_) && itor->human()) {
			return true;
		}
		break;

	case hotkey::HOTKEY_ABOLISH:
		if (tent::tower_mode()) {
			return false;
		}
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (itor.valid() && !itor->is_artifical() && (itor->side() == player_number_) && itor->human()) {
			if (itor->task() != unit::TASK_NONE) {
				return true;
			}
		}
		break;

	case hotkey::HOTKEY_EXTRACT:
		if (!tent::tower_mode()) {
			return false;
		}
		if (tent::mode == mode_tag::SIEGE) {
			return false;
		}
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (itor.valid() && !itor->is_artifical() && (itor->side() == player_number_)) {
			return true;
		}
		break;

	case hotkey::HOTKEY_ADVANCE:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return false;
		}
		if (tent::mode != mode_tag::LAYOUT || !itor.valid() || itor->side() != player_number_) {
			return false;
		}
		if (itor->is_artifical()) {
			return hero::is_ea_artifical(itor->type()->master());
		}
		break;

	case hotkey::HOTKEY_DEMOLISH:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return false;
		}
		if (tent::mode == mode_tag::SIEGE) {
			return false;
		}
		if (rpg::stratum == hero_stratum_citizen) {
			return false;
		}
		if (!actor_can_continue_action(units_, player_number_)) {
			return false;
		}
		if (!itor.valid()) {
			return false;
		}

		if ((tent::tower_mode() || itor->is_artifical()) && !unit_is_city(&*itor) && !itor->fort()) {
			if (tent::tower_mode() || unit::actor->side() == itor->side()) {
				return true;
			}
		}
		break;

	case hotkey::HOTKEY_INTERIOR:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return false;
		}
		return !events::commands_disabled && !itor.valid();

	case hotkey::HOTKEY_TECHNOLOGY_TREE:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return false;
		}
		if (tent::mode == mode_tag::SIEGE) {
			return false;
		}
		return !events::commands_disabled && !itor.valid();

	case hotkey::HOTKEY_EMPLOY:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_ || tent::mode != mode_tag::TOWER) {
			return false;
		}
		return replaying_ || (!events::commands_disabled && !itor.valid());

	case hotkey::HOTKEY_UPLOAD:
		return linger_ && !rpging_;

	case hotkey::HOTKEY_FINAL_BATTLE:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_ || tent::mode != mode_tag::RPG) {
			return false;
		}
		return !events::commands_disabled && (teams_.size() >= 3) && !itor.valid();

	case hotkey::HOTKEY_LIST:
	case hotkey::HOTKEY_SYSTEM:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return false;
		}
		return replaying_ || (!events::commands_disabled && !itor.valid() && (has_network_player() || current_team.is_human()));

	case hotkey::HOTKEY_RPG_EXCHANGE:
	case hotkey::HOTKEY_RPG_INDEPENDENCE:
		if (rpg::stratum != hero_stratum_mayor || linger_ || replaying_ || events::commands_disabled || has_network_player()) {
			return false;
		}
	case hotkey::HOTKEY_RPG_DETAIL:
		if (mouse_handler_.in_multistep_state()) {
			return false;
		}
		if (rpging_) {
			return true;
		}
		break;
	case hotkey::HOTKEY_RPG_TREASURE:
		return false;

	default:
		return true;
	}

	return false;
}

bool play_controller::actived_context_menu(const std::string& id) const
{
	return actived_context_menu2(id, mouse_handler_.get_selected_hex());
}

bool play_controller::actived_context_menu2(const std::string& id, const map_location& loc) const
{
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	hotkey::HOTKEY_COMMAND command = hotkey::get_hotkey(item.first).get_id();

	const team& current_team = teams_[player_number_ - 1];
	const std::vector<artifical*>& holden_cities = current_team.holden_cities();
	unit_map::const_iterator itor;
	if (!replaying_) {
		itor = find_visible_unit(loc, current_team);
	}
	const artifical* city = units_.city_from_loc(loc);

	switch(command) {
	case hotkey::HOTKEY_EMPLOY:
		return !emploies_.empty();

	case hotkey::HOTKEY_BUILD_M:
		// in tower_mode(), if can build one type only, it must be wall.
		if (tent::tower_mode() && current_team.builds().size() == 1) {
			if (units_.count(loc, false)) {
				return false;
			}
			const unit_type* wall = unit_types.find_wall();
			if (!wall->terrain_matches(map_.get_terrain(loc))) {
				return false;
			}
			if (!current_team.may_build_wall_count()) {
				return false;
			}
		}
		break;

	case hotkey::HOTKEY_BUILD:
		if (!actived_build_menu(item.second)) {
			return false;
		}
		break;

	case hotkey::HOTKEY_RECRUIT:
		if (rpg::stratum == hero_stratum_citizen) {
			return false;
		} else if (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h) {
			return false;
		}
		if (city->fresh_heros().empty()) {
			return false;
		}
		break;
	case hotkey::HOTKEY_EXPEDITE:
		if (city->reside_troops().empty()) {
			return false;
		}
		if (city->is_surrounded()) {
			return false;
		}
		break;
	case hotkey::HOTKEY_ARMORY:
		{
			const std::vector<unit*>& reside_troops = city->reside_troops();
			const std::vector<hero*>& fresh_heros = city->fresh_heros();
			size_t reside_troops_size = reside_troops.size();
			if (!reside_troops_size || (reside_troops_size == 1 && !reside_troops[0]->second().valid() && fresh_heros.empty())) {
				return false;
			}
			for (std::vector<unit*>::const_iterator it = reside_troops.begin(); it != reside_troops.end(); ++ it) {
				const unit& u = **it;
				if (u.human()) {
					return true;
				}
			}
			return false;
		}
	case hotkey::HOTKEY_MOVE:
		if (rpg::stratum == hero_stratum_citizen) {
			if (city->cityno() != rpg::h->city_ || rpg::h->status_ != hero_status_idle) {
				return false;
			}
		} else if (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h) {
			return false;
		}
		if (current_team.holden_cities().size() < 2 || city->fresh_heros().empty()) {
			return false;
		}
		break;

	case hotkey::HOTKEY_EXTRACT:
		if (!itor.valid() || itor->is_artifical() || (itor->side() != player_number_) || !itor->second().valid()) {
			return false;
		}
		break;

	case hotkey::HOTKEY_ADVANCE:
		if (!itor.valid() || itor->side() != player_number_) {
			return false;
		}
		return itor->type()->can_advance();

	case hotkey::HOTKEY_DEMOLISH:
		if (!itor.valid() || itor->side() != player_number_ || unit_is_city(&*itor)) {
			return false;
		}
		if (itor->is_artifical() && !tent::tower_mode()) {
			const artifical* art = unit_2_artifical(&*itor);
			const map_location* tiles;
			size_t adjacent_size;
			// to mayor, can domolish it that owner his city.
			if (rpg::stratum == hero_stratum_mayor) {
				if (art->type() == unit_types.find_market()) {
					std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(loc);
					if (units_.city_from_cityno(it->second)->mayor() != rpg::h) {
						return false;
					}
				} else if (art->type() == unit_types.find_keep() || art->type() == unit_types.find_wall()) {
					bool found_rpg_city = false;
					tiles = itor->adjacent_2_;
					adjacent_size = itor->adjacent_size_2_;
					for (int i = 0; i != adjacent_size; ++i) {
						unit_map::const_iterator another = units_.find(tiles[i]);
						if (another.valid() && another->is_city()) {
							const artifical* city = unit_2_artifical(&*another);
							if (city->mayor() == rpg::h) {
								found_rpg_city = true;
								break;
							}
						}
					}
					if (!found_rpg_city) {
						return false;
					}
				}
			}
			// there is enemy unit in 2 grids, cannot demolish.
			for (int times = 0; times < 2; times ++) {
				if (times == 0) {
					tiles = art->adjacent_;
					adjacent_size = art->adjacent_size_;
				} else {
					tiles = itor->adjacent_2_;
					adjacent_size = itor->adjacent_size_2_;
				}
				for (int i = 0; i != adjacent_size; ++i) {
					unit* another = units_.find_unit(tiles[i]);
					if (another && !another->is_artifical() && current_team.is_enemy(another->side())) {
						return false;
					}
				}
			}
			return true;			
		}
		break;
	case hotkey::HOTKEY_RPG_TREASURE:
		for (std::vector<artifical*>::const_iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
			const artifical* city = *it;

			const std::vector<unit*>& resides = city->reside_troops();
			for (std::vector<unit*>::const_iterator it2 = resides.begin(); it2 != resides.end(); ++ it2) {
				unit* u = *it2;
				if (!u->human()) {
					continue;
				}
				return true;
			}

			const std::vector<hero*>& freshes = city->fresh_heros();
			const std::vector<hero*>& finishes = city->finish_heros();
			if (city->mayor() == rpg::h || rpg::stratum == hero_stratum_leader) {
				if (!freshes.empty() || !finishes.empty()) {
					return true;
				}
			}
			if (rpg::stratum == hero_stratum_citizen) {
				if (std::find(freshes.begin(), freshes.end(), rpg::h) != freshes.end()) {
					return true;
				}
				if (std::find(finishes.begin(), finishes.end(), rpg::h) != finishes.end()) {
					return true;
				}
			}
		}
		return false;
	case hotkey::HOTKEY_RPG_EXCHANGE:
	case hotkey::HOTKEY_RPG_INDEPENDENCE:
		if (holden_cities.size() < 2) {
			return false;
		}
		break;
	}	

	return true;
}

void play_controller::prepare_show_menu(gui2::tbutton& widget, const std::string& id, int width, int height) const
{
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	hotkey::HOTKEY_COMMAND command = hotkey::get_hotkey(item.first).get_id();

	const team& t = current_team();
	std::stringstream strstr;

	if (command == hotkey::HOTKEY_BUILD) {
		int cost_exponent = t.cost_exponent();

		const std::set<const unit_type*>& can_build = t.builds();

		const unit_type* ut = unit_types.id_type(item.second);
		if (!ut) {
			return;
		} 
		
		strstr.str("");
		strstr << "buttons/" << item.second << ".png";
		int cost = ut->cost() * cost_exponent / 100;
		if (tent::tower_mode()) {
			// increase wall's cost.
			cost *= game_config::tower_cost_ratio;
		}
		surface surf = generate_surface(width, height, strstr.str(), cost, false);
		widget.set_surface(surf, width, height);

	} else if (command == hotkey::HOTKEY_BUILD_M) {
		if (tent::tower_mode()) {
			strstr.str("");
			strstr << "buttons/" << item.first << ".png";

			surface surf = generate_surface(width, height, strstr.str(), t.may_build_wall_count(), false);
			widget.set_surface(surf, width, height);
		}

	} else if (command == hotkey::HOTKEY_ABOLISH) {
		strstr.str("");
		strstr << "buttons/" << item.first << ".png";
		surface surf = generate_pip_surface(width, height, strstr.str(), "buttons/icon-guard.png");
		widget.set_surface(surf, width, height);
	}
}

void play_controller::refresh_city_buttons(const artifical& city) const
{
	static hotkey::HOTKEY_COMMAND city_buttons[] = {
		hotkey::HOTKEY_RECRUIT, 
		hotkey::HOTKEY_EXPEDITE, 
		hotkey::HOTKEY_ARMORY, 
		hotkey::HOTKEY_MOVE
	};
	static int nb = sizeof(city_buttons) / sizeof(hotkey::HOTKEY_COMMAND);
	for (int i = 0; i < nb; i ++) {
		gui2::tbutton* widget = dynamic_cast<gui2::tbutton*>(gui_->get_theme_object(hotkey::get_hotkey(city_buttons[i]).get_command()));
		if (widget) {
			widget->set_active(actived_context_menu2(widget->id(), city.get_location()));
		}
	}
}

const std::string& play_controller::select_victory_music() const
{
	if (victory_music_.empty()) {
		return null_str;
	}

	const size_t p = gamestate_.rng().get_next_random() % victory_music_.size();
	assert(p < victory_music_.size());
	return victory_music_[p];
}

const std::string& play_controller::select_defeat_music() const
{
	if (defeat_music_.empty()) {
		return null_str;
	}

	const size_t p = gamestate_.rng().get_next_random() % defeat_music_.size();
	assert(p < defeat_music_.size());
	return defeat_music_[p];
}


void play_controller::set_victory_music_list(const std::string& list)
{
	victory_music_ = utils::split(list);
	if(victory_music_.empty())
		victory_music_ = utils::split(game_config::default_victory_music);
}

void play_controller::set_defeat_music_list(const std::string& list)
{
	defeat_music_  = utils::split(list);
	if(defeat_music_.empty())
		defeat_music_ = utils::split(game_config::default_defeat_music);
}

void play_controller::check_victory()
{
	check_end_level();

	// cities_teams saved this teams that holded one or more city
	std::vector<unsigned> cities_teams;

	bool human_defeated = false;

	// Clear villages for teams that have no city && no field troop
	for (std::vector<team>::iterator tm_beg = teams_.begin(), tm = tm_beg, tm_end = teams_.end(); tm != tm_end; ++tm) {
		if (!tm->holden_cities().empty()) {
			// don't conside side that hasn't city
			if (!tm->is_empty()) {
				cities_teams.push_back(tm->side());
			}
		} else if (teams_[tm->side() - 1].is_human()) {
			human_defeated = true;
		}
		const std::pair<unit**, size_t> p = tm->field_troop();
		if (!p.second && tm->holden_cities().empty()) {
			// invalidate_all() is overkill and expensive but this code is
			// run rarely so do it the expensive way.
			gui_->invalidate_all();
		}
	}
	if (human_defeated) {
		throw end_level_exception(DEFEAT);
	}

	if (final_capital_.first) {
		if (final_capital_.second && teams_[final_capital_.second->side() - 1].is_human()) {
			throw end_level_exception(VICTORY);
		} else if (!teams_[final_capital_.first->side() - 1].is_human()) {
			throw end_level_exception(DEFEAT);
		} 
	}

	bool found_player = false;

	// found_player: 
	//   ture: human控制的阵营已不存在敌对阵营
	//   false: 
	for (size_t n = 0; n != cities_teams.size(); ++n) {
		size_t side = cities_teams[n] - 1;

		for (size_t m = n +1 ; m != cities_teams.size(); ++m) {
			if (teams_[side].is_enemy(cities_teams[m])) {
				return;
			}
		}

		if (teams_[side].is_human()) {
			found_player = true;
		}
	}

	if (found_player) {
		game_events::fire("enemies defeated");
		check_end_level();
	}

	if (!victory_when_enemy_no_city_ && (found_player || is_observer())) {
		// This level has asked not to be ended by this condition.
		return;
	}

	throw end_level_exception(VICTORY);
}

void play_controller::process_oos(const std::string& msg) const
{
	std::stringstream message;
	message << _("The game is out of sync. It might not make much sense to continue. Do you want to save your game?");
	message << "\n\n" << _("Error details:") << "\n\n" << msg;

	config snapshot;
	to_config(snapshot);
	savegame::oos_savegame save(heros_, heros_start_, snapshot);
	save.save_game_interactive(resources::screen->video(), message.str(), gui::YES_NO); // can throw end_level_exception
}

void play_controller::process_oos2(const std::string& msg) const
{
	config snapshot;
	to_config(snapshot);
	savegame::oos_savegame save(heros_, heros_start_, snapshot);
	save.save_game_automatic(gui_->video()); // can throw end_level_exception

	std::stringstream strstr;
	utils::string_map symbols;
	symbols["save"] = tintegrate::generate_format(save.filename(), "yellow");
	strstr << msg;
	strstr << "\n\n";
	strstr << vgettext("wesnoth-lib", "Sorry, run into program BUG. Program has save scene to $save. Hope you to upload it to sever, so that developer can solve it.", symbols);

	gui2::show_message(gui_->video(), "", strstr.str());
	throw end_level_exception(QUIT);
}

void play_controller::set_final_capital(artifical* human, artifical* ai)
{
	final_capital_.first = human;
	final_capital_.second = ai;
}

bool play_controller::do_recruit(artifical* city, int cost_exponent, bool rpg_mode)
{
	int side_num = city->side();
	const map_location& city_loc = city->get_location();
	team& current_team = teams_[side_num - 1];

	const hero *master, *second, *third;
	const unit_type* ut;
	if (city->fresh_heros().empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There are no hero available to recruit"));
		return false;
	}
	if (city->recruits(-1).empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There are no unit type available to recruit."));
		return false;
	}

	{
		gui2::trecruit dlg(*gui_, teams_, units_, heros_, gamestate_, *city, cost_exponent, rpg_mode);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return false;
		}
		// player may generate troop when select heors, and these heros were changed to hero_status_military status.
		// it is time to be back hero_status_idle.
		std::vector<hero*>& fresh_heros = city->fresh_heros();
		for (std::vector<hero*>::iterator itor = fresh_heros.begin(); itor != fresh_heros.end(); ++ itor) {
			(*itor)->status_ = hero_status_idle;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}
		ut = dlg.unit_type_ptr();
		master = dlg.master();
		second = dlg.second();
		third = dlg.third();
		if (!master->valid()) {
			return false;
		}
	}

	if (ut->cost() * cost_exponent / 100 > current_team.gold()) {
		gui2::show_transient_message(gui_->video(), "", _("You don't have enough gold to recruit that unit"));
		return false;
	}
	const events::command_disabler disable_commands;

	bool commercial_changed = false;

	// generated troop: 1)can full movement; 2)can full attack.
	std::vector<const hero*> v;
	v.push_back(&heros_[master->number_]);
	if (second->valid()) {
		v.push_back(&heros_[second->number_]);
	}
	if (third->valid()) {
		v.push_back(&heros_[third->number_]);
	}
	std::sort(v.begin(), v.end(), sort_recruit(ut));

	::do_recruit(units_, heros_, teams_, current_team, ut, v, *city, ut->cost() * cost_exponent / 100, true, true, gamestate_);

	menu_handler_.clear_undo_stack(side_num);
	refresh_city_buttons(*city);

	return true;
}

bool play_controller::generate_commoner(artifical& city, const unit_type* ut, bool replay)
{
	VALIDATE(ut->master() != HEROS_INVALID_NUMBER, "play_controller::generate_commoner, desired master is HEROS_INVALID_NUMBER");

	int side_num = city.side();
	const map_location& city_loc = city.get_location();
	team& current_team = teams_[side_num - 1];

	std::vector<const hero*> v;
	v.push_back(&heros_[ut->master()]);
	type_heros_pair pair(ut, v);
	if (!replay) {
		// recorder.add_recruit(ut->id(), city.get_location(), v, 0, false);
	}

	unit* new_unit = new unit(units_, heros_, teams_, gamestate_, pair, city.cityno(), true, false);
	new_unit->set_movement(new_unit->total_movement());

	current_team.spend_gold(0);
	city.troop_come_into(new_unit, -1);

	menu_handler_.clear_undo_stack(side_num);

	return true;
}

void calculate_commoner_summary(const artifical& city, int& reside_trader, int& reside_transporter, int& field_trader, int& field_transporter)
{
	reside_trader = reside_transporter = field_trader = field_transporter = 0;
	const std::vector<unit*>& reside_commoners = city.reside_commoners();
	for (std::vector<unit*>::const_iterator it = reside_commoners.begin(); it != reside_commoners.end(); ++ it) {
		const unit& u = **it;
		if (u.transport()) {
			reside_transporter ++;
		} else {
			reside_trader ++;
		}
	}
	const std::vector<unit*>& field_commoners = city.field_commoners();
	for (std::vector<unit*>::const_iterator it = field_commoners.begin(); it != field_commoners.end(); ++ it) {
		const unit& u = **it;
		if (u.transport()) {
			field_transporter ++;
		} else {
			field_trader ++;
		}
	}
}

void play_controller::calculate_commoner_gotos(team& current_team, std::vector<std::pair<unit*, int> >& gotos)
{
	gotos.clear();

	const int random = get_random();
	int derived_random;
	std::map<map_location, int> forts;
	std::vector<unit*> can_redirect_transport;
	std::vector<artifical*> can_generate_transport_cities;

	// field commoner
	const std::pair<unit**, size_t> p = current_team.field_troop();
	unit** troops = p.first;
	size_t troops_vsize = p.second;
	for (size_t i = 0; i < troops_vsize; i ++) {
		unit& u = *troops[i];
		if (u.is_commoner()) {
			if (!u.transport()) {
				artifical* goto_city = units_.city_from_loc(u.get_goto());
				if (goto_city->side() != current_team.side() ||
					(u.block_turns() >= 1 && !u.get_from().valid())) {
					// goto city is captured, be back!
					u.set_from(goto_city->get_location());
					u.set_goto(units_.city_from_cityno(u.cityno())->get_location());
					u.set_task(unit::TASK_BACK);
					u.set_block_turns(0);
				}
				gotos.push_back(std::make_pair(&u, -1));
			} else {
				// it is commoner: transport
				const map_location& loc = u.get_goto();
				VALIDATE(loc.valid(), "play_controller, field transport's goto is invalid location!");
				const unit& fort = *units_.find(loc, false);
				if (fort.side() == current_team.side()) {
					std::map<map_location, int>::iterator find = forts.find(loc);
					if (find != forts.end()) {
						find->second ++;
					} else {
						forts.insert(std::make_pair(loc, 1));
					}
					gotos.push_back(std::make_pair(&u, -1));
				} else {
					can_redirect_transport.push_back(&u);
				}
			}
		} else if (u.fort()) {
			// it is fort
			if (u.side() == current_team.side()) {
				const map_location& loc = u.get_location();
				std::map<map_location, int>::iterator find = forts.find(loc);
				if (find == forts.end()) {
					forts.insert(std::make_pair(u.get_location(), 0));
				}
			}
		}
	}

	const std::vector<artifical*>& holded_cities = current_team.holden_cities();
	for (std::vector<artifical*>::const_iterator it = holded_cities.begin(); it != holded_cities.end(); ++ it) {
		artifical& city = **it;

		int reside_transporter = 0;
		int field_transporter = 0;

		const std::vector<unit*>& reside_commoners = city.reside_commoners();
		for (std::vector<unit*>::const_iterator it2 = reside_commoners.begin(); it2 != reside_commoners.end(); ++ it2) {
			const unit& u = **it2;
			if (u.transport()) {
				const map_location& goto_loc = u.get_goto();
				VALIDATE(goto_loc.valid(), "calculate_commoner_gotos, reside transport's goto is invalid location!");

				// this fort maybe not my side(captured), redirect fort duration on field.
				std::map<map_location, int>::iterator find = forts.find(goto_loc);
				if (find != forts.end()) {
					find->second ++;
				}
				reside_transporter ++;
			}
		}
		const std::vector<unit*>& field_commoners = city.field_commoners();
		for (std::vector<unit*>::const_iterator it2 = field_commoners.begin(); it2 != field_commoners.end(); ++ it2) {
			const unit& u = **it2;
			if (u.transport()) {
				field_transporter ++;
			}
		}

		if (reside_transporter + field_transporter < 4) {
			can_generate_transport_cities.push_back(&city);
		}
	}

	//
	// generate transport before calcuate commoner's goto
	// it is necessary that form goto back-order.
	//
	static int max_fort_food = 100;
	for (std::map<map_location, int>::iterator it = forts.begin(); it != forts.end(); ++ it) {
		const unit& fort = *units_.find(it->first, false);
		int lack_food = max_fort_food - fort.food();
		int lack_transport = 4 - it->second;

		if (lack_food <= 0 || lack_transport <= 0) {
			continue;
		}
		while (!can_redirect_transport.empty() && lack_transport > 0) {
			unit& transport = *can_redirect_transport[0];
			can_redirect_transport.erase(can_redirect_transport.begin());

			transport.set_goto(fort.get_location());

			gotos.push_back(std::make_pair(&transport, -1));
			lack_transport --;
		}
		if (!lack_transport) {
			continue;
		}

		while (!can_generate_transport_cities.empty() && lack_transport > 0) {
			int index = random % can_generate_transport_cities.size();
			std::vector<artifical*>::iterator hit = can_generate_transport_cities.begin();
			std::advance(hit, index);
			artifical& city = **hit;
			can_generate_transport_cities.erase(hit);

			std::vector<unit*>& reside_commoners = city.reside_commoners();
			generate_commoner(city, unit_types.master_type(hero::number_transport), false);
			reside_commoners.back()->set_goto(fort.get_location());
			lack_transport --;
		}
	}

	// reside commoner
	for (std::vector<artifical*>::const_iterator it = holded_cities.begin(); it != holded_cities.end(); ++ it) {
		artifical* city = *it;

		int reside_trader, reside_transporter, field_trader, field_transporter;
		calculate_commoner_summary(*city, reside_trader, reside_transporter, field_trader, field_transporter);

		// recruit
		std::vector<unit*>& reside_commoners = city->reside_commoners();
		int ratio;

		if (city->police() >= game_config::min_tradable_police) {
			derived_random = random + city->master().number_ + city->hitpoints();

			// recruit probability: 1/10
			ratio = 10;
			int generate_ratio = ratio * (200 - city->generate_speed_) / 100;
			if (!generate_ratio) {
				generate_ratio = 1;
			}
			if (reside_trader + field_trader < city->max_commoners()) {
				if (!(random % generate_ratio)) {
					if (derived_random & 1) {
						generate_commoner(*city, unit_types.master_type(hero::number_businessman), false);
					} else {
						generate_commoner(*city, unit_types.master_type(hero::number_scholar), false);
					}
				}
			}
		}

		// trade/transfer
		const std::set<int>& roaded_cities = city->roaded_cities();
		std::vector<artifical*> candidate, transferable_cities;
		for (std::set<int>::const_iterator it2 = roaded_cities.begin(); it2 != roaded_cities.end(); ++ it2) {
			artifical* roaded = units_.city_from_cityno(*it2);
			if (roaded->side() == current_team.side() && roaded->police() >= game_config::min_tradable_police) {
				candidate.push_back(roaded);
				if (int(roaded->reside_commoners().size() + roaded->field_commoners().size()) < roaded->max_commoners()) {
					transferable_cities.push_back(roaded);
				}
			}
		}
		if (!reside_commoners.empty()) {
			int index = reside_commoners.size() - 1;
			bool transfered = false, traded = false;
			for (std::vector<unit*>::reverse_iterator it2 = reside_commoners.rbegin(); it2 != reside_commoners.rend(); ++ it2, index --) {
				unit& u = **it2;

				derived_random = random + u.hitpoints() + u.experience();
				// trade probability: 1/6
				ratio = 6;
				int trade_ratio = ratio * (200 - city->trade_speed_) / 100;
				if (!trade_ratio) {
					trade_ratio = 1;
				}

				if (u.transport()) {
					const map_location& goto_loc = u.get_goto();
					VALIDATE(goto_loc.valid(), "calculate_commoner_gotos, reside transport's goto is invalid location!");

					gotos.push_back(std::make_pair(city, index));

				} else if (city->police() < game_config::min_tradable_police) {
					// transfer probability: 1/3
					if (!transfered && !(random % 3) && !transferable_cities.empty()) {
						u.set_task(unit::TASK_TRANSFER);
						u.set_goto(transferable_cities[random % transferable_cities.size()]->get_location());
						gotos.push_back(std::make_pair(city, index));
						// avoid another commoner to transfer.
						transfered = true;
					} else {
						u.set_task(unit::TASK_NONE);
					}

				} else if (!traded && !(derived_random % trade_ratio) && !candidate.empty()) {
					u.set_task(unit::TASK_TRADE);
					u.set_goto(candidate[derived_random % candidate.size()]->get_location());
					gotos.push_back(std::make_pair(city, index));
					// avoid another commoner to trade.
					traded = true;
				} else {
					u.set_task(unit::TASK_NONE);
				}
			}
		}
	}

	for (std::vector<unit*>::iterator it = can_redirect_transport.begin(); it != can_redirect_transport.end(); ++ it) {
		unit& u = **it;
		if (u.block_turns() < 3) {
			u.set_block_turns(u.block_turns() + 1);
		} else {
			units_.erase(&u);
		}
	}
}

void play_controller::commoner_back(artifical* entered_city, artifical* belong_city, const map_location& src_loc)
{
	unit& back_commoner = *entered_city->reside_commoners().back();
	const map_location& entered_loc = entered_city->get_location();

	VALIDATE(entered_city != belong_city, "commoner_back, entered_city is same as belong_city!");

	std::vector<map_location> steps;
	const std::vector<map_location>& r = road(entered_city->cityno(), belong_city->cityno());
	std::vector<map_location>::const_iterator stand_it = std::find(r.begin(), r.end(), src_loc);
	bool direct_road = r.front() == entered_loc;
	if (direct_road) {
		std::advance(stand_it, 1);
		std::copy(r.begin(), stand_it, std::back_inserter(steps));
	} else {
		for (std::vector<map_location>::const_reverse_iterator it2 = r.rbegin(); it2 != r.rend(); ++ it2) {
			const map_location& l = *it2;
			steps.push_back(l);
			if (l == src_loc) break;
		}
	}
	unit_display::move_unit(steps, back_commoner, teams_);
	back_commoner.set_movement(0);

	VALIDATE(!units_.city_from_loc(src_loc), "commoner_back, cannot support one turn to out and home between city, check map!");

	back_commoner.set_from(entered_city->get_location());
	back_commoner.set_goto(belong_city->get_location());
	back_commoner.set_cityno(belong_city->cityno());

	units_.insert(new std::pair<map_location, unit*>(src_loc, &back_commoner));
	entered_city->commoner_go_out(back_commoner, false);
}

void play_controller::do_commoner(team& current_team)
{
	std::vector<std::pair<unit*, int> > ping, pang;
	std::vector<std::pair<unit*, int> >* curr = &ping, *background = &pang;
	map_location goto_loc;
	std::pair<unit*, int> param;

	recorder.add_do_commoner();

	calculate_commoner_gotos(current_team, *curr);

	int times = 0;
	while (++ times <= 2 && !curr->empty()) {
		for (std::vector<std::pair<unit*, int> >::iterator g = curr->begin(); g != curr->end(); ++g) {
			artifical* belong_city;
			const map_location src_loc = g->first->get_location();

			// desired move troop may move into one city. after it, g->first became to access denied.
			// so update goto before move_unit.
			if (g->second >= 0) {
				belong_city = unit_2_artifical(g->first);
				goto_loc = belong_city->reside_commoners()[g->second]->get_goto();
			} else {
				belong_city = units_.city_from_cityno(g->first->cityno());
				goto_loc = g->first->get_goto();
			}
			const map_location arrived_at = move_unit(false, current_team, *g, goto_loc, false);
			if (g->second < 0 && arrived_at == src_loc) {
				const unit& u = *units_.find(src_loc);
				if (u.movement_left()) {
					background->push_back(*g);
				}
			}
		}
		curr->clear();
		if (curr == &ping) {
			curr = &pang;
			background = &ping;
		} else {
			curr = &ping;
			background = &pang;
		}
	}
}

map_location play_controller::move_unit(bool troop, team& current_team, const std::pair<unit*, int>& pair, map_location to, bool dst_must_reachable)
{
	unit* unit_ptr = NULL;
	if (pair.second >= 0) {
		if (troop) {
			unit_ptr = unit_2_artifical(pair.first)->reside_troops()[pair.second];
		} else {
			unit_ptr = unit_2_artifical(pair.first)->reside_commoners()[pair.second];
		}
	} else {
		unit_ptr = pair.first;
	}

	map_location from = unit_ptr->get_location();
	map_location unit_location_ = from;

	if (from != to) {
		// it shouldn't use move_spectator of unit_ptr! when expediate, un will became invalid.
		move_unit_spectator move_spectator(units_);
		std::vector<map_location> steps;

		if (!unit_ptr->is_path_along_road()) {
			// check_before
			// do an A*-search

			const pathfind::shortest_path_calculator calc(*unit_ptr, current_team, units_, teams_, map_, false, true);
			int ignore_units = (!tent::tower_mode())? 1: 3;
			double stop_at = dst_must_reachable? 10000.0: calc.getUnitHoldValue() * ignore_units + 10000.0;

			//allowed teleports
			std::set<map_location> allowed_teleports;
			pathfind::plain_route route_ = a_star_search(from, to, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);

			if (route_.steps.empty()) {
				return map_location();
			}
			// modify! last location must no unit on it.
			while (!dst_must_reachable) {
				to = route_.steps.back();
				if (!units_.count(to)) {
					unit_map::const_iterator base = units_.find(to, false);
					if (!base.valid() || base->can_stand(*unit_ptr)) {
						break;
					}
				} else if (artifical* c = units_.city_from_loc(to)) {
					if (c->side() == unit_ptr->side()) { 
						// same side's city. 
						break;
					}
				}
				if (to == from) {
					// A(city)(city), A will enter it
					return map_location();
				}
				route_.steps.pop_back();
			}
			steps = route_.steps;
		} else {
			// cannot use a_star_search, when expeting, returned unit on loc of city is expeting troop! 
			// play_controll::road will mistake, avoid it!
			artifical* start_city;
			artifical* stop_city;
			bool forward = !unit_ptr->get_from().valid();
			if (forward) {
				start_city = units_.city_from_cityno(unit_ptr->cityno());
				stop_city = units_.city_from_loc(to);
			} else {
				start_city = units_.city_from_loc(unit_ptr->get_from());
				stop_city = units_.city_from_cityno(unit_ptr->cityno());
			}
			
			const std::vector<map_location>& r = road(start_city->cityno(), stop_city->cityno());
			std::vector<map_location>::const_iterator from_it = std::find(r.begin(), r.end(), from);
			int index = std::distance(r.begin(), from_it);
			int stop_at;
			bool direct_road = r.front() != to;
			if (direct_road) {
				// maybe expedite, +1
				stop_at = index + unit_ptr->movement_left() + 1;
				if (stop_at >= (int)r.size()) {
					stop_at = (int)r.size() - 1;
				}
				for ( ; index <= stop_at; index ++) {
					steps.push_back(r[index]);
				}
			} else {
				stop_at = 0;
				if (index > unit_ptr->movement_left() + 1) {
					stop_at = index - (unit_ptr->movement_left() + 1);
				}
				for ( ; index >= stop_at; index --) {
					steps.push_back(r[index]);
				}
			}
		}
		// do_execute
		move_spectator.set_unit(units_.find(from));
		
		bool gamestate_changed = false;

		if (steps.size() >= 2) {
			if (pair.second >= 0) {
				artifical* expediting_city = unit_2_artifical(pair.first);
				// std::vector<unit*>& reside_troops = expediting_city->reside_troops();
				gui_->place_expedite_city(*expediting_city);
				units_.set_expediting(expediting_city, troop, pair.second);
				::move_unit(
					/*move_unit_spectator* move_spectator*/ &move_spectator,
					/*std::vector<map_location> route*/ steps,
					/*replay* move_recorder*/ &recorder,
					/*undo_list* undo_stack*/ NULL,
					/*bool show_move*/ true,
					/*map_location *next_unit*/ NULL,
					/*bool continue_move*/ true, //@todo: 1.7 set to false after implemeting interrupt awareness
					/*bool should_clear_shroud*/ true,
					/*bool is_replay*/ false);
				if (!move_spectator.get_unit().valid() || move_spectator.get_unit()->get_location() != from) {
					// move_unit may be not move! reside troop ratain in city.
				} else {
					unit_ptr->remove_movement_ai();
				}

			} else {
				::move_unit(
					/*move_unit_spectator* move_spectator*/ &move_spectator,
					/*std::vector<map_location> route*/ steps,
					/*replay* move_recorder*/ &recorder,
					/*undo_list* undo_stack*/ NULL,
					/*bool show_move*/ true,
					/*map_location *next_unit*/ NULL,
					/*bool continue_move*/ true, //@todo: 1.7 set to false after implemeting interrupt awareness
					/*bool should_clear_shroud*/ true,
					/*bool is_replay*/ false);
			}
			// once unit moved one or more grid, unit_ptr may invalid. below code must not use un!
			if (!move_spectator.get_unit().valid()) {
				// mover is died!
				unit_location_ = map_location();
			} else if (move_spectator.get_ambusher().valid() || !move_spectator.get_seen_enemies().empty() || !move_spectator.get_seen_friends().empty() ) {
				unit_location_ = move_spectator.get_unit()->get_location();
				gamestate_changed = true;
			} else if (move_spectator.get_unit().valid()){
				unit_location_ = move_spectator.get_unit()->get_location();
				const unit& u = *move_spectator.get_unit();
				if (unit_location_ != from || (u.is_commoner() && !u.movement_left() && u.get_from().valid())) {
					gamestate_changed = true;
				}
			}
		}

		if (gamestate_changed) {
			unit_map::iterator un = units_.find(unit_location_);
			if (unit_location_ != from && un.valid() && un->movement_left() > 0) {
				un->remove_movement_ai();
			}
		}
		return unit_location_;
	} else {
		return from;
	}
}

void play_controller::rpg_update()
{
	std::string message;
	int incident = game_events::INCIDENT_APPOINT;
	if (rpg::stratum == hero_stratum_citizen) {
		bool control_more = false;
		if (rpg::h->meritorious_ >= 8000) {
			artifical* city = units_.city_from_cityno(rpg::h->city_);

			city->select_mayor(rpg::h);

			// all reside/field troop to human
			std::vector<unit*>& resides = city->reside_troops();
			for (std::vector<unit*>::iterator it = resides.begin(); it != resides.end(); ++ it) {
				unit& u = **it;
				u.set_human(true);
			}
			std::vector<unit*>& fields = city->field_troops();
			for (std::vector<unit*>::iterator it = fields.begin(); it != fields.end(); ++ it) {
				unit& u = **it;
				if (!u.human()) {
					u.set_human(true);
					u.set_goto(map_location());
				}
			}
		} else if (rpg::h->meritorious_ >= 5000 && rpg::humans.size() < 4) {
			control_more = true;
		} else if (rpg::h->meritorious_ >= 2500 && rpg::humans.size() < 3) {
			control_more = true;
		} else if (rpg::h->meritorious_ >= 1000 && rpg::humans.size() < 2) {
			control_more = true;
		}
		if (control_more) {
			std::vector<const unit*> units_ptr;
			team& current_team = (*resources::teams)[rpg::h->side_];
			std::vector<artifical*>& holden_cities = current_team.holden_cities();
			for (std::vector<artifical*>::iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
				artifical* city = *it;
				std::vector<unit*>& resides = city->reside_troops();
				for (std::vector<unit*>::iterator it = resides.begin(); it != resides.end(); ++ it) {
					unit& u = **it;
					if (!u.human()) {
						units_ptr.push_back(&u);
					}
				}
			
				std::vector<unit*>& fields = city->field_troops();
				for (std::vector<unit*>::iterator it = fields.begin(); it != fields.end(); ++ it) {
					unit& u = **it;
					if (!u.human()) {
						units_ptr.push_back(&u);
					}
				}
			}

			if (units_ptr.empty()) {
				return;
			}
			message = _("You can control more troop.");
			game_events::show_hero_message(&heros_[hero::number_civilian], NULL, message, incident);

			int res = -1;
			std::string name = "input";
			if (!replaying_) {
				gui2::tselect_unit dlg(*gui_, teams_, units_, heros_, units_ptr, gui2::tselect_unit::HUMAN);
				try {
					dlg.show(gui_->video());
				} catch(twml_exception& e) {
					e.show(*gui_);
				}

				res = dlg.troop_index();

				config cfg;
				cfg["value"] = res;
				recorder.user_input(name, cfg);
			} else {
				do_replay_handle(player_number_, name);
				const config *action = get_replay_source().get_next_action();
				if (!action || !*(action = &action->child(name))) {
					replay::process_error("[" + name + "] expected but none found\n");
				}
				res = action->get("value")->to_int(-1);
			}
			unit& u = *(const_cast<unit*>(units_ptr[res]));
			// end this turn
			u.set_movement(0);
			u.set_attacks(0);

			u.set_human(true);
			u.set_goto(map_location());
		}
	}
}

// return value:
//   true: executed ally operation. so next need do some post operation, for example 
bool play_controller::ally_all_ai(bool force)
{
	if (all_ai_allied_) {
		return false;
	}
	if (tent::mode != mode_tag::RPG || has_network_player()) {
		// in network state, don't ally all ai.
		return false;
	}

	int human_team_index = -1;
	int allied_teams = 0;

	std::vector<artifical*> cities;
	for (size_t side = 0; side != teams_.size(); side ++) {
		if (teams_[side].is_human()) {
			human_team_index = side;
		}
		std::vector<artifical*>& side_cities = teams_[side].holden_cities();
		for (std::vector<artifical*>::iterator itor = side_cities.begin(); itor != side_cities.end(); ++ itor) {
			artifical* city = *itor;
			cities.push_back(city);
		}
	}
	if (human_team_index < 0) {
		return false;
	}
	if (force || teams_[human_team_index].holden_cities().size() * 3 >= cities.size()) {
		// erase all strategy
		for (size_t i = 0; i < teams_.size(); i ++) {
			teams_[i].erase_strategies(false);
		}

		// all ai sides concluded treaty of alliance
		for (int from_side = 0; from_side != teams_.size(); from_side ++) {
			if (from_side == human_team_index || !teams_[from_side].holden_cities().size() || teams_[from_side].is_empty() || !teams_[human_team_index].is_enemy(from_side + 1)) {
				continue;
			}
			for (int to_side = 0; to_side != teams_.size(); to_side ++) {
				if (to_side == human_team_index || to_side == from_side || !teams_[to_side].holden_cities().size() || teams_[to_side].is_empty() || !teams_[human_team_index].is_enemy(to_side + 1)) {
					continue;
				}
				if (teams_[from_side].is_enemy(to_side + 1)) {
					teams_[from_side].set_ally(to_side + 1);
				}
			}
			allied_teams ++;
		}
		all_ai_allied_ = true;
		return allied_teams >= 2? true: false;
	}
	return false;
}

artifical* play_controller::decide_ai_capital() const
{
	int score, best_score = -1;
	ai_plan& plan = ai::manager::get_active_ai_plan_for_side(human_team_);
	plan.mrs_[0].calculate_mass(units_, teams_[human_team_]);
	std::vector<map_location> avoid;
	const int min_threaten = 5;
	for (std::vector<tmess_data>::const_iterator it = plan.mrs_[0].messes.begin(); it != plan.mrs_[0].messes.end(); ++ it) {
		const tmess_data& mess = *it;
		if (best_score == -1) {
			avoid.push_back(map_location(mess.cumulate_x / mess.selfs.size(), mess.cumulate_y / mess.selfs.size()));
			best_score = mess.selfs.size();
		} else if ((int)mess.selfs.size() > best_score) {
			if (best_score < min_threaten) {
				avoid.pop_back();
			}
			avoid.push_back(map_location(mess.cumulate_x / mess.selfs.size(), mess.cumulate_y / mess.selfs.size()));
			best_score = mess.selfs.size();
		} else if (mess.selfs.size() > min_threaten) {
			avoid.push_back(map_location(mess.cumulate_x / mess.selfs.size(), mess.cumulate_y / mess.selfs.size()));
		}
	}

	artifical* capital_of_ai = NULL;

	best_score = -1;
	std::vector<artifical*> ai_holded_cities;
	for (size_t side = 0; side != teams_.size(); side ++) {
		if (teams_[side].is_human()) {
			continue;
		}
		const std::vector<artifical*>& side_cities = teams_[side].holden_cities();
		for (std::vector<artifical*>::const_iterator it = side_cities.begin(); it != side_cities.end(); ++ it) {
			artifical& city = **it;
			ai_holded_cities.push_back(&city);
			score = city.office_heros();
			for (std::vector<map_location>::const_iterator it2 = avoid.begin(); it2 != avoid.end(); ++ it2) {
				int distance = distance_between(*it2, city.get_location());
				if (distance <= 5) {
					score = 0;
				} else if (distance <= 10) {
					score -= 15;
				} else if (distance <= 15) {
					score -= 10;
				} else if (distance <= 20) {
					score -= 5;
				}
			}
			if (score < 0) {
				score = 0;
			}
			if (score > best_score) {
				best_score = score;
				capital_of_ai = &city;
			}
		}
	}
	return capital_of_ai;
}

void play_controller::recommend(int random)
{
	std::vector<team>& teams = teams_;

	std::string message;
	artifical* selected_city = NULL;
	hero* selected_hero = NULL;

	std::vector<artifical*> cities, full_cities;
	for (size_t side = 0; side != teams.size(); side ++) {
		std::vector<artifical*>& side_cities = teams[side].holden_cities();
		for (std::vector<artifical*>::iterator itor = side_cities.begin(); itor != side_cities.end(); ++ itor) {
			artifical* city = *itor;
			full_cities.push_back(city);
			if (!city->wander_heros().size()) {
				continue;
			}
			cities.push_back(city);
		}
	}
	if (!cities.size()) {
		return;
	}
	// move max_move_wander_heros wander hero
	int hero_index = 0;
	if (tent::mode == mode_tag::RPG && full_cities.size() > 1) {
		artifical* from_city = cities[turn() % cities.size()];
		hero* leader = teams[from_city->side() - 1].leader();
		artifical* to_city = full_cities[random % full_cities.size()];
		
		while (to_city == from_city) {
			to_city = full_cities[random % full_cities.size()];
			random ++;
		}
		std::vector<hero*>& from_wander_heros = from_city->wander_heros();
		int max_move_wander_heros = 2;
		for (std::vector<hero*>::iterator itor = from_wander_heros.begin(); hero_index < max_move_wander_heros && itor != from_wander_heros.end();) {
			hero* h = *itor;
			if (from_city->side() == team::empty_side || h->loyalty(*leader) < game_config::move_loyalty_threshold) {
				to_city->wander_into(*h, teams[from_city->side() - 1].is_human()? false: true);
				itor = from_wander_heros.erase(itor);
				hero_index ++;
			} else {
				++ itor;
			}
		}
	}
	// recommand wander hero
	if (cities.size() == 1 && !cities[0]->wander_heros().size()) {
		return;
	}

	do {
		selected_city = cities[random % cities.size()];
		random ++;
	} while (!selected_city->wander_heros().size());

	std::vector<hero*>& wander_heros = selected_city->wander_heros();
	hero_index = random % wander_heros.size();
	std::vector<hero*>::iterator erase_it = wander_heros.begin();
	std::advance(erase_it, hero_index);

	selected_hero = wander_heros[hero_index];

	hero* leader = teams[selected_city->side() - 1].leader();
	if (tent::mode == mode_tag::RPG) {
		if (selected_city->side() == team::empty_side || selected_hero->loyalty(*leader) < game_config::wander_loyalty_threshold) {
			artifical* into_city = cities[random % cities.size()];
			into_city->wander_into(*selected_hero, teams[selected_city->side() - 1].is_human()? false: true);
			wander_heros.erase(erase_it);
			return;

		} 
	}
	if (runtime_groups::exist_member(*selected_hero, *leader)) {
		selected_hero->to_unstage(hero::UNSTAGE_GROUP);
		wander_heros.erase(erase_it);
		return;	
	}

	// update heros list in artifical
	selected_hero->status_ = hero_status_idle;
	selected_hero->side_ = selected_city->side() - 1;
	selected_city->fresh_heros().push_back(selected_hero);
	wander_heros.erase(erase_it);

	message = _("Let me join in. I will do my best to maintenance our honor.");

	map_location loc2(MAGIC_HERO, selected_hero->number_);
	game_events::fire("post_recommend", selected_city->get_location(), loc2);

	join_anim(selected_hero, selected_city, message);
}

bool play_controller::do_ally(bool alignment, int my_side, int to_ally_side, int emissary_number, int target_side, int strategy_index)
{
	if (!alignment) {
		// current not support discard alignment.
		return false;
	}

	hero& emissary = heros_[emissary_number];
	team& my_team = teams_[my_side - 1];
	team& to_ally_team = teams_[to_ally_side - 1];
	team& target_team = teams_[target_side - 1];
	
	std::vector<strategy>& strategies = my_team.strategies();
	strategy& s = strategies[strategy_index];
	artifical* target_city = units_.city_from_cityno(s.target_);

	// display dialog
	utils::string_map symbols;
	symbols["city"] = tintegrate::generate_format(target_city->name(), tintegrate::object_color);
	symbols["first"] = tintegrate::generate_format(my_team.name(), tintegrate::hero_color);
	symbols["second"] = tintegrate::generate_format(to_ally_team.name(), tintegrate::hero_color);
	
	if (s.type_ != strategy::DEFEND || !target_team.is_human() || !to_ally_team.is_enemy(target_side)) {
		// calculate to_ally_team aggreen with whether or not.
		std::vector<mr_data> mrs;
		units_.calculate_mrs_data(gamestate_, mrs, to_ally_side, false);
		if (!mrs[0].enemy_cities.empty() && mrs[0].enemy_cities[0]->side() == my_side) {
			// game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext("$first wants to ally with $second, but $second refuse.", symbols), game_events::INCIDENT_ALLY);
			return false;
		}
	}

	my_team.set_ally(to_ally_side);
	to_ally_team.set_ally(my_side);

	s.allies_.insert(to_ally_side);

	// emissary from fresh to finish.
	artifical* city = units_.city_from_cityno(emissary.city_);
	city->hero_go_out(emissary);
	city->finish_into(emissary, hero_status_backing);

	std::string message;
	if (s.type_ == strategy::AGGRESS) {
		message = vgettext("In order to aggress upon $city, $first and $second concluded treaty of alliance.", symbols);
	} else if (s.type_ == strategy::DEFEND) {
		message = vgettext("In order to defend $city, $first and $second concluded treaty of alliance.", symbols);
	} else {
		VALIDATE(false, "ally, unknown strategy type.");
	}
	if (my_team.is_human() || to_ally_team.is_human() || target_team.is_human()) {
		game_events::show_hero_message(&heros_[hero::number_scout], NULL, message, game_events::INCIDENT_ALLY);
	}
	return true;
}

void play_controller::construct_road()
{
	VALIDATE(!unit_map::scout_unit_, "play_controller::construct_road, detect unit_map::scout_unit_ isn't null.");
	const unit_type *ut = unit_types.find_scout();
	std::vector<const hero*> scout_heros;
	scout_heros.push_back(&heros_[hero::number_scout]);
	
	// select a valid cityno
	int cityno = -1;
	city_map& citys = units_.get_city_map();
	for (city_map::const_iterator i = citys.begin(); i != citys.end(); ++ i) {
		cityno = i->cityno();
		break;
	}
	if (cityno == -1) {
		throw game::game_error("You must define at least one city.");
	}
	// recruit
	type_heros_pair pair(ut, scout_heros);
	unit_map::scout_unit_ = new unit(units_, heros_, teams_, gamestate_, pair, cityno, false, false);

	std::multimap<int, int> roads_from_cfg;
	std::vector<std::string> vstr = utils::parenthetical_split(level_["roads"]);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::vector<std::string> vstr1 = utils::split(*it);
		if (vstr1.size() == 2) {
			int no1 = lexical_cast_default<int>(vstr1[0]);
			int no2 = lexical_cast_default<int>(vstr1[1]);
			VALIDATE(units_.city_from_cityno(no1), "play_controller::construct_road, invalid cityno " + vstr1[0]);
			VALIDATE(units_.city_from_cityno(no2), "play_controller::construct_road, invalid cityno " + vstr1[1]);
			VALIDATE(no1 != no2, "play_controller::construct_road, invalid road " + *it);
			roads_from_cfg.insert(std::make_pair(no1, no2));
		}
	}

	artifical* a;
	artifical* b;
	std::map<artifical*, std::set<int> > roaded_cities;
	std::map<artifical*, std::set<int> >::iterator find;
	size_t shortest_road = -1;
	std::pair<int, int> shortest_cities;
	for (std::multimap<int, int>::const_iterator it = roads_from_cfg.begin(); it != roads_from_cfg.end(); ++ it) {
		if (it->first < it->second) {
			a = units_.city_from_cityno(it->first);
			b = units_.city_from_cityno(it->second);
		} else {
			a = units_.city_from_cityno(it->second);
			b = units_.city_from_cityno(it->first);
		}
		find = roaded_cities.find(a);
		if (find == roaded_cities.end()) {
			std::set<int> s;
			s.insert(b->cityno());
			roaded_cities.insert(std::make_pair(a, s));
		} else {
			find->second.insert(b->cityno());
		}
		find = roaded_cities.find(b);
		if (find == roaded_cities.end()) {
			std::set<int> s;
			s.insert(a->cityno());
			roaded_cities.insert(std::make_pair(b, s));
		} else {
			find->second.insert(a->cityno());
		}

		const pathfind::emergency_path_calculator calc(*unit_map::scout_unit_, map_);
		double stop_at = 10000.0;
		//allowed teleports
		std::set<map_location> allowed_teleports;
		// NOTICE! although start and end are same, but rank filp, result maybe differenrt! so use cityno to make sure same.
		pathfind::plain_route route = a_star_search(a->get_location(), b->get_location(), stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
		if (!route.steps.empty()) {
			if (route.steps.size() < shortest_road) {
				shortest_road = route.steps.size();
				shortest_cities = std::make_pair(a->cityno(), b->cityno());
			}
			roads_.insert(std::make_pair(std::make_pair(a->cityno(), b->cityno()), route.steps));
		}
	}
	if (!roads_.empty()) {
		// avoid one trade can city I thoughtout city II.
		const unit_type_data::unit_type_map& types = unit_types.types();
		for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
			const unit_type& ut = it->second;
			if (hero::is_commoner(ut.master())) {
				if (ut.movement() + 6 > (int)shortest_road) {
					std::stringstream err;
					utils::string_map symbols;
					symbols["from"] = tintegrate::generate_format(units_.city_from_cityno(shortest_cities.first)->name(), "yellow");
					symbols["to"] = tintegrate::generate_format(units_.city_from_cityno(shortest_cities.second)->name(), "yellow");
					symbols["shortest"] = tintegrate::generate_format(ut.movement() + 6, "red");
					err << vgettext("wesnoth-lib", "Map error. Too short road from $from to $to, at least $shortest!", symbols);
					VALIDATE(false, err.str());
				}
			}
		}
	
		for (std::map<artifical*, std::set<int> >::const_iterator it = roaded_cities.begin(); it != roaded_cities.end(); ++ it) {
			artifical& city = *it->first;
			city.set_roaded_cities(it->second);
		}

		gui_->construct_road_locs(roads_);
	}
}

const std::vector<map_location>& play_controller::road(int a, int b) const
{
	VALIDATE(a != b, "road, must tow city when get road.");
	std::map<std::pair<int, int>, std::vector<map_location> >::const_iterator find;
	int new_a, new_b;

	if (a < b) {
		new_a = a;
		new_b = b;
	} else {
		new_a = b;
		new_b = a;
	}
	find = roads_.find(std::make_pair(new_a, new_b));
	if (find != roads_.end()) {
		return find->second;
	}
	return null_val::vector_location;
}

const std::vector<map_location>& play_controller::road(const unit& u) const
{
	const artifical* start_city;
	const artifical* stop_city;
	bool forward = !u.get_from().valid();
	if (forward) {
		start_city = units_.city_from_cityno(u.cityno());
		stop_city = units_.city_from_loc(u.get_goto());
	} else {
		start_city = units_.city_from_loc(u.get_from());
		stop_city = units_.city_from_cityno(u.cityno());
	}
	return road(start_city->cityno(), stop_city->cityno());
}

void play_controller::insert_rename(int number, const std::string& name)
{
	renames_.insert(std::make_pair(number, name));

	hero& h = heros_[number];
	h.set_name(name);
	unit* u = units_.find_unit(h);
	if (u && u->master().number_ == number) {
		u->regenerate_name();
	}
}

void play_controller::do_build(team& builder_team, unit* builder, const unit_type* ut, const map_location& art_loc, int cost)
{
	// below maybe use animation, use disable_commands to avoid re-enter.
	const events::command_disabler disable_commands;
	bool replay = cost >= 0;

	gui_->remove_temporary_unit();
	// 单位站的格子和建造格子肯定相邻,不必移动
	// 生成建筑
	if (cost < 0) {
		if (tent::mode == mode_tag::LAYOUT) {
			cost = 0;
		} else {
			int cost_exponent = builder_team.cost_exponent();
			cost = ut->cost() * cost_exponent / 100;
			if (tent::tower_mode() && builder_team.is_human()) {
				// increase wall's cost.
				cost *= game_config::tower_cost_ratio;
			}
		}
	}
	builder_team.spend_gold(cost);

	// find city that owner this economy grid.
	artifical* ownership_city = NULL;
	std::map<const map_location, int>::const_iterator it = unit_map::economy_areas_.find(art_loc);
	if (it != unit_map::economy_areas_.end()) {
		ownership_city = units_.city_from_cityno(it->second);
	} else if (!tent::tower_mode() && (ut == unit_types.find_keep() || ut == unit_types.find_wall())) {
		ownership_city = find_city_for_wall(units_, art_loc);
	}

	VALIDATE(ownership_city || builder, "play_controller::do_build, cannot find ownership, program error!");

	if (!ownership_city) {
		// build wall in tower mode
		ownership_city = units_.city_from_cityno(builder->cityno());
	}

	if (!replay) {
		map_location builder_loc;
		if (builder) {
			builder_loc = builder->get_location();
		}
		recorder.add_build(ut, ownership_city->get_location(), builder_loc, art_loc, cost);
		// 取消单位选中
		mouse_handler_.select_hex(map_location(), false);
	}

	// reconstruct artifical that has trait
	std::vector<const hero*> v;
	if (ut->master() != HEROS_INVALID_NUMBER) {
		v.push_back(&heros_[ut->master()]);
	} else {
		v.push_back(&ownership_city->master());
	}
	type_heros_pair pair(ut, v);
	artifical new_art(units_, heros_, teams_, gamestate_, pair, ownership_city->cityno(), true);
	if (!tent::tower_mode()) {
		new_art.set_state(ustate_tag::BUILDING, true);
		new_art.set_hitpoints(new_art.max_hitpoints() * game_config::start_percent_hp / 100);
	}

	units_.add(art_loc, &new_art);
	artifical* map_art = unit_2_artifical(&*units_.find(art_loc, !new_art.base()));
	unit_display::unit_build(*map_art);
}

void play_controller::click_access_list(void* cookie, int type)
{
	unit* u = reinterpret_cast<unit*>(cookie);
	hero* h = reinterpret_cast<hero*>(cookie);

	if (type == game_display::taccess_list::HERO) {
		mouse_handler_.set_hero_placing(h);
		
	} else if (!mouse_handler_.in_multistep_state()) {
		// sound::play_UI_sound("select-unit.wav");
		const map_location& loc = u->get_location();
		if (!u->is_city() || u->side() - 1 != gui_->playing_team()) {
			gui_->scroll_to_tile(loc, display::WARP);
			mouse_handler_.select_hex(map_location(), false);

			u->set_selecting();
			gui_->invalidate_unit();
			// now, selectedHex_ is invalid, hide context menu.
			gui_->goto_main_context_menu();			
		} else {
			mouse_handler_.select_hex(map_location(), false);

			gui2::tside_report dlg(*gui_, teams_, units_, heros_, u->side());
			dlg.show(gui_->video());
		}
	}
}