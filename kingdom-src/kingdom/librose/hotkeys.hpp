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

/** Available hotkey scopes. The scope is used to allow command from
 * non-overlapping areas of the game share the same key
 */
enum scope {
	SCOPE_GENERAL,
	SCOPE_GAME,
	SCOPE_EDITOR,
	SCOPE_COUNT
};


enum HOTKEY_COMMAND {
	HOTKEY_UNDO, HOTKEY_REDO, HOTKEY_UP, HOTKEY_DOWN,
	HOTKEY_ZOOM_IN, HOTKEY_ZOOM_OUT, HOTKEY_ZOOM_DEFAULT,
	HOTKEY_SCREENSHOT, HOTKEY_MAP_SCREENSHOT,
	HOTKEY_RPG, HOTKEY_RPG_DETAIL, HOTKEY_RPG_TREASURE, HOTKEY_RPG_EXCHANGE, HOTKEY_RPG_INDEPENDENCE,
	HOTKEY_RECRUIT, HOTKEY_EXPEDITE, HOTKEY_ARMORY, HOTKEY_MOVE, 
	HOTKEY_SWITCH_LIST, HOTKEY_ENDTURN,
	HOTKEY_TOGGLE_GRID, HOTKEY_MUTE, HOTKEY_MOUSE_SCROLL,
	HOTKEY_SPEAK, HOTKEY_CHANGE_SIDE, HOTKEY_PREFERENCES,
	HOTKEY_OBJECTIVES, HOTKEY_STATISTICS, HOTKEY_STOP_NETWORK, HOTKEY_START_NETWORK, HOTKEY_QUIT_GAME,
	HOTKEY_LABEL_TEAM_TERRAIN, HOTKEY_LABEL_TERRAIN, HOTKEY_CLEAR_LABELS,
	HOTKEY_DELAY_SHROUD, HOTKEY_UPDATE_SHROUD,
	HOTKEY_SPEAK_ALLY, HOTKEY_SPEAK_ALL, HOTKEY_HELP,
	HOTKEY_CHAT_LOG, 
	HOTKEY_PLAY_REPLAY, HOTKEY_STOP_REPLAY, HOTKEY_REPLAY_NEXT_TURN,
	HOTKEY_REPLAY_NEXT_SIDE, HOTKEY_REPLAY_SHOW_EVERYTHING,
	HOTKEY_REPLAY_SHOW_EACH, HOTKEY_REPLAY_SHOW_TEAM1,
	HOTKEY_REPLAY_SKIP_ANIMATION,
	HOTKEY_ANIMATE_MAP,
	HOTKEY_BUILD_M, HOTKEY_BUILD, HOTKEY_GUARD, HOTKEY_ABOLISH, HOTKEY_EXTRACT, HOTKEY_ADVANCE, HOTKEY_DEMOLISH,
	HOTKEY_UNIT_DETAIL,
	HOTKEY_CHAT,
	HOTKEY_PLAY_CARD,

	HOTKEY_INTERIOR,
	HOTKEY_TECHNOLOGY_TREE,
	HOTKEY_UPLOAD,
	HOTKEY_FINAL_BATTLE,
	HOTKEY_EMPLOY,
	HOTKEY_LIST,
	HOTKEY_SYSTEM,

	HOTKEY_TACTIC0, HOTKEY_TACTIC1, HOTKEY_TACTIC2,
	HOTKEY_BOMB,

	// build item
	HOTKEY_BUILD_KEEP, HOTKEY_BUILD_WALL, 
	HOTKEY_INTERIOR_M, 
	HOTKEY_BUILD_MARKET, HOTKEY_BUILD_TECHNOLOGY, HOTKEY_BUILD_TACTIC, 

