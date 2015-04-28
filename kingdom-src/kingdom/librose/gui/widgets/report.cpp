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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/report.hpp"

#include "gui/widgets/label.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/report.hpp"
#include "gui/auxiliary/window_builder/report.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/dialog.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(report)

treport::treport(int unit_w, int unit_h, int gap)
	: tscrollbar_container(COUNT)
	, state_(ENABLED)
	, unit_size_(unit_w, unit_h)
	, gap_(gap)
	, toggle_(true)
	, segment_(false)
	, fixed_cols_(npos)
	, definition_()
	, extendable_(false)
	, front_childs_(0)
	, back_childs_(0)
	, start_(0)
	, segment_childs_(0)
	, max_height_(0)
	, previous_(NULL)
	, stuff_widget_(NULL)
	, next_(NULL)
	, boddy_(NULL)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &treport::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

void treport::finalize_subclass()
{
	VALIDATE(content_grid_, "content_grid_ must not be NULL!");

	extendable_ = get_vertical_scrollbar_mode() != always_invisible;

	if (!extendable_) {
		VALIDATE(get_horizontal_scrollbar_mode() == always_invisible, "Don't support horizontal scrollbar in tabbar!");
	}
}

void treport::insert_child(twidget& widget, int at)
{
	VALIDATE(!definition_.empty(), "Must call tabbar_init or multiline_init before insert_child!");

	if (at != twidget::npos) {
		at += front_childs_;
	} else {
		int size = content_grid_->children_vsize();
		if (size >= front_childs_ + back_childs_) {
			at = size - back_childs_;
		}
	}

	content_grid_->insert_child(unit_size_.x, unit_size_.y, widget, at);

	if (!extendable_) {
		validate_start();
	}
}

#define get_visible2(children, at)	(!((children)[at].flags_ & tgrid::USER_PRIVATE))

void treport::erase_child(int at)
{
	const tgrid::tchild* children = content_grid()->children();
	bool original_visible = get_visible2(children, front_childs_ + at);

	content_grid_->erase_child(at + front_childs_);

	if (original_visible) {
		validate_start();
	}
}

