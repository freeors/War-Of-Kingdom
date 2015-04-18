/* $Id: preferences.cpp 47642 2010-11-21 13:58:27Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
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
 *  Get and set user-preferences.
 */

#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "config.hpp"
#include "filesystem.hpp"
#include "gui/widgets/settings.hpp"
#include "hotkeys.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "display.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "lobby.hpp"

#include <sys/stat.h> // for setting the permissions of the preferences file

static lg::log_domain log_filesystem("filesystem");
#define ERR_FS LOG_STREAM(err, log_filesystem)

namespace {

bool color_cursors = false;

bool no_preferences_save = false;

bool fps = false;

int draw_delay_ = 20;

config prefs;
}

namespace preferences {

std::string public_account = "kingdom";
std::string public_password = "kingdom";

base_manager::base_manager()
{
	scoped_istream stream = istream_file(get_prefs_file(), true);
	read(prefs, *stream);

	if (member().empty()) {
		std::stringstream strstr;
		std::map<int, int> member;
		member.insert(std::make_pair(172, 5));
		member.insert(std::make_pair(176, 6));
		member.insert(std::make_pair(244, 7));
		member.insert(std::make_pair(279, 8));

		member.insert(std::make_pair(103, 8));
		member.insert(std::make_pair(106, 9));
		member.insert(std::make_pair(120, 9));
		member.insert(std::make_pair(381, 9));
		for (std::map<int ,int>::const_iterator it = member.begin(); it != member.end(); ++ it) {
			if (it != member.begin()) {
				strstr << ", ";
			}
			strstr << ((it->second << 16) | it->first);
		}
		set_member(strstr.str());
	}
}

base_manager::~base_manager()
{
	if (no_preferences_save) return;

	// Set the 'hidden' preferences.
	prefs["scroll_threshold"] = mouse_scroll_threshold();

	write_preferences();
}

void write_preferences()
{
#ifndef _WIN32
    bool prefs_file_existed = access(get_prefs_file().c_str(), F_OK) == 0;
#endif

	try {
		scoped_ostream prefs_file = ostream_file(get_prefs_file(), true);
		write(*prefs_file, prefs);
	} catch(io_exception&) {
		ERR_FS << "error writing to preferences file '" << get_prefs_file() << "'\n";
	}


    #ifndef _WIN32

    if(!prefs_file_existed) {

        if(chmod(get_prefs_file().c_str(), 0600) == -1) {
			ERR_FS << "error setting permissions of preferences file '" << get_prefs_file() << "'\n";
        }

    }

    #endif


}

void set(const std::string &key, bool value)
{
	prefs[key] = value;
}

void set(const std::string &key, int value)
{
	prefs[key] = value;
}

void set(const std::string &key, char const *value)
{
	prefs[key] = value;
}

void set(const std::string &key, const std::string &value)
{
	prefs[key] = value;
}

void clear(const std::string& key)
{
	prefs.recursive_clear_value(key);
}

void set_child(const std::string& key, const config& val) 
{
	prefs.clear_children(key);
	prefs.add_child(key, val);
}

const config &get_child(const std::string& key)
{
	return prefs.child(key);
}

void erase(const std::string& key) {
	prefs.remove_attribute(key);
}

std::string get(const std::string& key) {
	return prefs[key];
}

bool get(const std::string &key, bool def)
{
	return prefs[key].to_bool(def);
}

void disable_preferences_save() {
	no_preferences_save = true;
}

config* get_prefs()
{
	config* pointer = &prefs;
	return pointer;
}

bool get_hero(hero& h, int default_image)
{
	const config& cfg = prefs.child("hero");
	if (cfg) {
		h.set_name(cfg["name"].str());
		h.set_surname(cfg["surname"].str());
		h.set_biography(cfg["biography"].str());

		h.gender_ = cfg["gender"].to_int(hero_gender_male);
		h.image_ = cfg["image"].to_int(default_image);

		h.leadership_ = ftofxp9(cfg["leadership"].to_int(1));
		h.force_ = ftofxp9(cfg["force"].to_int(1));
		h.intellect_ = ftofxp9(cfg["intellect"].to_int(1));
		h.spirit_ = ftofxp9(cfg["spirit"].to_int(1));
		h.charm_ = ftofxp9(cfg["charm"].to_int(1));

		std::stringstream str;
		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			str.str("");
			str << "arms" << i;
			h.arms_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			str.str("");
			str << "skill" << i;
			h.skill_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}

		h.side_feature_ = cfg["side_feature"].to_int(HEROS_NO_FEATURE);
		if (h.side_feature_ < 0 || h.side_feature_ > HEROS_MAX_FEATURE) {
			h.side_feature_ = HEROS_NO_FEATURE;
		}
		h.feature_ = cfg["feature"].to_int(HEROS_NO_FEATURE);
		if (h.feature_ < 0 || h.feature_ > HEROS_MAX_FEATURE) {
			h.feature_ = HEROS_NO_FEATURE;
		}
		h.tactic_ = cfg["tactic"].to_int(HEROS_NO_TACTIC);
		h.utype_ = cfg["utype"].to_int(HEROS_NO_UTYPE);
		h.character_ = cfg["character"].to_int(HEROS_NO_CHARACTER);

		h.base_catalog_ = cfg["base_catalog"].to_int();
		h.float_catalog_ = ftofxp8(h.base_catalog_);

		return true;
	} else {
		// h.set_name(_("Press left button to create hero"));
		h.set_name(public_account);
		h.set_surname("Mr.");

		h.gender_ = hero_gender_male;
		h.image_ = default_image;

		h.leadership_ = ftofxp9(1);
		h.force_ = ftofxp9(1);
		h.intellect_ = ftofxp9(1);
		h.spirit_ = ftofxp9(1);
		h.charm_ = ftofxp9(1);

		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			h.arms_[i] = 0;
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			h.skill_[i] = 0;
		}

