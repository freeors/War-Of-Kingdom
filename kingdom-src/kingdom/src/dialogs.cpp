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
#include "foreach.hpp"
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
#include "widgets/progressbar.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/game_save.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/dialogs/select_unit.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"

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

void advance_unit(const map_location &loc, bool random_choice, bool choose_from_random)
{
	game_display& gui = *resources::screen;

	unit_map::iterator u = resources::units->find(loc);
	if (u == resources::units->end() || u->advances() == false)
		return;

	const std::vector<std::string>& options = u->advances_to();

	//
	std::string id = u->id();
	std::string name = u->name();
	int exp = u->experience();
	int cityno = u->cityno();
	int side = u->side();
	std::stringstream id2;
	for (std::vector<std::string>::const_iterator it = options.begin(); it != options.end(); ++ it) {
		if (it != options.end()) {
			id2 << *it; 
		} else {
			id2 << ", "<< *it; 
		}
	}
	//

	std::vector<std::string> lang_options;

	std::vector<unit> sample_units;
	unit* sample_unit;
	for(std::vector<std::string>::const_iterator op = options.begin(); op != options.end(); ++op) {
		sample_unit = ::get_advanced_unit(&*u, *op);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		const unit& type = sample_units.back();

		lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u->image_mods() + COLUMN_SEPARATOR + type.type_name());
		preferences::encountered_units().insert(*op);
	}

	bool always_display = false;
	foreach (const config &mod, u->get_modification_advances())
	{
		if (utils::string_bool(mod["always_display"])) always_display = true;
		std::string to;
		if (u->packed()) {
			to = u->packee_type_id();
		} else {
			to = u->type_id();
		}
		sample_unit = ::get_advanced_unit(&*u, to);
		sample_units.push_back(*sample_unit);
		delete sample_unit;
		sample_units.back().add_modification(mod);
		const unit& type = sample_units.back();
		if (!mod["image"].empty()) {
			lang_options.push_back(IMAGE_PREFIX + mod["image"].str() + COLUMN_SEPARATOR + mod["description"].str());
		} else {
			lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u->image_mods() + COLUMN_SEPARATOR + mod["description"].str());
		}
	}

	if (lang_options.empty()) {
		return;
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
				if (ut->character() != -1) {
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

	animate_unit_advancement(loc, size_t(res));

	// loc maybe is loc_ of advanced unit, so is invalid after advance,
	// don't call relatve loc_, else use copy of loc.
}

bool animate_unit_advancement(const map_location &loc, size_t choice)
{
	const events::command_disabler cmd_disabler;

	unit_map::iterator u = resources::units->find(loc);
	if (u == resources::units->end() || u->advances() == false) {
		return false;
	}

	const std::vector<std::string>& options = u->advances_to();
	std::vector<config> mod_options = u->get_modification_advances();

	if(choice >= options.size() + mod_options.size()) {
		return false;
	}

	// When the unit advances, it fades to white, and then switches
	// to the new unit, then fades back to the normal colour

	game_display* disp = resources::screen;
	rect_of_hexes& draw_area = disp->draw_area();
	bool force_scroll = (!unit_display::player_number_ || ((unit_display::player_number_ > 0) && ((*resources::teams)[unit_display::player_number_ - 1].is_human() || preferences::scroll_to_action())))? true: false;
	bool animate = force_scroll || point_in_rect_of_hexes(u->get_location().x, u->get_location().y, draw_area);
	if (!resources::screen->video().update_locked() && animate) {
		unit_animator animator;
		bool with_bars = true;
		animator.add_animation(&*u,"levelout", u->get_location(), map_location(), 0, with_bars);
		animator.start_animations();
		animator.wait_for_end();
	}

	if(choice < options.size()) {
		// chosen_unit is not a reference, since the unit may disappear at any moment.
		std::string chosen_unit = options[choice];
		::advance_unit(loc, chosen_unit);
	} else {
		unit* amla_unit = &*u;
		const config &mod_option = mod_options[choice - options.size()];
		
		game_events::fire("advance",loc);

		amla_unit->get_experience(increase_xp::attack_ublock(*amla_unit), -amla_unit->max_experience()); // subtract xp required
		// ALMA may want to change status, but add_modification in modify_according_to_hero cannot change state,
		// so it need call amla_unit->add_modification instead of amla_unit->modify_according_to_hero.
		amla_unit->add_modification(mod_option);

		game_events::fire("post_advance",loc);
	}

	u = resources::units->find(loc);
	resources::screen->invalidate_unit();

	if (u != resources::units->end() && !resources::screen->video().update_locked()) {
		if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
			unit_animator animator;
			animator.add_animation(&*u,"levelin",u->get_location(), map_location(), 0, true);
			animator.start_animations();
			animator.wait_for_end();
			animator.set_all_standing();
		
			resources::screen->invalidate(loc);
			resources::screen->draw();
		}
		events::pump();
	}

	resources::screen->invalidate_all();
	if (force_scroll || point_in_rect_of_hexes(loc.x, loc.y, draw_area)) {
		resources::screen->draw();
	}

	return true;
}

