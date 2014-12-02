/* $Id: mouse_events.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "mouse_events.hpp"

#include "dialogs.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "play_controller.hpp"
#include "sound.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "rng.hpp"
#include "tod_manager.hpp"
#include "wml_separators.hpp"
#include "artifical.hpp"
#include "card.hpp"
#include "unit_display.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/unit_attack.hpp"
#include "gui/dialogs/hero_selection.hpp"
#include "gui/dialogs/troop_selection.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/tactic.hpp"
#include "gui/dialogs/formation_attack.hpp"
#include "gui/dialogs/interior.hpp"
#include "gui/dialogs/technology_tree.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/hero.hpp"
#include "gui/dialogs/build_ea.hpp"
#include "gui/widgets/window.hpp"
#include "formula_string_utils.hpp"
#include "game_preferences.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>


bool can_execute_command(const std::vector<team>& teams, const unit& u, bool browse)
{
	if (!browse && !events::commands_disabled) {
		return true;
	}
	return slot_cache::cache.size() < game_config::max_slots && teams[u.side() - 1].allow_intervene;
}

bool normal_can_execute_command(bool browse)
{
	return !browse && !events::commands_disabled;
}

namespace events{


mouse_handler::mouse_handler(play_controller& controller, game_display* gui, std::vector<team>& teams,
		unit_map& units, hero_map& heros, gamemap& map, tod_manager& tod_mng,
		game_state& gamestate, undo_list& undo_stack) :
	mouse_handler_base()
	, controller_(controller)
	, map_(map)
	, gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, tod_manager_(tod_mng)
	, gamestate_(gamestate)
	, undo_stack_(undo_stack)
	, previous_hex_()
	, previous_free_hex_()
	, selected_hex_()
	, next_unit_()
	, current_route_()
	, current_paths_()
	, enemy_paths_(false)
	, path_turns_(0)
	, side_num_(1)
	, undo_(false)
	, over_route_(false)
	, attackmove_(false)
	, reachmap_invalid_(false)
	, show_partial_move_(false)
	, building_bldg_(NULL)
	, playing_card_(NULL)
	, placing_hero_(NULL)
	, placing_unit_(NULL)
	, tasking_unit_(NULL)
{
	singleton_ = this;
}

mouse_handler::~mouse_handler()
{
	rand_rng::clear_new_seed_callback();
	singleton_ = NULL;
}

void mouse_handler::set_side(int side_number)
{
	side_num_ = side_number;
}

int mouse_handler::drag_threshold() const
{
	return 14;
}

bool mouse_handler::in_tip_state() const
{
	return placing_hero_ || placing_unit_ || tasking_unit_ || building_bldg_ || playing_card_;
}

bool mouse_handler::in_multistep_state() const
{
	return is_moving() || expediting_ || building_bldg_ || playing_card_ || selecting_;
}

void mouse_handler::mouse_motion(int x, int y, const bool browse, bool update)
{
	mouse_handler_base::mouse_motion(x, y, browse, update);
	
	if (attackmove_) {
		return;
	}
	
	// we ignore the position coming from event handler
	// because it's always a little obsolete and we don't need
	// to hightlight all the hexes where the mouse passed.
	// Also, sometimes it seems to have one *very* obsolete
	// and isolated mouse motion event when using drag&drop
	SDL_GetMouseState(&x, &y);  // <-- modify x and y

	if (mouse_handler_base::mouse_motion_default(x, y, update)) return;

	const map_location new_hex = gui().hex_clicked_on(x,y);

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#else
	if (!in_tip_state() && !units_.count(new_hex) && !units_.count(new_hex, false)) {
		gui_->hide_tip();
	}
#endif

	if (new_hex != last_hex_) {
		update = true;
		if (last_hex_.valid()) {
			// we store the previous hexes used to propose attack direction
			previous_hex_ = last_hex_;
			// the hex of the selected unit is also "free"
			if (last_hex_ == selected_hex_ || find_unit(last_hex_) == units_.end()) {
				previous_free_hex_ = last_hex_;
			}
		}
		last_hex_ = new_hex;
	}

	if (building_bldg_ || playing_card_ || selecting_ || placing_hero_ || placing_unit_ || tasking_unit_) {
		if (commands_disabled || !update) {
			return;
		}
	}

	if (reachmap_invalid_) update = true;

	if (update) {
		unit_map::iterator umapiter = units_.find(new_hex);
		bool over_is_selfcity = (umapiter.valid() && umapiter->can_reside() && (umapiter->side() == side_num_))? true: false;

		if (reachmap_invalid_) {
			reachmap_invalid_ = false;
			if (!current_paths_.destinations.empty() && !show_partial_move_) {
				unit_map::iterator u = find_unit(selected_hex_);
				if (selected_hex_.valid() && u != units_.end() ) {
					// reselect the unit without firing events (updates current_paths_)
					// select_hex(selected_hex_, true);
				}
				// we do never deselect here, mainly because of canceled attack-move
			}
		}

		// reset current_route_ and current_paths if not valid anymore
		// we do it before cursor selection, because it uses current_paths_
		if (new_hex.valid() == false) {
			current_route_.steps.clear();
			gui().set_route(NULL);
		}

		if (enemy_paths_) {
			enemy_paths_ = false;
			current_paths_ = pathfind::paths();
			gui().unhighlight_reach();
		} else if(over_route_) {
			over_route_ = false;
			current_route_.steps.clear();
			gui().set_route(NULL);
		}

		if (!in_tip_state()) {
			gui().highlight_hex(new_hex);
		}

		const unit_map::iterator selected_itor = find_unit(selected_hex_);
		unit* selected_unit = NULL;
		// maybe no unit on selected_hex_. in this case call *selected_itor will result in invalid.
		if (selected_itor.valid()) {
			selected_unit = &*selected_itor;
		}
		const unit_map::iterator mouseover_unit = find_unit(new_hex);

		// we search if there is an attack possibility and where
		map_location attack_from;
		// we search if there is an tactic possibility and where
		map_location tactic_from;
		// we search if there is an formation possibility and where
		map_location formation_from;
		// we search if there is an interior possibility and where
		map_location interior_from;
		// we search if there is an builder possibility and where
		map_location build_from;
		// we search if there is card playing to possibility and where
		map_location card_play_to;
		// we search if there is selecting to possibility and where
		map_location select_from;
		// we search if there is placable to possibility and where
		map_location placable_from;
		// we search if there is taskable to possibility and where
		map_location taskable_from;
		// we search if there is clear_formationed to possibility and where
		map_location clear_formationed_from;
		// we search if there is intervene_move to possibility and where
		map_location intervene_move_from;


		if (building_bldg_) {
			build_from = current_unit_build_from(new_hex);
			
			gui_->remove_temporary_unit();
			building_bldg_->set_location(last_hex_);
			if (building_bldg_->get_animation()) {
				building_bldg_->get_animation()->update_parameters(last_hex_, last_hex_.get_direction(building_bldg_->facing()));
			}
			gui_->place_temporary_unit(building_bldg_);
		} else if (playing_card_) {
			const card& c = *playing_card_;
			if (current_team().condition_card(c, new_hex)) {
				card_play_to = new_hex;
			}
		} else if (placing_unit_) {
			placable_from = current_unit_placable_from(new_hex);

		} else if (tasking_unit_) {
			taskable_from = current_unit_taskable_from(new_hex, unit::TASK_GUARD);

		} else {
			attack_from = current_unit_attacks_from(new_hex);
			tactic_from = current_unit_tactic_from(new_hex);
			formation_from = current_unit_formation_from(new_hex);
			interior_from = current_unit_interior_from(new_hex);
			select_from = current_unit_select_from(new_hex);
			clear_formationed_from = current_unit_clear_formationed_from(new_hex);
			intervene_move_from = current_unit_intervene_move_from(new_hex);
		}

		//see if we should show the normal cursor, the movement cursor, or
		//the attack cursor
		//If the cursor is on WAIT, we don't change it and let the setter
		//of this state end it
		if (cursor::get() != cursor::WAIT) {
			if (playing_card_) {
				if (card_play_to.valid()) {
					// cursor: playing!
					cursor::set(dragging_started_ ? cursor::PLACE_DRAG : cursor::PLACE);
				} else {
					// cursor: invalid
					cursor::set(dragging_started_ ? cursor::ILLEGAL_DRAG : cursor::ILLEGAL);
				}
			} else if (placing_unit_) {
				if (placable_from.valid()) {
					// cursor: move!
					cursor::set(dragging_started_ ? cursor::MOVE_DRAG : cursor::MOVE);
				} else {
					// cursor: invalid
					cursor::set(dragging_started_ ? cursor::ILLEGAL_DRAG : cursor::ILLEGAL);
				}
			} else if (tasking_unit_) {
				if (taskable_from.valid()) {
					// cursor: placable!
					cursor::set(dragging_started_ ? cursor::PLACE_DRAG : cursor::PLACE);
				} else {
					// cursor: invalid
					cursor::set(dragging_started_ ? cursor::ILLEGAL_DRAG : cursor::ILLEGAL);
				}
			} else if (selected_unit && selected_unit->side() == side_num_ && !selected_unit->incapacitated() && !browse) {
				move_unit_spectator& spectator = selected_unit->get_move_spectator();
				if (selecting_) {
					if (select_from.valid()) {
						cursor::set(cursor::NORMAL);
					} else {
						cursor::set(dragging_started_ ? cursor::ILLEGAL_DRAG : cursor::ILLEGAL);
					}
				} else if (attack_from.valid() && !gui_->formation_indicator().second.valid()) {
					// cursor: attack!
					cursor::set(dragging_started_ ? cursor::ATTACK_DRAG : cursor::ATTACK);
				} else if (tactic_from.valid() || clear_formationed_from.valid() || intervene_move_from.valid()) {
					// cursor: tactic!
					cursor::set(dragging_started_ ? cursor::TACTIC_DRAG : cursor::TACTIC);
				} else if (formation_from.valid()) {
					// cursor: formation!
					cursor::set(dragging_started_ ? cursor::TACTIC_DRAG : cursor::TACTIC);
				} else if (interior_from.valid()) {
					// cursor: interior!
					cursor::set(dragging_started_ ? cursor::INTERIOR_DRAG : cursor::INTERIOR);
				} else if (build_from.valid()) {
					// cursor: build!
					cursor::set(dragging_started_ ? cursor::BUILD_DRAG : cursor::BUILD);
				} else if (!building_bldg_ && !playing_card_ && !selected_unit->is_artifical() && (mouseover_unit == units_.end() || mouseover_unit->can_stand(*selected_unit) || over_is_selfcity) && current_paths_.destinations.contains(new_hex)) {
					// cursor: footprint
					if (over_is_selfcity) {
						cursor::set(dragging_started_ ? cursor::ENTER_DRAG : cursor::ENTER);
					} else {
						cursor::set(dragging_started_ ? cursor::MOVE_DRAG : cursor::MOVE);
					}
				} else {
					// selecte unit can't attack or move there
					cursor::set(cursor::NORMAL);
				}
			} else if (gui_->set_ea_build_indicator(new_hex, false)) {
				cursor::set(cursor::BUILD);

			} else {
				// no selected unit or we can't move it
				cursor::set(cursor::NORMAL);
			}
		}

		// the destination is the pointed hex or the adjacent hex
		// used to attack it
		map_location dest;
		unit_map::const_iterator dest_un;
		if (attack_from.valid()) {
			dest = attack_from;
			dest_un = find_unit(dest);
		} else if (build_from.valid()) {
			dest = build_from;
			dest_un = find_unit(dest);
		} else {
			dest = new_hex;
			dest_un = mouseover_unit;
		}

		if (dest == selected_hex_ || (selected_unit && dest_un.valid() && !dest_un->can_stand(*selected_unit) && !over_is_selfcity)) {
			// 鼠标落回选中格子 || 落在的格子是个站了单位的格子(除去城市)
			// ===>擦除A--B路线
			current_route_.steps.clear();
			gui().set_route(NULL);
		} else if (!commands_disabled && map_.on_board(selected_hex_) && map_.on_board(new_hex)) {
			if (selected_unit && !selected_unit->incapacitated() && (!tent::tower_mode() || !teams_[selected_unit->side() - 1].is_human())) {
				// the movement_reset is active only if it's not the unit's turn
				unit_movement_resetter move_reset(*selected_unit, selected_unit->side() != side_num_);
				current_route_ = get_route(&*selected_itor, dest, viewing_team());
				if (!browse) {
					gui().set_route(&current_route_);
				}
			}
		}

		unit_map::iterator un = mouseover_unit;

		if (un != units_.end() && !gui().fogged(un->get_location())) {
			if (!un->is_commoner() && un->side() != side_num_) {
				if (!over_is_selfcity && current_paths_.destinations.empty()) {
					//unit under cursor is not on our team, highlight reach
					unit_movement_resetter move_reset(*un);

					bool teleport = un->get_ability_bool("teleport");
					current_paths_ = pathfind::paths(map_,units_,new_hex,teams_,
										false,teleport,viewing_team(),path_turns_);
					gui().highlight_reach(current_paths_);
					enemy_paths_ = true;
				}
			} else {
				//unit is on our team, show path if the unit has one
				const map_location go_to = un->get_goto();
				if(map_.on_board(go_to)) {
					pathfind::marked_route route = get_route(&*un, go_to, current_team());
					gui().set_route(&route);
				}
				over_route_ = true;
			}
		}
	}
}

unit_map::iterator mouse_handler::selected_unit()
{
	unit_map::iterator res = find_unit(selected_hex_);
	if(res != units_.end()) {
		return res;
	} else {
		return find_unit(last_hex_);
	}
}

unit_map::iterator mouse_handler::find_unit(const map_location& hex)
{
	unit_map::iterator it = find_visible_unit(hex, viewing_team());
	if (it.valid())
		return it;
	else
		return resources::units->end();
}

unit_map::const_iterator mouse_handler::find_unit(const map_location& hex) const
{
	return find_visible_unit(hex, viewing_team());
}

map_location mouse_handler::current_unit_attacks_from(const map_location& loc)
{
	const std::set<map_location>& attack_indicator_dst = gui_->attack_indicator();
	const std::set<map_location>& alternatable_indicator = gui_->alternatable_indicator();
	
	if (attack_indicator_dst.find(loc) != attack_indicator_dst.end() && alternatable_indicator.find(loc) == alternatable_indicator.end()) {
		return selected_hex_;
	} else {
		return map_location();
	}
}

map_location mouse_handler::current_unit_tactic_from(const map_location& loc)
{
	const std::pair<map_location, int>& indicator = gui_->tactic_indicator();
	if (indicator.second >= 0 && indicator.first.valid() && indicator.first == loc) {
		const unit& tactician = *units_.find(indicator.first);
		if (tactician.master().tactic_ != HEROS_NO_TACTIC) {
			return selected_hex_;
		}
		if (tactician.second().valid() && tactician.second().tactic_ != HEROS_NO_TACTIC) {
			return selected_hex_;
		}
		if (tactician.third().valid() && tactician.third().tactic_ != HEROS_NO_TACTIC) {
			return selected_hex_;
		}
	}
	return map_location();
}

map_location mouse_handler::current_unit_clear_formationed_from(const map_location& loc)
{
	const std::pair<map_location, int>& indicator = gui_->clear_formationed_indicator();
	if (indicator.second > 0 && indicator.first.valid() && indicator.first == loc) {
		return selected_hex_;
	}
	return map_location();
}

map_location mouse_handler::current_unit_intervene_move_from(const map_location& loc)
{
	const std::pair<map_location, int>& indicator = gui_->intervene_move_indicator();
	if (indicator.second >= 0 && indicator.first.valid() && indicator.first == loc) {
		return selected_hex_;
	}
	return map_location();
}

map_location mouse_handler::current_unit_formation_from(const map_location& loc)
{
	const std::pair<map_location, map_location>& formation_indicator = gui_->formation_indicator();
	if (formation_indicator.second.valid() && formation_indicator.second == loc && formation_indicator.first == selected_hex_) {
		return selected_hex_;
	}
	return map_location();
}

map_location mouse_handler::current_unit_interior_from(const map_location& loc)
{
	const map_location& interior_indicator = gui_->interior_indicator();
	if (!expediting_ && interior_indicator.valid() && interior_indicator == loc) {
		const unit& interior = *units_.find(interior_indicator);
		if (interior.is_city()) {
			return selected_hex_;
		}
	}
	return map_location();
}

map_location mouse_handler::current_unit_alternate_from(const map_location& loc)
{
	const std::set<map_location>& alternatable_indicator = gui_->alternatable_indicator();
	if (alternatable_indicator.find(loc) != alternatable_indicator.end()) {
		return selected_hex_;
	}
	return map_location();
}

map_location mouse_handler::current_unit_select_from(const map_location& loc)
{
	const std::set<map_location>& selectable_indicator = gui_->selectable_indicator();
	if (selectable_indicator.find(loc) != selectable_indicator.end()) {
		return slot_cache::selected_u? slot_cache::selected_u->get_location(): selected_hex_;
	}
	return map_location();
}

map_location mouse_handler::current_unit_build_from(const map_location& loc)
{
	std::set<map_location>& build_indicator_dst = gui_->build_indicator();
	if (std::find(build_indicator_dst.begin(), build_indicator_dst.end(), loc) != build_indicator_dst.end()) {
		return selected_hex_;
	} else {
		return map_location();
	}
}

const map_location& mouse_handler::current_unit_placable_from(const map_location& loc) const
{
	const std::set<map_location>& placable_indicator = gui_->placable_indicator();
	std::set<map_location>::const_iterator find = std::find(placable_indicator.begin(), placable_indicator.end(), loc);
	if (find != placable_indicator.end()) {
		return *find;
	} else {
		return map_location::null_location;
	}
}

const map_location& mouse_handler::current_unit_joinable_from(const map_location& loc) const
{
	const std::set<map_location>& joinable_indicator = gui_->joinable_indicator();
	std::set<map_location>::const_iterator find = std::find(joinable_indicator.begin(), joinable_indicator.end(), loc);
	if (find != joinable_indicator.end()) {
		return *find;
	} else {
		return map_location::null_location;
	}
}

map_location mouse_handler::current_unit_taskable_from(const map_location& loc, int task) const
{
	if (!tasking_unit_) {
		return map_location::null_location;
	}

	if (task == unit::TASK_GUARD) {
		const unit* u = units_.find_unit(loc);
		if (u && u->side() != side_num_) {
			return map_location::null_location;
		}
		int cost = tasking_unit_->movement_cost(map_[loc]);
		return cost != unit_movement_type::UNREACHABLE? loc: map_location::null_location;
	}
	return map_location::null_location;
}

pathfind::marked_route mouse_handler::get_route(unit* un, map_location go_to, team &team)
{
	// The pathfinder will check unit visibility (fogged/stealthy).
	std::set<map_location> allowed_teleports = pathfind::get_teleport_locations(*un, viewing_team());

	pathfind::plain_route route;

	double stop_at = 10000.0;
	if (!un->is_path_along_road()) {
		const pathfind::shortest_path_calculator calc(*un, team, units_, teams_, map_);
		// standard shortest path
		if (un->provoked_turns()) {
			stop_at += 2 * calc.getUnitHoldValue();
		} else if (un->transport()) {
			// target maybe captured by enemy. still let dipslay route.
			stop_at += calc.getUnitHoldValue();
		}
		route = pathfind::a_star_search(un->get_location(), go_to, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
	} else if (!expediting_) {
		// when expeting, returned unit on loc of city is expeting troop! play_controll::road will mistake, avoid it!
		const pathfind::commoner_path_calculator calc(*un, team, units_, teams_, map_, resources::controller->road(*un));
		// standard shortest path
		// for unit that path along road(commoner etc), there is one road only. road maybe standard some enemy unit.
		stop_at += calc.getNoPathValue();
		route = pathfind::a_star_search(un->get_location(), go_to, stop_at, &calc, map_.w(), map_.h(), &allowed_teleports);
	}

	std::vector<map_location> waypoints;
	return mark_route(route, waypoints);
}

void mouse_handler::mouse_press(const SDL_MouseButtonEvent& event, const bool browse)
{
	mouse_handler_base::mouse_press(event, browse);
}

bool mouse_handler::right_click(int x, int y, const bool browse)
{
	// undo_ = false;
	if (mouse_handler_base::right_click(x, y, browse)) return false;

	do_right_click(browse);
	return false;
}

void mouse_handler::do_right_click(const bool browse)
{
	bool is_replaying = resources::controller->is_replaying();

	if (!is_replaying && commands_disabled && !slot_cache::selected_u) {
		// when move result to commands_disable not-zero, moving_ is true.
		return;
	}

	gui_->unhighlight_disctrict();

	if (building_bldg_) {
		gui_->remove_temporary_unit();
		exit_building(false);
/*
		const theme::menu* current_context_menu = gui_->get_theme().context_menu("");
		if (current_context_menu && current_context_menu->parent()) {
			gui_->get_theme().set_current_context_menu(const_cast<theme::menu*>(current_context_menu->parent()));
			resources::controller->show_context_menu(NULL, *gui_);
			return;
		}
*/		
		// redisplay context menu
		show_menu_ = true;
		// player cancel build, regenerate attack indicator.
		gui_->set_attack_indicator(&*units_.find(selected_hex_));
		gui_->clear_build_indicator();
		// recorder.add_build_cancel();

		return;
	}
	if (selecting_) {
		exit_selecting(false);
	}
	if (playing_card_ || placing_hero_ || placing_unit_ || tasking_unit_) {
		exit_placing_unit(false);
	}
	// 后面须加条件last_hex_ != selected_hex_。在出征时按下要能弹出武将菜单
	if (expediting_) {
		// artifical* city = unit_2_artifical(&units_.find(selected_hex_)->second);
		// 即使此次是建造过程，由于建造会确保建造者站的格子不会是城市，还是满足条件(last_hex_ != selected_hex_)
		// 当前正处于出征状态，鼠标左键落在不是出征单位上，认为时取消出征

		// 1.移除临时单位
		gui().remove_expedite_city();
		// 2.告知uint_map层,可以正常访问出征城郡所在格子
		units_.set_expediting();
		// 3.置标志
		expediting_ = false;

		// enable undo/endturn buttons
		gui_->enable_menu("play_card", true);
		if (!undo_stack_.empty()) {
			gui_->enable_menu("undo", true);
		}
		gui_->enable_menu("endturn", true);
	}
	if (units_.count(selected_hex_)) {
		gui_->invalidate_unit();
	}

	select_hex(map_location(), browse);
	
	// press right key, goto main context menu.
	gui_->goto_main_context_menu();
	
	return;
}