	HOTKEY_EDITOR_QUIT_TO_DESKTOP,
	HOTKEY_EDITOR_CLOSE_MAP, HOTKEY_EDITOR_SWITCH_MAP,
	HOTKEY_EDITOR_SETTINGS,
	HOTKEY_EDITOR_PARTIAL_UNDO,
	HOTKEY_EDITOR_MAP_NEW, HOTKEY_EDITOR_MAP_LOAD, HOTKEY_EDITOR_MAP_SAVE,
	HOTKEY_EDITOR_MAP_SAVE_AS, HOTKEY_EDITOR_MAP_SAVE_ALL,
	HOTKEY_EDITOR_MAP_REVERT, HOTKEY_EDITOR_MAP_INFO,
	HOTKEY_EDITOR_TERRAIN_PALETTE_SWAP,
	HOTKEY_EDITOR_TOOL_NEXT, HOTKEY_EDITOR_TOOL_PAINT, HOTKEY_EDITOR_TOOL_FILL,
	HOTKEY_EDITOR_TOOL_SELECT, HOTKEY_EDITOR_TOOL_STARTING_POSITION,
	HOTKEY_EDITOR_BRUSH_NEXT, HOTKEY_EDITOR_BRUSH_DEFAULT,
	HOTKEY_EDITOR_CUT, HOTKEY_EDITOR_COPY, HOTKEY_EDITOR_PASTE,
	HOTKEY_EDITOR_EXPORT_SELECTION_COORDS,
	HOTKEY_EDITOR_SELECT_ALL, HOTKEY_EDITOR_SELECT_INVERSE,
	HOTKEY_EDITOR_SELECT_NONE,
	HOTKEY_EDITOR_CLIPBOARD_ROTATE_CW, HOTKEY_EDITOR_CLIPBOARD_ROTATE_CCW,
	HOTKEY_EDITOR_CLIPBOARD_FLIP_HORIZONTAL, HOTKEY_EDITOR_CLIPBOARD_FLIP_VERTICAL,
	HOTKEY_EDITOR_SELECTION_ROTATE, HOTKEY_EDITOR_SELECTION_FLIP,
	HOTKEY_EDITOR_SELECTION_FILL,
	HOTKEY_EDITOR_SELECTION_GENERATE, HOTKEY_EDITOR_SELECTION_RANDOMIZE,
	HOTKEY_EDITOR_MAP_RESIZE, HOTKEY_EDITOR_MAP_ROTATE,
	HOTKEY_EDITOR_MAP_GENERATE, HOTKEY_EDITOR_MAP_APPLY_MASK,
	HOTKEY_EDITOR_MAP_CREATE_MASK_TO,
	HOTKEY_EDITOR_REFRESH, HOTKEY_EDITOR_UPDATE_TRANSITIONS,
	HOTKEY_EDITOR_AUTO_UPDATE_TRANSITIONS,
	HOTKEY_EDITOR_REFRESH_IMAGE_CACHE,
	HOTKEY_EDITOR_MAP,
	HOTKEY_EDITOR_TERRAIN_GROUP,
	HOTKEY_EDITOR_BRUSH,
	
	//misc.
	HOTKEY_CLEAR_MSG,
	/* Gui2 specific hotkeys. */
	TITLE_SCREEN__RELOAD_WML,
	GLOBAL__HELPTIP,
	HOTKEY_NULL
};

void deactivate_all_scopes();
void set_scope_active(scope s, bool set = true);
bool is_scope_active(scope s);

class hotkey_item {
public:
	hotkey_item() :
		id_(HOTKEY_NULL),
		command_(),
		description_(),
		scope_(SCOPE_GENERAL),
		type_(UNBOUND),
		character_(0),
		ctrl_(false),
		alt_(false),
		cmd_(false),
		keycode_(0),
		shift_(false),
		hidden_(false)
		{}

	hotkey_item(HOTKEY_COMMAND id, const std::string& command,
		const t_string &description, bool hidden = false,
		scope s=SCOPE_GENERAL);

	HOTKEY_COMMAND get_id() const { return id_; };
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


	/** @return the scope of this hotkey */
	scope get_scope() const { return scope_; }

