/* $Id: lobby_main.cpp 47582 2010-11-16 23:28:08Z ai0867 $ */
/*
   Copyright (C) 2009 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/lobby_main.hpp"
// #include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/timer.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/minimap.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/widgets/scrollbar_panel.hpp"

#include "formula_string_utils.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "lobby_preferences.hpp"
#include "log.hpp"
#include "network.hpp"
#include "playmp_controller.hpp"
#include "preferences_display.hpp"
#include "sound.hpp"
#include "multiplayer.hpp"

#include <time.h>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>

static lg::log_domain log_network("network");
#define DBG_NW LOG_STREAM(debug, log_network)
#define LOG_NW LOG_STREAM(info, log_network)
#define ERR_NW LOG_STREAM(err, log_network)

static lg::log_domain log_engine("engine");
#define LOG_NG LOG_STREAM(info, log_engine)
#define ERR_NG LOG_STREAM(err, log_engine)

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

static lg::log_domain log_lobby("lobby");
#define DBG_LB LOG_STREAM(debug, log_lobby)
#define LOG_LB LOG_STREAM(info, log_lobby)
#define ERR_LB LOG_STREAM(err, log_lobby)
#define SCOPE_LB log_scope2(log_lobby, __func__)

extern void regenerate_heros(hero_map& heros);

namespace gui2 {

REGISTER_DIALOG(lobby_main)

void tlobby_main::do_notify(t_notify_mode mode)
{
	if (preferences::lobby_sounds()) {
		switch (mode) {
			case NOTIFY_WHISPER:
			case NOTIFY_WHISPER_OTHER_WINDOW:
			case NOTIFY_OWN_NICK:
				sound::play_UI_sound(game_config::sounds::receive_message_highlight);
				break;
			case NOTIFY_FRIEND_MESSAGE:
				sound::play_UI_sound(game_config::sounds::receive_message_friend);
				break;
			case NOTIFY_SERVER_MESSAGE:
				sound::play_UI_sound(game_config::sounds::receive_message_server);
				break;
			case NOTIFY_LOBBY_QUIT:
				sound::play_UI_sound(game_config::sounds::user_leave);
				break;
			case NOTIFY_LOBBY_JOIN:
				sound::play_UI_sound(game_config::sounds::user_arrive);
				break;
			case NOTIFY_MESSAGE:
				break;
			default:
				break;
		}
	}
}

tlobby_main::tlobby_main(const config& game_config, lobby_info& info, display& disp, hero_map& heros, hero_map& heros_start)
	: tchat_(disp, group, MIN_PAGE, CHAT_PAGE, CHATING_PAGE)
	, legacy_result_(QUIT)
	, game_config_(game_config)
	, gamelistbox_(NULL)
	, window_(NULL)
	, lobby_info_(info)
	, open_windows_()
	, active_window_(0)
	, selected_game_id_()
	, gamelist_dirty_(false)
	, last_gamelist_update_(0)
	, gamelist_diff_update_(true)
	, disp_(disp)
	, heros_(heros)
	, heros_start_(heros_start)
	, gamelist_id_at_row_()
	, delay_gamelist_update_(false)
	, joined_game_id_(-1)
{
}

struct lobby_delay_gamelist_update_guard
{
	lobby_delay_gamelist_update_guard(tlobby_main& l) : l(l) {
		l.delay_gamelist_update_ = true;
	}
	~lobby_delay_gamelist_update_guard() {
		l.delay_gamelist_update_ = false;
	}
	tlobby_main& l;
};

tlobby_main::~tlobby_main()
{
}

void tlobby_main::post_build(CVideo& video, twindow& window)
{
}

namespace {

void add_label_data(std::map<std::string, string_map>& map,
					const std::string& key, const std::string& label)
{
	string_map item;
	item["label"] = label;
	map.insert(std::make_pair(key, item));
}

void add_tooltip_data(std::map<std::string, string_map>& map,
					const std::string& key, const std::string& label)
{
	string_map item;
	item["tooltip"] = label;
	map.insert(std::make_pair(key, item));
}

void modify_grid_with_data(tgrid* grid, const std::map<std::string, string_map>& map)
{
	typedef std::map<std::string, string_map> strstrmap;
	BOOST_FOREACH (const strstrmap::value_type v, map) {
		const std::string& key = v.first;
		const string_map& strmap = v.second;
		twidget* w = grid->find(key, false);
		if (w == NULL) continue;
		tcontrol* c = dynamic_cast<tcontrol*>(w);
		if (c == NULL) continue;
		BOOST_FOREACH (const string_map::value_type& vv, strmap) {
			if (vv.first == "label") {
				c->set_label(vv.second);
			} else if (vv.first == "tooltip") {
				c->set_tooltip(vv.second);
			}
		}
	}
}

void set_visible_if_exists(tgrid* grid, const char* id, bool visible)
{
	twidget* w = grid->find(id, false);
	if (w) {
		w->set_visible(visible ? twidget::VISIBLE : twidget::INVISIBLE);
	}
}

std::string colorize(const std::string& str, const std::string& color)
{
	return "<format>color='" + color +"' text='" + str + "'</format>";
}

std::string tag(const std::string& str, const std::string& tag)
{
	return str;
}

} //end anonymous namespace

void tlobby_main::update_gamelist()
{
	SCOPE_LB;
	gamelistbox_->clear();
	gamelist_id_at_row_.clear();
	lobby_info_.make_games_vector();
	int select_row = -1;
	for (unsigned i = 0; i < lobby_info_.games().size(); ++i) {
		const game_info& game = *lobby_info_.games()[i];
		if (game.id == selected_game_id_) {
			select_row = i;
		}
		gamelist_id_at_row_.push_back(game.id);
		LOG_LB << "Adding game to listbox (1)" << game.id << "\n";
		gamelistbox_->add_row(make_game_row_data(game));
		tgrid* grid = gamelistbox_->get_row_grid(gamelistbox_->get_item_count() - 1);
		adjust_game_row_contents(game, gamelistbox_->get_item_count() - 1, grid);
	}
	if (select_row >= 0 && select_row != gamelistbox_->get_selected_row()) {
		gamelistbox_->select_row(select_row);
	}
	update_selected_game();
	gamelist_dirty_ = false;
	last_gamelist_update_ = SDL_GetTicks();
	lobby_info_.sync_games_display_status();
	lobby_info_.apply_game_filter();
	update_gamelist_header();
	gamelistbox_->set_row_shown(lobby_info_.games_visibility());
	window_->invalidate_layout();
}

void tlobby_main::update_gamelist_diff()
{
	SCOPE_LB;
	lobby_info_.make_games_vector();
	int select_row = -1;
	unsigned list_i = 0;
	int list_rows_deleted = 0;
	std::vector<int> next_gamelist_id_at_row;
	for (unsigned i = 0; i < lobby_info_.games().size(); ++i) {
		const game_info& game = *lobby_info_.games()[i];
		if (game.display_status == game_info::NEW) {
			LOG_LB << "Adding game to listbox " << game.id << "\n";
			if (list_i != gamelistbox_->get_item_count()) {
				gamelistbox_->add_row(make_game_row_data(game), list_i);
				DBG_LB << "Added a game listbox row not at the end"
					<< list_i << " " << gamelistbox_->get_item_count() << "\n";
				list_rows_deleted--;
			} else {
				gamelistbox_->add_row(make_game_row_data(game));
			}
			tgrid* grid = gamelistbox_->get_row_grid(gamelistbox_->get_item_count() - 1);
			adjust_game_row_contents(game, gamelistbox_->get_item_count() - 1, grid);
			list_i++;
			next_gamelist_id_at_row.push_back(game.id);
		} else {
			if (list_i >= gamelistbox_->get_item_count()) {
				ERR_LB << "Ran out of listbox items -- triggering a full refresh\n";
				network::send_data(config("refresh_lobby"), 0);
				return;
			}
			if (list_i + list_rows_deleted >= gamelist_id_at_row_.size()) {
				ERR_LB << "gamelist_id_at_row_ overflow! "
					<< list_i << " + " << list_rows_deleted
					<< " >= " << gamelist_id_at_row_.size()
					<< " -- triggering a full refresh\n";
				network::send_data(config("refresh_lobby"), 0);
				return;
			}
			int listbox_game_id = gamelist_id_at_row_[list_i + list_rows_deleted];
			if (game.id != listbox_game_id) {
				ERR_LB << "Listbox game id does not match expected id "
					<< listbox_game_id << " " << game.id << " (row " << list_i << ")\n";
				network::send_data(config("refresh_lobby"), 0);
				return;
			}
			if (game.display_status == game_info::UPDATED) {
				LOG_LB << "Modyfying game in listbox " << game.id << " (row " << list_i << ")\n";
				tgrid* grid = gamelistbox_->get_row_grid(list_i);
				modify_grid_with_data(grid, make_game_row_data(game));
				adjust_game_row_contents(game, list_i, grid);
				++list_i;
				next_gamelist_id_at_row.push_back(game.id);
			} else if (game.display_status == game_info::DELETED) {
				LOG_LB << "Deleting game from listbox " << game.id << " (row " << list_i << ")\n";
				gamelistbox_->remove_row(list_i);
				++list_rows_deleted;
			} else {
				//clean
				LOG_LB << "Clean game in listbox " << game.id << " (row " << list_i << ")\n";
				next_gamelist_id_at_row.push_back(game.id);
				++list_i;
			}
		}
	}
	for (unsigned i = 0; i < next_gamelist_id_at_row.size(); ++i) {
		if (next_gamelist_id_at_row[i] == selected_game_id_) {
			select_row = i;
		}
	}
	next_gamelist_id_at_row.swap(gamelist_id_at_row_);
	if (select_row >= static_cast<int>(gamelistbox_->get_item_count())) {
		ERR_LB << "Would select a row beyond the listbox"
			<< select_row << " " << gamelistbox_->get_item_count() << "\n";
		select_row = gamelistbox_->get_item_count() - 1;
	}
	if (select_row >= 0 && select_row != gamelistbox_->get_selected_row()) {
		gamelistbox_->select_row(select_row);
	}
	update_selected_game();
	gamelist_dirty_ = false;
	last_gamelist_update_ = SDL_GetTicks();
	lobby_info_.sync_games_display_status();
	lobby_info_.apply_game_filter();
	update_gamelist_header();
	gamelistbox_->set_row_shown(lobby_info_.games_visibility());
	window_->invalidate_layout();
}

void tlobby_main::update_gamelist_header()
{
	utils::string_map symbols;
	symbols["num_shown"] = lexical_cast<std::string>(lobby_info_.games_filtered().size());
	symbols["num_total"] = lexical_cast<std::string>(lobby_info_.games().size());
	std::string games_string = VGETTEXT("Games: showing $num_shown out of $num_total", symbols);
#ifndef GUI2_EXPERIMENTAL_LISTBOX
	find_widget<tlabel>(gamelistbox_, "map", false).set_label(games_string);
#endif
}

std::map<std::string, string_map> tlobby_main::make_game_row_data(const game_info& game)
{
	std::map<std::string, string_map> data;

	const char* color_string;
	if (game.vacant_slots > 0) {
		if (game.reloaded || game.started) {
			color_string = "yellow"; // yellow
		} else {
			color_string = "green"; // green"
		}
	} else {
		if (game.observers) {
			color_string = "white"; // #ddd
		} else {
			color_string = "red"; // red
		}
	}
	if (!game.have_era && (game.vacant_slots > 0 || game.observers)) {
		color_string = "black"; // #444
	}

	add_label_data(data, "status", colorize(game.status, color_string));
	add_label_data(data, "name", colorize(game.name, color_string));

	add_label_data(data, "era", game.era);
	add_label_data(data, "era_short", game.era_short);
	add_label_data(data, "map_info", game.map_info);
	add_label_data(data, "scenario", game.scenario);
	add_label_data(data, "map_size_text", game.map_size_info);
	add_label_data(data, "time_limit", game.time_limit);
	add_label_data(data, "gold_text", game.gold);
	add_label_data(data, "xp_text", game.xp);
	add_label_data(data, "vision_text", game.vision);
	add_label_data(data, "time_limit_text", game.time_limit);
	if (game.observers) {
		add_label_data(data, "observer_icon", "misc/eye.png");
		add_tooltip_data(data, "observer_icon", _("Observers allowed"));
	} else {
		add_label_data(data, "observer_icon", "misc/no_observer.png");
		add_tooltip_data(data, "observer_icon", _("Observers not allowed"));
	}

	const char* vision_icon;
	if (game.fog) {
		if (game.shroud) {
			vision_icon = "misc/vision-fog-shroud.png";
		} else {
			vision_icon = "misc/vision-fog.png";
		}
	} else {
		if (game.shroud) {
			vision_icon = "misc/vision-shroud.png";
		} else {
			vision_icon = "misc/vision-none.png";
		}
	}
	add_label_data(data, "vision_icon", vision_icon);
	add_tooltip_data(data, "vision_icon", game.vision);
	return data;
}

void tlobby_main::adjust_game_row_contents(const game_info& game, int idx, tgrid* grid)
{
	ttoggle_panel& row_panel =
			find_widget<ttoggle_panel>(grid, "panel", false);

	row_panel.set_callback_mouse_left_double_click(boost::bind(
		&tlobby_main::join_or_observe, this, idx));

	set_visible_if_exists(grid, "time_limit_icon", !game.time_limit.empty());
	set_visible_if_exists(grid, "vision_fog", game.fog);
	set_visible_if_exists(grid, "vision_shroud", game.shroud);
	set_visible_if_exists(grid, "vision_none", !(game.fog || game.shroud));
	set_visible_if_exists(grid, "observers_yes", game.observers);
	set_visible_if_exists(grid, "shuffle_sides_icon", game.shuffle_sides);
	set_visible_if_exists(grid, "observers_no", !game.observers);
	set_visible_if_exists(grid, "needs_password", game.password_required);
	set_visible_if_exists(grid, "reloaded", game.reloaded);
	set_visible_if_exists(grid, "started", game.started);
	set_visible_if_exists(grid, "no_era", !game.have_era);


	tbutton* join_button = dynamic_cast<tbutton*>(grid->find("join", false));
	if (join_button) {
		connect_signal_mouse_left_click(*join_button, boost::bind(
				  &tlobby_main::join_button_callback
				, this
				, boost::ref(*window_)));
		join_button->set_active(game.can_join());
	}
	tbutton* observe_button = dynamic_cast<tbutton*>(grid->find("observe", false));
	if (observe_button) {
		connect_signal_mouse_left_click(*observe_button, boost::bind(
				  &tlobby_main::observe_button_callback
				, this
				, boost::ref(*window_)));
		observe_button->set_active(game.can_observe());
		observe_button->set_active(false);
	}
	tminimap* minimap = dynamic_cast<tminimap*>(grid->find("minimap", false));
	if (minimap) {
		minimap->set_config(&game_config_);
		minimap->set_map_data(tminimap::TILE_MAP, game.map_data);
	}
}

void tlobby_main::update_gamelist_filter()
{
	DBG_LB << "tlobby_main::update_gamelist_filter\n";
	lobby_info_.apply_game_filter();
	DBG_LB << "Games in lobby_info: " << lobby_info_.games().size()
		<< ", games in listbox: " << gamelistbox_->get_item_count() << "\n";
	assert(lobby_info_.games().size() == gamelistbox_->get_item_count());
	gamelistbox_->set_row_shown(lobby_info_.games_visibility());
}

void tlobby_main::update_selected_game()
{
	int idx = gamelistbox_->get_selected_row();
	bool can_join = false, can_observe = false;
	if (idx >= 0) {
		const game_info& game = *lobby_info_.games()[idx];
		can_observe = game.can_observe();
		can_join = game.can_join();
		selected_game_id_ = game.id;
	} else {
		selected_game_id_ = 0;
	}
}

void tlobby_main::update_network_status(twindow& window, bool connected)
{
	std::stringstream strstr;
	strstr << dgettext("wesnoth-lib", "Chat");
	if (!connected) {
		strstr << tintegrate::generate_img("misc/network-disconnected.png");
	} else {
		strstr << tintegrate::generate_img("misc/network-connected.png");
	}
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_label(strstr.str());
	if (current_page_ == LOBBY_PAGE) {
		find_widget<tbutton>(&window, "create", false).set_active(connected);
		find_widget<tbutton>(&window, "refresh", false).set_active(connected);
	}
}

void tlobby_main::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_escape_disabled(true);
	window_ = &window;

	// further window(cancel mp_create_side etc) will back to it.
	// it is necessary to validate group.
	regenerate_heros(heros_);
	heros_start_ = heros_;

	sheet_.insert(std::make_pair((int)LOBBY_PAGE, find_widget<ttoggle_button>(&window, "lobby", false, true)));
	sheet_.insert(std::make_pair((int)CHAT_PAGE, find_widget<ttoggle_button>(&window, "chat", false, true)));
	for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
		it->second->set_callback_state_change(boost::bind(&tlobby_main::sheet_toggled, this, _1));
		it->second->set_data(it->first);
	}

	page_panel_ = find_widget<tscrollbar_panel>(&window, "page", false, true);
	swap_page(window, LOBBY_PAGE, false);
	sheet_.begin()->second->set_value(true);

	update_network_status(window, lobby.ready());
}

void tlobby_main::post_show(twindow& /*window*/)
{
	window_ = NULL;
}

