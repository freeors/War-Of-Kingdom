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
 * E.g. Unitlist, status_table, save_game, save_map, chat, show_help, etc.
 */

#include "global.hpp"

#include "builder.hpp"
#include "ai/manager.hpp"
#include "dialogs.hpp"
#include "formatter.hpp"
#include "filechooser.hpp"
#include "game_end_exceptions.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "gui/dialogs/edit_label.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/gamestate_inspector.hpp"
#include "gui/dialogs/data_manage.hpp"
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
#include "statistics_dialog.hpp"
#include "unit_display.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"
#include "scripting/lua.hpp"
#include "widgets/combo.hpp"
#include "artifical.hpp"
#include "game_preferences.hpp"

#include <boost/bind.hpp>

extern double tower_cost_ratio;

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
		delete &u;
		units_.erase(units_.begin() + index);
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
			message = vgettext("$first, do you really want to discard it?", symbols);
		}
		const int res = gui::dialog(disp_, "", message, gui::YES_NO).show();
		if (res != 0) {
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

menu_handler::menu_handler(game_display* gui, unit_map& units, hero_map& heros, card_map& cards, std::vector<team>& teams,
		const config& level, const gamemap& map,
		const config& game_config, const tod_manager& tod_mng, game_state& gamestate,
		undo_list& undo_stack) :
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
	textbox_info_(),
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

gui::floating_textbox& menu_handler::get_textbox()
{
	return textbox_info_;
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

void menu_handler::show_statistics(int side_num)
{
	team &current_team = teams_[side_num - 1];
	// Current Player name
	const std::string &player = current_team.current_player();
	//add player's name to title of dialog
	std::stringstream title_str;
	title_str <<  _("Statistics") << " (" << player << ")";
	statistics_dialog stats_dialog(*gui_, title_str.str(),
		side_num, current_team.save_id(), player);
	stats_dialog.show();
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
		strstr << dgettext("wesnoth-lib", "Reside unit");
	} else if (troop && !commoner) {
		strstr << dgettext("wesnoth-lib", "Reside Troop");
	} else {
		strstr << dgettext("wesnoth-lib", "Reside commoner");
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
		strstr << dgettext("wesnoth-lib", "Field unit");
	} else if (troop && !commoner) {
		strstr << dgettext("wesnoth-lib", "Field Troop");
	} else {
		strstr << dgettext("wesnoth-lib", "Field commoner");
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
	gui2::tlist dlg(*gui_, teams_, units_, heros_, gamestate_, side_num);
	try {
		dlg.show(gui_->video());
	} catch(twml_exception& e) {
		e.show(*gui_);
		return;
	}
}

void menu_handler::system(int side_num)
{
	play_controller* controller = resources::controller;
	enum {_SAVE, _SAVEREPLAY, _SAVEMAP, _LOAD, _PREFERENCES, _HELP, _QUIT};

	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;
	{
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Save Game"), !controller->is_replaying()));
		rets.push_back(_SAVE);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Save Replay"), !controller->is_replaying()));
		rets.push_back(_SAVEREPLAY);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Load Game")));
		rets.push_back(_LOAD);

		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Preferences")));
		rets.push_back(_PREFERENCES);

		if (!game_config::tiny_gui) {
			items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Help")));
			rets.push_back(_HELP);
		}
		items.push_back(gui2::tsystem::titem(dgettext("wesnoth-lib", "Quit")));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(*gui_, items);
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
		controller->save_game();

	} else if (rets[retval] == _SAVEREPLAY) {
		controller->save_replay();

	} else if (rets[retval] == _SAVEMAP) {
		controller->save_map();

	} else if (rets[retval] == _LOAD) {
		controller->load_game();

	} else if (rets[retval] == _PREFERENCES) {
		controller->preferences();

	} else if (rets[retval] == _HELP) {
		controller->show_help();

	} else if (rets[retval] == _QUIT) {
		if (gui_->in_game()) {
			const int res = gui2::show_message(gui_->video(), _("Quit"), dgettext("wesnoth-lib", "Do you really want to quit?"), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
			if (res != gui2::twindow::CANCEL) {
				throw end_level_exception(QUIT);
			}
		}
	}
}

namespace {
class leader_scroll_dialog : public gui::dialog {
public:
	leader_scroll_dialog(display &disp, const std::string &title,
			std::vector<bool> &leader_bools, int selected,
			gui::DIALOG_RESULT extra_result) :
		dialog(disp, title, "", gui::NULL_DIALOG),
		scroll_btn_(new gui::standard_dialog_button(disp.video(), _("Scroll To"), 0, false)),
		leader_bools_(leader_bools),
		extra_result_(extra_result)
	{
		scroll_btn_->enable(leader_bools[selected]);
		add_button(scroll_btn_, gui::dialog::BUTTON_STANDARD);
		add_button(new gui::standard_dialog_button(disp.video(),
			_("Close"), 1, true), gui::dialog::BUTTON_STANDARD);
	}
	void action(gui::dialog_process_info &info) {
		const bool leader_bool = leader_bools_[get_menu().selection()];
		scroll_btn_->enable(leader_bool);
		if(leader_bool && (info.double_clicked || (!info.key_down
		&& (info.key[SDLK_RETURN] || info.key[SDLK_KP_ENTER])))) {
			set_result(get_menu().selection());
		} else if(!info.key_down && info.key[SDLK_ESCAPE]) {
			set_result(gui::CLOSE_DIALOG);
		} else if(!info.key_down && info.key[SDLK_SPACE]) {
			set_result(extra_result_);
		} else if(result() == gui::CONTINUE_DIALOG) {
			dialog::action(info);
		}
	}
private:
	gui::standard_dialog_button *scroll_btn_;
	std::vector<bool> &leader_bools_;
	gui::DIALOG_RESULT extra_result_;
};
} //end anonymous namespace

void menu_handler::save_map()
{
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
			const std::string msg = vgettext("Could not save the map: $msg",symbols);
			gui2::show_transient_message(gui_->video(), "", msg);
		}
	}
}

void menu_handler::preferences()
{
	gui2::show_preferences_dialog(*gui_);

	gui_->redraw_everything();
}

void menu_handler::show_chat_log()
{
	std::string text = recorder.build_chat_log();
	gui::show_dialog(*gui_,NULL,_("Chat Log"),"",gui::CLOSE_ONLY,NULL,NULL,"",&text);
}

void menu_handler::show_help()
{
	help::show_help(*gui_);
}

void menu_handler::speak()
{
	textbox_info_.show(gui::TEXTBOX_MESSAGE,_("Message:"),
		has_friends() ? is_observer() ? _("Send to observers only") : _("Send to allies only")
					  : "", preferences::message_private(), *gui_);
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
		disband_reside_troop disbander(*this, *gui_, current_team, *city, reside_troops);
		gui2::texpedite dlg(*this, *gui_, map_, teams_, units_, heros_, *city, &disbander);
		try {
			dlg.show(gui_->video());
		} catch(twml_exception& e) {
			e.show(*gui_);
			return;
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

	statistics::recall_unit(un);
	clear_shroud(side_num);

	units_.set_expediting(city, true, troop_index);
	events::mouse_handler* mousehandler = events::mouse_handler::get_singleton();
	mousehandler->set_expedite(city, troop_index);

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

		gui_->hide_context_menu(NULL, true);
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
	for (unit_map::iterator itor = units_.begin(); itor != units_.end(); ++ itor) {
		if (itor->is_city() && (itor->side() == side_num)) {
			if (src_city != &*itor) {
				dst_city = unit_2_artifical(&*itor);
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
	if (tent::mode == TOWER_MODE) {
		cost *= tower_cost_ratio;
	}
	if (cost > current_team.gold()) {
		gui2::show_transient_message(gui_->video(), "", _("You don't have enough gold to build that artifical"));
		return;
	}

	// recorder.add_build_begin(builder.get_location(), type);
	// recorder.add_build_begin(type);
	
	if (tent::mode != TOWER_MODE) {
		// construct a temporary artifical used to show animiator to player.
		artifical* city = units_.city_from_cityno(builder.cityno());
		std::vector<const hero*> v;
		if (ut->master() != HEROS_INVALID_NUMBER) {
			v.push_back(&heros_[ut->master()]);
		} else {
			v.push_back(&city->master());
		}
		type_heros_pair pair(ut, v);
		artifical* new_unit = new artifical(units_, heros_, teams_, pair, builder.cityno(), false);
	
		gui_->set_build_indicator(&builder, new_unit);

		if (gui_->build_indicator().empty()) {
			// recorder.add_build_cancel();
			if (!new_unit->wall()) {
				gui2::show_transient_message(gui_->video(), "", _("There hasn't valid grid to build that artifical."));
			} else {
				// will build is wall, find proper reason.
				size_t i, i2;
				for (i = 0; i < builder.adjacent_size_; i ++) {
					const map_location& adj_loc = builder.adjacent_[i];
					unit_map::const_iterator u_itor = units_.find(adj_loc);
					if (!u_itor.valid()) {
						u_itor = units_.find(adj_loc, false);
					}
					if (!u_itor.valid() && new_unit->terrain_matches(map_.get_terrain(adj_loc))) {
						map_offset* adjacent2_ptr = adjacent_2[adj_loc.x & 0x1];
						size_t size2 = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
						for (i2 = 0; i2 < size2; i2 ++) {
							artifical* city = units_.city_from_loc(map_location(adj_loc.x + adjacent2_ptr[i2].x, adj_loc.y + adjacent2_ptr[i2].y));
							if (city && city->side() == new_unit->side() && calculate_keeps(units_, *city) < 2) {
								break;
							}
						}
						if (i2 != size2) {
							break;
						}
					}
				}
				if (i == builder.adjacent_size_) {
					gui2::show_transient_message(gui_->video(), "", _("There hasn't valid grid to build that artifical."));
				} else {
					gui2::show_transient_message(gui_->video(), "", _("Must exist tow keep at least before building wall."));
				}
			}
			delete new_unit;
			return;
		}
		mousehandler.set_building(new_unit);
	} else {
		resources::controller->do_build(builder, ut, builder.get_location());
	}
}

void menu_handler::extract(mouse_handler& mousehandler, unit& u)
{
	int side = u.side();

	std::vector<hero*> vh;
	std::vector<std::string> items;
	std::stringstream strstr;

	strstr.str("");
	strstr << IMAGE_PREFIX << u.second().image() << "~SCALE(48, 60)" << COLUMN_SEPARATOR;
	strstr << u.second().name();
	items.push_back(strstr.str());
	vh.push_back(&u.second());

	if (u.third().valid()) {
		strstr.str("");
		strstr << IMAGE_PREFIX << u.third().image() << "~SCALE(48, 60)" << COLUMN_SEPARATOR;
		strstr << u.third().name();
		items.push_back(strstr.str());
		vh.push_back(&u.third());
	}

	mousehandler.clear();
	clear_undo_stack(side);

	hero* extracted = NULL;
	if (vh.size() == 1) {
		extracted = vh.front();
	} else {
		gui2::tcombo_box dlg(items, 0, gui2::tcombo_box::EXTRACT);
		dlg.show(gui_->video());
		if (dlg.get_retval() == gui2::twindow::OK) {
			std::vector<hero*>::iterator it = vh.begin();
			std::advance(it, dlg.selected_index());
			extracted = *it;
		}
	}
	if (extracted) {
		reform_captain(units_, u, *extracted, false, false);
	}

	gui_->goto_main_context_menu();
}

void menu_handler::demolish(mouse_handler& mousehandler, unit* u)
{
	std::string message, color;
	utils::string_map symbols;
	if (u->is_artifical()) {
		color = help::tintegrate::object_color;
	} else {
		color = help::tintegrate::hero_color;
	}
	symbols["type"] = help::tintegrate::generate_format(u->name(), color);
	message = vgettext("Do you really want to demolish $type?", symbols);
	int res = gui2::show_message(gui_->video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	int side = u->side();
	team& current_team = teams_[side - 1];
	int income = u->is_artifical()? 0: calculate_disband_income(*u, current_team.cost_exponent(), true);
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
	gui_->hide_context_menu(NULL, true);
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
		unit_map::iterator u = units_.find(route.front());
		const unit_map::iterator u_end = units_.find(route.back());

		VALIDATE(u != units_.end(), "menu_handler::undo, Illegal 'undo' found. Possible abuse of [allow_undo]?");
		
		if (map_.is_village(route.front())) {
			get_village(route.front(), action.original_village_owner + 1);
			// MP_COUNTDOWN take away capture bonus
			if (action.countdown_time_bonus) {
				current_team.set_action_bonus_count(current_team.action_bonus_count() - 1);
			}
		}
		if (!unit_is_city(&*u)) {
			// start: non-city
			if (!u_end.valid() || !unit_is_city(&*u_end)) {
				// end: non-city
				action.starting_moves = u->movement_left();

				unit_display::move_unit(route, *u, teams_);
				std::pair<map_location, unit*>* up = units_.extract(u->get_location());
				up->second->set_goto(map_location());
				up->second->set_movement(starting_moves);
				up->first = route.back();
				// units_.insert(up);
				units_.place(up);
				up->second->set_standing();

				gui_->refresh_access_troops(side_num - 1, game_display::REFRESH_ENABLE, up->second);
			} else {
				// end: city
				unit_display::move_unit(route, *u, teams_);
				u->set_goto(map_location());
				u->set_movement(starting_moves);
							
				artifical* stop_city = unit_2_artifical(&*u_end);

				stop_city->troop_come_into2(&*u, action.recall_pos);

				gui_->refresh_access_troops(side_num - 1, game_display::REFRESH_ERASE, &*u);
				// unit_display::move_unit will update centor location of u->second to location of city.
				// Centor location of u->second must be back to u->first before call units_.erase.
				// u->second.set_location(u->first);
				// units_.erase(&*u, false);
			}
		} else {
			// start: city (end must is non-city)
			VALIDATE(!u_end.valid() || !unit_is_city(&*u_end), "menu_handler::undo, cannot support one turn to out and home between city, check map!");

			artifical* start_city = unit_2_artifical(&*u);
			unit& undo_troop = *start_city->reside_troops().back();

			unit_display::move_unit(route, undo_troop, teams_);
			undo_troop.set_goto(map_location());
			undo_troop.set_movement(starting_moves);

			units_.add(route.back(), &undo_troop);
			start_city->troop_go_out(start_city->reside_troops().size() - 1);
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
			gui_->enable_menu("undo", false);
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
	gui_->enable_menu("undo", false);
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

	if (tent::mode == TOWER_MODE) {
		std::vector<unit*> actives = current_team.active_tactics();
		if ((int)actives.size() >= game_config::active_tactic_slots) {
			return true;
		}
		symbols["count"] = help::tintegrate::generate_format(game_config::active_tactic_slots - actives.size(), help::tintegrate::hero_color, 0, true, true);
		
		strstr << vgettext("There is $count vacant tactic. Do you really want to end your turn?", symbols);
		int res = gui2::show_message(gui_->video(), "", strstr.str(), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
		if (res != gui2::twindow::OK) {
			return false;
		}
		return true;

	} else if (tent::mode == NONE_MODE || rpg::stratum == hero_stratum_citizen) {
		return true;
	}
	

	const std::vector<artifical*>& holded_cities = current_team.holded_cities();

	size_t freshes = 0;
	size_t finishes = 0;
	size_t non_commercial_officials = 0;

	std::set<artifical*> fresh_imloyalty_cities;
	size_t fresh_imloyalty = 0;
	size_t finish_imloyalty = 0;
	for (std::vector<artifical*>::const_iterator it = holded_cities.begin(); it != holded_cities.end(); ++ it) {
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
			if (h->official_ != HEROS_NO_OFFICIAL && h->official_ != hero_official_commercial) {
				non_commercial_officials ++;
			}
		}
		freshes += fresh_heros.size();

		const std::vector<hero*>& finish_heros = city->finish_heros();
		for (std::vector<hero*>::const_iterator h_it = finish_heros.begin(); h_it != finish_heros.end(); ++ h_it) {
			const hero* h = *h_it;
			if (h->loyalty(*current_team.leader()) <= game_config::wander_loyalty_threshold) {
				finish_imloyalty ++;
			}
			if (h->official_ != HEROS_NO_OFFICIAL && h->official_ != hero_official_commercial) {
				non_commercial_officials ++;
			}
		}
		finishes += finish_heros.size();
	}

	if (fresh_imloyalty) {
		strstr.str("");
		symbols["city"] = (*fresh_imloyalty_cities.begin())->name();
		strstr.str("");
		strstr << fresh_imloyalty_cities.size();
		symbols["cities"] = strstr.str();
		strstr.str("");
		strstr << fresh_imloyalty;
		symbols["heros"] = strstr.str();

		strstr.str("");
		if (fresh_imloyalty_cities.size() > 1) {
			strstr << vgettext("$city and other $cities cities have $heros low loyalty heros. If not process, heros may wander in next turn. Do you really want to end your turn?", symbols);
		} else {
			strstr << vgettext("$city has $heros low loyalty heros. If not process, heros may wander in next turn. Do you really want to end your turn?", symbols);
		}
		int res = gui2::show_message(gui_->video(), "", strstr.str(), gui2::tmessage::yes_no_buttons, "hero-256/0.png");
		if (res != gui2::twindow::OK) {
			return false;
		}
	}

	return true;
}

void menu_handler::switch_list(int side_num)
{
	gui_->set_current_list_type(gui_->next_list_type());
}

unit_map::iterator menu_handler::current_unit()
{
	const mouse_handler& mousehandler = resources::controller->get_mouse_handler_base();

	unit_map::iterator res = find_visible_unit(mousehandler.get_last_hex(),
		teams_[gui_->viewing_team()]);
	if(res != units_.end()) {
		return res;
	} else {
		return find_visible_unit(mousehandler.get_selected_hex(),
			teams_[gui_->viewing_team()]);
	}
}

void menu_handler::change_side(mouse_handler& mousehandler)
{
	const map_location& loc = mousehandler.get_last_hex();
	const unit_map::iterator i = units_.find(loc);
	if(i == units_.end()) {
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

void menu_handler::label_terrain(mouse_handler& mousehandler, bool team_only)
{
	const map_location& loc = mousehandler.get_last_hex();
	if (map_.on_board(loc) == false) {
		return;
	}
	gui::dialog d(*gui_, _("Place Label"), "", gui::OK_CANCEL);
	const terrain_label* old_label = gui_->labels().get_label(loc);
	d.set_textbox(_("Label: "), (old_label ? old_label->text() : ""), map_labels::get_max_chars());
	d.add_option(_("Team only"), team_only, gui::dialog::BUTTON_CHECKBOX_LEFT);

	if(!d.show()) {
		std::string team_name;
		SDL_Color colour = font::LABEL_COLOR;

		if (d.option_checked()) {
			team_name = gui_->labels().team_name();
		} else {
			colour = int_to_color(team::get_side_rgb(gui_->viewing_side()));
		}
		const std::string& old_team_name = old_label ? old_label->team_name() : "";
		// remove the old label if we changed the team_name
		if (d.option_checked() == (old_team_name == "")) {
			const terrain_label* old = gui_->labels().set_label(loc, "", old_team_name, colour);
			if (old) recorder.add_label(old);
		}
		const terrain_label* res = gui_->labels().set_label(loc, d.textbox_text(), team_name, colour);
		if (res)
			recorder.add_label(res);
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

void menu_handler::execute_gotos(mouse_handler &mousehandler, int side)
{
	// we will loop on all gotos and try to fully move a maximum of them,
	// but we want to avoid multiple blocking of the same unit,
	// so, if possible, it's better to first wait that the blocker move

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
			const map_location& goto_loc = ui.get_goto();
			map_location stop_loc;

			if (goto_loc == current_loc){
				ui.set_goto(map_location());
				continue;
			}

			if(!map_.on_board(goto_loc))
				continue;

			// avoid pathfinding calls for finished units
			if(fully_moved.count(current_loc))
				continue;

			pathfind::marked_route route = mousehandler.get_route(&ui, goto_loc, current_team);

			if (route.steps.size() <= 1) { // invalid path
				fully_moved.insert(current_loc);
				continue;
			}

			// look where we will stop this turn (turn_1 waypoint or goto)
			map_location next_stop = goto_loc;
			pathfind::marked_route::mark_map::const_iterator w = route.marks.begin();
			for(; w != route.marks.end(); ++w) {
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
			if (units_.count(next_stop)) {
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
				// whether turn1's "end" exist enemy or not. 
				map_offset* adjacent_ptr;
				size_t i, size;
				map_location adjacent_loc;
				unit* u;

				// range=1
				size = (sizeof(adjacent_1) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_1[next_stop.x & 0x1];
				for (i = 0; i < size && !show_move; i ++) {
					adjacent_loc.x = next_stop.x + adjacent_ptr[i].x;
					adjacent_loc.y = next_stop.y + adjacent_ptr[i].y;
					if (map_.on_board(adjacent_loc)) {
						u = units_.find_unit(adjacent_loc);
						if (u && current_team.is_enemy(u->side())) {
							show_move = true;
						}
					}
				}
				// range=2
				size = (sizeof(adjacent_2) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_2[next_stop.x & 0x1];
				for (i = 0; i < size && !show_move; i ++) {
					adjacent_loc.x = next_stop.x + adjacent_ptr[i].x;
					adjacent_loc.y = next_stop.y + adjacent_ptr[i].y;
					if (map_.on_board(adjacent_loc)) {
						u = units_.find_unit(adjacent_loc);
						if (u && current_team.is_enemy(u->side())) {
							show_move = true;
						} 
					}
				}
				// range=3
				size = (sizeof(adjacent_3) / sizeof(map_offset)) >> 1;
				adjacent_ptr = adjacent_3[next_stop.x & 0x1];
				for (i = 0; i < size && !show_move; i ++) {
					adjacent_loc.x = next_stop.x + adjacent_ptr[i].x;
					adjacent_loc.y = next_stop.y + adjacent_ptr[i].y;
					if (map_.on_board(adjacent_loc)) {
						u = units_.find_unit(adjacent_loc);
						if (u && current_team.is_enemy(u->side())) {
							show_move = true;
						}
					}
				}
			}
			int moves = ::move_unit(NULL, route.steps, &recorder, &undo_stack_, show_move, &stop_loc, false);
			change = moves > 0;

			if (change) {
				// something changed, resume waiting blocker (maybe one can move now)
				wait_blocker_move = true;

				bool stop_on_city = units_.city_from_loc(stop_loc)? true: false;
				// once come into city(clicked_selfcity), selected_itor become invalid.
				if (!stop_on_city && !unit_can_move(ui)) {
					gui_->refresh_access_troops(ui.side() - 1, game_display::REFRESH_DISABLE, &ui);
				} else if (stop_on_city) {
					// once ui enter into city, field_troop of team became invalid.
					artifical* city = units_.city_from_loc(stop_loc);
					// refresh
					i = 0;
					p = current_team.field_troop();
					troops = p.first;
					troops_vsize = p.second;
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


void menu_handler::toggle_ellipses()
{
	preferences::set_ellipses(!preferences::ellipses());
	gui_->invalidate_all();
}

void menu_handler::toggle_grid()
{
	preferences::set_grid(!preferences::grid());
	gui_->invalidate_all();
}

void menu_handler::do_speak(){
	//None of the two parameters really needs to be passed since the informations belong to members of the class.
	//But since it makes the called method more generic, it is done anyway.
	chat_handler::do_speak(textbox_info_.box()->text(),textbox_info_.check() != NULL ? textbox_info_.check()->checked() : false);
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
				print("help", VGETTEXT("Unknown command '$command', try $help_command "
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
			command_failed(VGETTEXT("Missing argument $arg_id", symbols));
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
			print(_("help"), VGETTEXT("Available commands $flags_description:\n$list_of_commands", symbols));
			print(_("help"), VGETTEXT("Type $help_command <command> for more info.", symbols));
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
	network::send_data(data, 0);
}

void chat_command_handler::do_room_query()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	config data;
	config& q = data.add_child("room_query");
	q["room"] = get_arg(1);
	q.add_child(get_cmd());
	network::send_data(data, 0);
}

void chat_command_handler::do_gen_room_query()
{
	if (get_data(1).empty()) return command_failed_need_arg(1);
	config data;
	config& q = data.add_child("room_query");
	q["room"] = get_arg(1);
	config& c = q.add_child(get_arg(2));
	c["value"] = get_data(3);
	network::send_data(data, 0);
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
				print(_("ignores list"),  VGETTEXT("Added to ignore list: $nick", symbols));
				chat_handler_.user_relation_changed(get_arg(i));
			} else {
				command_failed(VGETTEXT("Invalid username: $nick", symbols));
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
				print(_("friends list"),  VGETTEXT("Added to friends list: $nick", symbols));
			} else {
				command_failed(VGETTEXT("Invalid username: $nick", symbols));
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
		print(_("friends and ignores list"), VGETTEXT("Removed from list: $nick", symbols));
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
	print(_("version"), game_config::revision);
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
		msg = VGETTEXT("registering with password *** and "
			"email address $email", symbols);
	}
	print(_("nick registration"), msg);

	network::send_data(data, 0);
}

void chat_command_handler::do_drop() {
	config data;
	config& nickserv = data.add_child("nickserv");

	nickserv.add_child("drop");

	print(_("nick registration"), _("dropping your username"));

	network::send_data(data, 0);
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
	print(_("nick registration"), VGETTEXT("setting $var to $value", symbols));

	network::send_data(data, 0);
}

void chat_command_handler::do_info() {
	if (get_data(1).empty()) return command_failed_need_arg(1);

	config data;
	config& nickserv = data.add_child("nickserv");

	nickserv.add_child("info")["name"] = get_data(1);
	utils::string_map symbols;
	symbols["nick"] = get_arg(1);
	print(_("nick registration"), VGETTEXT("requesting information for user $nick", symbols));

	network::send_data(data, 0);
}

void chat_command_handler::do_details() {

	config data;
	config& nickserv = data.add_child("nickserv");
	nickserv.add_child("details");

	network::send_data(data, 0);
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
	network::send_data(cfg, 0);
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

	network::send_data(cfg, 0);
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
				vgettext("Unknown debug level: '$level'.", symbols);
		add_chat_message(time(NULL), _("error"), 0, msg);
		return;
	}
	if (!lg::set_log_domain_severity(domain, severity)) {
		utils::string_map symbols;
		symbols["domain"] = domain;
		const std::string& msg =
				vgettext("Unknown debug domain: '$domain'.", symbols);
		add_chat_message(time(NULL), _("error"), 0, msg);
		return;
	} else {
		utils::string_map symbols;
		symbols["level"] = level;
		symbols["domain"] = domain;
		const std::string& msg =
				vgettext("Switched domain: '$domain' to level: '$level'.", symbols);
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
	network::send_data(data, 0);
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
	network::send_data(data, 0);
}

void chat_handler::add_whisper_sent(const std::string& receiver, const std::string& message)
{
	utils::string_map symbols;
	symbols["receiver"] = receiver;
	add_chat_message(time(NULL), VGETTEXT("whisper to $receiver", symbols), 0, message);
}

void chat_handler::add_whisper_received(const std::string& sender, const std::string& message)
{
	utils::string_map symbols;
	symbols["sender"] = sender;
	add_chat_message(time(NULL), VGETTEXT("whisper: $sender", symbols), 0, message);
}

void chat_handler::send_chat_room_message(const std::string& room,
	const std::string& message)
{
	config cmsg, data;
	cmsg["room"] = room;
	cmsg["message"] = message;
	cmsg["sender"] = preferences::login();
	data.add_child("message", cmsg);
	network::send_data(data, 0);
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