		return false;
	}
}

void set_hero(const hero_map& heros, hero& h)
{
	config* ptr = NULL;
	if (config& cfg = prefs.child("hero")) {
		ptr = &cfg;
	} else {
		ptr = &prefs.add_child("hero");
	}
	config& cfg = *ptr;
	std::stringstream str;

	cfg["name"] = h.name();
	cfg["surname"] = h.surname();
	cfg["biography"] = h.biography2(heros);

	cfg["leadership"] = fxptoi9(h.leadership_);
	cfg["force"] = fxptoi9(h.force_);
	cfg["intellect"] = fxptoi9(h.intellect_);
	cfg["spirit"] = fxptoi9(h.spirit_);
	cfg["charm"] = fxptoi9(h.charm_);

	cfg["gender"] = h.gender_;
	cfg["image"] = h.image_;

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		str.str("");
		str << "arms" << i;
		cfg[str.str()] = fxptoi12(h.arms_[i]);
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		str.str("");
		str << "skill" << i;
		cfg[str.str()] = fxptoi12(h.skill_[i]);
	}

	cfg["base_catalog"] = h.base_catalog_;

	cfg["feature"] = h.feature_;
	cfg["side_feature"] = h.side_feature_;

	cfg["tactic"] = h.tactic_;
	cfg["utype"] = h.utype_;
	cfg["character"] = h.character_;

	write_preferences();
}

bool fullscreen()
{
	return get("fullscreen", false);
}

void _set_fullscreen(bool ison)
{
	prefs["fullscreen"] = ison;
}

bool scroll_to_action()
{
	return get("scroll_to_action", false);
}

void _set_scroll_to_action(bool ison)
{
	prefs["scroll_to_action"] = ison;
}

int min_allowed_width()
{
	return 480;
}

int min_allowed_height()
{
	return 320;
}