void tlobby_main::keyboard_shown(twindow& window)
{
	tchat_::keyboard_shown(window);
	find_widget<ttoggle_button>(&window, "lobby", false, true)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::INVISIBLE);
}

void tlobby_main::keyboard_hidden(twindow& window)
{
	tchat_::keyboard_hidden(window);
	find_widget<ttoggle_button>(&window, "lobby", false, true)->set_visible(twidget::VISIBLE);
	find_widget<ttoggle_button>(&window, "chat", false, true)->set_visible(twidget::VISIBLE);
}

void tlobby_main::fill_lobby(twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	std::stringstream str;
	str << label->label() << "-" << preferences::login();
	str << "(" << tintegrate::generate_format(lobby.host(), "green") << ")";
	label->set_label(str.str());

	SCOPE_LB;
	gamelistbox_ =  find_widget<tlistbox>(&window, "game_list", false, true);
#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(*gamelistbox_, boost::bind(
				  &tlobby_main::gamelist_change_callback
				, *this
				, boost::ref(window)));
#else
	gamelistbox_->set_callback_value_change(
			dialog_callback<tlobby_main
				, &tlobby_main::gamelist_change_callback>);
#endif

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "create", false)
			, boost::bind(
				  &tlobby_main::create_button_callback
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "refresh", false)
			, boost::bind(
				  &tlobby_main::refresh_button_callback
				, this
				, boost::ref(window)));

	ttoggle_button& skip_replay =
			find_widget<ttoggle_button>(&window, "skip_replay", false);
	skip_replay.set_value(preferences::skip_mp_replay());
	skip_replay.set_callback_state_change(boost::bind(&tlobby_main::skip_replay_changed_callback, this, _1));
	skip_replay.set_visible(twidget::INVISIBLE);

	room_window_open("lobby", true);
	game_filter_reload();

	join();
	// Force first update to be directly.
	refresh_button_callback(window);
}

