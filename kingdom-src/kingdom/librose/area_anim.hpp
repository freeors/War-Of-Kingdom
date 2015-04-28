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

namespace anim_field_tag {
enum tfield {NONE, X, Y, OFFSET_X, OFFSET_Y, IMAGE, IMAGE_MOD, TEXT, ALPHA};

bool is_progressive_field(tfield field);
tfield find(const std::string& tag);
const std::string& rfind(tfield tag);
}

namespace anim2 {
enum ttype {NONE, LOCATION, HSCROLL_TEXT, TITLE_SCREEN, 
	OPERATING, MIN_APP_ANIM};

int find(const std::string& tag);
const std::string& rfind(int at);

void fill_anims(const config& cfg);
const animation* anim(int at);
void utype_anim_create_cfg(const std::string& anim_renamed_key, const std::string& tpl_id, config& dst, const utils::string_map& symbols);

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

class float_animation: public animation
{
public:
	explicit float_animation(const animation& anim);

	void set_scale(int w, int h, bool constrained, bool up);

	bool invalidate(const surface& screen, frame_parameters& value);
	void redraw(surface& screen, const SDL_Rect& rect);
	void undraw(surface& screen);

	void clear_bufs() { bufs_.clear(); }

	void set_special_rect(const SDL_Rect& rect) { special_rect_ = rect; }
	const SDL_Rect& special_rect() const { return special_rect_; }

private:
	int std_w_;
	int std_h_;
	bool constrained_scale_;
	bool up_scale_;
	SDL_Rect special_rect_;
	std::vector<std::pair<SDL_Rect, surface> > bufs_;
};

float_animation* start_float_anim_th(display& disp, int type, int* id);
void start_float_anim_bh(float_animation& anim, bool cycles);

int start_cycle_float_anim(display& disp, const config& cfg);
void draw_canvas_anim(display& disp, int id, surface& canvas, const SDL_Rect& rect);

#endif
