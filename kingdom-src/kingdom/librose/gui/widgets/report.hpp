/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_REPORT_HPP_INCLUDED
#define GUI_WIDGETS_REPORT_HPP_INCLUDED

#include "gui/widgets/scrollbar_container.hpp"

namespace gui2 {

class tspacer;

namespace implementation {
	struct tbuilder_report;
}

/**
 * Label showing a text.
 *
 * This version shows a scrollbar if the text gets too long and has some
 * scrolling features. In general this widget is slower as the normal label so
 * the normal label should be preferred.
 */
class treport : public tscrollbar_container
{
	friend struct implementation::tbuilder_report;
public:

	treport();

	void init_report(int unit_w, int unit_h, int gap);
	const tpoint& get_unit_size() { return unit_size_; }

	/** Inherited from tcontainer_. */
	void set_self_active(const bool active)
		{ state_ = active ? ENABLED : DISABLED; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	bool get_active() const { return state_ != DISABLED; }

	unsigned get_state() const { return state_; }

	// below 5 function special to report control.
	void insert_child(twidget& widget, size_t at = -1);
	void erase_child(size_t at);
	void replacement_children();
	void erase_children();
	void hide_children();

	void set_content_size(const tpoint& origin, const tpoint& size);

private:
	/** Inherited from tcontrol. */
	void impl_draw_background(
			  surface& frame_buffer
			, int x_offset
			, int y_offset);

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

//  It's not needed for now so keep it disabled, no definition exists yet.
//	void set_state(const tstate state);

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	void finalize_subclass();

	/***** ***** ***** inherited ****** *****/

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_left_button_down(const event::tevent event);

	tpoint unit_size_;
	int gap_;
	bool extendable_;
};

} // namespace gui2

#endif