void tlobby_main::close_lobby(twindow& window)
{
}

void tlobby_main::fill_chat(twindow& window)
{
	tchat_::pre_show(window);
	refresh_button_callback(window);
}

void tlobby_main::close_chat(twindow& window)
{
	tchat_::post_show(window);
}

room_info* tlobby_main::active_window_room()
{
	const tlobby_chat_window& t = open_windows_[active_window_];
	if (t.whisper) return NULL;
	return lobby_info_.get_room(t.name);
}

tlobby_chat_window* tlobby_main::room_window_open(const std::string& room, bool open_new)
{
	return search_create_window(room, false, open_new);
}

tlobby_chat_window* tlobby_main::whisper_window_open(const std::string& name, bool open_new)
{
	return search_create_window(name, true, open_new);
}

tlobby_chat_window* tlobby_main::search_create_window(const std::string& name, bool whisper, bool open_new)
{
	BOOST_FOREACH (tlobby_chat_window& t, open_windows_) {
		if (t.name == name && t.whisper == whisper) return &t;
	}
	if (open_new) {
		open_windows_.push_back(tlobby_chat_window(name, whisper));
		std::map<std::string, string_map> data;
		utils::string_map symbols;
		symbols["name"] = name;
		if (whisper) {
		} else {
			lobby_info_.open_room(name);
		}
		std::map<std::string, string_map> data2;
		add_label_data(data2, "room", whisper ? "<" + name + ">" : name);

		return &open_windows_.back();
	}
	return NULL;
}

