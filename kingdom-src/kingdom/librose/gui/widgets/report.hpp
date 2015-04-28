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
#include <boost/function.hpp>

namespace gui2 {

class tspacer;
class tbutton;
class tpanel;

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

	treport(int unit_w, int unit_h, int gap);

	const tpoint& get_unit_size() const { return unit_size_; }
	int get_unit_width() const { return unit_size_.x; }
	int get_unit_height() const { return unit_size_.y; }

	/** Inherited from tcontainer_. */
	void set_self_active(const bool active)
		{ state_ = active ? ENABLED : DISABLED; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	bool get_active() const { return state_ != DISABLED; }

	unsigned get_state() const { return state_; }

	// below 5 function special to report control.
	void insert_child(twidget& widget, int at = npos);
	void erase_child(int at);
	void replacement_children();
	void erase_children();
	void hide_children();

	void layout_init(const bool full_initialization);
	tpoint calculate_best_size() const;
	void place_content_grid(const tpoint& content_origin, const tpoint& content_size, const tpoint& desire_origin);

	static treport* get_report(const twidget* widget);

	//
	// tabbar function
	//
	void tabbar_init(bool toggle, const std::string& definition, bool always = false, int width = 0, int height = 0, int gap = npos);

	tcontrol* create_child(const std::string& id, const std::string& tooltip, void* cookie);
	void set_child_visible(int at, bool visible);
	bool get_child_visible(int at) const;

	int get_at(const twidget* widget) const;
	int get_at2(const void* cookie) const;
	const tgrid::tchild& get_child(int at) const;
	tcontrol* get_widget(int at) const;
	void select(int index);
	void select(twidget* widget);
	tcontrol* cursel() const;
	void tabbar_set_callback_show(boost::function<void (treport*, tcontrol* widget)> callback) { callback_show_ = callback; }

	void multiline_init(bool toggle, const std::string& definition, int width = 0, int height = 0, int gap = npos, int fixed_cols = npos);
	void multiline_set(int gap, int fixed_cols = npos);

	void set_boddy(twidget* boddy);

	const tgrid::tchild* child_begin() const;
	int childs() const;

private:
	void tabbar_erase_children();
	void tabbar_hide_children();
	void tabbar_replacement_children();

	void multiline_replacement_children();

	bool pre_toggle(twidget* widget);
	void click(bool previous);
	void validate_start();
	int calculate_requrie_back_widgets() const;

	void click_report(twidget* widget, bool& handled, bool& halt);

private:
	/** Inherited from tcontrol. */
	void impl_draw_background(
			  surface& frame_buffer
			, int x_offset
			, int y_offset);

	void finalize_subclass();

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_left_button_down(const event::tevent event);

private:
	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	tpoint unit_size_;
	int gap_;

	// true: it is multi-line report.
	// false: it is single-line report. same as tabbar
	bool extendable_;

	int front_childs_;
	int back_childs_;

	//
	// tabbar member
	//

	// true: widget is toggle_button
	// false: widget is button
	bool toggle_;

	// true:  space for the previous/next is always reserved,
	// false: space for the previous/next is reserved according to current state.
	bool segment_;

	// when segment_, segment_childs_ is visible widget count at one group.
	// when !segment_, segment_childs_ is visible widget count at this time.
	int segment_childs_;

	// valid only multiline report.
	int fixed_cols_;

	std::string definition_;
	int start_;

	tbutton* previous_;
	tspacer* stuff_widget_;
	tbutton* next_;
	boost::function<void (treport*, tcontrol* widget)> callback_show_;
	int max_height_;

	tpanel* boddy_;
};

} // namespace gui2

#endif