void mouse_handler::begin_moving(const map_location& src, const map_location& dst)
{
	VALIDATE(!moving_, "program error, begin_moving, reenter moving");

	gui_->enable_menu("play_card", false);
	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
	gui_->clear_attack_indicator();
	gui_->clear_build_indicator();
	gui_->set_moving_src_loc(src);
	gui_->set_moving_dst_loc(dst);
	moving_ = true;
}

void mouse_handler::end_moving()
{
	if (moving_) {
		gui_->enable_menu("play_card", true);
		if (!undo_stack_.empty()) {
			gui_->enable_menu("undo", true);
		}
		gui_->enable_menu("endturn", true);
		gui_->clear_moving_src_loc();
		gui_->clear_moving_dst_loc();
		moving_ = false;
		gui_->redraw_minimap();
		gui_->draw();
	}
}

// 1. if not enter city and unmovable, disable access portrait.
// 2. if enter city, call end_moving and hide context menu.
// 3. if not enter city, show context menu. set ambusher if ambushed.
// @mover: mover is valid only stop_on_city is false! when stop_on_city is true, don't use mover!
bool mouse_handler::post_move_unit(unit& mover, const map_location& stop_loc)
{
	bool stop_on_city = units_.city_from_loc(stop_loc)? true: false;
	if (!stop_on_city) {
		if (tent::turn_based) {
			gui_->resort_access_troops(mover);
		}

		if (undo_stack_.empty() || mover.get_move_spectator().get_ambusher().valid()) {
			// ambusher when move!
			clear_undo_stack();
		}

		// if has attackable, display indicate automatic. else cancel.
		select_hex(stop_loc, false);
		gui_->set_attack_indicator(&mover, !current_can_action(mover));
		if (!gui_->attack_indicator().empty()) {
			// 2.show context menu
			show_menu_ = true;
		} else {
			select_hex(map_location(), false);
		}
	}

	return stop_on_city;
}

