/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/guide.hpp"

#include "gettext.hpp"

#include "display.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/track.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

tguide_::tguide_(display& disp, const std::vector<std::string>& items, int retval)
	: disp_(disp)
	, items_(items)
	, retval_(retval)
	, cursel_(0)
	, can_exit_(false)
{
	VALIDATE(!items.empty(), "Must set items!");
}

void tguide_::pre_show(CVideo& /*video*/, twindow& window)
{
	track_ = find_widget<ttrack>(&window, "image", false, true);
	track_->set_canvas_variable("background_image", variant(null_str));
	track_->set_callback_timer(boost::bind(&tguide::callback_timer, this, _1, _2, _3, _4, _5, twidget::npos));
	track_->set_callback_control_drag_detect(boost::bind(&tguide_::callback_control_drag_detect, this, _1, _2, _3));
	track_->set_callback_set_drag_coordinate(boost::bind(&tguide_::callback_set_drag_coordinate, this, _1, _2, _3));
	track_->set_enable_drag_draw_coordinate(false);

	if (retval_ != twindow::NONE) {
		connect_signal_mouse_left_click(
			*track_
			, boost::bind(
				&tguide_::set_retval
				, this
				, boost::ref(window)
				, retval_));
	}
}

void tguide_::callback_timer(ttrack& widget, surface& frame_buffer, const tpoint& offset, int state2, bool from_timer, int drag_offset_x)
{
	if (from_timer && drag_offset_x == twidget::npos) {
		return;
	}
	if (!from_timer) {
		drag_offset_x = 0;
	}

	SDL_Rect widget_rect = widget.get_rect();

	if (from_timer) {
		sdl_blit(widget.background_surf(), NULL, frame_buffer, &widget_rect);
	}

	const int xsrc = offset.x + widget_rect.x;
	const int ysrc = offset.y + widget_rect.y;

	int left_sel = twidget::npos, right_sel = twidget::npos;
	if (drag_offset_x < 0) {
		left_sel = cursel_;
		if (cursel_ + 1 < (int)items_.size()) {
			right_sel = cursel_ + 1;
		}

	} else if (drag_offset_x > 0) {
		if (cursel_ > 0) {
			left_sel = cursel_ - 1;
		}
		right_sel = cursel_;

	} else {
		left_sel = cursel_;
	}
	
	if (surfs_.empty()) {
		// in order to smooth, preload all image
		std::stringstream err;
		for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
			surfs_.push_back(image::get_image(scale_with_size(*it, widget_rect.w, widget_rect.h)));
			err.str("");
			err << *it << " isn't existed!";
			VALIDATE(surfs_.back(), err.str());
		}
	}

	std::string img;
	surface left_surf, right_surf;
	if (left_sel != twidget::npos) {
		left_surf = surfs_[left_sel];
	}

	if (right_sel != twidget::npos) {
		right_surf = surfs_[right_sel];
	}

	SDL_Rect left_src, left_dst, right_src, right_dst;

	if (!drag_offset_x) {
		VALIDATE(left_sel != twidget::npos && right_sel == twidget::npos, null_str);
		left_src = ::create_rect(0, 0, left_surf->w, left_surf->h);
		left_dst = ::create_rect(xsrc, ysrc, 0, 0);
		sdl_blit(left_surf, NULL, frame_buffer, &left_dst);

		post_blit(frame_buffer, left_sel, left_src, left_dst, right_sel, right_src, right_dst);
		return;
	}

	const int abs_drag_offset_x = abs(drag_offset_x);
	if (left_surf) {
		if (drag_offset_x < 0) {
			left_src = ::create_rect(abs_drag_offset_x, 0, left_surf->w - abs_drag_offset_x, left_surf->h);
			left_dst = ::create_rect(xsrc, ysrc, 0, 0);
		} else {
			left_src = ::create_rect(left_surf->w - abs_drag_offset_x, 0, abs_drag_offset_x, left_surf->h);
			left_dst = ::create_rect(xsrc, ysrc, 0, 0);
		}

		sdl_blit(left_surf, &left_src, frame_buffer, &left_dst);
	}
	if (right_surf) {
		if (drag_offset_x < 0) {
			right_src = ::create_rect(0, 0, abs_drag_offset_x, right_surf->h);
			right_dst = ::create_rect(xsrc + left_surf->w - abs_drag_offset_x, ysrc, 0, 0);
		} else {
			right_src = ::create_rect(0, 0, right_surf->w - abs_drag_offset_x, right_surf->h);
			right_dst = ::create_rect(xsrc + abs_drag_offset_x, ysrc, 0, 0);
		}

		sdl_blit(right_surf, &right_src, frame_buffer, &right_dst);
	}

	post_blit(frame_buffer, left_sel, left_src, left_dst, right_sel, right_src, right_dst);
}

