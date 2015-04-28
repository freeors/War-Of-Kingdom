/* $Id: hotkeys.cpp 47754 2010-11-29 12:35:07Z shadowmaster $ */
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

#include "global.hpp"

#define GETTEXT_DOMAIN "rose-lib"

#include "display.hpp"
#include "hotkeys.hpp"
#include "game_end_exceptions.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "filesystem.hpp"
#include "log.hpp"
#include "wml_separators.hpp"
#include "hero.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_config("config");
#define ERR_G LOG_STREAM(err, lg::general)
#define LOG_G LOG_STREAM(info, lg::general)
#define DBG_G LOG_STREAM(debug, lg::general)
#define ERR_CF LOG_STREAM(err, log_config)

static std::map<int, hotkey::hotkey_item> hotkeys;
static hotkey::hotkey_item null_hotkey;

std::string hotkey_tag_name = "hotkey";

namespace hotkey {

const std::string CLEARED_TEXT = "__none__";

hotkey_item::hotkey_item(int id,
		const std::string& command, const t_string &description, bool hidden) :
	id_(id),
	command_(command),
	description_(description),
	type_(UNBOUND),
	character_(0),
	ctrl_(false),
	alt_(false),
	cmd_(false),
	keycode_(0),
	shift_(false),
	hidden_(hidden)
{
}

// There are two kinds of "key" values.  One refers to actual keys, like
// F1 or SPACE.  The other refers to characters produced, eg 'M' or ':'.
// For the latter, specifying shift+; doesn't make sense, because ; is
// already shifted on French keyboards, for example.  You really want to
// say ':', however that is typed.  However, when you say shift+SPACE,
// you're really referring to the space bar, as shift+SPACE usually just
// produces a SPACE character.
void hotkey_item::load_from_config(const config& cfg)
{
	const std::string& key = cfg["key"];

	alt_ = cfg["alt"].to_bool();
	cmd_ = cfg["cmd"].to_bool();
	ctrl_ = cfg["ctrl"].to_bool();
	shift_ = cfg["shift"].to_bool();

	if (key.empty()) return;

	if (key == CLEARED_TEXT)
	{
		type_ = hotkey_item::CLEARED;
		return;
	}

	wide_string wkey = utils::string_to_wstring(key);

	// They may really want a specific key on the keyboard: we assume
	// that any single character keyname is a character.
	if (wkey.size() > 1) {
		type_ = BY_KEYCODE;

		keycode_ = sdl_keysym_from_name(key);
		if (keycode_ == SDLK_UNKNOWN) {
			if (tolower(key[0]) != 'f') {
				ERR_CF << "hotkey key '" << key << "' invalid\n";
			} else {
				int num = lexical_cast_default<int>(key.c_str() + 1);
				keycode_ = num + SDLK_F1 - 1;
			}
		}
	} else if (key == " " || shift_
#ifdef __APPLE__
		|| alt_
#endif
		) {
		// Space must be treated as a key because shift-space
		// isn't a different character from space, and control key
		// makes it go weird.  shift=yes should never be specified
		// on single characters (eg. key=m, shift=yes would be
		// key=M), but we don't want to break old preferences
		// files.
		type_ = BY_KEYCODE;
		keycode_ = wkey[0];
	} else {
		type_ = BY_CHARACTER;
		character_ = wkey[0];
	}
}

std::string hotkey_item::get_name() const
{
	std::stringstream str;
	if (type_ == BY_CHARACTER) {
		if (alt_)
			str << "alt+";
		if (cmd_)
			str << "cmd+";
		if (ctrl_)
			str << "ctrl+";
		str << static_cast<char>(character_);
	} else if (type_ == BY_KEYCODE) {
		if (alt_)
			str << "alt+";
		if (ctrl_)
			str << "ctrl+";
		if (shift_)
			str << "shift+";
		if (cmd_)
			str << "cmd+";
		str << SDL_GetKeyName(SDLKey(keycode_));
	}
	return str.str();
}

void hotkey_item::set_description(const t_string &description)
{
	description_ = description;
}
void hotkey_item::clear_hotkey()
{
	type_ = CLEARED;
}

void hotkey_item::set_key(int character, int keycode, bool shift, bool ctrl, bool alt, bool cmd)
{
	const std::string keyname = SDL_GetKeyName(SDLKey(keycode));

	LOG_G << "setting hotkey: char=" << lexical_cast<std::string>(character)
		   << " keycode="  << lexical_cast<std::string>(keycode) << " "
		   << (shift ? "shift," : "")
		   << (ctrl ? "ctrl," : "")
		   << (alt ? "alt," : "")
		   << (cmd ? "cmd," : "")
		   << "\n";

	// Sometimes control modifies by -64, ie ^A == 1.
	if (character < 64 && ctrl) {
		if (shift)
			character += 64;
		else
			character += 96;
		LOG_G << "Mapped to character " << lexical_cast<std::string>(character) << "\n";
	}

	// For some reason on Mac OS, if cmd and shift are down, the character doesn't get upper-cased
	if (cmd && character > 96 && character < 123 && shift)
		character -= 32;

	// We handle simple cases by character, others by the actual key.
	if (isprint(character) && !isspace(character)) {
		type_ = BY_CHARACTER;
		character_ = character;
		ctrl_ = ctrl;
		alt_ = alt;
		cmd_ = cmd;
		LOG_G << "type = BY_CHARACTER\n";
	} else {
		type_ = BY_KEYCODE;
		keycode_ = keycode;
		shift_ = shift;
		ctrl_ = ctrl;
		alt_ = alt;
		cmd_ = cmd;
		LOG_G << "type = BY_KEYCODE\n";
	}
}

scope_changer::scope_changer(const config& cfg, const std::string& hotkey_tag)
: cfg_(cfg)
, prev_tag_name_(hotkey_tag_name)
{
	hotkeys.clear();
	// hotkey::load_descriptions();
	// load_hotkeys(cfg_);
	// set_hotkey_tag_name(hotkey_tag);
}

scope_changer::~scope_changer()
{
	hotkeys.clear();
	// hotkey::load_descriptions();
	// set_hotkey_tag_name(prev_tag_name_);
	// load_hotkeys(cfg_);
}

void clear()
{
	hotkeys.clear();
}

void load_descriptions()
{
}

void set_hotkey_tag_name(const std::string& name)
{
	hotkey_tag_name = name;
}

void load_hotkeys(const config& cfg)
{
	BOOST_FOREACH (const config &hk, cfg.child_range(hotkey_tag_name))
	{
		hotkey_item& h = get_hotkey(hk["command"].str());
		if(h.get_id() != HOTKEY_NULL) {
			h.load_from_config(hk);
		}
	}
}

void insert_hotkey(int id, const std::string& command, const t_string& tooltip)
{
	std::stringstream err;
	err << "Duplicate hotkey: "<< id;
	VALIDATE(hotkeys.find(id) == hotkeys.end(), err.str());

	hotkeys.insert(std::make_pair(id, hotkey_item(id, command, tooltip)));
}

void save_hotkeys(config& cfg)
{
	cfg.clear_children(hotkey_tag_name);

	for(std::map<int, hotkey_item>::iterator it = hotkeys.begin(); it != hotkeys.end(); ++ it) {
		hotkey_item& key = it->second;
		if (key.hidden() || key.get_type() == hotkey_item::UNBOUND)
			continue;

		config& item = cfg.add_child(hotkey_tag_name);
		item["command"] = key.get_command();
		if (key.get_type() == hotkey_item::CLEARED)
		{
			item["key"] = CLEARED_TEXT;
			continue;
		}

		if (key.get_type() == hotkey_item::BY_KEYCODE) {
			item["key"] = SDL_GetKeyName(SDLKey(key.get_keycode()));
			item["shift"] = key.get_shift();
		} else if (key.get_type() == hotkey_item::BY_CHARACTER) {
			item["key"] = utils::wchar_to_string(key.get_character());
		}
		item["alt"] = key.get_alt();
		item["ctrl"] = key.get_ctrl();
		item["cmd"] = key.get_cmd();
	}
}

hotkey_item& get_hotkey(int id)
{
	std::map<int, hotkey_item>::iterator itor = hotkeys.find(id);
	if (itor == hotkeys.end())
		return null_hotkey;

	return itor->second;
}

hotkey_item& get_hotkey(const std::string& command)
{
	std::map<int, hotkey_item>::iterator itor;

	for (itor = hotkeys.begin(); itor != hotkeys.end(); ++ itor) {
		if (itor->second.get_command() == command)
			break;
	}

	if (itor == hotkeys.end())
		return null_hotkey;

	return itor->second;
}

hotkey_item& get_hotkey(int character, int keycode, bool shift, bool ctrl, bool alt, bool cmd)
{
	std::map<int, hotkey_item>::iterator itor;

	// Sometimes control modifies by -64, ie ^A == 1.
	if (0 < character && character < 64 && ctrl) {
		if (shift) {
			character += 64;
		} else {
			character += 96;
		}
	}

	// For some reason on Mac OS, if cmd and shift are down, the character doesn't get upper-cased
	if (cmd && character > 96 && character < 123 && shift)
		character -= 32;

	for (itor = hotkeys.begin(); itor != hotkeys.end(); ++itor) {
		hotkey_item& item = itor->second;
		if (item.get_type() == hotkey_item::BY_CHARACTER) {
			if (character == item.get_character()) {
				if (ctrl == item.get_ctrl()	&& alt == item.get_alt() && cmd == item.get_cmd()) {
					break;
				}
			}
		} else if (item.get_type() == hotkey_item::BY_KEYCODE) {
			if (keycode == item.get_keycode()) {
				if (shift == item.get_shift() && ctrl == item.get_ctrl() && alt == item.get_alt() && cmd == item.get_cmd()) {
					// Could match by keycode...yes";
					break;
				}
				// Could match by keycode... but modifiers different;
			}
		}
	}

	if (itor == hotkeys.end())
		return null_hotkey;

	return itor->second;
}

hotkey_item& get_hotkey(const SDL_KeyboardEvent& event)
{
	return get_hotkey(event.keysym.sym, event.keysym.sym,
			(event.keysym.mod & KMOD_SHIFT) != 0,
			(event.keysym.mod & KMOD_CTRL) != 0,
			(event.keysym.mod & KMOD_ALT) != 0,
			(event.keysym.mod & KMOD_LGUI) != 0);
}

}