bool mouse_handler::left_click(int x, int y, const bool browse)
{
	bool is_replaying = controller_.is_replaying();
	int mode = tent::mode;
	undo_ = false;
	
	if (gui_->unit_image_location_on(x, y)) {
		do_right_click(browse);
		return false;
	}
	if (mouse_handler_base::left_click(x, y, browse)) {
		return false;
	}

	gui_->unhighlight_disctrict();

	// bool check_shroud = current_team().auto_shroud_updates();

	// we use the last registered highlighted hex
	// since it's what update our global state
	map_location hex = last_hex_;

	if (building_bldg_) {
		const map_location& build_from = current_unit_build_from(hex);

		if (browse || commands_disabled) {
			return false;
		}
		//see if we're trying to do a build or move-and-build
		if (build_from.valid()) {
			unit& builder = *units_.find(build_from);
			const unit_type* ut = unit_types.find(building_bldg_->type_id());
			controller_.do_build(current_team(), &builder, ut, last_hex_);

			// temporary artifical is no use, delete it.
			exit_building(true);

			// next build, don't trigger sitdowm.
			gui_->clear_build_indicator();
			
			game_events::fire("post_build", last_hex_, build_from);

			gui_->goto_main_context_menu();

		} else {
			do_right_click(browse);
			// show_menu_ = true;
		}
		return false;
	}
	if (playing_card_) {
		team& curr_team = current_team();
		const card& c = *playing_card_;
		if (curr_team.condition_card(c, hex)) {
			if (!c.bomb()) {
				std::vector<std::pair<int, unit*> > maps;
				std::vector<std::pair<int, unit*> > pairs;
				std::string disable_str;

				curr_team.card_touched(c, hex, pairs, disable_str);
				if (pairs.empty()) {
					do_right_click(browse);
					return false;
				}

				if (!c.multitudinous() && (pairs.size() > 1 || !disable_str.empty())) {
					std::set<size_t> checked_pairs;
					int retval;
					if (c.target_hero()) {
						// display hero selection dialog
						gui2::thero_selection dlg(&teams_, &units_, heros_, pairs, side_num_, disable_str);
						try {
							dlg.show(gui_->video());
						} catch(twml_exception& e) {
							e.show(*gui_);
							do_right_click(browse);
							return false;
						}
						checked_pairs = dlg.checked_pairs();
						retval = dlg.get_retval();
					} else {
						gui2::ttroop_selection dlg(&teams_, &units_, heros_, pairs, side_num_, disable_str);
						try {
							dlg.show(gui_->video());
						} catch(twml_exception& e) {
							e.show(*gui_);
							do_right_click(browse);
							return false;
						}
						checked_pairs = dlg.checked_pairs();
						retval = dlg.get_retval();
					}
					if (retval != gui2::twindow::OK || checked_pairs.empty()) {
						do_right_click(browse);
						return false;
					}
					// gui_->draw();
					for (std::set<size_t>::const_iterator itor = checked_pairs.begin(); itor != checked_pairs.end(); ++ itor) {
						maps.push_back(pairs[*itor]);
					}
				} else {
					maps = pairs;
				}

				{
					// consume_card maybe use animation, use disable_commands avoid reenter.
					const events::command_disabler disable_commands;
					curr_team.consume_card(c, hex, maps, card_index_, true);
				}
			
				exit_placing_unit(true);
				refresh_card_button(curr_team, *gui_);

				gui_->goto_main_context_menu();
			} else {
				exit_placing_unit(true);

				unit& mover = *units_.find(hex);
				set_unit_placing(mover);
			}
		} else {
			do_right_click(browse);
		}
		return false;
	}

	unit* selected_u = get_visible_unit(selected_hex_, viewing_team());

	if (selecting_) {
		const map_location selecte_from = current_unit_select_from(hex);
		if (selecte_from.valid()) {
			cast_tactic_special(slot_cache::selected_u? *slot_cache::selected_u: *selected_u, units_.find_unit(hex), browse);
			exit_selecting(true);
		} else {
			do_right_click(browse);
		}
		return false;
	}

	// placable_from/joinable_from will erase in clear(), before used.
	const map_location placable_from = current_unit_placable_from(hex);
	const map_location joinable_from = current_unit_joinable_from(hex);
	const map_location taskable_from = current_unit_taskable_from(hex, unit::TASK_GUARD);

	if (placing_hero_) {
		if (placable_from.valid()) {
			place_at(placable_from, *placing_hero_);

		} else if (joinable_from.valid()) {
			join_in(joinable_from, *placing_hero_);

		} else {
			do_right_click(browse);
		}
		return false;

	} else if (placing_unit_) {
		if (placable_from.valid()) {
			direct_move_unit(*placing_unit_, placing_unit_->get_location(), placable_from);

		} else {
			do_right_click(browse);
		}

		return false;
	} else if (tasking_unit_) {
		if (taskable_from.valid()) {
			do_task(*tasking_unit_, taskable_from);

		} else {
			do_right_click(browse);
		}

		return false;
	}

	bool check_shroud = current_team().auto_shroud_updates();

	unit_map::iterator clicked_u = find_unit(hex);
	bool cancel_recall = false;

	if (expediting_ && (selected_hex_ != hex)) {
		if (clicked_u.valid() && !clicked_u->can_stand(*selected_u) && ((clicked_u->side() != selected_u->side()) || !unit_is_city(&*clicked_u) || (clicked_u->get_location() == expediting_city_->get_location()))) {
			cancel_recall = true;
		} else if (selected_u && selected_u->movement_cost(map_[hex]) == unit_movement_type::UNREACHABLE) {
			// during expediting moving left click, selected_u maybe is null.
			cancel_recall = true;
		} else if (current_team().shrouded(hex)) {
			cancel_recall = true;
		}

		// clicked_u的值(hex和selected_hex_不是同一坐标是属同一城市时)
		// 1. first: 指向城市中心坐标
		// 2. second: 城市

		if (cancel_recall) {
			// 当前正处于出征状态，鼠标左键落在非出征格子，并且是非已方城市单位上，认为时取消出征
			// 1.移除临时单位
			gui().remove_expedite_city();
			// 2.告知uint_map层,可以正常访问出征城郡所在格子
			units_.set_expediting();
			// 3.置标志
			expediting_ = false;

			select_hex(map_location(), browse);

			gui_->enable_menu("play_card", true);
			// enable undo/endturn buttons
			if (!undo_stack_.empty()) {
				gui_->enable_menu("undo", true);
			}
			gui_->enable_menu("endturn", true);
			return false;
		}
	}

	// if the unit is selected and then itself clicked on,
	// any goto command is cancelled
	if (selected_u && !browse && selected_hex_ == hex && !selected_u->provoked_turns() && selected_u->side() == side_num_) {
		if (teams_[selected_u->side() - 1].double_reset_goto) {
			selected_u->set_goto(map_location());
		}
	}

	const map_location src = selected_hex_;
	pathfind::paths orig_paths = current_paths_;
	map_location attack_from = current_unit_attacks_from(hex);
	const map_location tactic_from = current_unit_tactic_from(hex);
	const map_location formation_from = current_unit_formation_from(hex);
	const map_location interior_from = current_unit_interior_from(hex);
	bool build_from = !selected_u && gui_->set_ea_build_indicator(hex, false);
	const map_location alternate_from = current_unit_alternate_from(hex);
	const map_location clear_formationed_from = current_unit_clear_formationed_from(hex);
	const map_location intervene_move_from = current_unit_intervene_move_from(hex);

	artifical* cobj = units_.city_from_loc(hex);
	bool clicked_selfcity = (cobj && (cobj->side() == side_num_))? true: false;

	if (alternate_from.valid()) {
		if (!preferences::default_move()) {
			std::vector<std::string> items;
			std::stringstream strstr;
			strstr.str("");
			strstr << IMAGE_PREFIX << "misc/large-attack.png~SCALE(64, 64)" << COLUMN_SEPARATOR;
			strstr << dgettext("wesnoth-lib", "Attack wall");
			items.push_back(strstr.str());

			strstr.str("");
			strstr << IMAGE_PREFIX << "misc/large-move.png~SCALE(64, 64)" << COLUMN_SEPARATOR;
			strstr << dgettext("wesnoth-lib", "Move to wall");
			items.push_back(strstr.str());

			gui2::tcombo_box dlg(items, 0);
			dlg.show(gui_->video());
			if (dlg.selected_index() == 0) {
				attack_from = alternate_from;
			}
		}
	}

	// see if we're trying to do a attack
	// if (!browse && !commands_disabled && attack_from.valid() && !gui_->formation_indicator().second.valid()) {
	if (!browse && !commands_disabled && attack_from.valid()) {
		if (attack_enemy(*selected_u, *clicked_u, hex)) {
			return false;
		}
	} else if (!browse && !commands_disabled && formation_from.valid()) {
		if (do_formation_attack(*selected_u)) {
			return false;
		}
	} else if (tactic_from.valid() && can_execute_command(teams_, *selected_u, browse)) {
		if (cast_tactic(*selected_u, hex, browse)) {
			return false;
		}
	} else if (clear_formationed_from.valid() && can_execute_command(teams_, *selected_u, browse)) {
		clear_formationed(*selected_u, browse);

	} else if (intervene_move_from.valid() && can_execute_command(teams_, *selected_u, browse)) {
		intervene_move(*selected_u, hex, browse);

	} else if ((is_replaying || !commands_disabled) && interior_from.valid()) {
		if (interior(browse || selected_u->side() != side_num_ || rpg::stratum != hero_stratum_leader, *selected_u)) {
			return false;
		}
	} else if (!browse && !commands_disabled && build_from) {
		build_ea(hex);

	} else if (!commands_disabled && !browse && selected_hex_.valid() && selected_hex_ != hex &&
		selected_u && current_can_action(*selected_u) && (clicked_u == units_.end() || clicked_u->base() || clicked_selfcity) &&
		!current_team().shrouded(hex) && !current_route_.steps.empty() && current_route_.steps.front() == selected_hex_) {
		
		const pathfind::paths& path = pathfind::paths(map_, units_,
			selected_hex_, teams_, false, false, teams_[gui_->viewing_team()]);
		if (!path.destinations.contains(hex)) {
			utils::string_map symbols;
			std::string message;
			symbols["troop"] = selected_u->name();
			if (clicked_selfcity) {
				symbols["city"] = cobj->name();
				message = vgettext("Do you really command $troop to enter $city?", symbols);
			} else if(map_.is_village(hex)) {
				message = vgettext("Do you really command $troop to move to village?", symbols);
			} else {
				std::stringstream str;
				str << "(" << hex.x + 1 << ", " << hex.y + 1 << ")";
				symbols["loc"] = str.str();
				message = vgettext("Do you really command $troop to move to $loc?", symbols);
			}
			const int res = gui2::show_message(gui_->video(), "", message, gui2::tmessage::yes_no_buttons);
			if (res == gui2::twindow::CANCEL) {
				return false;
			}
		}
		gui().unhighlight_reach();
		const move_unit_spectator& spectator = selected_u->get_move_spectator();
		map_location stop_at_loc = move_unit_along_current_route(current_route_.steps, check_shroud, true);
		if (expediting_ && !stop_at_loc.valid()) {
			do_right_click(browse);
			return false;
		} else if (stop_at_loc.valid()) {
			bool stop_on_city = post_move_unit(*selected_u, stop_at_loc);
			if (stop_on_city || stop_at_loc == hex || spectator.get_ambusher().valid() || !spectator.get_seen_enemies().empty() || !spectator.get_seen_friends().empty()) {
				return false;
			}
		}
		// set goto
		selected_u->set_goto(hex);

		// goto will end move automatically
		select_hex(map_location(), false);
		gui_->goto_main_context_menu();
		show_menu_ = false;

	} else {
		if (units_.count(selected_hex_)) {
			// last select a non-empty hex. cancel context menu
			// gui_->goto_main_context_menu();
			gui_->invalidate_unit();
		}

		// we select a (maybe empty) hex
		// we block selection during attack+move (because motion is blocked)
		select_hex(hex, browse);

		// gui_->set_ea_build_indicator(hex);

		// if this select a non-empty hex, display context menu
		selected_u = get_visible_unit(selected_hex_, viewing_team());
		if (selected_u && (is_replaying || can_execute_command(teams_, *selected_u, browse)) && !expediting_ && !controller_.is_linger_mode()) {
			bool show_indicator = true;
			bool browse_indicator = false;
			if (is_replaying || !teams_[selected_u->side() - 1].allow_intervene) {
				bool can_action = current_can_action(*selected_u);
				if (!can_action && tent::tower_mode() && selected_u->side() == side_num_ && selected_u->human()) {
					can_action = true;
				}
				if (can_action || selected_u->is_artifical()) {
					browse_indicator = browse || commands_disabled;
				} else {
					show_indicator = false;
				}
			} else {
				slot_cache::selected_u = selected_u;
			}
			if (show_indicator) {
				gui_->set_attack_indicator(selected_u, browse_indicator);
			}
		}

		gui_->goto_main_context_menu();
	}
	return false;
}

