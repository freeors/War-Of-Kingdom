/* $Id: menu_events.cpp 42828 2010-05-24 14:38:03Z silene $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file menu_events.cpp
 * Operations activated from menus/hotkeys while playing a game.
 * E.g. Unitlist, status_table, save_game, save_map, chat, etc.
 */

#include "global.hpp"

#include "builder.hpp"
#include "ai/manager.hpp"
#include "dialogs.hpp"
#include "formatter.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/unit_create.hpp"
#include "gui/dialogs/recruit.hpp"
#include "gui/dialogs/expedite.hpp"
#include "gui/dialogs/hero_list.hpp"
#include "gui/dialogs/unit_detail.hpp"
#include "gui/dialogs/troop_detail.hpp"
#include "gui/dialogs/play_card.hpp"
#include "gui/dialogs/preferences.hpp"
#include "gui/dialogs/move_hero.hpp"
#include "gui/dialogs/final_battle.hpp"
#include "gui/dialogs/camp_armory.hpp"
#include "gui/dialogs/list.hpp"
#include "gui/dialogs/system.hpp"
#include "gui/dialogs/technology_tree.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "mouse_events.hpp"
#include "play_controller.hpp"
#include "preferences_display.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "sound.hpp"
#include "unit_display.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"
#include "scripting/lua.hpp"
#include "artifical.hpp"
#include "game_preferences.hpp"

#include <boost/bind.hpp>

struct ttask {
	ttask(int task, const guard_cache::tguard& guard)
		: task(task)
		, guard(guard)
	{}
	int task;
	guard_cache::tguard guard;
};

namespace events{

class disband_reside_troop : public gui2::button_action
{
public:
	disband_reside_troop(menu_handler& handler, game_display& disp, team& current_team, artifical& city, std::vector<unit*>& units) : 
		menu_handler_(handler)
		, disp_(disp)
		, current_team_(current_team), 
		city_(city), 
		units_(units) 
		{}
private:
	int button_pressed(int menu_selection);

	menu_handler& menu_handler_;
	game_display& disp_;
	team& current_team_;
	artifical &city_;
	std::vector<unit*>& units_;
};

int disband_reside_troop::button_pressed(int menu_selection)
{
	const size_t index = size_t(menu_selection);
	if(index < units_.size()) {
		unit& u = *units_[index];

		//If the unit is of level > 1, or is close to advancing,
		//we warn the player about it
		std::stringstream message;
		if (u.loyal()) {
			message << _("My lord, this unit is loyal and requires no upkeep! ") << (u.gender() == unit_race::MALE ? _("Do you really want to dismiss him?")
					: _("Do you really want to dismiss her?"));
		} else if(u.level() > 1) {
			message << _("My lord, this unit is an experienced one, having advanced levels! ") << (u.gender() == unit_race::MALE ? _("Do you really want to dismiss him?")
					: _("Do you really want to dismiss her?"));

		} else if(u.experience() > u.max_experience()/2) {
			message << _("My lord, this unit is close to advancing a level! ") << (u.gender() == unit_race::MALE ? _("Do you really want to dismiss him?")
					: _("Do you really want to dismiss her?"));
		}

		if (!message.str().empty()) {
			const int res = gui2::show_message(disp_.video(), "", message.str(), gui2::tmessage::yes_no_buttons);
			if (res == gui2::twindow::CANCEL) {
				return 0;
			}
		}
		// add gold to the gold amount
		int income = calculate_disband_income(u, current_team_.cost_exponent());
		current_team_.spend_gold(-1 * income);
		// add hero to the hero list in city
		city_.fresh_into(&u);

		// erase this unit from reside troop
		city_.troop_go_out(u);
		recorder.add_disband(index, city_.get_location(), income);

		menu_handler_.clear_undo_stack(city_.side());

		game_events::fire("post_disband", city_.get_location());
		return 1;
	} else {
		return 0;
	}
}

class discard_card : public gui2::button_action
{
public:
	discard_card(game_display& disp, team& current_team, card_map& cards) 
		: disp_(disp)
		, current_team_(current_team)
		, cards_(cards)
		{}
private:
	int button_pressed(int menu_selection);

	game_display& disp_;
	team& current_team_;
	card_map& cards_;
};

int discard_card::button_pressed(int menu_selection)
{
	const size_t index = size_t(menu_selection);
	std::vector<size_t>& holded_cards = current_team_.holded_cards();

	if (menu_selection == -1 || index < holded_cards.size()) {
		std::string message;
		utils::string_map symbols;
		
		if (menu_selection == -1) {
			message = _("Do you really want to discard one card randomly?");
		} else {
			symbols["first"] = cards_[holded_cards[menu_selection]].name();
			message = vgettext2("$first, do you really want to discard it?", symbols);
		}

		const int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return 0;
		}
		current_team_.erase_card(menu_selection);

		// card
		refresh_card_button(current_team_, disp_);
		return 1;
	} else {
		return 0;
	}
}

menu_handler::menu_handler(play_controller& controller, game_display* gui, unit_map& units, hero_map& heros, card_map& cards, std::vector<team>& teams,
		const config& level, const gamemap& map,
		const config& game_config, const tod_manager& tod_mng, game_state& gamestate,
		undo_list& undo_stack) :
	controller_(controller),
	gui_(gui),
	units_(units),
	heros_(heros),
	cards_(cards),
	teams_(teams),
	level_(level),
	map_(map),
	game_config_(game_config),
	tod_manager_(tod_mng),
	gamestate_(gamestate),
	undo_stack_(undo_stack),
	last_search_(),
	last_search_hit_(),
	last_recruit_()
{
}

menu_handler::~menu_handler()
{
}
const undo_list& menu_handler::get_undo_list() const
{
	 return undo_stack_;
}

std::string menu_handler::get_title_suffix(int side_num)
{
	int controlled_recruiters = 0;
	for(size_t i = 0; i < teams_.size(); ++i) {
		if (teams_[i].is_human() && units_.find_leader(i + 1) != units_.end()) {
			++controlled_recruiters;
		}
	}
	std::stringstream msg;
	if(controlled_recruiters >= 2) {
		unit_map::const_iterator leader = units_.find_leader(side_num);
		if(leader != units_.end() && !leader->name().empty()) {
			msg << " (" << leader->name(); msg << ")";
		}
	}
	return msg.str();
}
void menu_handler::objectives(int side_num)
{
	config cfg;
	cfg["side"] = str_cast(side_num);
	game_events::handle_event_command("show_objectives",
		game_events::queued_event("_from_interface", map_location(),
			map_location(), config()), vconfig(cfg));
	team &current_team = teams_[side_num - 1];
	dialogs::show_objectives(level_, current_team);
	current_team.reset_objectives_changed();
}

void menu_handler::reside_unit_list_in_city(const artifical* city, bool troop, bool commoner)
{
	if (!troop && !commoner) return;

	std::vector<const unit*> partial_troops;

	if (troop) {
		const std::vector<unit*>& reside_troops = city->reside_troops();
		for (std::vector<unit*>::const_iterator i = reside_troops.begin(); i != reside_troops.end(); ++i) {
			partial_troops.push_back(*i);
		}
	}
	if (commoner) {
		const std::vector<unit*>& reside_commoners = city->reside_commoners();
		for (std::vector<unit*>::const_iterator i = reside_commoners.begin(); i != reside_commoners.end(); ++i) {
			partial_troops.push_back(*i);
		}
	}

	std::stringstream strstr;
	if (troop && commoner) {
		strstr << _("Reside unit");
	} else if (troop && !commoner) {
		strstr << _("Reside Troop");
	} else {
		strstr << _("Reside commoner");
	}
	
	gui2::ttroop_detail dlg(*gui_, teams_, units_, heros_, partial_troops, strstr.str());
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	return;
}

void menu_handler::field_unit_list_in_city(const artifical* city, bool troop, bool commoner)
{
	if (!troop && !commoner) return;

	std::vector<const unit*> partial_troops;

	if (troop) {
		const std::vector<unit*>& field_troops = city->field_troops();
		for (std::vector<unit*>::const_iterator i = field_troops.begin(); i != field_troops.end(); ++i) {
			partial_troops.push_back(*i);
		}
	}
	if (commoner) {
		const std::vector<unit*>& field_commoners = city->field_commoners();
		for (std::vector<unit*>::const_iterator i = field_commoners.begin(); i != field_commoners.end(); ++i) {
			partial_troops.push_back(*i);
		}
	}

	std::stringstream strstr;
	if (troop && commoner) {
		strstr << _("Field unit");
	} else if (troop && !commoner) {
		strstr << _("Field Troop");
	} else {
		strstr << _("Field commoner");
	}

	gui2::ttroop_detail dlg(*gui_, teams_, units_, heros_, partial_troops, strstr.str());
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	return;
}