void show_objectives(const config &level, const std::string &objectives)
{
	static const std::string no_objectives(_("No objectives available"));
	gui2::show_transient_message(resources::screen->video(), level["name"],
		(objectives.empty() ? no_objectives : objectives), "", true);
}

void show_unit_description(const unit &u)
{
	const unit_type* t = u.type();
	if (t != NULL)
		show_unit_description(*t);
	else
		// can't find type, try open the id page to have feedback and unit error page
	  help::show_unit_help(*resources::screen, u.type_id());
}

void show_unit_description(const unit_type &t)
{
	help::show_unit_help(*resources::screen, t.id(), t.hide_help());
}

static network::connection network_data_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num, network::statistics (*get_stats)(network::connection handle))
{
	const size_t width = 300;
	const size_t height = 80;
	const size_t border = 20;

	const int left = disp.w()/2 - width/2;
	const int top  = disp.h()/2 - height/2;

	const events::event_context dialog_events_context;

	gui::button cancel_button(disp.video(),_("Cancel"));
	std::vector<gui::button*> buttons_ptr(1,&cancel_button);

	gui::dialog_frame frame(disp.video(), msg, gui::dialog_frame::default_style, true, &buttons_ptr);
	SDL_Rect centered_layout = frame.layout(left,top,width,height).interior;
	centered_layout.x = disp.w() / 2 - centered_layout.w / 2;
	centered_layout.y = disp.h() / 2 - centered_layout.h / 2;
	// HACK: otherwise we get an empty useless space in the dialog below the progressbar
	centered_layout.h = height;
	frame.layout(centered_layout);
	frame.draw();

	const SDL_Rect progress_rect = {centered_layout.x+border,centered_layout.y+border,centered_layout.w-border*2,centered_layout.h-border*2};
	gui::progress_bar progress(disp.video());
	progress.set_location(progress_rect);

	events::raise_draw_event();
	disp.flip();

	network::statistics old_stats = get_stats(connection_num);

	cfg.clear();
	for(;;) {
		const network::connection res = network::receive_data(cfg,connection_num,100);
		const network::statistics stats = get_stats(connection_num);
		if(stats.current_max != 0 && stats != old_stats) {
			old_stats = stats;
			progress.set_progress_percent((stats.current*100)/stats.current_max);
			std::ostringstream stream;
			stream << stats.current/1024 << "/" << stats.current_max/1024 << _("KB");
			progress.set_text(stream.str());
		}

		events::raise_draw_event();
		disp.flip();
		events::pump();

		if(res != 0) {
			return res;
		}


		if(cancel_button.pressed()) {
			return res;
		}
	}
}

network::connection network_send_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num)
{
	return network_data_dialog(disp, msg, cfg, connection_num,
							   network::get_send_stats);
}

network::connection network_receive_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num)
{
	return network_data_dialog(disp, msg, cfg, connection_num,
							   network::get_receive_stats);
}

} // end namespace dialogs

namespace {

class connect_waiter : public threading::waiter
{
public:
	connect_waiter(display& disp, gui::button& button) : disp_(disp), button_(button)
	{}
	ACTION process();

private:
	display& disp_;
	gui::button& button_;
};

connect_waiter::ACTION connect_waiter::process()
{
	events::raise_draw_event();
	disp_.flip();
	events::pump();
	if(button_.pressed()) {
		return ABORT;
	} else {
		return WAIT;
	}
}

}

namespace dialogs
{

network::connection network_connect_dialog(display& disp, const std::string& msg, const std::string& hostname, int port)
{
	const size_t width = 250;
	const size_t height = 20;
	const int left = disp.w()/2 - width/2;
	const int top  = disp.h()/2 - height/2;

	const events::event_context dialog_events_context;

	gui::button cancel_button(disp.video(),_("Cancel"));
	std::vector<gui::button*> buttons_ptr(1,&cancel_button);

	gui::dialog_frame frame(disp.video(), msg, gui::dialog_frame::default_style, true, &buttons_ptr);
	frame.layout(left,top,width,height);
	frame.draw();

	events::raise_draw_event();
	disp.flip();

	connect_waiter waiter(disp,cancel_button);
	return network::connect(hostname,port,waiter);
}

} // end namespace dialogs