bool tlobby_main::handle(tlobby::ttype type, const config& data)
{
	if (type == tlobby::t_connected || type == tlobby::t_disconnected) {
		update_network_status(*window_, type == tlobby::t_connected);
		process_network_status(type == tlobby::t_connected);
	}

	if (current_page_ == LOBBY_PAGE && gamelist_dirty_ && !delay_gamelist_update_
		&&(SDL_GetTicks() - last_gamelist_update_ > game_config::lobby_refresh)) {
		if (gamelist_diff_update_) {
			update_gamelist_diff();
		} else {
			update_gamelist();
			gamelist_diff_update_ = true;
		}
	}

	if (type != tlobby::t_data) {
		return false;
	}

	bool halt = true;
	if (const config &c = data.child("error")) {
		throw network::error(c["message"]);
	} else if (const config &c = data.child("message")) {
		process_message(c);
	} else if (const config &c = data.child("whisper")) {
		process_message(c, true);
	} else if(data.child("gamelist")) {
		process_gamelist(data);
	} else if (const config &c = data.child("gamelist_diff")) {
		process_gamelist_diff(c);
	} else if (const config &c = data.child("room_join")) {
		process_room_join(c);
	} else if (const config &c = data.child("room_part")) {
		process_room_part(c);
	} else if (const config &c = data.child("room_query_response")) {
		process_room_query_response(c);
	} else {
		halt = false;
	}
	return halt;
}