std::pair<int,int> resolution()
{
	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	std::string x = prefs['x' + postfix], y = prefs['y' + postfix];
	if (!x.empty() && !y.empty()) {
		std::pair<int,int> res(std::max(atoi(x.c_str()), min_allowed_width()),
		                       std::max(atoi(y.c_str()), min_allowed_height()));

		// Make sure resolutions are always divisible by 4
		//res.first &= ~3;
		//res.second &= ~3;
		return res;
	} else {
#if defined(__APPLE__) && TARGET_OS_IPHONE
		return std::pair<int, int>(480, 320);
#else
		// return std::pair<int, int>(1024, 768);
		return std::pair<int, int>(800, 600);
#endif
	}
}

int noble()
{
	int noble =  prefs["noble"].to_int();
	// if (noble < 0 || noble > unit_types.max_noble_level()) {
	// 	noble = 0;
	// }
	return noble;
}

void set_noble(int value)
{
	// if (value < 0 || value > unit_types.max_noble_level()) {
	// 	value = 0;
	// }
	prefs["noble"] = value;
}

int uid()
{
	return prefs["uid"].to_int();
}

void set_uid(int value)
{
	prefs["uid"] = value;
}

std::string city()
{
	return prefs["city"].str();
}

void set_city(const std::string& str)
{
	prefs["city"] = str;
}

std::string interior()
{
	return prefs["interior"].str();
}

void set_interior(const std::string& str)
{
	prefs["interior"] = str;
}

std::string signin()
{
	return prefs["signin"].str();
}

void set_signin(const std::string& str)
{
	prefs["signin"] = str;
}

std::string member()
{
	return prefs["member"].str();
}

void set_member(const std::string& str)
{
	prefs["member"] = str;
}

std::string exile()
{
	return prefs["exile"].str();
}

void set_exile(const std::string& str)
{
	prefs["exile"] = str;
}

std::string associate()
{
	return prefs["associate"].str();
}

void set_associate(const std::string& str)
{
	prefs["associate"] = str;
}

std::string layout()
{
	return prefs["layout"].str();
}

void set_layout(const std::string& str)
{
	prefs["layout"] = str;
}

std::string map()
{
	return prefs["map"].str();
}

void set_map(const std::string& str)
{
	prefs["map"] = str;
}

void set_coin(int value)
{
	prefs["coin"] = value;
}

int coin()
{
	return prefs["coin"].to_int();
}

void set_score(int value)
{
	prefs["score"] = value;
}

int score()
{
	return prefs["score"].to_int();
}

std::string login()
{
	const config& cfg = get_child("hero");

	if (cfg && !cfg["name"].empty()) {
		return cfg["name"].str();
	}
	return public_account;
}

void set_vip_expire(time_t value)
{
	std::stringstream strstr;
	strstr << login() << ", " << (long)value;
	prefs["vip_expire"] = strstr.str();
}

time_t vip_expire()
{
	std::vector<std::string> vstr = utils::split(prefs["vip_expire"].str());
	if (vstr.size() != 2) {
		return 0;
	}
	if (vstr[0] != login()) {
		return 0;
	}
	return lexical_cast_default<long>(vstr[1]);
}

void set_developer(bool ison)
{
	prefs["developer"] = ison;
}

bool developer()
{
	return prefs["developer"].to_bool();
}

std::string nick()
{
	return prefs["nick"].str();
}

void set_nick(const std::string& nick)
{
	prefs["nick"] = nick;
}

bool chat()
{
	return prefs["chat"].to_bool(true);
}

void set_chat(bool val)
{
	prefs["chat"] = val;
}

std::string chat_person()
{
	return prefs["chat_person"].str();
}

void set_chat_person(const std::string& person)
{
	prefs["chat_person"] = person;
}

std::string chat_channel()
{
	return prefs["chat_channel"].str();
}

void set_chat_channel(const std::string& channel)
{
	prefs["chat_channel"] = channel;
}

