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
class ttabbar;

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
	friend class ttabbar;
public:

	treport();
	~treport();

	void init_report(int unit_w, int unit_h, int gap);
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

	bool content_layouted_;
	ttabbar* tabbar_;
};

class ttabbar
{
	friend class treport;
public:
	static const int front_childs = 1; // 1: previous
	static const int back_childs = 2; // 1: stuff, 1: next
	static ttabbar* get_tabbar(twidget* widget);

	ttabbar(bool toggle, bool segment, const std::string& definition);
	~ttabbar();

	void set_report(treport* report, int width = 0, int height = 0);
	treport* report() const { return report_; }
	tcontrol* create_child(const std::string& id, const std::string& tooltip, void* cookie, const std::string& sparam);
	void insert_child(twidget& widget, int at = twidget::npos);
	void erase_child(int at);
	void erase_children(bool clear_additional = false);
	void hide_children();
	void replacement_children();
	void set_visible(int at, bool visible);
	bool get_visible(int at) const;

	int childs() const;
	int get_index(const twidget* widget) const;
	int get_index2(const void* cookie) const;
	const tgrid::tchild& get_child(int at) const;
	tcontrol* get_widget(int at) const;
	void select(int index);
	void select(twidget* widget);
	void set_callback_show(boost::function<void (ttabbar*, const tgrid::tchild& child)> callback) { callback_show_ = callback; }

private:
	void click(bool previous);
	void validate_start();
	int calculate_requrie_back_widgets() const;

private:
	treport* report_;
	bool toggle_;
	bool segment_;
	std::string definition_;
	int start_;

	// when segment_, segment_childs_ is visible widget count at one group.
	// when !segment_, segment_childs_ is visible widget count at this time.
	int segment_childs_;

	tbutton* previous_;
	tspacer* stuff_widget_;
	tbutton* next_;
	boost::function<void (ttabbar*, const tgrid::tchild& child)> callback_show_;
	int max_height_;
};

} // namespace gui2

#endif