void tlobby_main::process_gamelist(const config &data)
{
	lobby_info_.process_gamelist(data);
	DBG_LB << "Received gamelist\n";
	gamelist_dirty_ = true;
	gamelist_diff_update_ = false;

	process_userlist(lobby_info_.gamelist());
}

void tlobby_main::process_gamelist_diff(const config &data)
{
	if (lobby_info_.process_gamelist_diff(data)) {
		DBG_LB << "Received gamelist diff\n";
		gamelist_dirty_ = true;
	} else {
		ERR_LB << "process_gamelist_diff failed!\n";
	}
	int joined = data.child_count("insert_child");
	int left = data.child_count("remove_child");
	if (joined > 0 || left > 0) {
		if (left > joined) {
			do_notify(NOTIFY_LOBBY_QUIT);
		} else {
			do_notify(NOTIFY_LOBBY_JOIN);
		}
	}

	process_userlist(lobby_info_.gamelist());
}

void tlobby_main::process_room_join(const config &data)
{
	const std::string& room = data["room"];
	const std::string& player = data["player"];
	room_info* r = lobby_info_.get_room(room);
	DBG_LB << "room join: " << room << " " << player << " " << (void*)r << "\n";

	if (r) {
		if (player == preferences::login()) {
			if (const config& members = data.child("members")) {
				r->process_room_members(members);
			}
		} else {
			r->add_member(player);
		}
		
	} else {
		if (player == preferences::login()) {
			tlobby_chat_window* t = room_window_open(room, true);
			lobby_info_.open_room(room);
			r = lobby_info_.get_room(room);
			assert(r);
			if (const config& members = data.child("members")) {
				r->process_room_members(members);
			}
		} else {
			LOG_LB << "Discarding join info for a room the player is not in\n";
		}
	}
}

