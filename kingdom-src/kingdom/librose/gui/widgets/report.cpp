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
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/layout_exception.hpp"
#include "gui/dialogs/dialog.hpp"

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
	, extendable_(false)
	, content_layouted_(false)
	, tabbar_(NULL)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &treport::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

treport::~treport()
{
	if (tabbar_) {
		tabbar_->erase_children(true);
		tabbar_->set_report(NULL);
	}
}

void treport::init_report(int unit_w, int unit_h, int gap)
{
	unit_size_.x = unit_w;
	unit_size_.y = unit_h;
	gap_ = gap;
}

void treport::insert_child(twidget& widget, int at)
{
	if (!content_grid_->get_cols()) {
		VALIDATE(get_horizontal_scrollbar_mode() == always_invisible, "Don't support horizontal scrollbar!");
		extendable_ = get_vertical_scrollbar_mode() != always_invisible;
		if (extendable_) {
			// multi-report must use fixed size.
			// in order to calculate cols, first require know w_, h_
			VALIDATE(w_ >= (unsigned)(unit_size_.x + gap_ + unit_size_.x), "Width of report must not less than 2*unit_w + gap_w!");
		}

		content_grid_->init_report(unit_size_.x, unit_size_.y,gap_, extendable_);
	}
	content_grid_->insert_child(unit_size_.x, unit_size_.y, widget, at, extendable_);
}

void treport::erase_child(int at)
{
	content_grid_->erase_child(at, extendable_);
}