bool protect_slot_cache_selected_u = false;
class tprotect_slot_cache
{
public:
	tprotect_slot_cache()
	{
		protect_slot_cache_selected_u = true;
	}

	~tprotect_slot_cache()
	{
		protect_slot_cache_selected_u = false;
	}
};

void mouse_handler::select_hex(const map_location& hex, const bool browse) 
{
	if (hex == selected_hex_) {
		return;
	}

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if (!in_tip_state() && !units_.find_unit(hex)) {
		gui_->hide_tip();
	}
#endif

	if (slot_cache::selected_u && !hex.valid() && !in_multistep_state() && !protect_slot_cache_selected_u) {
		slot_cache::selected_u = NULL;
	}
	const unit* selected_unit = units_.find_unit(selected_hex_);
	if (selected_unit && selected_unit->task() == unit::TASK_GUARD) {
		gui().invalidate_all();
	}
	selected_hex_ = hex;
	gui().select_hex(hex);
	{
		gui_->clear_attack_indicator();
		gui_->clear_build_indicator();
		gui_->set_selectable_indicator(std::set<map_location>());
		gui_->clear_hero_indicator();
	}
	gui().set_route(NULL);
	show_partial_move_ = false;

	unit_map::iterator u = find_unit(hex);
	if (hex.valid() && u != units_.end() && !u->get_hidden()) {
		next_unit_ = u->get_location();

		{
			// if it's not the unit's turn, we reset its moves
			// and we restore them before the "select" event is raised
			unit_movement_resetter move_reset(*u, u->side() != side_num_);
			bool teleport = u->get_ability_bool("teleport");
			current_paths_ = pathfind::paths(map_, units_, hex, teams_,
				false, teleport, viewing_team(), path_turns_);
		}
		gui().highlight_reach(current_paths_);
		// the highlight now comes from selection
		// and not from the mouseover on an enemy
		enemy_paths_ = false;
		gui().set_route(NULL);

		// selection have impact only if we are not observing and it's our unit
		if (!browse && !commands_disabled && u->side() == gui().viewing_side()) {
			sound::play_UI_sound("select-unit.wav");
			u->set_selecting();
			game_events::fire("select", hex);
		}

	} else {
		gui().unhighlight_reach();
		current_paths_ = pathfind::paths();
		current_route_.steps.clear();
	}
}