void tlobby_main::process_room_part(const config &data)
{
	//todo close room window when the part message is sent
	const std::string& room = data["room"];
	const std::string& player = data["player"];
	DBG_LB << "Room part: " << room << " " << player << "\n";
	room_info* r = lobby_info_.get_room(room);
	if (r) {
		r->remove_member(player);
        
	} else {
		LOG_LB << "Discarding part info for a room the player is not in\n";
	}
}

void tlobby_main::process_room_query_response(const config& data)
{
	const std::string& room = data["room"];
	const std::string& message = data["message"];
	DBG_LB << "room query response: " << room << " " << message << "\n";
	if (room.empty()) {
		if (const config& rooms = data.child("rooms")) {
			//TODO: this should really open a nice join room dialog instead
			std::stringstream ss;
			ss << "Rooms:";
			BOOST_FOREACH (const config& r, rooms.child_range("room")) {
				ss << " " << r["name"];
			}
		}
	} else {
		if (room_window_open(room, false)) {
			if (const config& members = data.child("members")) {
				room_info* r = lobby_info_.get_room(room);
				assert(r);
				r->process_room_members(members);
			}
		}
	}
}

void tlobby_main::join_button_callback(gui2::twindow &window)
{
	LOG_LB << "join_button_callback\n";
	join_global_button_callback(window);
}