int menu_handler::hero_list(std::vector<hero*>& heros)
{
	gui2::thero_list dlg(&teams_, &units_, heros_, heros);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return 0;
	}
	return 0;
}

void menu_handler::interior(int side_num)
{
	bool browse = rpg::stratum != hero_stratum_leader;
	gui2::ttechnology_tree dlg(*gui_, teams_, units_, heros_, side_num, browse);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	const ttechnology* ret = dlg.ing_technology();
	team& current_team = teams_[side_num - 1];
	if (!browse && ret != current_team.ing_technology()) {
		current_team.select_ing_technology(ret);
		recorder.add_ing_technology(ret->id());
	}
}

void menu_handler::technology_tree(int side_num)
{
	bool browse = rpg::stratum != hero_stratum_leader;
	gui2::ttechnology_tree dlg(*gui_, teams_, units_, heros_, side_num, browse);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	const ttechnology* ret = dlg.ing_technology();
	team& current_team = teams_[side_num - 1];
	if (!browse && ret != current_team.ing_technology()) {
		clear_undo_stack(side_num);

		current_team.select_ing_technology(ret);
		recorder.add_ing_technology(ret->id());
	}
}

void menu_handler::list(int side_num)
{
	gui2::tlist dlg(*gui_, teams_, units_, heros_, gamestate_, game_config_, side_num);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
}

void menu_handler::system(int side_num)
{
}

void menu_handler::save_map()
{
/*
	std::string input_name = get_dir(get_dir(get_user_data_dir() + "/editor") + "/maps/");
	int res = 0;
	int overwrite = 1;
	do {
		res = dialogs::show_file_chooser_dialog_save(*gui_, input_name, _("Save the Map As"));
		if (res == 0) {

			if (file_exists(input_name)) {
				overwrite = gui::dialog(*gui_, "",
					_("The map already exists. Do you want to overwrite it?"),
					gui::YES_NO).show();
			}
			else
				overwrite = 0;
		}
	} while (res == 0 && overwrite != 0);



	// Try to save the map, if it fails we reset the filename.
	if (res == 0) {
		try {
			write_file(input_name, map_.write());
			gui2::show_transient_message(gui_->video(), "", _("Map saved."));
		} catch (io_exception& e) {
			utils::string_map symbols;
			symbols["msg"] = e.what();
			const std::string msg = vgettext2("Could not save the map: $msg",symbols);
			gui2::show_transient_message(gui_->video(), "", msg);
		}
	}
*/
}

void menu_handler::preferences()
{
	preferences::show_preferences_dialog(*gui_, true);
}

void menu_handler::speak()
{
/*
	textbox_info_.show(gui::TEXTBOX_MESSAGE,_("Message:"),
		has_friends() ? is_observer() ? _("Send to observers only") : _("Send to allies only")
					  : "", preferences::message_private(), *gui_);
*/
}

void menu_handler::whisper()
{
	preferences::set_message_private(true);
	speak();
}

void menu_handler::shout()
{
	preferences::set_message_private(false);
	speak();
}

bool menu_handler::has_friends() const
{
	if(is_observer()) {
		return !gui_->observers().empty();
	}

	for(size_t n = 0; n != teams_.size(); ++n) {
		if(n != gui_->viewing_team() && teams_[gui_->viewing_team()].team_name() == teams_[n].team_name() && teams_[n].is_network()) {
			return true;
		}
	}

	return false;
}

void menu_handler::expedite(int side_num, const map_location &last_hex)
{
	int troop_index;

	team &current_team = teams_[side_num - 1];
	artifical* city = units_.city_from_loc(last_hex);
	std::vector<unit*>& reside_troops = city->reside_troops();
	size_t orignal_size = reside_troops.size();
	// sort the available units into order by value
	// so that the most valuable units are shown first
	// sort_units(reside_troop_team);

	if (reside_troops.empty()) {
		gui2::show_transient_message(gui_->video(), "",
			_("There are no troops available to recall\n(You must have"
			" veteran survivors from a previous scenario)"));
		return;
	}

	{
		std::map<int, ttask> tasks;
		for (std::vector<unit*>::const_iterator it = reside_troops.begin(); it != reside_troops.end(); ++ it) {
			const unit& u = **it;
			const guard_cache::tguard* guard = &guard_cache::null_guard;
			if (u.task() == unit::TASK_GUARD) {
				guard = &guard_cache::find(u);
			}
			tasks.insert(std::make_pair(u.signature, ttask(u.task(), *guard)));
		}

		disband_reside_troop disbander(*this, *gui_, current_team, *city, reside_troops);
		gui2::texpedite dlg(*this, *gui_, map_, teams_, units_, heros_, *city, &disbander);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		for (std::vector<unit*>::const_iterator it = reside_troops.begin(); it != reside_troops.end(); ++ it) {
			const unit& u = **it;
			std::map<int, ttask>::const_iterator find = tasks.find(u.signature);
			const map_location& at = u.task() == unit::TASK_GUARD? guard_cache::find(u).loc: map_location::null_location;
			if (u.task() != find->second.task || at != find->second.guard.loc) {
				recorder.add_set_task(u, u.task(), at);
			}
		}

		if (dlg.get_retval() != gui2::twindow::OK) {
			if (orignal_size != reside_troops.size()) {
				resources::controller->refresh_city_buttons(*city);
			}
			return;
		}
		troop_index = dlg.troop_index();
	}

	if (current_team.gold() < game_config::recall_cost) {
		utils::string_map i18n_symbols;
		i18n_symbols["cost"] = lexical_cast<std::string>(game_config::recall_cost);
		std::string msg = vngettext(
			"You must have at least 1 gold piece to recall a unit",
			"You must have at least $cost gold pieces to recall a unit",
			game_config::recall_cost, i18n_symbols);
		gui2::show_transient_message(gui_->video(), "", msg);
		return;
	}

	const events::command_disabler disable_commands;

	map_location loc = last_hex;

	// recorder.add_recall(res, loc);
	unit& un = *city->reside_troops()[troop_index];

	clear_shroud(side_num);

	units_.set_expediting(city, true, troop_index);
	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	mousehandler->set_expedite(city, un);

	gui_->clear_attack_indicator();
	return;
}

void menu_handler::play_card(bool browse, int side_num)
{
	if (browse) {
		return;
	}

	team &current_team = teams_[side_num - 1];
	std::vector<size_t>& holded_cards = current_team.holded_cards();

	if (holded_cards.empty()) {
		gui2::show_transient_message(gui_->video(), "",
			_("There are no holded card"));
		return;
	}

	{
		discard_card discard(*gui_, current_team, cards_);
		gui2::tplay_card dlg(*gui_, teams_, units_, heros_, cards_, gamestate_, side_num, &discard);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
		mousehandler->set_card_playing(current_team, dlg.card_index());

		gui_->hide_context_menu();
	}	
}

void menu_handler::move(bool browse, int side_num, const map_location &last_hex)
{
	if (browse) {
		return;
	}

	team& current_team = teams_[side_num - 1];

	artifical* src_city = units_.city_from_loc(last_hex);
	std::vector<hero*>& heros = src_city->fresh_heros();

	if (heros.empty()) {
		gui2::show_transient_message(gui_->video(), "", _("There are no hero available to move"));
		return;
	}

	artifical* dst_city = NULL;
	for (unit_map::iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* u = dynamic_cast<unit*>(&*it);
		if (u->is_city() && (u->side() == side_num)) {
			if (src_city != u) {
				dst_city = unit_2_artifical(u);
				break;
			}
		}
	}
	if (!dst_city) {
		gui2::show_transient_message(gui_->video(), "", _("There are no city available to move"));
		return;
	}

	std::set<size_t> checked_heros;
	{
		gui2::tmove_hero dlg(*gui_, teams_, units_, heros_, *src_city);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
		}
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		checked_heros = dlg.checked_heros();
		dst_city = dlg.dst_city();

	} // this will kill the dialog before scrolling
	if (!dst_city || !checked_heros.size()) {
		return;
	}

	recorder.add_move_heros(src_city->get_location(), dst_city->get_location(), checked_heros);

	clear_undo_stack(side_num);

	// move heros from src_city to dst_city
	std::vector<hero*>& dst_heros = dst_city->finish_heros();
	size_t has_erase_heros = 0;

	for (std::set<size_t>::iterator itor = checked_heros.begin(); itor != checked_heros.end(); ++ itor) {
		hero* h = heros[*itor - has_erase_heros];
		src_city->hero_go_out(*h);
		dst_city->move_into(*h);
		has_erase_heros ++;
	}
	gui_->invalidate(src_city->get_location());
	gui_->invalidate(dst_city->get_location());

	game_events::fire("post_moveheros", dst_city->get_location(), src_city->get_location());

	resources::controller->refresh_city_buttons(*src_city);
}