void mouse_handler::deselect_hex() {
	select_hex(map_location(), true);
}

void mouse_handler::clear_undo_stack()
{
	apply_shroud_changes(undo_stack_, side_num_);
	undo_stack_.clear();
	gui_->enable_menu("undo", false);
}

class end_moving_lock
{
public:
	end_moving_lock(mouse_handler& handler)
		: handler_(handler)
	{}
	~end_moving_lock()
	{
		handler_.end_moving();
	}

private:
	mouse_handler& handler_;
};

map_location mouse_handler::move_unit_along_current_route(const std::vector<map_location>& caller_steps, bool check_shroud, bool attackmove)
{
	const std::vector<map_location> steps = caller_steps;
	if (steps.empty()) {
		return map_location();
	}

	end_moving_lock lock(*this);

	begin_moving(steps.front(), steps.back());
	const unit_map::iterator src_itor = units_.find(steps.front());

	// do not show footsteps during movement
	gui().set_route(NULL);

	// do not keep the hex highlighted that we started from
	selected_hex_ = map_location();
	gui().select_hex(map_location());

	// will be invalid after the move
	current_paths_ = pathfind::paths();
	current_route_.steps.clear();

	attackmove_ = attackmove;
	size_t moves = 0;
	try {
		gui_->hide_context_menu(NULL, true);

		src_itor->get_move_spectator().reset(units_);
		moves = ::move_unit(&src_itor->get_move_spectator(), steps, &recorder, &undo_stack_, true, &next_unit_, false, check_shroud);

		resources::controller->show_context_menu(NULL, *gui_);
	} catch(end_turn_exception&) {
		attackmove_ = false;
		cursor::set(cursor::NORMAL);
		gui().invalidate_game_status();
		throw;
	}
	attackmove_ = false;

	// must verify moves before if (expediting_)!!
	if (moves == 0) {
		if (expediting_) {
			select_hex(steps.front(), false);
			// gui().select_hex(steps.front());
		}
		return map_location();
	}

	// 移动结束,设置出征第二步
	if (expediting_) {
		// 3.置标志
		expediting_ = false;
	}

	cursor::set(cursor::NORMAL);

	gui().invalidate_game_status();

	VALIDATE(moves <= steps.size(), "Check move_unit_along_current_route");
	const map_location& dst = steps[moves-1];

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	// After moveing, mouse_handler::mouse_motion get current loc.x/loc.y use SDL_GetMouseState, 
	// if generate scroll screen when moving, return loc of SDL_GetMouseState will not be dst.
	// On iOS/Android, there is no mouse. let SDL_GetMouseState return dst.
	SDL_WarpMouseInWindow(SDL_GetMouseFocus(), gui_->get_location_x(dst) + display::default_zoom_ / 2, gui_->get_location_y(dst) + display::default_zoom_ / 2);
#endif

	// mover maybe died!
	return units_.find(dst).valid()? dst: map_location();
}

