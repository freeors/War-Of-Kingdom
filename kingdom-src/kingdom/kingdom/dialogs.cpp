/* $Id: dialogs.cpp 41192 2010-02-13 17:54:41Z silene $ */
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
 * @file dialogs.cpp
 * Various dialogs: advance_unit, show_objectives, save+load game, network::connection.
 */

#include "global.hpp"

#include "dialogs.hpp"
#include "game_events.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "help.hpp"
#include "language.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "mouse_handler_base.hpp"
#include "minimap.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "savegame.hpp"
#include "thread.hpp"
#include "wml_separators.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/game_save.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "integrate.hpp"
#include "play_controller.hpp"

#include <boost/foreach.hpp>
#include <clocale>

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_display("display");
#define LOG_DP LOG_STREAM(info, log_display)

#define ERR_G  LOG_STREAM(err, lg::general)

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

namespace dialogs
{

bool advance_unit(unit& u, bool random_choice, bool choose_from_random)
{
	game_display& gui = *resources::screen;

	if (u.advances() == false) {
		return false;
	}

	const std::vector<std::string>& options = u.advances_to();
	std::vector<std::string> lang_options;

	std::vector<unit> sample_units;
	unit* sample_unit;
	for(std::vector<std::string>::const_iterator op = options.begin(); op != options.end(); ++op) {
		sample_unit = ::get_advanced_unit(&u, *op);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		const unit& type = sample_units.back();

		lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u.image_mods() + COLUMN_SEPARATOR + type.type_name());
		preferences::encountered_units().insert(*op);
	}

	bool always_display = false;
	BOOST_FOREACH (const config &mod, u.get_modification_advances())
	{
		if (utils::string_bool(mod["always_display"])) always_display = true;
		std::string to;
		if (u.packed()) {
			to = u.packee_type_id();
		} else {
			to = u.type_id();
		}
		sample_unit = ::get_advanced_unit(&u, to);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		sample_units.back().add_modification(mod);
		const unit& type = sample_units.back();
		if (!mod["image"].empty()) {
			lang_options.push_back(IMAGE_PREFIX + mod["image"].str() + COLUMN_SEPARATOR + mod["description"].str());
		} else {
			lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u.image_mods() + COLUMN_SEPARATOR + mod["description"].str());
		}
	}

	if (lang_options.empty()) {
		return false;
	}

	int res = 0;

	const config* ran_results = NULL;
	if (choose_from_random) {
		get_random();
		ran_results = get_random_results();
	}

	if (ran_results) {
		res = (*ran_results)["choose"].to_int();

	} else if (random_choice) {
		if (lang_options.size() > 1) {
			// select character if existed.
			size_t index = 0;
			for (std::vector<std::string>::const_iterator it = options.begin(); it != options.end(); ++ it, index ++) {
				const unit_type* ut = unit_types.find(*it);
				if (ut->especial() != -1) {
					break;
				}
			}
			if (index == sample_units.size()) {
				res = rand()%lang_options.size();
			} else {
				res = index;
			}
		} else {
			res = 0;
		}
	} else if (lang_options.size() > 1 || always_display) {

		std::vector<const unit*> sample_units_ptr;
		for (std::vector<unit>::const_iterator iter = sample_units.begin(); iter != sample_units.end(); ++ iter) {
			sample_units_ptr.push_back(&*iter);
		}
		gui2::tselect_unit dlg(gui, *resources::teams, *resources::units, *resources::heros, sample_units_ptr, gui2::tselect_unit::ADVANCE);
		try {
			dlg.show(gui.video());
		} catch(twml_exception& e) {
			e.show(gui);
		}
		res = dlg.troop_index();
	}

	if (choose_from_random) {
		if (!ran_results) {
			config cfg;
			cfg["choose"] = res;
			set_random_results(cfg);
		}
	} else {
		recorder.choose_option(res);
	}

	return animate_unit_advancement(u, size_t(res));

	// loc maybe is loc_ of advanced unit, so is invalid after advance,
	// don't call relatve loc_, else use copy of loc.
}

bool animate_unit_advancement(unit& u, size_t choice)
{
	const events::command_disabler cmd_disabler;

	if (u.advances() == false) {
		return false;
	}

	const std::vector<std::string>& options = u.advances_to();
	std::vector<config> mod_options = u.get_modification_advances();

	if (choice >= options.size() + mod_options.size()) {
		return false;
	}

	// When the unit advances, it fades to white, and then switches
	// to the new unit, then fades back to the normal colour

	game_display* disp = resources::screen;
	rect_of_hexes& draw_area = disp->draw_area();
	bool force_scroll = preferences::scroll_to_action();
	bool animate = force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area);
	if (!resources::screen->video().update_locked() && animate) {
		unit_animator animator;
		bool with_bars = true;
		animator.add_animation(&u, "levelout", u.get_location(), map_location(), 0, with_bars);
		animator.start_animations();
		animator.wait_for_end(*resources::controller);
	}

	if(choice < options.size()) {
		// chosen_unit is not a reference, since the unit may disappear at any moment.
		std::string chosen_unit = options[choice];
		::advance_unit(u, chosen_unit);
	} else {
		unit* amla_unit = &u;
		const config &mod_option = mod_options[choice - options.size()];
		
		game_events::fire("advance", u.get_location());

		amla_unit->get_experience(increase_xp::attack_ublock(*amla_unit), -amla_unit->max_experience()); // subtract xp required
		// ALMA may want to change status, but add_modification in modify_according_to_hero cannot change state,
		// so it need call amla_unit->add_modification instead of amla_unit->modify_according_to_hero.
		amla_unit->add_modification(mod_option);

		game_events::fire("post_advance", u.get_location());
	}

	resources::screen->invalidate_unit();

	if (!resources::screen->video().update_locked()) {
		if (force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area)) {
			unit_animator animator;
			animator.add_animation(&u, "levelin", u.get_location(), map_location(), 0, true);
			animator.start_animations();
			animator.wait_for_end(*resources::controller);
			animator.set_all_standing();
		
			resources::screen->invalidate(u.get_location());
			resources::screen->draw();
		}
		events::pump();
	}

	resources::screen->invalidate_all();
	if (force_scroll || point_in_rect_of_hexes(u.get_location().x, u.get_location().y, draw_area)) {
		resources::screen->draw();
	}

	return true;
}

void show_objectives(const config &level, const team& t)
{
	static const std::string no_objectives(_("No objectives available"));
	const std::string& objectives = t.objectives();

	gui2::show_transient_message(resources::screen->video(), level["name"],	
		t.form_results_of_battle_tip(objectives.empty() ? no_objectives : objectives), "", true);
}

} // end namespace dialogs