std::string encode_pw(const std::string& str)
{
	std::stringstream ss;
	ss << "pw_";
	if (utils::is_utf8str(str)) {
		ss << str;
	}
	return ss.str();
}

std::string decode_pw(const std::string& str)
{
	if (!utils::is_utf8str(str)) {
		return null_str;
	}

	int pos = str.find("pw_");
	if (pos != std::string::npos) {
		return str.substr(3);
	} else {
		return str;
	}
}

std::string password()
{
	if (login() == public_account) {
		return public_password;
	} else if (remember_password()) {
		return decode_pw(preferences::get("password"));
	}
	return "";
}

void set_password(const std::string& password)
{
	if (remember_password()) {
		preferences::set("password", encode_pw(password));
	}
}

bool remember_password()
{
	return preferences::get("remember_password", false);
}

void set_remember_password(bool remember)
{
	preferences::set("remember_password", remember);
}

bool turbo()
{
	return get("turbo", false);
}

void _set_turbo(bool ison)
{
	prefs["turbo"] = ison;
}

double turbo_speed()
{
	return prefs["turbo_speed"].to_double(2.0);
}

void save_turbo_speed(const double speed)
{
	prefs["turbo_speed"] = speed;
}

bool default_move()
{
	return  get("default_move", true);
}

void _set_default_move(const bool ison)
{
	prefs["default_move"] = ison;
}

std::string language()
{
	return prefs["locale"];
}

void set_language(const std::string& s)
{
	preferences::set("locale", s);
}

bool grid()
{
	return get("grid", false);
}

void _set_grid(bool ison)
{
	preferences::set("grid", ison);
}

int zoom()
{
	return display::adjust_zoom(prefs["zoom"].to_int(game_config::tiny_gui? display::ZOOM_48: display::ZOOM_72));
}

void _set_zoom(int value)
{
	preferences::set("zoom", display::adjust_zoom(value));
}

size_t sound_buffer_size()
{
	// Sounds don't sound good on Windows unless the buffer size is 4k,
	// but this seems to cause crashes on other systems...
	#ifdef _WIN32
		const size_t buf_size = 4096;
	#else
		const size_t buf_size = 1024;
	#endif

	return prefs["sound_buffer_size"].to_int(buf_size);
}

void save_sound_buffer_size(const size_t size)
{
	#ifdef _WIN32
		const char* buf_size = "4096";
	#else
		const char* buf_size = "1024";
	#endif

	const std::string new_size = lexical_cast_default<std::string>(size, buf_size);
	if (get("sound_buffer_size") == new_size)
		return;

	preferences::set("sound_buffer_size", new_size);

	sound::reset_sound();
}

int music_volume()
{
	return prefs["music_volume"].to_int(100);
}

void set_music_volume(int vol)
{
	if(music_volume() == vol) {
		return;
	}

	prefs["music_volume"] = vol;
	sound::set_music_volume(music_volume());
}

int sound_volume()
{
	return prefs["sound_volume"].to_int(100);
}

void set_sound_volume(int vol)
{
	if(sound_volume() == vol) {
		return;
	}

	prefs["sound_volume"] = vol;
	sound::set_sound_volume(sound_volume());
}

int bell_volume()
{
	return prefs["bell_volume"].to_int(100);
}

void set_bell_volume(int vol)
{
	if(bell_volume() == vol) {
		return;
	}

	prefs["bell_volume"] = vol;
	sound::set_bell_volume(bell_volume());
}

int UI_volume()
{
	return prefs["UI_volume"].to_int(100);
}

void set_UI_volume(int vol)
{
	if(UI_volume() == vol) {
		return;
	}

	prefs["UI_volume"] = vol;
	sound::set_UI_volume(UI_volume());
}

bool turn_bell()
{
	return get("turn_bell", true);
}