bool tguide_::callback_control_drag_detect(tcontrol* control, bool start, const twidget::tdrag_direction type)
{
	if (!start) {
		int drag_offset_x = control->last_drag_coordinate().x - control->first_drag_coordinate().x;

		if (abs(drag_offset_x) >= (int)control->get_width() / 3) {
			if (drag_offset_x < 0) {
				if (cursel_ + 1 < (int)items_.size()) {
					cursel_ ++;
				} else {
					can_exit_ = true;
				}
			} else if (drag_offset_x > 0 && cursel_ > 0) {
				cursel_ --;
			}
		}
		callback_set_drag_coordinate(control, control->last_drag_coordinate(), control->last_drag_coordinate());
	}

	return false;
}

bool tguide_::callback_set_drag_coordinate(tcontrol* control, const tpoint& first, const tpoint& last)
{
	int drag_offset_x = last.x - first.x;
	ttrack& widget = find_widget<ttrack>(control->get_window(), "image", false);
	callback_timer(widget, widget.get_frame_buffer(), widget.get_frame_offset(), widget.get_state2(), true, drag_offset_x);

	return false;
}

void tguide_::set_retval(twindow& window, int retval)
{
	if (!can_exit_) {
		return;
	}
	window.set_retval(retval);
}

REGISTER_DIALOG(guide)

tguide::tguide(display& disp, const std::vector<std::string>& items, const std::string& startup_img, const int percent)
	: tguide_(disp, items, twindow::OK)
	, startup_img_(startup_img)
	, percent_(percent)
	, startup_rect_(empty_rect)
{}

void tguide::pre_show(CVideo& video, twindow& window)
{
	window.canvas()[0].set_variable("background_image",	variant("dialogs/white-background.png"));

	tguide_::pre_show(video, window);
}

void tguide::post_blit(surface& frame_buffer, int left_sel, const SDL_Rect& left_src, const SDL_Rect& left_dst, int right_sel, const SDL_Rect& right_src, const SDL_Rect& right_dst)
{
	const int last_at = surfs_.size() - 1;
	if (left_sel != last_at && right_sel != last_at) {
		return;
	}
	if (startup_img_.empty()) {
		return;
	}
	surface surf = image::get_image(startup_img_);
	if (!surf) {
		return;
	}

	surface& last_surf = surfs_.back();
	SDL_Rect dst = empty_rect;

	if (left_sel == last_at) {
		VALIDATE(right_sel == twidget::npos, null_str);
		dst.x = left_dst.x + (last_surf->w - surf->w) / 2;
		dst.y = left_dst.y + last_surf->h * percent_ / 100;

		dst.x -= left_src.x;

	} else {
		dst.x = right_dst.x + (last_surf->w - surf->w) / 2;
		dst.y = right_dst.y + last_surf->h * percent_ / 100;

		dst.x -= right_src.x;
	}

	sdl_blit(surf, NULL, frame_buffer, &dst);

	if (left_sel == last_at && !left_src.x) {
		startup_rect_ = ::create_rect(dst.x, dst.y, surf->w, surf->h);
	} else {
		startup_rect_ = empty_rect;
	}
}

void tguide::set_retval(twindow& window, int retval)
{
	if (startup_img_.empty()) {
		tguide_::set_retval(window, retval);
		return;
	}

	const tpoint& first = track_->first_drag_coordinate();
	const tpoint& last = track_->last_drag_coordinate();
	if (first != last || !point_in_rect(first.x, first.y, startup_rect_) || !point_in_rect(last.x, last.y, startup_rect_)) {
		return;
	}

	window.set_retval(retval);
}

}