void treport::replacement_children()
{
	if (content_grid_->get_width() <= 0) {
		return;
	}

	if (!extendable_) {
		tabbar_replacement_children();

	} else {
		content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);

		// let next twindow::draw draw whole report. multi-line report require scroll-bar.
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::erase_children()
{
	if (!extendable_) {
		tabbar_erase_children();
	} else {
		content_grid_->erase_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);

		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::hide_children()
{
	if (!extendable_) {
		tabbar_hide_children();

	} else {
		content_grid_->hide_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::layout_init(const bool full_initialization)
{
	const gui2::tgrid::tchild* children = content_grid_->children();
	int childs = content_grid_->children_vsize();

	if (!extendable_ && childs) {
		children[0].widget_->set_visible(twidget::VISIBLE);
		int max_height = children[0].widget_->get_best_size().y;
		if (!unit_size_.x) {
			// after layout_init, gui2 enter calculate_best_size.
			// because this set almost invisible, calculate content_grid of report is mistake.
			// width is determinated by window.
			// height is determinated by report.
			int height;
			for (int n = 1; n < childs - back_childs_; n ++) {
				children[n].widget_->set_visible(twidget::INVISIBLE);
				height = children[n].widget_->get_best_size().y;
				if (height > max_height) {
					max_height = height;
				}
			}
		}
		max_height_ = max_height;
		// let stuff widget's to be mixman.
/*
		children[childs].widget_->set_origin(tpoint(0, 0));
		content_grid_->update_last_draw_end();
*/
	}

	tscrollbar_container::layout_init(full_initialization);
}

tpoint treport::calculate_best_size() const
{
	tpoint result = tscrollbar_container::calculate_best_size();
	if (!extendable_) {
		if (result.y < max_height_) {
			result.y = max_height_;
		}
	}
	return result;
}

void treport::place_content_grid(const tpoint& content_origin, const tpoint& content_size, const tpoint& desire_origin)
{
	if (!extendable_) {
		content_grid_->twidget::place(desire_origin, content_size);
		replacement_children();

	} else {
		const SDL_Rect& rect = content_grid_->fix_rect();
		if (!rect.w || !rect.h) {
			content_grid_->twidget::place(desire_origin, content_size);
			replacement_children();
		} else {
			const tpoint size(std::max(rect.w, content_size.x), std::max(rect.h, content_size.y));
			tpoint origin2 = validate_content_grid_origin(content_origin, content_size, desire_origin, size);
			if (origin2 != desire_origin) {
				content_grid_->set_origin(origin2);
			}
		}
	}
}

void treport::impl_draw_background(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	if (!extendable_) {
		content_grid_->update_last_draw_end();
	}
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

treport* treport::get_report(const twidget* widget)
{
	twidget* content_grid = widget->parent();
	treport* report = dynamic_cast<treport*>(content_grid->parent());
	return report;
}

void treport::validate_start()
{
	if (!start_) {
		return;
	}
	gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;

	if (segment_) {
		// to segment_, start_ must be integer times of segment_childs_
		start_ -= start_ % segment_childs_;
	}
	int n, tmp;
	for (n = front_childs_, tmp = 0; n < childs && tmp <= start_; n ++) {
		if (get_visible2(children, n)) {
			tmp ++;
		}
	}
	while (start_ && tmp <= start_) {
		if (segment_) {
			start_ -= segment_childs_;
		} else {
			start_ --;
		}
	}
}

int treport::calculate_requrie_back_widgets() const
{
	VALIDATE(!segment_ && start_, "Must in !segment_ and (start_ != 0)");
		
	const tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;

	int n = front_childs_ - 1, tmp = 0;
	// 1. calculate n that crospond to start_.
	do {
		n ++;
		if (get_visible2(children, n)) {
			tmp ++;
		}
	} while (tmp != start_);


	int grid_width = content_grid_->get_width();
	int additive_width = next_->fix_width();
	int unit_w = unit_size_.x;
	int last_width = 0;

	// 2. backward caculate how many widgets can hold?
	for (tmp = 0, n --; n >= 0 && additive_width < grid_width; n --) {
		if (!get_visible2(children, n)) {
			continue;
		}
		twidget* widget = children[n].widget_;

		if (!unit_size_.x) {
			unit_w = widget->get_best_size().x;
		}
		additive_width += unit_w + gap_;
		last_width = unit_w;
		tmp ++;
	}
	if (tmp < start_) {
		// require visible "previous"
		additive_width -= last_width + gap_;
		tmp --;

		if (grid_width - additive_width < previous_->fix_width()) {
			// invisible one still cannot hold "previous", invisible more.
			// assume "previous" width cannot exceed tow width.
			tmp --;
		}
	} else if (additive_width > grid_width) {
		// needn't visible "previous"
		tmp --;
	}
	return tmp;
}

void treport::click(bool previous)
{
	if (previous) {
		if (segment_) {
			start_ -= segment_childs_;
		} else {
			start_ -= calculate_requrie_back_widgets();
		}
	} else {
		start_ += segment_childs_;
	}

	tabbar_replacement_children();

	if (previous) {
		// when back to #0 group, preview button cannot redraw. dirty whole report make it redraw.
		// On the other handle, once press "preview", the whole bar should redraw, so dirty whole report is acceptable.
		set_dirty();
	}
}

void treport::tabbar_init(bool toggle, const std::string& definition, bool always, int width, int height, int gap)
{
	VALIDATE(!content_grid_->children_vsize(), "Duplicate call!");
	VALIDATE(!extendable_, "It is multi-line report, cannot call tabbar_init!");

	std::string type = toggle? "toggle_button": "button";
	VALIDATE(valid_control_definition(type, definition), "Muset set a valid definition!");

	front_childs_ = 1; // 1: previous
	back_childs_ = 2; // 1: stuff, 1: next

	toggle_ = toggle;
	definition_ = definition;
	start_ = 0;
	segment_childs_ = 0;
	segment_ = always;

	if (gap != npos) {
		gap_ = gap;
	}

	// calculate width/height of previous/next.
	if (unit_size_.x) {
		width = unit_size_.x;
		height = unit_size_.y;
	}
	if (!width) {
		std::string type = toggle_? "toggle_button": "button";
		tresolution_definition_ptr ptr = get_control(type, definition_);
		width = ptr->default_height;
		height = ptr->default_height;
	}
	VALIDATE(width > 0 && height > 0, "Must valid width and height!");

	// previous arrow
	previous_ = create_surface_button("previous", NULL);
	previous_->set_fix_size(width, height);
	previous_->set_surface(image::get_image("misc/arrow-left.png"), width, height);
	connect_signal_mouse_left_click(
		*previous_
		, boost::bind(
			&treport::click
			, this
			, true));

	stuff_widget_ = create_spacer("sutff");
	if (unit_size_.x) {
		stuff_widget_->set_visible(twidget::INVISIBLE);
	}

	// next arrow
	next_ = create_surface_button("next", NULL);
	next_->set_fix_size(width, height);
	next_->set_surface(image::get_image("misc/arrow-right.png"), width, height);
	connect_signal_mouse_left_click(
		*next_
		, boost::bind(
			&treport::click
			, this
			, false));

	insert_child(*previous_);
	insert_child(*stuff_widget_);
	insert_child(*next_);
}

void treport::multiline_init(bool toggle, const std::string& definition, int width, int height, int gap, int fixed_cols)
{
	VALIDATE(!content_grid_->children_vsize(), "Duplicate call!");
	VALIDATE(extendable_, "It is tabbar report, cannot call multiline_init!");

	std::string type = toggle? "toggle_button": "button";
	VALIDATE(valid_control_definition(type, definition), "Muset set a valid definition!");

	toggle_ = toggle;
	definition_ = definition;
	start_ = 0;
	segment_childs_ = 0;

	if (!unit_size_.x) {
		unit_size_.x = width;
	}
	if (!unit_size_.y) {
		unit_size_.y = height;
	}
	if (!unit_size_.x) {
		tresolution_definition_ptr ptr = get_control(type, definition);
		unit_size_.x = ptr->default_width;
		unit_size_.y = ptr->default_height;
	}
	VALIDATE(unit_size_.x > 0 && unit_size_.y > 0, "Must valid width and height!");

	if (gap != npos) {
		gap_ = gap;
	}
	fixed_cols_ = fixed_cols;
}

void treport::multiline_set(int gap, int fixed_cols)
{
	if (gap != npos) {
		gap_ = gap;
	}
	if (fixed_cols_ != fixed_cols) {
		content_grid_->clear_stuff_widget();
		fixed_cols_ = fixed_cols;
	}

	replacement_children();
}

void treport::multiline_replacement_children()
{
	content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);

	// let next twindow::draw draw whole report. multi-line report require scroll-bar.
	// it must be same-size, to save time, don't calcuate linked_group.
	invalidate_layout(false);
}

bool treport::pre_toggle(twidget* widget)
{
	tdialog* dlg = dialog();
	tcontrol* previous = cursel();

	VALIDATE(previous, "previous must not be NULL!");
	return dlg->pre_toggle_tabbar(widget, previous);
}

void treport::click_report(twidget* widget, bool& handled, bool& halt)
{
	tdialog* dlg = dialog();
	if (dlg->click_report(widget)) {
		return;
	}

	twindow* window = get_window();

	erase_child(get_at(widget));
	replacement_children();

	handled = true;
	halt = true;
}

tcontrol* treport::create_child(const std::string& id, const std::string& tooltip, void* cookie)
{
	tcontrol* widget;
	if (toggle_) {
		ttoggle_button* widget2 = create_toggle_button(id, definition_, cookie);
		widget2->set_tooltip(tooltip);
		widget2->set_radio(true);
		widget2->set_callback_state_pre_change(boost::bind(&treport::pre_toggle, this, _1));
		widget2->set_callback_state_change(boost::bind(&tdialog::toggle_report, dialog(), _1));
		widget = widget2;

	} else {
		widget = create_button(id, definition_, cookie);
		widget->set_tooltip(tooltip);
		connect_signal_mouse_left_click(
			*widget
			, boost::bind(
				&treport::click_report
				, this
				, widget
				, _3, _4));
	}
	return widget;
}

void treport::tabbar_erase_children()
{
	// user call it to erase children. 
	// window don't call it. it erase children in tgrid::~tgrid.
	tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();

	int i;
	if (false) {
		// back childs
		for (i = childs - 1; i > childs - back_childs_ - 1; i --) {
			children[i].widget_->set_parent(NULL);
			children[i].widget_ = NULL;
			content_grid_->erase_child(i);
		}
		next_ = NULL;
		stuff_widget_ = NULL;
	}

	// content childs
	for (i = childs - back_childs_ - 1; i > 0; i --) {
		content_grid_->erase_child(i);
	}

	if (false) {
		// front childs
		children[0].widget_->set_parent(NULL);
		children[0].widget_ = NULL;
		content_grid_->erase_child(0);
		previous_ = NULL;
	}

	start_ = 0;
	segment_childs_ = 0;
}

void treport::tabbar_hide_children()
{
	content_grid_->hide_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);

	start_ = 0;
	segment_childs_ = 0;
}

void treport::tabbar_replacement_children()
{
	const tgrid::tchild* children = content_grid()->children();
	int vsize = content_grid_->children_vsize();

	int grid_width = content_grid_->get_width();
	int grid_height = content_grid_->get_height();
	if (grid_width <= 0 || !vsize) {
		return;
	}
	
	int unit_w = unit_size_.x;
	if (segment_) {
		if (!segment_childs_) {
			segment_childs_ = (grid_width + gap_) / (unit_size_.x + gap_) - 2;
			VALIDATE(segment_childs_ > 0, "It is segment tabbar, accommodatable widgets must be large than 2!");
		}
	} else {
		segment_childs_ = 0;
	}

	int last_n = twidget::npos, last_width = 0;
	bool require_next = false;

	int n = front_childs_;
	for (int tmp = 0; tmp < start_; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		// these widget not in current group, as if user require visilbe, but invisible.
		widget->set_visible(twidget::INVISIBLE);

		if (get_visible2(children, n)) {
			tmp ++;
		}
	}

	twidget::tvisible previous_visible = twidget::INVISIBLE;
	if (start_) {
		previous_visible = twidget::VISIBLE;
	} else if (segment_) {
		previous_visible = twidget::HIDDEN;
	}
	children[0].widget_->set_visible(previous_visible);
	int additive_width = previous_visible != twidget::INVISIBLE? children[0].widget_->fix_width(): 0;
	int additive_child = 0;

	int end = vsize - back_childs_;
	for (; n < end; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		if (!get_visible2(children, n)) {
			// USER_PRIVATE(=1): user force it invisible.
			widget->set_visible(twidget::INVISIBLE);
			continue;
		}

		if (!unit_size_.x) {
			unit_w = widget->get_best_size().x;
		}
		if (require_next || additive_width + unit_w > grid_width || (segment_ && additive_child == segment_childs_)) {
			widget->set_visible(twidget::INVISIBLE);
			if (!require_next) {
				if (!segment_) {
					if (grid_width - additive_width >= next_->fix_width()) {
						last_n = twidget::npos;
					} else {
						// residue width cannot hold "next" button, invisible last widget to lengthen residue width.
						additive_width -= last_width + gap_;
					}
				}
				require_next = true;
			}
			continue;
		}
		widget->set_visible(twidget::VISIBLE);
		if (callback_show_) {
			callback_show_(this, dynamic_cast<tcontrol*>(child.widget_));
		}

		if (!segment_) {
			additive_width += unit_w + gap_;
			last_n = n;
			last_width = unit_w;
			segment_childs_ ++;
		} else {
			additive_child ++;
		}
	}

	int spacer_width = 0;
	if (require_next) {
		if (last_n != twidget::npos) {
			children[last_n].widget_->set_visible(twidget::INVISIBLE);
			segment_childs_ --;
		}
		spacer_width = grid_width - additive_width - next_->fix_width();
	}
	if (!unit_size_.x) {
		// when fix size, don't insert spacer. this width maybe diffirent from fix width.
		stuff_widget_->set_best_size(tpoint(spacer_width, grid_height));
		stuff_widget_->set_visible(twidget::VISIBLE);
	}
	next_->set_visible(require_next? twidget::VISIBLE: twidget::INVISIBLE);

	content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_, fixed_cols_, *content_);

	if (boddy_) {
		twidget* widget = cursel();
		if (widget && widget->get_visible() == twidget::VISIBLE) {
			boddy_->set_hole_variable(widget->get_x(), widget->get_x() + widget->get_width());
		} else {
			boddy_->set_hole_variable(0, 0);
		}
	}
}

const tgrid::tchild* treport::child_begin() const
{
	return content_grid_->children() + front_childs_;
}

int treport::childs() const
{
	return content_grid_->children_vsize() - front_childs_ - back_childs_;
}

void treport::set_boddy(twidget* boddy) 
{ 
	boddy_ = dynamic_cast<tpanel*>(boddy);
}

void treport::set_child_visible(int at, bool visible)
{
	at += front_childs_;
	tgrid::tchild* children = content_grid_->children();
	if (get_visible2(children, at) == visible) {
		return;
	}
	tgrid::tchild& child = children[at];
	if (!visible) {
		child.flags_ |= tgrid::USER_PRIVATE;
	} else {
		child.flags_ &= ~tgrid::USER_PRIVATE;
	}

	validate_start();
}

bool treport::get_child_visible(int at) const
{
	const tgrid::tchild* children = content_grid_->children();
	return get_visible2(children, at + front_childs_);
}

int treport::get_at(const twidget* widget) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;
	for (int i = front_childs_; i < childs; i ++) {
		if (children[i].widget_ == widget) {
			return i - front_childs_;
		}
	}
	return twidget::npos;
}

int treport::get_at2(const void* cookie) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;
	for (int i = front_childs_; i < childs; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			return i - front_childs_;
		}
	}
	return twidget::npos;
}

