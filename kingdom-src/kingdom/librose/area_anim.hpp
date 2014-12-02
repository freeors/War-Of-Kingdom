/* $Id: unit_display.hpp 47623 2010-11-21 13:57:27Z mordante $ */
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
 *  Display units performing various actions: moving, attacking, and dying.
 */

#ifndef AREA_ANIM_HPP_INCLUDED
#define AREA_ANIM_HPP_INCLUDED

#include "config.hpp"
#include "animation.hpp"

class display;

namespace area_anim {
enum ttype {NONE, REINFORCE, INDIVIDUALITY, TACTIC, FORMATION_ATTACK, PERFECT,
	STRATAGEM_UP, STRATAGEM_DOWN, LOCATION, HSCROLL_TEXT, TITLE_SCREEN, 
	LOAD_SCENARIO, FLAGS, TEXT, PLACE, UPGRADE, FOCUS, MAX_ROSE_ANIM = FOCUS};

extern std::map<int, animation> anims;

int find(const std::string& tag);
const std::string& rfind(int tag);

void fill_anims(const config& cfg);
const animation* anim(int type);

struct rt_instance
{
	void set(tanim_type _type, double _zoomx, double _zoomy, const SDL_Rect& _rect)
	{
		type = _type;
		zoomx = _zoomx;
		zoomy = _zoomy;
		rect = _rect;
	}

	tanim_type type;
	double zoomx;
	double zoomy;
	SDL_Rect rect;
};
extern rt_instance rt;

}

// mini callback that app provided
int app_fill_anim_tags(std::map<const std::string, int>& tags);
void app_fill_anim(int type, const config& cfg);

class float_animation: public animation
{
public:
	explicit float_animation(const animation& anim);

	void set_scale(int w, int h, bool constrained, bool up);

	bool invalidate(const surface& screen, frame_parameters& value);
	void redraw(surface& screen, const SDL_Rect& rect);
	void undraw(surface& screen);

	void clear_bufs() { bufs_.clear(); }

private:
	int std_w_;
	int std_h_;
	bool constrained_scale_;
	bool up_scale_;
	std::vector<std::pair<SDL_Rect, surface> > bufs_;
};

float_animation* start_float_anim_th(display& disp, area_anim::ttype type);
void start_float_anim_bh(float_animation& anim);

int start_cycle_float_anim(display& disp, const config& cfg);
void draw_canvas_anim(display& disp, int id, surface& canvas, const SDL_Rect& rect);

#endif
