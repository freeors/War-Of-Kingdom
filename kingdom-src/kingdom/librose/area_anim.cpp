/* $Id: unit_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#include "global.hpp"
#include "area_anim.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "display.hpp"
#include "wml_exception.hpp"

#include "sound.hpp"
#include <boost/foreach.hpp>

namespace anim_field_tag {

enum tfield {NONE, X, Y, OFFSET_X, OFFSET_Y, IMAGE, IMAGE_MOD, TEXT, ALPHA};
std::map<const std::string, tfield> tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("x", X));
	tags.insert(std::make_pair("Y", Y));
	tags.insert(std::make_pair("offset_x", OFFSET_X));
	tags.insert(std::make_pair("offset_y", OFFSET_Y));
	tags.insert(std::make_pair("image", IMAGE));
	tags.insert(std::make_pair("image_mod", IMAGE_MOD));
	tags.insert(std::make_pair("text", TEXT));
	tags.insert(std::make_pair("alpha", ALPHA));
}

bool is_progressive_field(tfield field)
{
	return field == X || field == Y || field == OFFSET_X || field == OFFSET_Y || field == ALPHA;
}

tfield find(const std::string& tag)
{
	std::map<const std::string, tfield>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(tfield tag)
{
	for (std::map<const std::string, tfield>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

}

namespace area_anim {
std::map<const std::string, int> tags;
std::map<int, animation> anims;
int max_anim;

void fill_tags()
{
	anim_field_tag::fill_tags();

	if (!tags.empty()) return;
	tags.insert(std::make_pair("reinforce", REINFORCE));
	tags.insert(std::make_pair("individuality", INDIVIDUALITY));
	tags.insert(std::make_pair("tactic", TACTIC));
	tags.insert(std::make_pair("formation_attack", FORMATION_ATTACK));
	tags.insert(std::make_pair("perfect", PERFECT));
	tags.insert(std::make_pair("stratagem_up", STRATAGEM_UP));
	tags.insert(std::make_pair("stratagem_down", STRATAGEM_DOWN));
	tags.insert(std::make_pair("location", LOCATION));
	tags.insert(std::make_pair("hscroll_text", HSCROLL_TEXT));
	tags.insert(std::make_pair("title_screen", TITLE_SCREEN));
	tags.insert(std::make_pair("load_scenario", LOAD_SCENARIO));
	tags.insert(std::make_pair("flags", FLAGS));
	tags.insert(std::make_pair("text", TEXT));
	tags.insert(std::make_pair("place", PLACE));
	tags.insert(std::make_pair("upgrade", UPGRADE));
	tags.insert(std::make_pair("focus", FOCUS));

	max_anim = app_fill_anim_tags(tags);
}

int find(const std::string& tag)
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(int tag)
{
	for (std::map<const std::string, int>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

void fill_anims(const config& cfg)
{
	fill_tags();
	anims.clear();

	std::set<int> constructed;
	BOOST_FOREACH (const config &anim, cfg.child_range("area_anim")) {
		const std::string id = anim["id"].str();
		if (id.empty()) {
			throw config::error("[area_anim] error, area_anim must has id attribute");
		}
		const config& sub = anim.child("anim");
		if (!sub) {
			throw config::error("[area_anim] error, area_anim must has [anim] tag");
		}
		int type = find(id);
		if (type == NONE) {
			throw config::error("[area_anim] error, area_anim has invalid id: " + id);
		}
		if (constructed.find(type) != constructed.end()) {
			throw config::error("[area_anim] error, duplicate id: " + id);
		}
		if (type <= max_anim) {
			anims.insert(std::make_pair(type, animation(sub)));
		} else {
			app_fill_anim(type, sub);
		}
	}
}

const animation* anim(int type)
{
	std::map<int, animation>::const_iterator i = anims.find(type);
	if (i != anims.end()) {
		return &i->second;
	}
	return NULL;
}

rt_instance rt;

}

float_animation::float_animation(const animation& anim)
	: animation(anim)
	, std_w_(0)
	, std_h_(0)
	, constrained_scale_(true)
	, up_scale_(false)
	, bufs_()
{
}

namespace {
struct event {
	int x, y, w, h;
	bool in;
	event(const SDL_Rect& rect, bool i) : x(i ? rect.x : rect.x + rect.w), y(rect.y), w(rect.w), h(rect.h), in(i) { }
};
bool operator<(const event& a, const event& b) {
	if (a.x != b.x) return a.x < b.x;
	if (a.in != b.in) return a.in;
	if (a.y != b.y) return a.y < b.y;
	if (a.h != b.h) return a.h < b.h;
	if (a.w != b.w) return a.w < b.w;
	return false;
}
bool operator==(const event& a, const event& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.in == b.in;
}

struct segment {
	int x, count;
	segment() : x(0), count(0) { }
	segment(int x, int count) : x(x), count(count) { }
};


std::vector<SDL_Rect> update_rects;
std::vector<event> events;
std::map<int, segment> segments;

void calc_rects()
{
	events.clear();

	BOOST_FOREACH(SDL_Rect const &rect, update_rects) {
		events.push_back(event(rect, true));
		events.push_back(event(rect, false));
	}

	std::sort(events.begin(), events.end());
	std::vector<event>::iterator events_end = std::unique(events.begin(), events.end());

	segments.clear();
	update_rects.clear();

	for (std::vector<event>::iterator iter = events.begin(); iter != events_end; ++iter) {
		std::map<int, segment>::iterator lower = segments.find(iter->y);
		if (lower == segments.end()) {
			lower = segments.insert(std::make_pair(iter->y, segment())).first;
			if (lower != segments.begin()) {
				std::map<int, segment>::iterator prev = lower;
				--prev;
				lower->second = prev->second;
			}
		}

		if (lower->second.count == 0) {
			lower->second.x = iter->x;
		}

		std::map<int, segment>::iterator upper = segments.find(iter->y + iter->h);
		if (upper == segments.end()) {
			upper = segments.insert(std::make_pair(iter->y + iter->h, segment())).first;
			std::map<int, segment>::iterator prev = upper;
			--prev;
			upper->second = prev->second;
		}

		if (iter->in) {
			while (lower != upper) {
				++lower->second.count;
				++lower;
			}
		} else {
			while (lower != upper) {
				lower->second.count--;
				if (lower->second.count == 0) {
					std::map<int, segment>::iterator next = lower;
					++next;

					int x = lower->second.x, y = lower->first;
					unsigned w = iter->x - x;
					unsigned h = next->first - y;
					SDL_Rect a = create_rect(x, y, w, h);

					if (update_rects.empty()) {
						update_rects.push_back(a);
					} else {
						SDL_Rect& p = update_rects.back(), n;
						int pa = p.w * p.h, aa = w * h, s = pa + aa;
						int thresh = 51;

						n.w = std::max<int>(x + w, p.x + p.w);
						n.x = std::min<int>(p.x, x);
						n.w -= n.x;
						n.h = std::max<int>(y + h, p.y + p.h);
						n.y = std::min<int>(p.y, y);
						n.h -= n.y;

						if (s * 100 < thresh * n.w * n.h) {
							update_rects.push_back(a);
						} else {
							p = n;
						}
					}

					if (lower == segments.begin()) {
						segments.erase(lower);
					} else {
						std::map<int, segment>::iterator prev = lower;
						--prev;
						if (prev->second.count == 0) segments.erase(lower);
					}

					lower = next;
				} else {
					++lower;
				}
			}
		}
	}
}

}

bool float_animation::invalidate(const surface& screen, frame_parameters& value)
{
	value.primary_frame = t_true;
	std::vector<SDL_Rect> rects = unit_anim_.get_overlaped_rect(value, src_, dst_);
	value.primary_frame = t_false;
	for (std::map<std::string, particular>::iterator anim_itor = sub_anims_.begin(); anim_itor != sub_anims_.end() ; ++ anim_itor) {
		std::vector<SDL_Rect> tmp = anim_itor->second.get_overlaped_rect(value, src_, dst_);
		rects.insert(rects.end(), tmp.begin(), tmp.end());
	}

	// display* disp = display::get_singleton();
	const SDL_Surface& fb = *screen;
	update_rects.clear();

	for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		SDL_Rect rect = *it;
		if (rect.x < 0) {
			if (rect.x * -1 >= int(rect.w)) {
				continue;
			}
			rect.w += rect.x;
			rect.x = 0;
		}
		if (rect.y < 0) {
			if (rect.y*-1 >= int(rect.h)) {
				continue;
			}
			rect.h += rect.y;
			rect.y = 0;
		}
		if (rect.x >= fb.w || rect.y >= fb.h) {
			continue;
		}
		if (rect.x + rect.w > fb.w) {
			rect.w = fb.w - rect.x;
		}
		if (rect.y + rect.h > fb.h) {
			rect.h = fb.h - rect.y;
		}

		update_rects.push_back(rect);
	}

	calc_rects();

	bufs_.clear();
	for (std::vector<SDL_Rect>::const_iterator it = update_rects.begin(); it != update_rects.end(); ++ it) {
		const SDL_Rect& rect = *it;
		bufs_.push_back(std::make_pair(rect, surface()));
	}
	return false;
}

void float_animation::set_scale(int w, int h, bool constrained, bool up)
{
	VALIDATE((w > 0 && h > 0) || !w && !h, "both w and h must large than 0 or equal 0!");
	std_w_ = w;
	std_h_ = h;
	constrained_scale_ = constrained;
	up_scale_ = up;
}

void float_animation::redraw(surface& screen, const SDL_Rect& rect)
{
	double scale_x = std_w_? 1.0 * rect.w / std_w_: 1.0;
	double scale_y = std_h_? 1.0 * rect.h / std_h_: 1.0;
	if (constrained_scale_ && scale_x != scale_y) {
		if (scale_x > scale_y) {
			scale_x = scale_y;
		} else {
			scale_y = scale_x;
		}
	}
	if (!up_scale_) {
		if (scale_x > 1) {
			scale_x = 1;
		}
		if (scale_y > 1) {
			scale_y = 1;
		}
	}
	area_anim::rt.set(type_, scale_x, scale_y, rect);

	animation::invalidate(screen);
	for (std::vector<std::pair<SDL_Rect, surface> >::iterator it = bufs_.begin(); it != bufs_.end(); ++ it) {
		SDL_Rect rect = it->first;

		it->second = create_compatible_surface(screen, rect.w, rect.h);

		// to copy alpha, must set src-alpha to blendmode_none.
		SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
		SDL_GetSurfaceBlendMode(screen, &blendMode);
		if (blendMode != SDL_BLENDMODE_NONE) {
			SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_NONE);
		}
		sdl_blit(screen, &rect, it->second, NULL);
		if (blendMode != SDL_BLENDMODE_NONE) {
			SDL_SetSurfaceBlendMode(screen, blendMode);
		}

		SDL_SetSurfaceBlendMode(it->second, SDL_BLENDMODE_NONE);
	}

	animation::redraw(screen, rect);
}

void float_animation::undraw(surface& screen)
{
	for (std::vector<std::pair<SDL_Rect, surface> >::const_reverse_iterator it = bufs_.rbegin(); it != bufs_.rend(); ++ it) {
		SDL_Rect rect = it->first;
		sdl_blit(it->second, NULL, screen, &rect);
	}
}


float_animation* start_float_anim_th(display& disp, area_anim::ttype type)
{
	const animation* tpl = area_anim::anim(type);
	if (!tpl) {
		return NULL;
	}

	float_animation* clone = new float_animation(*tpl);
	clone->set_type(anim_float);
	disp.insert_area_anim2(clone);
	return clone;
}

void start_float_anim_bh(float_animation& anim)
{
	new_animation_frame();
	anim.start_animation(0);
}

int start_cycle_float_anim(display& disp, const config& cfg)
{
	int type = area_anim::find(cfg["id"]);
	if (type == area_anim::NONE) {
		return INVALID_ANIM_ID;
	}
	const animation* tpl = area_anim::anim(type);
	float_animation* clone = new float_animation(*tpl);
		
	int id = disp.insert_area_anim2(clone);

	float_animation& anim = *clone;
	anim.set_type(anim_canvas);
	anim.set_scale(cfg["width"].to_int(), cfg["height"].to_int(), cfg["constrained"].to_bool(), cfg["up"].to_bool(true));

	std::vector<std::string> vstr;
	BOOST_FOREACH (const config::attribute& attr, cfg.attribute_range()) {
		if (attr.first.find("-") == std::string::npos) {
			continue;
		}
		vstr = utils::split(attr.first, '-');
		if (vstr.size() != 2) {
			continue;
		}
		anim_field_tag::tfield field = anim_field_tag::find(vstr[0]);
		if (field == anim_field_tag::NONE) {
			continue;
		}
		if (anim_field_tag::is_progressive_field(field)) {
			anim.replace_progressive(vstr[0], vstr[1], attr.second);

		} else if (field == anim_field_tag::IMAGE) {
			anim.replace_image_name(vstr[1], attr.second);

		} else if (field == anim_field_tag::IMAGE_MOD) {
			anim.replace_image_mod(vstr[1], attr.second);

		} else if (field == anim_field_tag::TEXT) {
			anim.replace_static_text(vstr[1], attr.second);

		}
	}

	new_animation_frame();
	anim.start_animation(0, map_location::null_location, map_location::null_location, true);
	return id;
}

void draw_canvas_anim(display& disp, int id, surface& canvas, const SDL_Rect& rect)
{
	if (id == INVALID_ANIM_ID) {
		return;
	}
	float_animation& anim = *dynamic_cast<float_animation*>(&disp.area_anim(id));
	if (!anim.started()) {
		return;
	}

	new_animation_frame();
	anim.update_last_draw_time();
	anim.redraw(canvas, rect);

	disp.drawing_buffer_commit(canvas);
}