bool mouse_handler::attack_enemy(unit& attacker, unit& defender, const map_location& target_loc)
{
	try {
		return attack_enemy_(attacker, defender, target_loc);
	} catch(std::bad_alloc) {
		lg::wml_error << "Memory exhausted a unit has either a lot hitpoints or a negative amount.\n";
		return false;
	}
}

bool mouse_handler::attack_enemy_(unit& attacker, unit& defender, const map_location& target_loc)
{
	//we must get locations by value instead of by references, because the iterators
	//may become invalidated later
	const map_location attacker_loc = attacker.get_location();
	const map_location defender_loc = defender.get_location();

	// std::vector<std::string> items;
	std::vector<range_locs_pair>& attack_indicator_each = gui_->attack_indicator_each();

	std::vector<battle_context> bc_vector;
	unsigned int i, best = 0;
	for (i = 0; i < attacker.attacks().size(); i++) {
		const attack_type& attack = attacker.attacks()[i];
		// skip weapons with cann't reach this range
		// use target_loc instead of defender.get_location().
		if (std::find(attack_indicator_each[i].second.begin(), attack_indicator_each[i].second.end(), target_loc) == attack_indicator_each[i].second.end()) {
			continue;
		}
		
		if (attack.attack_weight() > 0) {
			battle_context bc(units_, attacker, defender, i, -1, 0.5);
			bc_vector.push_back(bc);
			if (bc.better_attack(bc_vector[best], 0.5)) {
				// there is maybe below weapons.
				// melee 2x4
				// ranged 4x9
				// melee 3x4  <---cast 3x4 (right)
				// It is mistaken in [unit_type], but program should permit it!
				best = i;
				if (best >= bc_vector.size()) {
					best = bc_vector.size() - 1;
				}
			}
		}
	}

	//make it so that when we attack an enemy, the attacking unit
	//is again shown in the status bar, so that we can easily
	//compare between the attacking and defending unit
	gui().highlight_hex(map_location());
	gui().draw(true, true);

	int res = 0;

	{
		gui2::tunit_attack dlg(*gui_, units_, attacker, defender, bc_vector, best);

		dlg.show(resources::screen->video());
		if (dlg.get_retval() != gui2::twindow::OK) {
			res = bc_vector.size();
		} else {
			res = dlg.get_selected_weapon();
		}
	}

	cursor::set(cursor::NORMAL);
	if(size_t(res) < bc_vector.size()) {
		commands_disabled ++;
		const battle_context::unit_stats &att = bc_vector[res].get_attacker_stats();
		const battle_context::unit_stats &def = bc_vector[res].get_defender_stats();

		attacker.set_goto(map_location());
		clear_undo_stack();

		current_paths_ = pathfind::paths();
		gui().clear_attack_indicator();
		gui().clear_build_indicator();
		gui().unhighlight_reach();
		select_hex(map_location(), false);

		//@TODO: change ToD to be location specific for the defender
		recorder.add_attack(attacker, defender, att.attack_num, def.attack_num, 
			attacker.type_id(), defender.type_id(), att.level, 
			def.level, resources::tod_manager->turn(), resources::tod_manager->get_time_of_day());
		rand_rng::invalidate_seed();

		// for duel, hide base information, for context-menu. troop portrait.
		gui_->invalidate_unit();
		gui_->hide_context_menu(NULL, true);
		gui().draw();

		perform_attack(attacker, defender, att.attack_num, def.attack_num, rand_rng::get_last_seed());

		gui_->goto_main_context_menu();
		return true;
	} else {
		return false;
	}
}

bool mouse_handler::cast_tactic(unit& tactician, const map_location& target_loc, bool browse)
{
	//make it so that when we attack an enemy, the attacking unit
	//is again shown in the status bar, so that we can easily
	//compare between the attacking and defending unit
	gui().highlight_hex(map_location());
	gui().draw(true, true);

	team& current_team = teams_[tactician.side() - 1];
	selected_hero_ = NULL;
	{
		int operate = gui2::ttactic2::BROWSE;
		if (tent::mode == mode_tag::SCENARIO || tent::mode == mode_tag::RPG || current_team.allow_intervene) {
			operate = gui2::ttactic2::CAST;
		} else if (current_team.allow_active) {
			operate = gui2::ttactic2::ACTIVE;
		}

		gui2::ttactic2 dlg(*gui_, teams_, units_, heros_, tactician, operate);
		try {
			dlg.show(gui_->video());
			if (dlg.get_retval() == gui2::twindow::OK) {
				selected_hero_ = dlg.get_selected_hero();
			}
		} catch (twml_exception& e) {
			e.show(*gui_);
			return false;
		}
	}
	
	cursor::set(cursor::NORMAL);

	if (tent::mode == mode_tag::TOWER) {
		// in tower mode, must be enable allow_active, it cannot exist with allow_intervene
		gui_->hide_context_menu(NULL, true);
		if (selected_hero_) {
			do_add_active_tactic(tactician, *selected_hero_, true);
		}
		clear();
		gui_->goto_main_context_menu();
		return false;
	} else if (!selected_hero_) {
		return false;
	}

	const ttactic& t = unit_types.tactic(selected_hero_->tactic_);
	if (!t.select_one()) {
		cast_tactic_special(tactician, NULL, browse);

	} else {
		const std::map<int, std::vector<map_location> > touched = t.touch_units(units_, tactician);
		// find key atomic tactic, use its touched
		std::set<map_location> selectable;
		for (std::map<int, std::vector<map_location> >::const_iterator it = touched.begin(); it != touched.end(); ++ it) {
			const std::vector<const ttactic*>& parts = t.parts();
			const ttactic& part_tactic = *(t.parts()[it->first]);
			if (ttactic::select_one(part_tactic.apply_to())) {
				for (std::vector<map_location>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++ it2) {
					selectable.insert(*it2);
				}
				break;
			}
		}

		if (selectable.size() != 1) {
			gui_->set_selectable_indicator(selectable);
			selecting_ = true;
		} else {
			unit* special = &*find_visible_unit(*selectable.begin(), teams_[tactician.side() - 1]);
			cast_tactic_special(tactician, special, browse);
		}
	}
	return true;
}

