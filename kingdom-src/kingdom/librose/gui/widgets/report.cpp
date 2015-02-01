/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/report.hpp"

#include "gui/widgets/label.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/report.hpp"
#include "gui/auxiliary/window_builder/report.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/layout_exception.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(report)

treport::treport()
	: tscrollbar_container(COUNT)
	, state_(ENABLED)
	, unit_size_(0, 0)
	, gap_(0)
{
	self_layout_content_ = true;

	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &treport::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

void treport::init_report(int unit_w, int unit_h, int gap)
{
	unit_size_.x = unit_w;
	unit_size_.y = unit_h;
	gap_ = gap;
}

void treport::insert_child(twidget& widget, size_t at)
{
	if (!content_grid_->get_cols()) {
		VALIDATE(w_ >= (unsigned)(unit_size_.x + gap_ + unit_size_.x), "Width of report must not less than 2*unit_w + gap_w!");
		VALIDATE(get_horizontal_scrollbar_mode() == always_invisible, "Don't support horizontal scrollbar!");
		extendable_ = get_vertical_scrollbar_mode() != always_invisible;

		content_grid_->init_report(unit_size_.x, unit_size_.y,gap_, extendable_);
	}
	content_grid_->insert_child(unit_size_.x, unit_size_.y, widget, at, extendable_);
}

void treport::erase_child(size_t at)
{
	content_grid_->erase_child(at, extendable_);
}

void treport::replacement_children()
{
	content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_);
	if (extendable_) {
		content_resize_request();
	}
}

void treport::erase_children()
{
	content_grid_->erase_children(unit_size_.x, unit_size_.y, gap_, extendable_);

	if (extendable_) {
		content_resize_request();
	}
}

void treport::hide_children()
{
	content_grid_->hide_children(unit_size_.x, unit_size_.y, gap_, extendable_);
	if (extendable_) {
		content_resize_request();
	}
}

void treport::set_content_size(const tpoint& origin, const tpoint& size)
{
	const SDL_Rect& rect = content_grid_->fix_rect();
	if (!unit_size_.x || !rect.w || !rect.h) {
		content_grid_->place(origin, size);
	}
}

void treport::finalize_subclass()
{
	VALIDATE(content_grid_, "content_grid_ must not be NULL!");
}

void treport::impl_draw_background(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	content_grid_->update_last_draw_end();
	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);
}

const std::string& treport::get_control_type() const
{
	static const std::string type = "report";
	return type;
}

void treport::signal_handler_left_button_down(const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	get_window()->keyboard_capture(this);
}

} // namespace gui2

