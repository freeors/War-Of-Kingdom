/* $Id: hotkeys.hpp 47606 2010-11-20 14:54:03Z shadowmaster $ */
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
#ifndef HOTKEYS_HPP_INCLUDED
#define HOTKEYS_HPP_INCLUDED

#include "events.hpp"
#include "tstring.hpp"

class config;
class display;

//the hotkey system allows hotkey definitions to be loaded from
//configuration objects, and then detect if a keyboard event
//refers to a hotkey command being executed.
namespace hotkey {

class hotkey_item {
public:
	hotkey_item() :
		id_(HOTKEY_NULL),
		command_(),
		description_(),
		type_(UNBOUND),
		character_(0),
		ctrl_(false),
		alt_(false),
		cmd_(false),
		keycode_(0),
		shift_(false),
		hidden_(false)
		{}

	hotkey_item(int id, const std::string& command,
		const t_string &description, bool hidden = false);

	int get_id() const { return id_; };
	const std::string& get_command() const { return command_; };
	const t_string &get_description() const { return description_; };

	void load_from_config(const config& cfg);

	void set_description(const t_string &description);
	void clear_hotkey();
	void set_key(int character, int keycode, bool shift, bool ctrl, bool alt, bool cmd);

	enum type {
		UNBOUND,
		BY_KEYCODE,
		BY_CHARACTER,
		CLEARED
	};

	enum type get_type() const { return type_; }


	// Returns unicode value of keypress.
	int get_character() const { return character_; }
	bool get_alt() const { return alt_; }
	bool get_cmd() const { return cmd_; }
	bool get_ctrl() const { return ctrl_; }

	// Return the actual key code.
	int get_keycode() const { return keycode_; }
	bool get_shift() const { return shift_; }

	// Return "name" of hotkey for example :"ctrl+alt+g"
	std::string get_name() const;

	bool null() const { return id_ == HOTKEY_NULL; };
	bool hidden() const { return hidden_; };
private:
	int id_;
	std::string command_;
	t_string description_;

	// UNBOUND means unset, CHARACTER means see character_, KEY means keycode_.
	enum type type_;

	// Actual unicode character
	int character_;
	bool ctrl_;
	bool alt_;
	bool cmd_;

	// These used for function keys (which don't have a unicode value) or
	// space (which doesn't have a distinct unicode value when shifted).
	int keycode_;
	bool shift_;

	bool hidden_;

};


class scope_changer {
public:
	scope_changer(const config& cfg, const std::string& hotkey_tag);
	~scope_changer();
private:
	const config& cfg_;
	std::string prev_tag_name_;
};

void load_descriptions();

void set_hotkey_tag_name(const std::string& name);
void load_hotkeys(const config& cfg);
void save_hotkeys(config& cfg);

void insert_hotkey(int id, const std::string& command, const t_string& tooltip);

hotkey_item& get_hotkey(int id);
hotkey_item& get_hotkey(const std::string& command);

hotkey_item& get_hotkey(int character, int keycode, bool shift, bool ctrl,
	bool alt, bool cmd);
hotkey_item& get_hotkey(const SDL_KeyboardEvent& event);

// void key_event(display& disp, const SDL_KeyboardEvent& event, command_executor* executor);

}

#endif