void mouse_handler::clear_formationed(unit& u, bool browse)
{
	{
		clear_undo_stack();
		clear();

		if (!teams_[u.side() - 1].allow_intervene) {
			do_clear_formationed(*gui_, teams_, units_, u, 0, true);
		} else {
			u.insert_clear_formationed(*gui_);
		}
	}
	gui_->goto_main_context_menu();
}

void mouse_handler::intervene_move(unit& u, const map_location& to, bool browse)
{
	set_unit_placing(u);
}

bool mouse_handler::do_formation_attack(unit& master)
{
	//make it so that when we attack an enemy, the attacking unit
	//is again shown in the status bar, so that we can easily
	//compare between the attacking and defending unit
	gui().highlight_hex(map_location());
	gui().draw(true, true);

	{
		gui2::tformation_attack dlg(*gui_, teams_, units_, heros_, master, false);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return false;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}
	}

	// decide to attack.
	cursor::set(cursor::NORMAL);
	clear_undo_stack();

	clear();

	::do_formation_attack(teams_, units_, *gui_, current_team(), master, true, false);

	gui_->goto_main_context_menu();
	return true;
}

void mouse_handler::clear()
{
	current_paths_ = pathfind::paths();
	gui_->unhighlight_reach();
	gui_->invalidate_unit();
	select_hex(map_location(), false);
	gui_->highlight_hex(map_location());
	gui_->hide_context_menu(NULL, true);
}

void mouse_handler::cast_tactic_special(unit& tactician, unit* special, bool browse)
{
	{
		tactician.set_goto(map_location());
		clear_undo_stack();

		clear();

		if (!teams_[tactician.side() - 1].allow_intervene) {
			::cast_tactic(teams_, units_, tactician, *selected_hero_, special);
		} else {
			tactician.insert_cast_tactic(*gui_, *selected_hero_, special);
		}
	}
	gui_->goto_main_context_menu();
}

void mouse_handler::exit_placing_unit(bool ok)
{
	gui_->enable_menu("play_card", true);
	gui_->enable_menu("endturn", true);

	gui_->clear_hero_indicator();
	playing_card_ = NULL;
	placing_hero_ = NULL;
	placing_unit_ = NULL;
	tasking_unit_ = NULL;

	slot_cache::selected_u = NULL;
	if (ok) {
		clear_undo_stack();
	}

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	gui_->hide_tip();
#endif
}

void mouse_handler::exit_building(bool ok)
{
	gui_->enable_menu("play_card", true);
	if (!undo_stack_.empty()) {
		gui_->enable_menu("undo", true);
	}
	gui_->enable_menu("endturn", true);

	delete building_bldg_;
	building_bldg_ = NULL;
	if (ok) {
		clear_undo_stack();
	}
}

void mouse_handler::exit_selecting(bool ok)
{
	gui_->set_selectable_indicator(std::set<map_location>());
	selecting_ = false;

	slot_cache::selected_u = NULL;
	if (ok) {
		clear_undo_stack();
	}
}

void mouse_handler::place_at(const map_location& at, hero& h)
{
	clear();
	exit_placing_unit(true);

	artifical* city = units_.city_from_cityno(h.city_);
	team& current_team = teams_[h.side_];
	int cost_exponent = current_team.cost_exponent();

	VALIDATE(h.utype_ != HEROS_NO_UTYPE, "mouse_handler::place_at, deirse hero has no unit type!");
	const unit_type* ut = unit_types.keytype(h.utype_);

	std::vector<const hero*> v;
	v.push_back(&h);
	
	int cost = ut->cost();
	if (tent::mode == mode_tag::LAYOUT) {
		cost = 0;
	}
	::do_recruit(units_, heros_, teams_, current_team, ut, v, *city, cost * cost_exponent / 100, true, true, gamestate_);

	do_direct_expedite(teams_, units_, *city, city->reside_troops().size() - 1, at, true);

	gui_->goto_main_context_menu();
}

void mouse_handler::join_in(const map_location& at, hero& h)
{
	clear();
	exit_placing_unit(true);

	artifical* city = units_.city_from_cityno(h.city_);
	unit* u = units_.find_unit(at);

	reform_captain(units_, *u, h, true, false);
	gui_->goto_main_context_menu();
}

void mouse_handler::direct_move_unit(unit& u, const map_location& from, const map_location& to)
{
	clear();

	team& current_team = teams_[u.side() - 1];
	if (!current_team.allow_intervene) {
		do_bomb(*gui_, current_team, true);	
		do_direct_move(teams_, units_, map_, u, to, 0, true);
	} else {
		u.insert_intervene_move(*gui_, to);
	}

	exit_placing_unit(true);
	gui_->invalidate_all();
	gui_->goto_main_context_menu();
}

void mouse_handler::do_task(unit& u, const map_location& at)
{
	clear();

	do_set_task(units_, u, unit::TASK_GUARD, at, true);

	exit_placing_unit(true);
	gui_->invalidate_all();
	gui_->goto_main_context_menu();
}

bool mouse_handler::interior(bool browse, unit& u)
{
	//make it so that when we attack an enemy, the attacking unit
	//is again shown in the status bar, so that we can easily
	//compare between the attacking and defending unit
	gui().highlight_hex(map_location());
	gui().draw(true, true);

	{
		const events::command_disabler disable_commands;

		clear_undo_stack();

		current_paths_ = pathfind::paths();
		gui().clear_attack_indicator();
		gui().clear_build_indicator();
		gui().unhighlight_reach();
		select_hex(map_location(), false);
		gui().invalidate_unit();
		gui_->hide_context_menu(NULL, true);

		team& current_team = teams_[side_num_ - 1];
		std::vector<hero*> before_commercials = current_team.commercials();

		artifical* city = unit_2_artifical(&u);
		hero* mayor = city->mayor();
		gui2::tinterior dlg(*gui_, teams_, units_, heros_, *city, browse);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return false;
		}

		if (!browse) {
			if (city->mayor() != mayor) {
				recorder.add_interior(*city);
			}
			if (city->get_state(ustate_tag::DEPUTE) != dlg.get_depute()) {
				bool value = dlg.get_depute();
				city->set_state(ustate_tag::DEPUTE, value);

				std::map<ustate_tag::state_t, bool> states;
				states.insert(std::make_pair(ustate_tag::DEPUTE, value));
				recorder.add_set_states(*city, states);

				gui_->refresh_access_troops(city->side() - 1, game_display::REFRESH_DRAW, (unit*)city);
			}
		}
		
	}

	cursor::set(cursor::NORMAL);
	gui_->goto_main_context_menu();
	return true;
}