const tgrid::tchild& treport::get_child(int at) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	return children[front_childs_ + at];
}

tcontrol* treport::get_widget(int at) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	return dynamic_cast<tcontrol*>(children[front_childs_ + at].widget_);
}

void treport::select(int index)
{
	if (!toggle_) {
		return;
	}
	const tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();
	if (childs < front_childs_ + index + back_childs_) {
		return;
	}
	select(children[front_childs_ + index].widget_);
}

void treport::select(twidget* widget)
{
	if (!toggle_) {
		return;
	}

	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();
	for (int i = front_childs_; i < childs - back_childs_; i ++) {
		ttoggle_button* that = dynamic_cast<ttoggle_button*>(children[i].widget_);
		if (!get_visible2(children, i)) {
			continue;
		}
		if (that != widget && that->get_value()) {
			that->set_value(false);
		}
	}
	if (widget) {
		ttoggle_button* widget2 = dynamic_cast<ttoggle_button*>(widget);
		if (!widget2->get_value()) {
			widget2->set_value(true);
		}

		if (boddy_) {
			boddy_->set_hole_variable(widget2->get_x(), widget2->get_x() + widget->get_width());
		}
	}
}

tcontrol* treport::cursel() const
{
	if (!toggle_) {
		return NULL;
	}

	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();
	for (int i = front_childs_; i < childs - back_childs_; i ++) {
		ttoggle_button* that = dynamic_cast<ttoggle_button*>(children[i].widget_);
		if (!get_visible2(children, i)) {
			continue;
		}
		if (that->get_value()) {
			return that;
		}
	}
	return NULL;
}

} // namespace gui2

