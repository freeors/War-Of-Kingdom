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

#include "gui/widgets/scroll_label.hpp"

#include "gui/widgets/label.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/scroll_label.hpp"
#include "gui/auxiliary/window_builder/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/layout_exception.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(scroll_label)

tscroll_label::tscroll_label()
	: tscrollbar_container(COUNT)
	, state_(ENABLED)
	, auto_scroll_(false)
	, last_scroll_ticks_(0)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &tscroll_label::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

void tscroll_label::set_label(const std::string& label)
{
	// Inherit.
	tcontrol::set_label(label);

	if (content_grid()) {
		tlabel* widget =
				find_widget<tlabel>(content_grid(), "_label", false, true);
		std::string orig = widget->label();
		if (orig == label) {
			return;
		}
		widget->set_label(label);

		// scroll_label hasn't linked_group widget, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void tscroll_label::finalize_subclass()
{
	assert(content_grid());
	tlabel* lbl = dynamic_cast<tlabel*>(
			content_grid()->find("_label", false));

	assert(lbl);
	lbl->set_label(label());

	/**
	 * @todo wrapping should be a label setting.
	 * This setting shoul be mutual exclusive with the horizontal scrollbar.
	 * Also the scroll_grid needs to set the status for the scrollbars.
	 */
	lbl->set_can_wrap(true);
}

tpoint tscroll_label::calculate_best_size() const
{
	const twindow* window = get_window();
	unsigned w = best_width_(window->variables());

	unsigned maximum_width = settings::screen_width;
	if (w > settings::screen_width) {
		const tpoint vertical_scrollbar = scrollbar_size(*vertical_scrollbar_grid_, vertical_scrollbar_mode_);
		maximum_width = w - settings::screen_width - vertical_scrollbar.x;
	}

	tlabel* label = dynamic_cast<tlabel*>(content_grid_->find("_label", false));

	ttext_maximum_width_lock lock(*label, maximum_width);
	return tscrollbar_container::calculate_best_size();
}

void tscroll_label::set_content_size(const tpoint& origin, const tpoint& desire_size)
{
	tlabel* label = dynamic_cast<tlabel*>(content_grid()->find("_label", false));
	label->set_text_maximum_width(desire_size.x);

	const tpoint actual_size = content_grid_->get_best_size();
	bool changed = calculate_scrollbar(actual_size, desire_size);
	if (changed) {
		label->clear_label_size_cache();
	}

	const tpoint size(std::max(actual_size.x, desire_size.x), std::max(actual_size.y, desire_size.y));
	tscrollbar_container::set_content_size(origin, size);
}

bool tscroll_label::content_empty() const
{
	const tlabel* label = dynamic_cast<const tlabel*>(content_grid()->find("_label", false));
	return label->label().empty();
}

void tscroll_label::set_text_editable(bool editable)
{
	if (content_grid()) {
		tlabel* widget = find_widget<tlabel>(content_grid(), "_label", false, true);
		widget->set_text_editable(editable);
	}
}

bool tscroll_label::exist_anim()
{
	bool dirty = false;
	Uint32 now = SDL_GetTicks();

	if (auto_scroll_ && vertical_scrollbar_->get_visible() == twidget::VISIBLE) {
		const Uint32 interval = 50;
		if (last_scroll_ticks_) {
			if (now >= last_scroll_ticks_ + interval) {
				last_scroll_ticks_ = now;
				dirty = true;

				unsigned item_position = vertical_scrollbar_->get_item_position();
				item_position += 10;
				vertical_scrollbar_->set_item_position(item_position);
				scrollbar_moved();
			}
		}
	} else {
		last_scroll_ticks_ = now;
	}

	if (!dirty) {
		dirty = tcontrol::exist_anim();
	}
	return dirty;
}

const std::string& tscroll_label::get_control_type() const
{
	static const std::string type = "scroll_label";
	return type;
}

void tscroll_label::signal_handler_left_button_down(const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	get_window()->keyboard_capture(this);
}

} // namespace gui2