void mouse_handler::build_ea(const map_location& loc)
{
	clear();

	team& t = current_team();
	int cost_exponent = t.cost_exponent();
	const unit_type* ut = NULL;
	{
		gui2::tbuild_ea dlg(*gui_, teams_, units_, heros_, map_, t, cost_exponent, loc);
		try {
			dlg.show(gui_->video());
			if (dlg.get_retval() == gui2::twindow::OK) {
				ut = dlg.get_selected_ut();
			}
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (ut) {
			resources::controller->do_build(t, NULL, ut, loc);
		}
	}
	gui_->goto_main_context_menu();
}

void mouse_handler::perform_attack(unit& attacker, unit& defender,
	int attacker_weapon, int defender_weapon, rand_rng::seed_t seed)
{
	// this function gets it's arguments by value because the calling function
	// object might get deleted in the clear callback call below, invalidating
	// const ref arguments
	rand_rng::clear_new_seed_callback();
	recorder.add_seed("attack", seed);
	//MP_COUNTDOWN grant time bonus for attacking
	current_team().set_action_bonus_count(1 + current_team().action_bonus_count());

	std::pair<map_location, map_location> to_locs;
	try {
		to_locs = attack_unit(attacker, defender, attacker_weapon, defender_weapon);
		commands_disabled--;
	} catch(end_level_exception&) {
		commands_disabled--;
		//if the level ends due to a unit being killed, still see if
		//either the attacker or defender should advance
		unit_map::iterator acku = units_.find(to_locs.first);
		if (acku != units_.end()) {
			dialogs::advance_unit(*acku);
		}
		unit_map::iterator defu = units_.find(to_locs.second);
		if (defu != units_.end()) {
			bool defender_human = teams_[defu->side()-1].is_human();
			dialogs::advance_unit(*defu, !defender_human);
		}
		throw;
	}

	unit* acku = units_.find_unit(to_locs.first);
	if (acku) {
		dialogs::advance_unit(*acku);
	}
	unit* defu = units_.find_unit(to_locs.second);
	if (defu) {
		bool defender_human = teams_[defu->side() - 1].is_human();
		dialogs::advance_unit(*defu, !defender_human);
	}

	if (tent::turn_based) {
		if (acku && teams_[acku->side() - 1].is_human()) {
			gui_->resort_access_troops(*acku);
		}
	}

	resources::controller->check_victory();
	gui().draw();
}

std::set<map_location> mouse_handler::get_adj_enemies(const map_location& loc, int side) const
{
	std::set<map_location> res;

	const team& uteam = teams_[side-1];

	map_location adj[6];
	get_adjacent_tiles(loc, adj);
	BOOST_FOREACH (const map_location &aloc, adj) {
		unit_map::const_iterator i = find_unit(aloc);
		if (i != units_.end() && uteam.is_enemy(i->side()))
			res.insert(aloc);
	}
	return res;
}

// @expedite_city: city on expedite
// @u: index on reside troop of expediting city
void mouse_handler::set_expedite(artifical* expedite_city, unit& u)
{
	expediting_ = true;
	expediting_city_ = expedite_city;
	expediting_unit_ = &u;

	// 先关闭掉先前的上下文菜单
	gui_->hide_context_menu(NULL, true);

	// 具体执行
	selected_hex_ = expedite_city->get_location();

	const map_location& hex = expedite_city->get_location();

	bool teleport = u.get_ability_bool("teleport");

	// 放置一个临时单位
	gui().place_expedite_city(*expedite_city);
	
	unit_map::iterator iter = find_unit(hex);

	// current_paths_ = paths(map_, units_, hex, teams_, false, teleport, viewing_team(), path_turns_);
	current_paths_ = pathfind::paths(map_, units_, u, hex, teams_, false, teleport, viewing_team(), path_turns_);
	gui().highlight_reach(current_paths_);
	// the highlight now comes from selection
	// and not from the mouseover on an enemy
	enemy_paths_ = false;
	gui().set_route(NULL);

	// disable undo/endturn button
	gui_->enable_menu("play_card", false);
	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
}

void mouse_handler::set_building(artifical* bldg)
{
	if (building_bldg_) {
		delete bldg;
		VALIDATE(false, "mouse_handler::set_card_playing, current is in building state!");
	}

	gui_->enable_menu("play_card", false);
	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
	gui_->clear_attack_indicator();

	building_bldg_ = bldg;

	gui_->goto_main_context_menu();

	std::stringstream strstr, img;
	img << bldg->type()->image() << "~SCALE(48, 48)";
	strstr << tintegrate::generate_img(img.str());
	strstr << "  " << bldg->name() << "\n";
	strstr << _("Select a grid to build on");
	gui_->show_tip(strstr.str(), map_location::null_location, true);
}

void mouse_handler::set_card_playing(team& t, int index)
{
	VALIDATE(!playing_card_, "mouse_handler::set_card_playing, current is in card_playing state!");

	gui_->enable_menu("play_card", false);
	gui_->enable_menu("undo", false);
	gui_->enable_menu("endturn", false);
	clear();

	card_index_ = index;
	if (index != -1) {
		playing_card_ = &t.holded_card(index);
	} else {
		playing_card_ = &resources::controller->cards().bomb();
	}
	gui_->goto_main_context_menu();

	std::stringstream strstr, img;
	img << playing_card_->image() << "~SCALE(32, 32)";
	strstr << tintegrate::generate_img(img.str());
	strstr << "  " << playing_card_->name() << "\n";
	strstr << _("Select a unit to action on");
	gui_->show_tip(strstr.str(), map_location::null_location, true);
}

void mouse_handler::set_hero_placing(hero* h)
{
	bool show = false;
	if (h != placing_hero_) {
		clear();
		if (h->status_ != hero_status_idle) {
			if (placing_hero_ && placing_hero_->status_ == hero_status_idle) {
				gui_->enable_menu("play_card", true);
				gui_->enable_menu("endturn", true);

				gui_->clear_hero_indicator();
				gui_->hide_tip();
			}
			show = true;
			placing_hero_ = NULL;
			gui_->goto_main_context_menu();
		} else {
			if (!placing_hero_) {
				gui_->enable_menu("play_card", false);
				gui_->enable_menu("endturn", false);
			}
			placing_hero_ = h;
			gui_->set_hero_indicator(*h);
		}

	} else {
		if (h->status_ != hero_status_idle) {
			return;
		}
		show = true;
	}
	if (show) {
		std::vector<hero*> vh;
		vh.push_back(h);
		gui2::thero dlg(heros_, *h);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
		}
	}

	if (placing_hero_) {
		std::stringstream strstr, img;
		img << h->image() << "~SCALE(32, 40)";
		strstr << tintegrate::generate_img(img.str());
		strstr << "  " << _("Select a grid to place, or a troop to join");
		gui_->show_tip(strstr.str(), map_location::null_location, true);
	}
}

void mouse_handler::set_unit_placing(unit& u)
{
	{
		tprotect_slot_cache lock;
		clear();
	}
	gui_->hide_tip();

	gui_->set_placable_indicator(u);
	placing_unit_ = &u;
	if (placing_unit_) {
		std::stringstream strstr, img;
		img << u.master().image() << "~SCALE(32, 40)";
		strstr << tintegrate::generate_img(img.str());
		strstr << "  " << _("Select a grid to move at");
		gui_->show_tip(strstr.str(), map_location::null_location, true);
	}
}

void mouse_handler::set_unit_tasking(unit& u)
{
	clear();
	gui_->hide_tip();

	tasking_unit_ = &u;
	if (tasking_unit_) {
		std::stringstream strstr, img;
		img << u.master().image() << "~SCALE(32, 40)";
		strstr << tintegrate::generate_img(img.str());
		strstr << "  " << dgettext("wesnoth-lib", "Select a grid to guard at");
		gui_->show_tip(strstr.str(), map_location::null_location, true);
	}
}

void mouse_handler::set_current_paths(pathfind::paths new_paths) {
	gui().unhighlight_reach();
	current_paths_ = new_paths;
	current_route_.steps.clear();
	gui().set_route(NULL);
}

mouse_handler *mouse_handler::singleton_ = NULL;
}