void tlobby_main::observe_button_callback(gui2::twindow &window)
{
	LOG_LB << "observe_button_callback\n";
	observe_global_button_callback(window);
}

void tlobby_main::observe_global_button_callback(gui2::twindow &window)
{
	LOG_LB << "observe_global_button_callback\n";
	if (do_game_join(gamelistbox_->get_selected_row(), true)) {
		legacy_result_ = OBSERVE;
		window.close();
	}
}

void tlobby_main::join_global_button_callback(gui2::twindow &window)
{
	LOG_LB << "join_global_button_callback\n";
	if (do_game_join(gamelistbox_->get_selected_row(), false)) {
		legacy_result_ = JOIN;
		window.close();
	}
}

void tlobby_main::join_or_observe(int idx)
{
	const game_info& game = *lobby_info_.games()[idx];
	if (do_game_join(idx, !game.can_join())) {
		legacy_result_ = game.can_join() ? JOIN : OBSERVE;
		window_->close();
	}
}

bool tlobby_main::do_game_join(int idx, bool observe)
{
	if (idx < 0 || idx > static_cast<int>(lobby_info_.games().size())) {
		ERR_LB << "Requested join/observe of a game with index out of range: "
			<< idx << ", games size is " << lobby_info_.games().size() << "\n";
		return false;
	}
	const game_info& game = *lobby_info_.games()[idx];
	if (observe) {
		if (!game.can_observe()) {
			ERR_LB << "Requested observe of a game with observers disabled\n";
			return false;
		}
	} else {
		if (!game.can_join()) {
			ERR_LB << "Requested join to a game with no vacant slots\n";
			return false;
		}
	}
	config response;
	config& join = response.add_child("join");
	join["id"] = lexical_cast<std::string>(game.id);
	join["observe"] = observe;
	if (join && !observe && game.password_required) {
		std::string password = game_config::checksum;
		if (!password.empty()) {
			join["password"] = password;
		}
	}
	// this command should send by wait.
	// network::send_data(response, 0);
	if (observe && game.started) {
		playmp_controller::set_replay_last_turn(game.current_turn);
	}
	joined_game_id_ = game.id;
	return true;
}

void tlobby_main::create_button_callback(gui2::twindow& window)
{
	legacy_result_ = CREATE;
	window.close();
}

void tlobby_main::refresh_button_callback(gui2::twindow& /*window*/)
{
	network::send_data(config("refresh_lobby"), 0);
}

void tlobby_main::game_filter_reload()
{
	lobby_info_.clear_game_filter();
}

void tlobby_main::gamelist_change_callback(gui2::twindow &/*window*/)
{
	update_selected_game();
}

void tlobby_main::skip_replay_changed_callback(twidget* w)
{
	ttoggle_button* tb = dynamic_cast<ttoggle_button*>(w);
	assert(tb);
	preferences::set_skip_mp_replay(tb->get_value());
}

void tlobby_main::sheet_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_page = toggle->get_data();

	if (!toggle->get_value()) {
		// At most select one page. recheck it!
		toggle->set_value(true);
	} else {
		for (std::map<int, ttoggle_button*>::iterator it = sheet_.begin(); it != sheet_.end(); ++ it) {
			if (it->second == toggle) {
				continue;
			}
			it->second->set_value(false);
		}
		swap_page(*toggle->get_window(), toggled_page, true);
	}
}

void tlobby_main::desire_swap_page(twindow& window, int page, bool open)
{
	if (open) {
		if (page == LOBBY_PAGE) {
			fill_lobby(window);

		}

	} else {
		if (page == LOBBY_PAGE) {
			close_lobby(window);

		}
	}
}

} // namespace gui2