void treport::replacement_children()
{
	content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_);
	if (extendable_) {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::erase_children()
{
	content_grid_->erase_children(unit_size_.x, unit_size_.y, gap_, extendable_);

	if (extendable_) {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::hide_children()
{
	content_grid_->hide_children(unit_size_.x, unit_size_.y, gap_, extendable_);
	if (extendable_) {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(false);
	}
}

void treport::layout_init(const bool full_initialization)
{
	if (tabbar_) {
		const gui2::tgrid::tchild* children = content_grid_->children();
		int childs = content_grid_->children_vsize();
		children[0].widget_->set_visible(twidget::VISIBLE);
		int max_height = children[0].widget_->get_best_size().y;
		if (!unit_size_.x) {
			// after layout_init, gui2 enter calculate_best_size.
			// because this set almost invisible, calculate content_grid of report is mistake.
			// width is determinated by window.
			// height is determinated by report.
			int height;
			for (int n = 1; n < childs - ttabbar::back_childs; n ++) {
				children[n].widget_->set_visible(twidget::INVISIBLE);
				height = children[n].widget_->get_best_size().y;
				if (height > max_height) {
					max_height = height;
				}
			}
		}
		tabbar_->max_height_ = max_height;
		// let stuff widget's to be mixman.
		children[childs].widget_->set_origin(tpoint(0, 0));
		content_grid_->update_last_draw_end();
	}

	content_layouted_ = false;
	tscrollbar_container::layout_init(full_initialization);
}

tpoint treport::calculate_best_size() const
{
	tpoint result = tscrollbar_container::calculate_best_size();
	if (tabbar_) {
		if (result.y < tabbar_->max_height_) {
			result.y = tabbar_->max_height_;
		}
	}
	return result;
}

void treport::set_content_size(const tpoint& origin, const tpoint& desire_size)
{
	const SDL_Rect& rect = content_grid_->fix_rect();
	// multi-line report are both unit_size != 0 and fix_rect.
	if (!unit_size_.x || !rect.w || !rect.h) {
		// if valid, must be single line, and single line must not be horizontal bar.
		content_grid_->twidget::place(origin, desire_size);
		// if (!content_layouted_) {
			if (tabbar_) {
				tabbar_->replacement_children();
			} else {
				content_grid_->replacement_children(unit_size_.x, unit_size_.y, gap_, extendable_);
			}
			// content_layouted_ = true;
		// }
/*
		// const tpoint actual_size = content_grid_->get_best_size();
		// const tpoint size(std::max(actual_size.x, desire_size.x), std::max(actual_size.y, desire_size.y));
		content_grid_->place(origin, desire_size);
*/
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


ttabbar* ttabbar::get_tabbar(twidget* widget)
{
	twidget* content_grid = widget->parent();
	treport* report = dynamic_cast<treport*>(content_grid->parent());
	return report->tabbar_;
}

ttabbar::ttabbar(bool toggle, bool segment, const std::string& definition)
	: report_(NULL)
	, toggle_(toggle)
	, segment_(segment)
	, definition_(definition)
	, start_(0)
	, segment_childs_(0)
	, max_height_(0)
	, previous_(NULL)
	, stuff_widget_(NULL)
	, next_(NULL)
{}

ttabbar::~ttabbar()
{
	if (!report_) {
		if (previous_) {
			delete previous_;
		}
		if (stuff_widget_) {
			delete stuff_widget_;
		}
		if (next_) {
			delete next_;
		}
	} else {
		erase_children(true);

		report_->tabbar_ = NULL;
		report_ = NULL;
	}
}

#define get_visible2(children, at)	(!((children)[at].flags_ & tgrid::USER_PRIVATE))

void ttabbar::validate_start()
{
	if (!start_) {
		return;
	}
	gui2::tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize() - back_childs;

	if (segment_) {
		// to segment_, start_ must be integer times of segment_childs_
		start_ -= start_ % segment_childs_;
	}
	int n, tmp;
	for (n = front_childs, tmp = 0; n < childs && tmp <= start_; n ++) {
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

int ttabbar::calculate_requrie_back_widgets() const
{
	VALIDATE(!segment_ && start_, "Must in !segment_ and (start_ != 0)");
		
	tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize() - back_childs;

	int n = front_childs - 1, tmp = 0;
	// 1. calculate n that crospond to start_.
	do {
		n ++;
		if (get_visible2(children, n)) {
			tmp ++;
		}
	} while (tmp != start_);


	int grid_width = report_->content_grid_->get_width();
	int additive_width = next_->fix_width();
	int unit_w = report_->unit_size_.x;
	int last_width = 0;

	// 2. backward caculate how many widgets can hold?
	for (tmp = 0, n --; n >= 0 && additive_width < grid_width; n --) {
		if (!get_visible2(children, n)) {
			continue;
		}
		twidget* widget = children[n].widget_;

		if (!report_->unit_size_.x) {
			unit_w = widget->get_best_size().x;
		}
		additive_width += unit_w + report_->gap_;
		last_width = unit_w;
		tmp ++;
	}
	if (tmp < start_) {
		// require visible "previous"
		additive_width -= last_width + report_->gap_;
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

void ttabbar::click(bool previous)
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

	replacement_children();

	if (previous) {
		// when back to #0 group, preview button cannot redraw. dirty whole report make it redraw.
		// On the other handle, once press "preview", the whole bar should redraw, so dirty whole report is acceptable.
		report_->set_dirty();
	}
}

void ttabbar::set_report(treport* report, int width, int height)
{
	VALIDATE(!report_ || !report_->content_grid_->children_vsize(), "Duplicate call!");
	report_ = report;
	if (!report) {
		return;
	}
	start_ = 0;
	segment_childs_ = 0;

	VALIDATE(!segment_ || report_->unit_size_.x, "variable tabbar must not be segment!");
	if (report->unit_size_.x) {
		width = report->unit_size_.x;
		height = report->unit_size_.y;
	}
	if (!width) {
		std::string type = toggle_? "toggle_button": "button";
		tresolution_definition_ptr ptr = get_control(type, definition_);
		width = ptr->default_height;
		height = ptr->default_height;
	}
	VALIDATE(width > 0 && height > 0, "Must valid width and height!");

	if (!stuff_widget_) {
		// previous arrow
		previous_ = create_surface_button("previous", NULL);
		previous_->set_fix_size(width, height);
		previous_->set_surface(image::get_image("buttons/arrow_left.png"), width, height);
		connect_signal_mouse_left_click(
			*previous_
			, boost::bind(
				&ttabbar::click
				, this
				, true));

		stuff_widget_ = create_spacer("sutff");
		if (report->unit_size_.x) {
			stuff_widget_->set_visible(twidget::INVISIBLE);
		}

		// next arrow
		next_ = create_surface_button("next", NULL);
		next_->set_fix_size(width, height);
		next_->set_surface(image::get_image("buttons/arrow_right.png"), width, height);
		connect_signal_mouse_left_click(
			*next_
			, boost::bind(
				&ttabbar::click
				, this
				, false));
	}

	report->insert_child(*previous_);
	report->insert_child(*stuff_widget_);
	report->insert_child(*next_);

	report_ = report;
	report_->tabbar_ = this;
}

tcontrol* ttabbar::create_child(const std::string& id, const std::string& tooltip, void* cookie, const std::string& sparam)
{
	VALIDATE(report_, "Must valid report_!");

	tcontrol* widget;
	if (toggle_) {
		ttoggle_button* widget2 = create_toggle_button(id, definition_, cookie);
		widget2->set_tooltip(tooltip);
		widget2->set_radio(true);
		widget2->set_callback_state_change(boost::bind(&tdialog::toggle_tabbar, report_->dialog(), _1));
		widget = widget2;

	} else {
		widget = create_button(id, definition_, cookie);
		widget->set_tooltip(tooltip);
		connect_signal_mouse_left_click(
			*widget
			, boost::bind(
				&tdialog::click_tabbar
				, report_->dialog()
				, widget
				, sparam));
	}
	return widget;
}

void ttabbar::insert_child(twidget& widget, int at)
{
	if (at != twidget::npos) {
		at += front_childs;
	} else {
		int size = report_->content_grid_->children_vsize();
		at = size - back_childs;
	}
	report_->insert_child(widget, at);
	validate_start();
}

void ttabbar::erase_child(int at)
{
	const tgrid::tchild* children = report_->content_grid()->children();
	bool original_visible = get_visible2(children, front_childs + at);

	report_->erase_child(at + front_childs);

	if (original_visible) {
		validate_start();
	}
}

void ttabbar::erase_children(bool clear_additional)
{
	tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize();

	int i;
	if (clear_additional) {
		for (i = childs - 1; i > childs - back_childs - 1; i --) {
			children[i].widget_->set_parent(NULL);
			children[i].widget_ = NULL;
			report_->erase_child(i);
		}
		next_ = NULL;
		stuff_widget_ = NULL;
	}
	for (i = childs - back_childs - 1; i > 0; i --) {
		report_->erase_child(i);
	}
	if (clear_additional) {
		children[0].widget_->set_parent(NULL);
		children[0].widget_ = NULL;
		report_->erase_child(0);
		previous_ = NULL;
	}

	start_ = 0;
	segment_childs_ = 0;
}

void ttabbar::hide_children()
{
	report_->hide_children();

	start_ = 0;
	segment_childs_ = 0;
}

void ttabbar::replacement_children()
{
	int grid_width = report_->content_grid_->get_width();
	int grid_height = report_->content_grid_->get_height();
	if (grid_width <= 0) {
		return;
	}

	const tgrid::tchild* children = report_->content_grid()->children();
	int vsize = report_->content_grid_->children_vsize();
	
	int unit_w = report_->unit_size_.x;
	if (segment_) {
		if (!segment_childs_) {
			segment_childs_ = (grid_width + report_->gap_) / (report_->unit_size_.x + report_->gap_) - 2;
			VALIDATE(segment_childs_ > 0, null_str);
		}
	} else {
		segment_childs_ = 0;
	}

	int last_n = twidget::npos, last_width = 0;
	bool require_next = false;

	int n = front_childs;
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

	int end = vsize - back_childs;
	for (; n < end; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		if (!get_visible2(children, n)) {
			// USER_PRIVATE(=1): user force it invisible.
			widget->set_visible(twidget::INVISIBLE);
			continue;
		}

		if (!report_->unit_size_.x) {
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
						additive_width -= last_width + report_->gap_;
					}
				}
				require_next = true;
			}
			continue;
		}
		widget->set_visible(twidget::VISIBLE);
		if (callback_show_) {
			callback_show_(this, child);
		}

		if (!segment_) {
			additive_width += unit_w + report_->gap_;
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
	if (!report_->unit_size_.x) {
		// when fix size, don't insert spacer. this width maybe diffirent from fix width.
		stuff_widget_->set_best_size(tpoint(spacer_width, grid_height));
		stuff_widget_->set_visible(twidget::VISIBLE);
	}
	next_->set_visible(require_next? twidget::VISIBLE: twidget::INVISIBLE);

	report_->replacement_children();
}

int ttabbar::childs() const
{
	if (!report_) {
		return 0;
	}
	return report_->content_grid_->children_vsize() - front_childs - back_childs;
}

void ttabbar::set_visible(int at, bool visible)
{
	at += front_childs;
	tgrid::tchild* children = report_->content_grid_->children();
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

bool ttabbar::get_visible(int at) const
{
	const tgrid::tchild* children = report_->content_grid_->children();
	return get_visible2(children, at + front_childs);
}

int ttabbar::get_index(const twidget* widget) const
{
	const gui2::tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize() - back_childs;
	for (int i = front_childs; i < childs; i ++) {
		if (children[i].widget_ == widget) {
			return i - front_childs;
		}
	}
	return twidget::npos;
}

int ttabbar::get_index2(const void* cookie) const
{
	const gui2::tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize() - back_childs;
	for (int i = front_childs; i < childs; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			return i - front_childs;
		}
	}
	return twidget::npos;
}

const tgrid::tchild& ttabbar::get_child(int at) const
{
	const gui2::tgrid::tchild* children = report_->content_grid()->children();
	return children[front_childs + at];
}

tcontrol* ttabbar::get_widget(int at) const
{
	const gui2::tgrid::tchild* children = report_->content_grid()->children();
	return dynamic_cast<tcontrol*>(children[front_childs + at].widget_);
}

void ttabbar::select(int index)
{
	if (!toggle_) {
		return;
	}
	const tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize();
	if (childs < front_childs + index + back_childs) {
		return;
	}
	select(children[front_childs + index].widget_);
}

void ttabbar::select(twidget* widget)
{
	if (!toggle_) {
		return;
	}

	const gui2::tgrid::tchild* children = report_->content_grid()->children();
	int childs = report_->content_grid()->children_vsize();
	for (int i = front_childs; i < childs - back_childs; i ++) {
		ttoggle_button* that = dynamic_cast<ttoggle_button*>(children[i].widget_);
		if (that != widget && that->get_value()) {
			that->set_value(false);
		}
	}
	if (widget) {
		ttoggle_button* widget2 = dynamic_cast<ttoggle_button*>(widget);
		if (!widget2->get_value()) {
			widget2->set_value(true);
		}
	}
}

} // namespace gui2