bool set_turn_bell(bool ison)
{
	if(!turn_bell() && ison) {
		preferences::set("turn_bell", true);
		if(!music_on() && !sound_on() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set("turn_bell", false);
				return false;
			}
		}
	} else if(turn_bell() && !ison) {
		preferences::set("turn_bell", false);
		sound::stop_bell();
		if(!music_on() && !sound_on() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool UI_sound_on()
{
#ifdef _WIN32
	// fix bug
	return true;
#else
	return get("UI_sound", true);
#endif
}

bool set_UI_sound(bool ison)
{
	if(!UI_sound_on() && ison) {
		preferences::set("UI_sound", true);
		if(!music_on() && !sound_on() && !turn_bell()) {
			if(!sound::init_sound()) {
				preferences::set("UI_sound", false);
				return false;
			}
		}
	} else if(UI_sound_on() && !ison) {
		preferences::set("UI_sound", false);
		sound::stop_UI_sound();
		if(!music_on() && !sound_on() && !turn_bell())
			sound::close_sound();
	}
	return true;
}

bool message_bell()
{
	return get("message_bell", true);
}

bool sound_on()
{
	return get("sound", true);
}

bool set_sound(bool ison) {
	if(!sound_on() && ison) {
		preferences::set("sound", true);
		if(!music_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set("sound", false);
				return false;
			}
		}
	} else if(sound_on() && !ison) {
		preferences::set("sound", false);
		sound::stop_sound();
		if(!music_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool music_on()
{
	return get("music", true);
}

bool set_music(bool ison) {
	if(!music_on() && ison) {
		preferences::set("music", true);
		if(!sound_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set("music", false);
				return false;
			}
		}
		else
			sound::play_music();
	} else if(music_on() && !ison) {
		preferences::set("music", false);
		if(!sound_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
		else
			sound::stop_music();
	}
	return true;
}

namespace {
	double scroll = 0.2;
}

int scroll_speed()
{
	const int value = lexical_cast_in_range<int>(get("scroll"), 50, 1, 100);
	scroll = value/100.0;

	return value;
}

void set_scroll_speed(const int new_speed)
{
	prefs["scroll"] = new_speed;
	scroll = new_speed / 100.0;
}

bool middle_click_scrolls()
{
	return get("middle_click_scrolls", true);
}

bool mouse_scroll_enabled()
{
	return get("mouse_scrolling", true);
}

void enable_mouse_scroll(bool value)
{
	set("mouse_scrolling", value);
}

int mouse_scroll_threshold()
{
	return prefs["scroll_threshold"].to_int(10);
}

bool animate_map()
{
	return preferences::get("animate_map", true);
}

void set_animate_map(bool value)
{
	set("animate_map", value);
}

bool show_standing_animations()
{
	return preferences::get("unit_standing_animations", true);
}

bool show_fps()
{
	return fps;
}

void set_show_fps(bool value)
{
	fps = value;
}

int draw_delay()
{
	return draw_delay_;
}

void set_draw_delay(int value)
{
	draw_delay_ = value;
}

bool use_color_cursors()
{
	return color_cursors;
}

void _set_color_cursors(bool value)
{
	preferences::set("color_cursors", value);
	color_cursors = value;
}

void load_hotkeys() {
	hotkey::load_hotkeys(prefs);
}
void save_hotkeys() {
	hotkey::save_hotkeys(prefs);
}

void add_alias(const std::string &alias, const std::string &command)
{
	config &alias_list = prefs.child_or_add("alias");
	alias_list[alias] = command;
}


const config &get_alias()
{
	return get_child("alias");
}

unsigned int sample_rate()
{
	return prefs["sample_rate"].to_int(44100);
}

void save_sample_rate(const unsigned int rate)
{
	if (sample_rate() == rate)
		return;

	prefs["sample_rate"] = int(rate);

	// If audio is open, we have to re set sample rate
	sound::reset_sound();
}

} // end namespace preferences