void menu_handler::build(const std::string& type, mouse_handler& mousehandler, unit& builder)
{
	team& current_team = teams_[builder.side() - 1];
	int cost_exponent = current_team.cost_exponent();

	const unit_type* ut = unit_types.id_type(type);
	int cost = ut->cost() * cost_exponent / 100;
	if (tent::mode == mode_tag::LAYOUT) {
		cost = 0;
	} else if (tent::tower_mode()) {
		cost *= game_config::tower_cost_ratio;
	}
	if (cost > current_team.gold()) {
		gui2::show_transient_message(gui_->video(), "", _("You don't have enough gold to build that artifical"));
		return;
	}

	// recorder.add_build_begin(builder.get_location(), type);
	// recorder.add_build_begin(type);
	
	if (!tent::tower_mode() || ut->master() != hero::number_wall) {
		
	} else {
		resources::controller->do_build(current_team, &builder, ut, builder.get_location());
	}
}

void menu_handler::guard(mouse_handler& mousehandler, unit& u)
{
	mousehandler.set_unit_tasking(u);
}

void menu_handler::abolish(mouse_handler& mousehandler, unit& u)
{
	mousehandler.clear();

	do_set_task(units_, u, unit::TASK_NONE, map_location::null_location, true);

	clear_undo_stack(u.side());

	gui_->goto_main_context_menu();
}

void menu_handler::extract(mouse_handler& mousehandler, unit& u)
{
	int side = u.side();

	std::vector<gui2::tval_str> items;
	std::stringstream strstr;

	strstr.str("");
	strstr << tintegrate::generate_img(std::string(u.second().image()) + "~SCALE(48, 60)");
	strstr << u.second().name();
	items.push_back(gui2::tval_str(u.second().number_, strstr.str()));

	if (u.third().valid()) {
		strstr.str("");
		strstr << tintegrate::generate_img(std::string(u.third().image()) + "~SCALE(48, 60)");
		strstr << u.third().name();
		items.push_back(gui2::tval_str(u.third().number_, strstr.str()));
	}

	mousehandler.clear();
	clear_undo_stack(side);

	hero* extracted = NULL;
	if (items.size() == 1) {
		extracted = &u.second();
	} else {
		gui2::tcombo_box dlg(items, u.second().number_, gui2::tcombo_box::EXTRACT);
		dlg.show(gui_->video());
		if (dlg.get_retval() == gui2::twindow::OK) {
			extracted = &heros_[dlg.selected_val()];
		}
	}
	if (extracted) {
		reform_captain(units_, u, *extracted, false, false);
	}

	gui_->goto_main_context_menu();
}

void menu_handler::advance(mouse_handler& mousehandler, unit* u)
{
	if (!u->can_advance()) {
		return;
	}

	mousehandler.clear();

	if (u->experience() < u->max_experience()) {
		u->get_experience(increase_xp::generic_ublock(), u->max_experience() - u->experience());
	}
	{
		const events::command_disabler disable_commands;
		dialogs::advance_unit(*u, true);
	}

	mousehandler.clear();
	clear_undo_stack(u->side());

	gui_->goto_main_context_menu();
}