	bool is_in_active_scope() const { return is_scope_active(get_scope()); }

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
	HOTKEY_COMMAND id_;
	std::string command_;
	t_string description_;
	scope scope_;

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



class manager {
public:
	manager();
	static void init();
	static void wipe();
	~manager();
};

class scope_changer {
public:
	scope_changer(const config& cfg, const std::string& hotkey_tag);
	~scope_changer();
private:
	const config& cfg_;
	std::string prev_tag_name_;
	std::vector<bool> prev_scope_active_;
};

void load_descriptions();

void set_hotkey_tag_name(const std::string& name);
void load_hotkeys(const config& cfg);
void save_hotkeys(config& cfg);

hotkey_item& get_hotkey(HOTKEY_COMMAND id);
hotkey_item& get_hotkey(const std::string& command);

hotkey_item& get_hotkey(int character, int keycode, bool shift, bool ctrl,
	bool alt, bool cmd);
hotkey_item& get_hotkey(const SDL_KeyboardEvent& event);

hotkey_item& get_visible_hotkey(int index);

std::vector<hotkey_item>& get_hotkeys();

enum ACTION_STATE { ACTION_STATELESS, ACTION_ON, ACTION_OFF };

//abstract base class for objects that implement the ability
//to execute hotkey commands.
class command_executor
{
protected:
	virtual ~command_executor() {}
public:
	virtual void end_turn() {}
	virtual void undo() {}
	virtual void redo() {}
	virtual void switch_list() {}
	virtual void save_map() {}
	virtual void show_rpg() {}
	virtual void rpg_detail() {}
	virtual void assemble_treasure() {}
	virtual void rpg_exchange(const std::vector<size_t>& human_p, size_t ai_p) {}
	virtual void rpg_independence(bool replaying = false) {}
	virtual void toggle_grid() {}
	virtual void build(const std::string& type) {}
	virtual void guard() {}
	virtual void abolish() {}
	virtual void extract() {}
	virtual void advance() {}
	virtual void demolish() {}
	virtual void armory() {}
	virtual void unit_detail() {}
	virtual void chat() {}
	virtual void play_card() {}
	virtual void recruit() {}
	virtual void expedite() {}
	virtual void move() {}
	virtual void speak() {}
	virtual void whisper() {}
	virtual void shout() {}
	virtual void change_side() {}
	virtual void preferences() {}
	virtual void objectives() {}
	virtual void show_statistics() {}
	virtual void stop_network() {}
	virtual void start_network() {}
	virtual void label_terrain(bool /*team_only*/) {}
	virtual void clear_labels() {}
	virtual void toggle_shroud_updates() {}
	virtual void update_shroud_now() {}
	virtual void show_help() {}
	virtual void show_chat_log() {}
	virtual void clear_messages() {}
	virtual void play_replay() {}
	virtual void stop_replay() {}
	virtual void replay_next_turn() {}
	virtual void replay_next_side() {}
	virtual void replay_show_everything() {}
	virtual void replay_show_each() {}
	virtual void replay_show_team1() {}
	virtual void replay_skip_animation() {}

	virtual void interior() {};
	virtual void technology_tree() {};
	virtual void upload() {};
	virtual void final_battle(int side_num = -1, int human_capital = -1, int ai_capital = -1) {}
	virtual void employ() {};
	virtual void list() {}
	virtual void system() {}

	virtual void remove_active_tactic(int slot) {}
	virtual void bomb() {}

	//Gets the action's image (if any). Displayed left of the action text in menus.
	virtual std::string get_action_image(hotkey::HOTKEY_COMMAND /*command*/, int /*index*/) const { return ""; }
	//Does the action control a toggle switch? If so, return the state of the action (on or off)
	virtual ACTION_STATE get_action_state(hotkey::HOTKEY_COMMAND /*command*/, int /*index*/) const { return ACTION_STATELESS; }
	//Returns the appropriate menu image. Checkable items will get a checked/unchecked image.
	std::string get_menu_image(hotkey::HOTKEY_COMMAND command, int index=-1) const;
	//Returns a vector of images for a given menu
	std::vector<std::string> get_menu_images(display &, const std::vector<std::string>& items_arg);

	void show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu, display& gui);

	virtual bool can_execute_command(HOTKEY_COMMAND command, int index=-1) const = 0;
	virtual bool execute_command(HOTKEY_COMMAND command, int index=-1, std::string str = "");
};

//function to be called every time a key event is intercepted. Will
//call the relevant function in executor if the keyboard event is
//not NULL. Also handles some events in the function itself, and so
//is still meaningful to call with executor=NULL
void key_event(display& disp, const SDL_KeyboardEvent& event, command_executor* executor);

void execute_command(display& disp, HOTKEY_COMMAND command, command_executor* executor, int index=-1, std::string str = "");

//object which will ensure that basic keyboard events like escape
//are handled properly for the duration of its lifetime
struct basic_handler : public events::handler {
	basic_handler(display* disp, command_executor* exec=NULL);

	void handle_event(const SDL_Event& event);

private:
	display* disp_;
	command_executor* exec_;
};

}

#endif
