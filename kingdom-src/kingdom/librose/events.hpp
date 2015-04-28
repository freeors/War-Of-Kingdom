/* $Id: events.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef EVENTS_HPP_INCLUDED
#define EVENTS_HPP_INCLUDED

#include "SDL.h"
#include <vector>

//our user-defined double-click event type
#define DOUBLE_CLICK_EVENT SDL_USEREVENT
#define TIMER_EVENT (SDL_USEREVENT + 1)
#define HOVER_REMOVE_POPUP_EVENT (SDL_USEREVENT + 2)
#define DRAW_EVENT (SDL_USEREVENT + 3)
#define CLOSE_WINDOW_EVENT (SDL_USEREVENT + 4)
#define SHOW_HELPTIP_EVENT (SDL_USEREVENT + 5)

enum HOTKEY_COMMAND {
	HOTKEY_NULL,
	HOTKEY_ZOOM_IN, HOTKEY_ZOOM_OUT, HOTKEY_ZOOM_DEFAULT,
	HOTKEY_SCREENSHOT, HOTKEY_MAP_SCREENSHOT,
	HOTKEY_COPY, HOTKEY_PASTE, HOTKEY_CUT,
	HOTKEY_CHAT, HOTKEY_UNDO, HOTKEY_REDO, HOTKEY_HELP, HOTKEY_SYSTEM,
	GLOBAL__HELPTIP,
	HOTKEY_MIN = 100
};

struct tfinger
{
	tfinger(SDL_FingerID id, int x, int y, Uint32 active)
		: fingerId(id)
		, x(x)
		, y(y)
		, active(active)
	{}

	bool operator==(const tfinger& that) const
	{
		if (fingerId != that.fingerId) return false;
		return true;
	}
	bool operator!=(const tfinger& that) const { return !operator==(that); }


	SDL_FingerID fingerId;
	int x;
	int y;
	Uint32 active;
};

class base_finger
{
protected:
	base_finger();

	void process_event(const SDL_Event& event);

	// generate multigesture whether or not.
	bool multi_gestures() const;

	virtual bool finger_coordinate_valid(int x, int y) const { return true; }
	virtual bool mouse_wheel_coordinate_valid(int x, int y) const { return true; }

	virtual void handle_swipe(int x, int y, int dx, int dy) {}
	virtual void handle_pinch(int x, int y, bool out) {}
	virtual void handle_mouse_down(const SDL_MouseButtonEvent& button) {}
	virtual void handle_mouse_up(const SDL_MouseButtonEvent& button) {}
	virtual void handle_mouse_motion(const SDL_MouseMotionEvent& motion) {}
	virtual void handle_mouse_wheel(const SDL_MouseWheelEvent& wheel, int x, int y, Uint8 mouse_flags) {}

protected:
	std::vector<tfinger> fingers_;
	int pinch_distance_;
	int mouse_motions_;
	Uint32 pinch_noisc_time_;
	Uint32 last_pinch_ticks_;
};

namespace events
{

//any classes that derive from this class will automatically
//receive sdl events through the handle function for their lifetime,
//while the event context they were created in is active.
//
//NOTE: an event_context object must be initialized before a handler object
//can be initialized, and the event_context must be destroyed after
//the handler is destroyed.
class handler
{
public:
	virtual void handle_event(const SDL_Event& event) = 0;
	virtual void process_event() {}
	virtual void draw() {}

	virtual bool requires_event_focus(const SDL_Event * = NULL) const { return false; }

	virtual void join(); /*joins the current event context*/
	virtual void leave(); /*leave the event context*/

protected:
	handler(const bool auto_join=true);
	virtual ~handler();
	virtual std::vector<handler*> handler_members() {std::vector<handler*> h; return h;}

private:
	int unicode_;
	bool has_joined_;
};

void focus_handler(const handler* ptr);
void cycle_focus();

bool has_focus(const handler* ptr, const SDL_Event* event);

//event_context objects control the handler objects that SDL events are sent
//to. When an event_context is created, it will become the current event context.
//event_context objects MUST be created in LIFO ordering in relation to each other,
//and in relation to handler objects. That is, all event_context objects should be
//created as automatic/stack variables.
//
//handler objects need not be created as automatic variables (e.g. you could put
//them in a vector) however you must guarantee that handler objects are destroyed
//before their context is destroyed
struct event_context
{
	event_context();
	~event_context();
};

//causes events to be dispatched to all handler objects.
void pump();

class pump_monitor {
//pump_monitors receive notifcation after an events::pump() occurs
public:
	pump_monitor(bool auto_join = true);
	virtual ~pump_monitor();
	virtual void monitor_process() = 0;
	void join(); /*joins the current monitor*/
private:
	bool has_joined_;
};

int discard(Uint32 event_mask_min, Uint32 event_mask_max);

void raise_process_event();
void raise_draw_event();

extern bool ignore_finger_event;
}

typedef std::vector<events::handler*> handler_vector;

#define INPUT_MASK_MIN SDL_KEYDOWN
#define INPUT_MASK_MAX SDL_MULTIGESTURE

#endif
