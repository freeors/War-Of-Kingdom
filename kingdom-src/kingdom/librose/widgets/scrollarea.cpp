/* $Id: scrollarea.cpp 40489 2010-01-01 13:16:49Z mordante $*/
/*
   Copyright (C) 2004 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file widgets/scrollarea.cpp */

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "widgets/scrollarea.hpp"


namespace gui {

scrollarea::scrollarea(CVideo &video, const bool auto_join)
	: widget(video, auto_join), vscrollbar_(video), hscrollbar_(video, false),
	  old_vposition_(0), recursive_(false), 
	  shown_vscrollbar_(false), shown_hscrollbar_(false),
	  vshown_size_(0), hshown_size_(0), 
	  vfull_size_(0), hfull_size_(0)
{
	vscrollbar_.hide(true);
	hscrollbar_.hide(true);
}

bool scrollarea::has_scrollbar() const
{
	return vshown_size_ < vfull_size_ && vscrollbar_.is_valid_height(location().h);
}

bool scrollarea::has_hscrollbar() const
{
	return hshown_size_ < hfull_size_ && hscrollbar_.is_valid_height(location().w);
}

handler_vector scrollarea::handler_members()
{
	handler_vector h;
	h.push_back(&vscrollbar_);
	h.push_back(&hscrollbar_);
	return h;
}

void scrollarea::update_location(SDL_Rect const &rect)
{
	SDL_Rect r = rect;
	shown_vscrollbar_ = has_scrollbar();
	shown_hscrollbar_ = has_hscrollbar();
	if (shown_vscrollbar_) {
		SDL_Rect r_vscrollbar;
		int w = r.w - vscrollbar_.width();
		r_vscrollbar.x = r.x + w;
		r_vscrollbar.w = vscrollbar_.width();

		r_vscrollbar.y = r.y;
		if (shown_hscrollbar_) {
			r_vscrollbar.h = r.h - hscrollbar_.height();
		} else {
			r_vscrollbar.h = r.h;
		}
		vscrollbar_.set_location(r_vscrollbar);
		r.w = w;
	}
	if (shown_hscrollbar_) {
		SDL_Rect r_hscrollbar;
		int h = r.h - hscrollbar_.height();
		r_hscrollbar.y = r.y + h;
		r_hscrollbar.h = hscrollbar_.height();

		r_hscrollbar.x = r.x;
		r_hscrollbar.w = r.w;
		hscrollbar_.set_location(r_hscrollbar);
		r.h = h;
	}

	if (!hidden()) {
		vscrollbar_.hide(!shown_vscrollbar_);
		hscrollbar_.hide(!shown_hscrollbar_);
	}
	set_inner_location(r);
}

void scrollarea::test_vscrollbar()
{
	if (recursive_)
		return;
	recursive_ = true;
	if (shown_vscrollbar_ != has_scrollbar()) {
		bg_restore();
		bg_cancel();
		update_location(location());
	}
	recursive_ = false;
}

void scrollarea::hide(bool value)
{
	widget::hide(value);
	if (shown_vscrollbar_) {
		vscrollbar_.hide(value);
	}
	if (shown_hscrollbar_) {
		hscrollbar_.hide(value);
	}
}

unsigned scrollarea::get_position() const
{
	return vscrollbar_.get_position();
}

unsigned scrollarea::get_hposition() const
{
	return hscrollbar_.get_position();
}

unsigned scrollarea::get_max_position() const
{
	return vscrollbar_.get_max_position();
}

void scrollarea::set_position(unsigned pos)
{
	vscrollbar_.set_position(pos);
}

void scrollarea::adjust_position(unsigned pos)
{
	vscrollbar_.adjust_position(pos);
}

void scrollarea::move_position(int dep)
{
	vscrollbar_.move_position(dep);
}

void scrollarea::set_shown_size(unsigned h)
{
	vscrollbar_.set_shown_size(h);
	vshown_size_ = h;
	test_vscrollbar();
}

void scrollarea::set_hshown_size(unsigned w)
{
	hscrollbar_.set_shown_size(w);
	hshown_size_ = w;
}

void scrollarea::set_full_size(unsigned h)
{
	vscrollbar_.set_full_size(h);
	vfull_size_ = h;
	test_vscrollbar();
}

void scrollarea::set_hfull_size(unsigned w)
{
	hscrollbar_.set_full_size(w);
	hfull_size_ = w;
}

void scrollarea::set_scroll_rate(unsigned r)
{
	vscrollbar_.set_scroll_rate(r);
}

void scrollarea::process_event()
{
	int grip_vposition = vscrollbar_.get_position();
	int grip_hposition = hscrollbar_.get_position();
	if (grip_vposition == old_vposition_ && grip_hposition == old_hposition_) {
		return;
	}
	old_vposition_ = grip_vposition;
	old_hposition_ = grip_hposition;
	scroll(grip_vposition);
}

SDL_Rect scrollarea::inner_location() const
{
	SDL_Rect r = location();
	if (shown_vscrollbar_) {
		r.w -= vscrollbar_.width();
	}
	if (shown_hscrollbar_) {
		r.h -= hscrollbar_.height();
	}
	return r;
}

unsigned scrollarea::scrollbar_width() const
{
	return vscrollbar_.width();
}

unsigned scrollarea::hscrollbar_height() const
{
	return hscrollbar_.height();
}

void scrollarea::handle_event(const SDL_Event& event)
{
	if (mouse_locked() || hidden() || event.type != SDL_MOUSEBUTTONDOWN)
		return;

	SDL_MouseButtonEvent const &e = event.button;
	if (point_in_rect(e.x, e.y, inner_location())) {
	}
}

} // end namespace gui