void menu_handler::demolish(mouse_handler& mousehandler, unit* u)
{
	std::string message, color;
	utils::string_map symbols;
	if (u->is_artifical()) {
		color = tintegrate::object_color;
	} else {
		color = tintegrate::hero_color;
	}
	symbols["type"] = tintegrate::generate_format(u->name(), color);
	message = vgettext2("Do you really want to demolish $type?", symbols);
	int res = gui2::show_message(gui_->video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	int side = u->side();
	team& current_team = teams_[side - 1];
	int income = u->is_artifical()? 0: calculate_disband_income(*u, current_team.cost_exponent(), true);
	if (tent::mode == mode_tag::LAYOUT) {
		income = 0;
	}
	do_demolish(*gui_, units_, current_team, u, income, true);

	mousehandler.clear();
	clear_undo_stack(side);

	gui_->goto_main_context_menu();
}

void menu_handler::armory(mouse_handler& mousehandler, int side_num, artifical& art)
{
	std::vector<unit*> candidate_troops;
	std::vector<hero*> candidate_heros;
	std::vector<std::vector<hero*> > previous_captains;

	team& current_team = teams_[side_num - 1];
	std::vector<unit*>& reside_troops = art.reside_troops();
	std::vector<hero*>& fresh_heros = art.fresh_heros();
	size_t reside_troops_size = reside_troops.size();

	if (!reside_troops_size || (reside_troops_size == 1 && !reside_troops[0]->second().valid() && fresh_heros.empty())) {
		gui2::show_transient_message(gui_->video(), "", _("There are no available troop to rearm"));
		return;
	}
	
	// troops of armory
	int humans = 0;
	for (std::vector<unit*>::iterator itor = reside_troops.begin(); itor != reside_troops.end(); ++ itor) {
		unit& u = **itor;
		candidate_troops.push_back(&u);
		if (u.human()) {
			humans ++;
		}

		previous_captains.push_back(std::vector<hero*>(1, &u.master()));
		if (u.second().valid()) {
			previous_captains.back().push_back(&u.second());
		}
		if (u.third().valid()) {
			previous_captains.back().push_back(&u.third());
		}
	}
	if (!humans) {
		gui2::show_transient_message(gui_->video(), "", _("There are no available troop to rearm"));
		return;
	}

	// heros of armory
	for (std::vector<hero*>::iterator itor = fresh_heros.begin(); itor != fresh_heros.end(); ++ itor) {
		candidate_heros.push_back(*itor);
	}

	{
		gui2::tcamp_armory dlg(candidate_troops, candidate_heros, gui2::tcamp_armory::SCENARIO_CITY, &art);
		dlg.show(gui_->video());
		int res = dlg.get_retval();
	}

	std::vector<size_t> diff;
	for (size_t i = 0; i < reside_troops_size; i ++) {
		unit& u = *reside_troops[i];

		std::vector<hero*>& previous = previous_captains[i];
		if (&u.master() != previous[0]) {
			diff.push_back(i);
			continue;
		}
		if (u.second().valid()) {
			if (previous.size() < 2) {
				diff.push_back(i);
				continue;
			}
			if (&u.second() != previous[1]) {
				diff.push_back(i);
				continue;
			}
		} else if (previous.size() >= 2) {
			diff.push_back(i);
			continue;
		}
		if (u.third().valid()) {
			if (previous.size() < 3) {
				diff.push_back(i);
				continue;
			}
			if (&u.third() != previous[2]) {
				diff.push_back(i);
				continue;
			}
		} else if (previous.size() >= 3) {
			diff.push_back(i);
			continue;
		}
	}

	if (!diff.empty()) {
		fresh_heros.clear();
		for (std::vector<hero*>::iterator h = candidate_heros.begin(); h != candidate_heros.end(); ++ h) {
			(*h)->status_ = hero_status_idle;
			fresh_heros.push_back(*h);
		}
		// use a way to keep up fresh heros with replay. duration armory dialog, player may throw into confusion.
		std::sort(fresh_heros.begin(), fresh_heros.end(), compare_leadership);

		recorder.add_armory(art.get_location(), diff);

		clear_undo_stack(side_num);

		game_events::fire("post_armory", art.get_location());

		resources::controller->refresh_city_buttons(art);
	}
}

void menu_handler::unit_detail(const unit* interviewee)
{
	gui2::tunit_detail dlg(this, *gui_, teams_, units_, heros_, interviewee);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	return;
}


void menu_handler::undo(int side_num)
{
	VALIDATE(!undo_stack_.empty(), "menu_handler::undo, undo_stack_.empty() is true");

	// will move unit, hide context menu.
	gui_->hide_context_menu();
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#else
	gui_->hide_tip();
#endif
	{
		const events::command_disabler disable_commands;
		team &current_team = teams_[side_num - 1];

		undo_action& action = undo_stack_.back();
		
		// Undo a move action
		const int starting_moves = action.starting_moves;
		std::vector<map_location> route = action.route;
		std::reverse(route.begin(), route.end());
		unit* u = units_.find_unit(route.front(), true);
		unit* u_end = units_.find_unit(route.back(), true);

		VALIDATE(u, "menu_handler::undo, Illegal 'undo' found. Possible abuse of [allow_undo]?");
		
		if (map_.is_village(route.front())) {
			get_village(route.front(), action.original_village_owner + 1);
			// MP_COUNTDOWN take away capture bonus
			if (action.countdown_time_bonus) {
				current_team.set_action_bonus_count(current_team.action_bonus_count() - 1);
			}
		}

		VALIDATE(!unit_is_city(&*u), "menu_handler::undo, cannot support undo that start from city!");

		// start: non-city
		if (!u_end || !unit_is_city(u_end)) {
			// end: non-city
			action.starting_moves = u->movement_left();

			unit_display::move_unit(route, *u, teams_);
			units_.extract(u->get_location());

			u->set_goto(map_location());
			u->set_movement(starting_moves);

			units_.place(route.back(), u);
			u->set_standing();

			if (tent::turn_based) {
				gui_->resort_access_troops(*u);
			}
		} else {
			// end: city
			unit_display::move_unit(route, *u, teams_);
			u->set_goto(map_location());
			u->set_movement(starting_moves);
						
			artifical* stop_city = unit_2_artifical(u_end);

			stop_city->troop_come_into2(u, action.recall_pos);

			// unit_display::move_unit will update centor location of u->second to location of city.
			// Centor location of u->second must be back to u->first before call units_.erase.
			// u->second.set_location(u->first);
			// units_.erase(&*u, false);
		}

		unit::clear_status_caches();

		// recalculate fog/shroud on main-map
		const bool shroud_cleared = clear_shroud(side_num);

		// shroud_cleared indicate newly cleared only, cannot closed newly. invalidate all location!
		gui_->invalidate_all();
		// gui_->invalidate(route.back());
		gui_->draw();

		gui_->invalidate_unit();
		gui_->invalidate_game_status();

		undo_stack_.pop_back();

		recorder.undo();

		// const bool shroud_cleared = clear_shroud(side_num);
		if (true) { // if (shroud_cleared)
			gui_->recalculate_minimap();
		} else {
			gui_->redraw_minimap();
		}

		if (undo_stack_.empty()) {
			gui_->set_theme_object_active("undo", false);
		}
	}

	// will move unit, hide context menu.
	gui_->goto_main_context_menu();
}

void menu_handler::redo(int side_num)
{
}

bool menu_handler::clear_shroud(int side_num)
{
	bool cleared = teams_[side_num - 1].auto_shroud_updates() &&
		::clear_shroud(side_num);
	return cleared;
}

void menu_handler::clear_undo_stack(int side_num)
{
	if (!teams_[side_num - 1].auto_shroud_updates())
		apply_shroud_changes(undo_stack_, side_num);
	undo_stack_.clear();
	gui_->set_theme_object_active("undo", false);
}

void menu_handler::toggle_shroud_updates(int side_num)
{
	team &current_team = teams_[side_num - 1];
	bool auto_shroud = current_team.auto_shroud_updates();
	// If we're turning automatic shroud updates on, then commit all moves
	if (!auto_shroud) update_shroud_now(side_num);
	current_team.set_auto_shroud_updates(!auto_shroud);
}

void menu_handler::update_shroud_now(int side_num)
{
	clear_undo_stack(side_num);
}

bool menu_handler::end_turn(int side_num)
{
	const team& current_team = teams_[side_num - 1];
	std::stringstream strstr;
	utils::string_map symbols;
	if (!current_team.is_human()) {
		return true;
	}

	if (current_team.allow_intervene) {
		// VALIDATE(slot_cache::cache.empty(), "it is human team, must not exist delay call!");

	} else if (current_team.allow_active) {
		std::vector<unit*> actives = current_team.active_tactics();
		if ((int)actives.size() >= game_config::active_tactic_slots) {
			return true;
		}
		symbols["count"] = tintegrate::generate_format(game_config::active_tactic_slots - actives.size(), tintegrate::hero_color, 0, true, true);
		
		strstr << vgettext2("There is $count vacant tactic. Do you really want to end your turn?", symbols);
		int res = gui2::show_message(gui_->video(), "", strstr.str(), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
		if (res != gui2::twindow::OK) {
			return false;
		}
	}
	if (tent::mode != mode_tag::RPG || rpg::stratum == hero_stratum_citizen) {
		return true;
	}
	if (!unit::actor || !unit::actor->is_city()) {
		return true;
	}
	
	const std::vector<artifical*>& holden_cities = current_team.holden_cities();

	std::set<artifical*> fresh_imloyalty_cities;
	size_t fresh_imloyalty = 0;
	size_t finish_imloyalty = 0;
	for (std::vector<artifical*>::const_iterator it = holden_cities.begin(); it != holden_cities.end(); ++ it) {
		const artifical* city = *it;
		if (rpg::stratum == hero_stratum_mayor && city->mayor() != rpg::h) {
			continue;
		}
		const std::vector<hero*>& fresh_heros = city->fresh_heros();
		for (std::vector<hero*>::const_iterator h_it = fresh_heros.begin(); h_it != fresh_heros.end(); ++ h_it) {
			const hero* h = *h_it;
			if (h->loyalty(*current_team.leader()) <= game_config::wander_loyalty_threshold) {
				fresh_imloyalty_cities.insert(*it);
				fresh_imloyalty ++;
			}
		}

		const std::vector<hero*>& finish_heros = city->finish_heros();
		for (std::vector<hero*>::const_iterator h_it = finish_heros.begin(); h_it != finish_heros.end(); ++ h_it) {
			const hero* h = *h_it;
			if (h->loyalty(*current_team.leader()) <= game_config::wander_loyalty_threshold) {
				finish_imloyalty ++;
			}
		}
	}

	if (fresh_imloyalty) {
		strstr.str("");
		symbols["city"] = tintegrate::generate_format((*fresh_imloyalty_cities.begin())->name(), "red");
		strstr.str("");
		strstr << fresh_imloyalty_cities.size();
		symbols["cities"] = tintegrate::generate_format(strstr.str(), "red");
		strstr.str("");
		strstr << fresh_imloyalty;
		symbols["heros"] = tintegrate::generate_format(strstr.str(), "yellow");

		strstr.str("");
		if (fresh_imloyalty_cities.size() > 1) {
			strstr << vgettext2("$city and other $cities cities have $heros low loyalty heros. If not process, heros may wander in next turn. Do you really want to end your turn?", symbols);
		} else {
			strstr << vgettext2("$city has $heros low loyalty heros. If not process, heros may wander in next turn. Do you really want to end your turn?", symbols);
		}
		int res = gui2::show_message(gui_->video(), "", strstr.str(), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
		if (res != gui2::twindow::OK) {
			return false;
		}
	}

	// mouse_handler_.deselect_hex();

	return true;
}

void menu_handler::switch_list(int side_num)
{
	gui_->set_current_list_type(gui_->next_list_type());
}

unit* menu_handler::current_unit()
{
	const mouse_handler& mousehandler = resources::controller->get_mouse_handler_base();

	unit* res = find_visible_unit(mousehandler.get_last_hex(),	teams_[gui_->viewing_team()]);
	if (res) {
		return res;
	} else {
		return find_visible_unit(mousehandler.get_selected_hex(), teams_[gui_->viewing_team()]);
	}
}

void menu_handler::change_side(mouse_handler& mousehandler)
{
	const map_location& loc = mousehandler.get_last_hex();
	unit* i = units_.find_unit(loc, true);
	if (!i) {
		if(!map_.is_village(loc))
			return;

		// village_owner returns -1 for free village, so team 0 will get it
		int team = village_owner(loc, teams_) + 1;
		// team is 0-based so team=team::nteams() is not a team
		// but this will make get_village free it
		if(team > team::nteams()) {
			team = 0;
		}
		get_village(loc, team + 1);
	} else {
		int side = i->side();
		++side;
		if(side > team::nteams()) {
			side = 1;
		}
		i->set_side(side);

		if(map_.is_village(loc)) {
			get_village(loc, side);
		}
	}
}

void menu_handler::clear_labels()
{
	if (gui_->team_valid()
	   && !is_observer())
	{
		gui_->labels().clear(gui_->current_team_name(), false);
		recorder.clear_labels(gui_->current_team_name());
	}
}

void menu_handler::execute_gotos(mouse_handler& mousehandler, int side)
{
	// we will loop on all gotos and try to fully move a maximum of them,
	// but we want to avoid multiple blocking of the same unit,
	// so, if possible, it's better to first wait that the blocker move

	unit* actor = tent::turn_based? NULL: unit::actor;
	bool wait_blocker_move = true;
	std::set<map_location> fully_moved;

	bool change = false;
	bool blocked_unit = false;
	team& current_team = teams_[side - 1];
	do {
		change = false;
		blocked_unit = false;
		std::pair<unit**, size_t> p = current_team.field_troop();
		unit** troops = p.first;
		size_t troops_vsize = p.second;
		for (size_t i = 0; i < troops_vsize; i ++) {
			unit& ui = *troops[i];
			if (ui.is_artifical() || ui.is_commoner() || ui.movement_left() == 0) {
				continue;
			}
			
			const map_location& current_loc = ui.get_location();
			const map_location goto_loc = ui.get_goto();
			map_location stop_loc;

			if (goto_loc == current_loc){
				ui.set_goto(map_location());
				continue;
			}

			if (actor && actor != &ui) {
				continue;
			}

			if (!map_.on_board(goto_loc))
				continue;

			// avoid pathfinding calls for finished units
			if (fully_moved.count(current_loc)) {
				continue;
			}

			pathfind::marked_route route = mousehandler.get_route(&ui, goto_loc, current_team);

			if (route.steps.size() <= 1) { // invalid path
				fully_moved.insert(current_loc);
				continue;
			}

			// look where we will stop this turn (turn_1 waypoint or goto)
			map_location next_stop = goto_loc;
			pathfind::marked_route::mark_map::const_iterator w = route.marks.begin();
			for (; w != route.marks.end(); ++w) {
				if (w->second.turns == 1) {
					next_stop = w->first;
					break;
				}
			}

			if (next_stop == current_loc) {
				fully_moved.insert(current_loc);
				continue;
			}

			// we delay each blocked move because some other change
			// may open a another not blocked path
			if (units_.valid2(next_stop, true)) {
				blocked_unit = true;
				if (wait_blocker_move)
					continue;
			}

			gui_->set_route(&route);
			bool show_move = ui.movement_left() >= route.move_cost;
			if (!show_move) {
				// whether turn1's "end" exist city. Not path!
				const unit* u = units_.find_unit(next_stop);
				if (u && u->is_city()) {
					show_move = true;
				}
			}
			if (!show_move) {
				show_move = has_enemy_in_3range(units_, map_, current_team, next_stop);
			}
			int moves = ::move_unit(NULL, route.steps, &recorder, &undo_stack_, show_move, &stop_loc, false);
			change = moves > 0;

			if (change) {
				// something changed, resume waiting blocker (maybe one can move now)
				wait_blocker_move = true;

				artifical* city = units_.city_from_loc(current_loc);
				if (city) {
					std::string message;
					utils::string_map symbols;

					symbols["city"] = tintegrate::generate_format(city->name(), "green");
					if (city == units_.find_unit(goto_loc)) {
						message = vgettext2("Troop has come into $city as planned!", symbols);
					} else {
						symbols["loc"] = tintegrate::generate_format(format_loc(units_, goto_loc, side), "yellow");
						message = vgettext2("As planned, troop require to arrive at $loc, but come into $city in the middle. If want to continue arriving at $loc, please command again.", symbols);
					}
					game_events::show_hero_message(&ui.master(), city, message, game_events::INCIDENT_INVALID);

					// refresh
					i = 0;
					p = current_team.field_troop();
					troops = p.first;
					troops_vsize = p.second;

				} else if (tent::turn_based) {
					gui_->resort_access_troops(ui);
				}
			}
		}

		if (!change && wait_blocker_move) {
			// no change when waiting, stop waiting and retry
			wait_blocker_move = false;
			change = true;
		}
	} while (change && blocked_unit);

	// erase the footsteps after movement
	gui_->set_route(NULL);
	gui_->invalidate_game_status();
}

void menu_handler::toggle_grid()
{
	preferences::set_grid(*gui_, !preferences::grid());
	gui_->invalidate_all();
}

void menu_handler::do_speak()
{
}


void menu_handler::add_chat_message(const time_t& time,
		const std::string& speaker, int side, const std::string& message,
		events::chat_handler::MESSAGE_TYPE type)
{
	gui_->add_chat_message(time, speaker, side, message, type, false);
}

//simple command args parser, separated from command_handler for clarity.
//a word begins with a nonspace
//n-th arg is n-th word up to the next space
//n-th data is n-th word up to the end
//cmd is 0-th arg, begins at 0 always.
class cmd_arg_parser
{
	public:
		cmd_arg_parser() :
			str_(""),
			args(1, 0),
			args_end(false)
		{
		}

		explicit cmd_arg_parser(const std::string& str) :
			str_(str),
			args(1, 0),
			args_end(false)
		{
		}

		void parse(const std::string& str)
		{
			str_ = str;
			args.clear();
			args.push_back(0);
			args_end = false;
		}

		const std::string& get_str() const
		{
			return str_;
		}
		std::string get_arg(unsigned n) const
		{
			advance_to_arg(n);
			if (n < args.size()) {
				return std::string(str_, args[n], str_.find(' ', args[n]) - args[n]);
			} else {
				return "";
			}
		}
		std::string get_data(unsigned n) const
		{
			advance_to_arg(n);
			if (n < args.size()) {
				std::string data(str_, args[n]);
				return utils::strip(data);
			} else {
				return "";
			}
		}
		std::string get_cmd() const
		{
			return get_arg(0);
		}
	private:
		cmd_arg_parser& operator=(const cmd_arg_parser&);
		cmd_arg_parser(const cmd_arg_parser&);
		void advance_to_arg(unsigned n) const
		{
			while (n < args.size() && !args_end) {
				size_t first_space = str_.find_first_of(' ', args.back());
				size_t next_arg_begin = str_.find_first_not_of(' ', first_space);
				if (next_arg_begin != std::string::npos) {
					args.push_back(next_arg_begin);
				} else {
					args_end = true;
				}
			}
		}
		std::string str_;
		mutable std::vector<size_t> args;
		mutable bool args_end;
};

//A helper class template with a slim public interface
//This represents a map of strings to void()-member-function-of-Worker-pointers
//with all the common functionality like general help, command help and aliases
//Usage (of a derived class): Derived(specific-arguments) d; d.dispatch(command);
//Derived classes should override virtual functions where noted.
//The template parameter currently must be the dervived class itself,
//i.e. class X : public map_command_handler<X>
//To add a new command in a derived class:
//  * add a new private void function() to the derived class
//  * add it to the function map in init_map there, setting flags like
//    "D" for debug only (checking the flag is also done in the derived class)
//  * remember to add some help and/or usage information in init_map()
template <class Worker>
class map_command_handler
{
	public:
		typedef void (Worker::*command_handler)();
		struct command
		{
			command_handler handler;
			std::string help; //long help text
			std::string usage; //only args info
			std::string flags;
			explicit command(command_handler h, const std::string help="",
				const std::string& usage="", const std::string flags="")
			: handler(h), help(help), usage(usage), flags(flags)
			{
			}
			bool has_flag(const char f) const
			{
				return flags.find(f) != flags.npos;
			}
			command& add_flag(const char f)
			{
				flags += f;
				return *this;
			}
		};
		typedef std::map<std::string, command> command_map;
		typedef std::map<std::string, std::string> command_alias_map;

		map_command_handler() : cap_("")
		{
		}

		virtual ~map_command_handler() {}

		bool empty() const
		{
			return command_map_.empty();
		}
		//actual work function
		void dispatch(std::string cmd)
		{
			if (empty()) {
				init_map_default();
				init_map();
			}

			// We recursively resolve alias (100 max to avoid infinite recursion)
			for (int i=0; i < 100; ++i) {
				parse_cmd(cmd);
				std::string actual_cmd = get_actual_cmd(get_cmd());
				if (actual_cmd == get_cmd())
					break;
				std::string data = get_data(1);
				// translate the command and add space + data if any
				cmd = actual_cmd + (data.empty() ? "" : " ") + data;
			}

			if (get_cmd().empty()) {
				return;
			}

			if (const command* c = get_command(get_cmd())) {
				if (is_enabled(*c)) {
					(static_cast<Worker*>(this)->*(c->handler))();
				} else {
					print(get_cmd(), _("This command is currently unavailable."));
				}
			} else if (help_on_unknown_) {
				utils::string_map symbols;
				symbols["command"] = get_cmd();
				symbols["help_command"] = cmd_prefix_ + "help";
				print("help", vgettext2("Unknown command '$command', try $help_command "
					"for a list of available commands.", symbols));
			}
		}

	protected:
		void init_map_default()
		{
			register_command("help", &map_command_handler<Worker>::help,
				_("Available commands list and command-specific help. "
				"Use \"help all\" to include currently unavailable commands."),
				_("do not translate the 'all'^[all|<command>]"));
		}
		//derived classes initialize the map overriding this function
		virtual void init_map() = 0;
		//overridden in derived classes to actually print the messages somwehere
		virtual void print(const std::string& title, const std::string& message) = 0;
		//should be overridden in derived classes if the commands have flags
		//this should return a string describing what all the flags mean
		virtual std::string get_flags_description() const
		{
			return "";
		}
		//this should return a string describing the flags of the given command
		virtual std::string get_command_flags_description(const command& /*c*/) const
		{
			return "";
		}
		//this should be overridden if e.g. flags are used to control command
		//availability. Return false if the command should not be executed by dispatch()
		virtual bool is_enabled(const command& /*c*/) const
		{
			return true;
		}
		virtual void parse_cmd(const std::string& cmd_string)
		{
			cap_.parse(cmd_string);
		}
		//safe n-th argunment getter
		virtual std::string get_arg(unsigned argn) const
		{
			return cap_.get_arg(argn);
		}
		//"data" is n-th arg and everything after it
		virtual std::string get_data(unsigned argn = 1) const
		{
			return cap_.get_data(argn);
		}
		virtual std::string get_cmd() const
		{
			return cap_.get_cmd();
		}
		//command error reporting shorthands
		void command_failed(const std::string& message)
		{
			print(get_cmd(), _("Error:") + std::string(" ") + message);
		}
		void command_failed_need_arg(int argn)
		{
			utils::string_map symbols;
			symbols["arg_id"] = lexical_cast<std::string>(argn);
			command_failed(vgettext2("Missing argument $arg_id", symbols));
		}
		void print_usage()
		{
			help_command(get_cmd());
		}
		//take aliases into account
		std::string get_actual_cmd(const std::string& cmd) const
		{
			command_alias_map::const_iterator i = command_alias_map_.find(cmd);
			return i != command_alias_map_.end() ? i->second : cmd;
		}
		const command* get_command(const std::string& cmd) const
		{
			typename command_map::const_iterator i = command_map_.find(cmd);
			return i != command_map_.end() ? &i->second : 0;
		}
		command* get_command(const std::string& cmd)
		{
			typename command_map::iterator i = command_map_.find(cmd);
			return i != command_map_.end() ? &i->second : 0;
		}
		void help()
		{
			//print command-specific help if available, otherwise list commands
			if (help_command(get_arg(1))) {
				return;
			}
			std::stringstream ss;
			bool show_unavail = show_unavailable_ || get_arg(1) == "all";
			BOOST_FOREACH(typename command_map::value_type i, command_map_) {
				if (show_unavail || is_enabled(i.second)) {
					ss << i.first;
					//if (!i.second.usage.empty()) {
					//	ss << " " << i.second.usage;
					//}
					//uncomment the above to display usage information in command list
					//which might clutter it somewhat
					if (!i.second.flags.empty()) {
						ss << " (" << i.second.flags << ") ";
					}
					ss << "; ";
				}
			}
			utils::string_map symbols;
			symbols["flags_description"] = get_flags_description();
			symbols["list_of_commands"] = ss.str();
			symbols["help_command"] = cmd_prefix_ + "help";
			print(_("help"), vgettext2("Available commands $flags_description:\n$list_of_commands", symbols));
			print(_("help"), vgettext2("Type $help_command <command> for more info.", symbols));
		}
		//returns true if the command exists.
		bool help_command(const std::string& acmd)
		{
			std::string cmd = get_actual_cmd(acmd);
			const command* c = get_command(cmd);
			if (c) {
				std::stringstream ss;
				ss << cmd_prefix_ << cmd;
				if (c->help.empty() && c->usage.empty()) {
					ss << _(" No help available.");
				} else {
					ss << " - " << c->help;
				}
				if (!c->usage.empty()) {
					ss << " " << _("Usage:") << " " << cmd_prefix_ << cmd << " " << c->usage;
				}
				ss << get_command_flags_description(*c);
				const std::vector<std::string> l = get_aliases(cmd);
				if (!l.empty()) {
					ss << " (" << _("aliases:") << " " << utils::join(l," ") << ")";
				}
				print(_("help"), ss.str());
			}
			return c != 0;
		}
		cmd_arg_parser cap_;
	protected:
		//show a "try help" message on unknown command?
		static void set_help_on_unknown(bool value)
		{
			help_on_unknown_ = value;
		}
		//this is display-only
		static void set_cmd_prefix(std::string value)
		{
			cmd_prefix_ = value;
		}
		virtual void register_command(const std::string& cmd,
			command_handler h, const std::string& help="",
			const std::string& usage="", const std::string& flags="")
		{
			command c = command(h, help, usage, flags);
			std::pair<typename command_map::iterator, bool> r;
			r = command_map_.insert(typename command_map::value_type(cmd, c));
			if (!r.second) { //overwrite if exists
				r.first->second = c;
			}
		}
		virtual void assert_existence(const std::string& cmd) {
			assert(command_map_.count(cmd));
		}
		virtual void register_alias(const std::string& to_cmd,
			const std::string& cmd)
		{
			// disable the assert to allow alias to "command + args"
			// the fonction assert_existence seems unused now
			//assert_existence(to_cmd);
			command_alias_map_[cmd] = to_cmd;
		}
		//get all aliases of a command.
		static const std::vector<std::string> get_aliases(const std::string& cmd)
		{
			std::vector<std::string> aliases;
			typedef command_alias_map::value_type p;
			BOOST_FOREACH(p i, command_alias_map_) {
				if (i.second == cmd) {
					aliases.push_back(i.first);
				}
			}
			return aliases;
		}
	private:
		static command_map command_map_;
		static command_alias_map command_alias_map_;
		static bool help_on_unknown_;
		static bool show_unavailable_;
		static std::string cmd_prefix_;
};

//static member definitions
template <class Worker>
typename map_command_handler<Worker>::command_map map_command_handler<Worker>::command_map_;

template <class Worker>
typename map_command_handler<Worker>::command_alias_map map_command_handler<Worker>::command_alias_map_;

template <class Worker>
bool map_command_handler<Worker>::help_on_unknown_ = true;

template <class Worker>
bool map_command_handler<Worker>::show_unavailable_ = false;

template <class Worker>
std::string map_command_handler<Worker>::cmd_prefix_;

//command handler for chat /commands
class chat_command_handler : public map_command_handler<chat_command_handler>
{
	public:
		typedef map_command_handler<chat_command_handler> map;
		chat_command_handler(chat_handler& chathandler, bool allies_only)
		: map(), chat_handler_(chathandler), allies_only_(allies_only)
		{
		}

	protected:
		void do_emote();
		void do_network_send();
		void do_network_send_req_arg();
		void do_room_query();
		void do_room_query_noarg();
		void do_gen_room_query();
		void do_whisper();
		void do_chanmsg();
		void do_log();
		void do_ignore();
		void do_friend();
		void do_remove();
		void do_display();
		void do_version();

		/** Ask the server to register the currently used nick. */
		void do_register();

		/** Ask the server do drop the currently used (and registered) nick. */
		void do_drop();

		/** Update details for the currently used username. */
		void do_set();

		/** Request information about a user from the server. */
		void do_info();

		/**
		 * Request a list of details that can be set for a username
		 * as these might vary depending on the configuration of the server.
		 */
		void do_details();

		std::string get_flags_description() const {
			return _("(A) - admin command");
		}

		std::string get_command_flags_description(
			const map_command_handler<chat_command_handler>::command& c) const
		{
			if (c.has_flag('A')) {
				return std::string(" ") + _("(admin only)");
			} else {
				return "";
			}
		}

		bool is_enabled(
			const map_command_handler<chat_command_handler>::command& c) const
		{
			return !(c.has_flag('A') && !preferences::is_authenticated());
		}

		void print(const std::string& title, const std::string& message)
		{
			chat_handler_.add_chat_message(time(NULL), title, 0, message);
		}
		void init_map()
		{
			set_cmd_prefix("/");
			register_command("query", &chat_command_handler::do_network_send,
				_("Send a query to the server. Without arguments the server"
				" should tell you the available commands."));
			register_alias("query", "q");
			register_command("ban", &chat_command_handler::do_network_send_req_arg,
				_("Ban and kick a player or observer. If he is not in the"
				" game but on the server he will only be banned."), _("<nick>"));
			register_command("unban", &chat_command_handler::do_network_send_req_arg,
				_("Unban a user. He does not have to be in the game but on"
				" the server."), _("<nick>"));
			register_command("kick", &chat_command_handler::do_network_send_req_arg,
				_("Kick a player or observer."), _("<nick>"));
			register_command("mute", &chat_command_handler::do_network_send,
				_("Mute an observer. Without an argument displays the mute status."), _("<nick>"));
			register_command("unmute", &chat_command_handler::do_network_send,
				_("Unmute an observer. Without an argument unmutes everyone."), _("<nick>"));
			register_command("muteall", &chat_command_handler::do_network_send,
				_("Mute/Unmute all observers. (toggles)"), "");
			register_command("ping", &chat_command_handler::do_network_send,
				"");
			register_command("green", &chat_command_handler::do_network_send_req_arg,
				"", "", "A");
			register_command("red", &chat_command_handler::do_network_send_req_arg,
				"", "", "A");
			register_command("yellow", &chat_command_handler::do_network_send_req_arg,
				"", "", "A");
			register_command("adminmsg", &chat_command_handler::do_network_send_req_arg,
				_("Send a message to the server admins currently online"), "");
			register_command("emote", &chat_command_handler::do_emote,
				_("Send an emotion or personal action in chat."), _("<message>"));
			register_alias("emote", "me");
			register_command("whisper", &chat_command_handler::do_whisper,
				_("Sends a private message. "
				"You can't send messages to players that don't control "
				"a side in a running game you are in."), _("<nick> <message>"));
			register_alias("whisper", "msg");
			register_alias("whisper", "m");
			register_command("log", &chat_command_handler::do_log,
				_("Change the log level of a log domain."), _("<level> <domain>"));
			register_command("ignore", &chat_command_handler::do_ignore,
				_("Add a nick to your ignores list."), _("<nick>"));
			register_command("friend", &chat_command_handler::do_friend,
				_("Add a nick to your friends list."), _("<nick>"));
			register_command("remove", &chat_command_handler::do_remove,
				_("Remove a nick from your ignores or friends list."), _("<nick>"));
			register_command("list", &chat_command_handler::do_display,
				_("Show your ignores and friends list."));
			register_alias("list", "display");
			register_command("version", &chat_command_handler::do_version,
				_("Display version information."));
			register_command("register", &chat_command_handler::do_register,
				_("Register your nick"), _("<password> <email (optional)>"));
			register_command("drop", &chat_command_handler::do_drop,
				_("Drop your nick."));
			register_command("set", &chat_command_handler::do_set,
				_("Update details for your nick. For possible details see '/details'."),
				_("<detail> <value>"));
			register_command("info", &chat_command_handler::do_info,
				_("Request information about a nick."), _("<nick>"));
			register_command("details", &chat_command_handler::do_details,
				_("Request a list of details you can set for your registered nick."));
			register_command("join", &chat_command_handler::do_network_send_req_arg,
				_("Join a room."), _("<room>"));
			register_alias("join", "j");
			register_command("part", &chat_command_handler::do_network_send_req_arg,
				_("Part a room."), _("<room>"));
			register_command("names", &chat_command_handler::do_room_query,
				_("List room members."), _("<room>"));
			register_command("rooms", &chat_command_handler::do_room_query_noarg,
				_("List available rooms."));
			register_command("room", &chat_command_handler::do_chanmsg,
				_("Room message."), _("<room> <msg>"));
			register_command("room_query", &chat_command_handler::do_gen_room_query,
				_("Room query."), _("<room> <type> [value]"));
			register_alias("room_query", "rq");
		}
	private:
		chat_handler& chat_handler_;
		bool allies_only_;
};

void chat_command_handler::do_emote()
{
	chat_handler_.send_chat_message("/me " + get_data(), allies_only_);
}

void chat_command_handler::do_network_send()
{
	chat_handler_.send_command(get_cmd(), get_data());
}

void chat_command_handler::do_network_send_req_arg()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	do_network_send();
}

void chat_command_handler::do_room_query_noarg()
{
	config data;
	config& q = data.add_child("room_query");
	q.add_child(get_cmd());
	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_room_query()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	config data;
	config& q = data.add_child("room_query");
	q["room"] = get_arg(1);
	q.add_child(get_cmd());
	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_gen_room_query()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	config data;
	config& q = data.add_child("room_query");
	q["room"] = get_arg(1);
	config& c = q.add_child(get_arg(2));
	c["value"] = get_data(3);
	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_whisper()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	if (get_data(2).empty()) return command_failed_need_arg(2);
	chat_handler_.send_whisper(get_arg(1), get_data(2));
	chat_handler_.add_whisper_sent(get_arg(1), get_data(2));
}

void chat_command_handler::do_chanmsg()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	if (get_data(2).empty()) return command_failed_need_arg(2);
	chat_handler_.send_chat_room_message(get_arg(1), get_data(2));
	chat_handler_.add_chat_room_message_sent(get_arg(1), get_data(2));
}

void chat_command_handler::do_log()
{
	chat_handler_.change_logging(get_data());
}

void chat_command_handler::do_ignore()
{
	if (get_arg(1).empty()) {
		const std::set<std::string>& tmp = preferences::get_ignores();
		print(_("ignores list"), tmp.empty() ? _("(empty)") : utils::join(tmp));
	} else {
		for(int i = 1; !get_arg(i).empty(); i++){
			utils::string_map symbols;
			symbols["nick"] = get_arg(i);
			if (preferences::add_ignore(get_arg(i))) {
				print(_("ignores list"),  vgettext2("Added to ignore list: $nick", symbols));
				chat_handler_.user_relation_changed(get_arg(i));
			} else {
				command_failed(vgettext2("Invalid username: $nick", symbols));
			}
		}
	}
}

void chat_command_handler::do_friend()
{
	if (get_arg(1).empty()) {
		const std::set<std::string>& tmp = preferences::get_friends();
		print(_("friends list"), tmp.empty() ? _("(empty)") : utils::join(tmp));
	} else {
		for(int i = 1;!get_arg(i).empty();i++){
			utils::string_map symbols;
			symbols["nick"] = get_arg(i);
			if (preferences::add_friend(get_arg(i))) {
				chat_handler_.user_relation_changed(get_arg(i));
				print(_("friends list"), vgettext2("Added to friends list: $nick", symbols));
			} else {
				command_failed(vgettext2("Invalid username: $nick", symbols));
			}
		}
	}
}

void chat_command_handler::do_remove()
{
	for(int i = 1;!get_arg(i).empty();i++){
		preferences::remove_friend(get_arg(i));
		preferences::remove_ignore(get_arg(i));
		chat_handler_.user_relation_changed(get_arg(i));
		utils::string_map symbols;
		symbols["nick"] = get_arg(i);
		print(_("friends and ignores list"), vgettext2("Removed from list: $nick", symbols));
	}
}

void chat_command_handler::do_display()
{
	const std::set<std::string> & friends = preferences::get_friends();
	const std::set<std::string> & ignores = preferences::get_ignores();

	if (!friends.empty()) {
		print(_("friends list"), utils::join(friends));
	}

	if (!ignores.empty()) {
		print(_("ignores list"), utils::join(ignores));
	}

	if (friends.empty() && ignores.empty()) {
		print(_("friends and ignores list"), _("There are no players on your friends or ignore list."));
	}
}

void chat_command_handler::do_version() {
	print(_("version"), game_config::version);
}

void chat_command_handler::do_register() {
	config data;
	config& nickserv = data.add_child("nickserv");

	if (get_data(1).empty()) return command_failed_need_arg(1);

	config &reg = nickserv.add_child("register");
	reg["password"] = get_arg(1);
	if(!get_data(2).empty()) {
		reg["mail"] = get_arg(2);
	}
	std::string msg;
	if (get_data(2).empty()) {
		msg = _("registering with password *** and no email address");
	} else {
		utils::string_map symbols;
		symbols["email"] = get_data(2);
		msg = vgettext2("registering with password *** and "
			"email address $email", symbols);
	}
	print(_("nick registration"), msg);

	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_drop() {
	config data;
	config& nickserv = data.add_child("nickserv");

	nickserv.add_child("drop");

	print(_("nick registration"), _("dropping your username"));

	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_set() {
	config data;
	config& nickserv = data.add_child("nickserv");

	if (get_data(1).empty()) return command_failed_need_arg(1);
	if (get_data(2).empty()) return command_failed_need_arg(2);

	config &set = nickserv.add_child("set");
	set["detail"] = get_arg(1);
	set["value"] = get_data(2);
	utils::string_map symbols;
	symbols["var"] = get_arg(1);
	symbols["value"] = get_arg(2);
	print(_("nick registration"), vgettext2("setting $var to $value", symbols));

	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_info() {
	if (get_data(1).empty()) return command_failed_need_arg(1);

	config data;
	config& nickserv = data.add_child("nickserv");

	nickserv.add_child("info")["name"] = get_data(1);
	utils::string_map symbols;
	symbols["nick"] = get_arg(1);
	print(_("nick registration"), vgettext2("requesting information for user $nick", symbols));

	network::send_data(*lobby->chat, data);
}

void chat_command_handler::do_details() {

	config data;
	config& nickserv = data.add_child("nickserv");
	nickserv.add_child("details");

	network::send_data(*lobby->chat, data);
}

void menu_handler::send_chat_message(const std::string& message, bool allies_only)
{
	config cfg;
	cfg["id"] = preferences::login();
	cfg["message"] = message;

	const int side = is_observer() ? 0 : gui_->viewing_side();
	if(!is_observer()) {
		cfg["side"] = side;
	}

	bool private_message = has_friends() && allies_only;

	if(private_message) {
		if (is_observer()) {
			cfg["team_name"] = game_config::observer_team_name;
		} else {
			cfg["team_name"] = teams_[gui_->viewing_team()].team_name();
		}
	}

	recorder.speak(cfg);
	add_chat_message(time(NULL), cfg["id"], side, message,
			private_message ? events::chat_handler::MESSAGE_PRIVATE : events::chat_handler::MESSAGE_PUBLIC);

}

void menu_handler::clear_messages()
{
	gui_->clear_chat_messages();	// also clear debug-messages and WML-error-messages
}

void menu_handler::change_controller(const std::string& side, const std::string& controller)
{
	config cfg;
	config& change = cfg.add_child("change_controller");
	change["side"] = side;
	change["controller"] = controller;
	network::send_data(*lobby->chat, cfg);
}

void menu_handler::change_side_controller(const std::string& side, const std::string& player, bool own_side)
{
	config cfg;
	config& change = cfg.add_child("change_controller");
	change["side"] = side;
	change["player"] = player;

	if(own_side) {
		change["own_side"] = "yes";
	}

	network::send_data(*lobby->chat, cfg);
}

chat_handler::chat_handler()
{
}

chat_handler::~chat_handler()
{
}

/**
 * Change the log level of a log domain.
 *
 * @param data string of the form: "@<level@> @<domain@>"
 */
void chat_handler::change_logging(const std::string& data) {
	const std::string::const_iterator j =
			std::find(data.begin(), data.end(), ' ');
	if (j == data.end()) return;
	const std::string level(data.begin(),j);
	const std::string domain(j+1,data.end());
	int severity;
	if (level == "error") severity = 0;
	else if (level == "warning") severity = 1;
	else if (level == "info") severity = 2;
	else if (level == "debug") severity = 3;
	else {
		utils::string_map symbols;
		symbols["level"] = level;
		const std::string& msg =
				vgettext2("Unknown debug level: '$level'.", symbols);
		add_chat_message(time(NULL), _("error"), 0, msg);
		return;
	}
	if (!lg::set_log_domain_severity(domain, severity)) {
		utils::string_map symbols;
		symbols["domain"] = domain;
		const std::string& msg =
				vgettext2("Unknown debug domain: '$domain'.", symbols);
		add_chat_message(time(NULL), _("error"), 0, msg);
		return;
	} else {
		utils::string_map symbols;
		symbols["level"] = level;
		symbols["domain"] = domain;
		const std::string& msg =
				vgettext2("Switched domain: '$domain' to level: '$level'.", symbols);
		add_chat_message(time(NULL), "log", 0, msg);
	}
}

void chat_handler::send_command(const std::string& cmd, const std::string& args /* = "" */) {
	config data;
	if (cmd == "muteall") {
		data.add_child(cmd);
	} else if (cmd == "query") {
		data.add_child(cmd)["type"] = args;
	} else if (cmd == "ban" || cmd == "unban" || cmd == "kick"
	|| cmd == "mute" || cmd == "unmute") {
		data.add_child(cmd)["username"] = args;
	} else if (cmd == "ping") {
		data[cmd] = lexical_cast<std::string>(time(NULL));
	} else if (cmd == "green") {
		data.add_child("query")["type"] = "lobbymsg @" + args;
	} else if (cmd == "red") {
		data.add_child("query")["type"] = "lobbymsg #" + args;
	} else if (cmd == "yellow") {
		data.add_child("query")["type"] = "lobbymsg <255,255,0>" + args;
	} else if (cmd == "adminmsg") {
		data.add_child("query")["type"] = "adminmsg " + args;
	} else if (cmd == "join") {
		data.add_child("room_join")["room"] = args;
	} else if (cmd == "part") {
		data.add_child("room_part")["room"] = args;
	}
	network::send_data(*lobby->chat, data);
}

void chat_handler::do_speak(const std::string& message, bool allies_only)
{
	if(message == "" || message == "/") {
		return;
	}
	bool is_command = (message[0] == '/');
	bool quoted_command = (is_command && message[1] == ' ');

	if(!is_command) {
		send_chat_message(message, allies_only);
		return;
	} else if (quoted_command) {
		send_chat_message(std::string(message.begin() + 2, message.end()), allies_only);
		return;
	}
	std::string cmd(message.begin() + 1, message.end());
	chat_command_handler cch(*this, allies_only);
	cch.dispatch(cmd);
}

void chat_handler::user_relation_changed(const std::string& /*name*/)
{
}

void chat_handler::send_whisper(const std::string& receiver, const std::string& message)
{
	config cwhisper, data;
	cwhisper["receiver"] = receiver;
	cwhisper["message"] = message;
	cwhisper["sender"] = preferences::login();
	data.add_child("whisper", cwhisper);
	network::send_data(*lobby->chat, data);
}

void chat_handler::add_whisper_sent(const std::string& receiver, const std::string& message)
{
	utils::string_map symbols;
	symbols["receiver"] = receiver;
	add_chat_message(time(NULL), vgettext2("whisper to $receiver", symbols), 0, message);
}

void chat_handler::add_whisper_received(const std::string& sender, const std::string& message)
{
	utils::string_map symbols;
	symbols["sender"] = sender;
	add_chat_message(time(NULL), vgettext2("whisper: $sender", symbols), 0, message);
}

void chat_handler::send_chat_room_message(const std::string& room,
	const std::string& message)
{
	config cmsg, data;
	cmsg["room"] = room;
	cmsg["message"] = message;
	cmsg["sender"] = preferences::login();
	data.add_child("message", cmsg);
	network::send_data(*lobby->chat, data);
}

void chat_handler::add_chat_room_message_sent(const std::string &room, const std::string &message)
{
	add_chat_room_message_received(room, preferences::login(), message);
}

void chat_handler::add_chat_room_message_received(const std::string &room,
	const std::string &speaker, const std::string &message)
{
	add_chat_message(time(NULL), room + ": " + speaker, 0, message, events::chat_handler::MESSAGE_PRIVATE);
}

} // end namespace events

