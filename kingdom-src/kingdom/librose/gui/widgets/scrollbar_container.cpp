/* $Id: scrollbar_container.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/scrollbar_container_private.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/layout_exception.hpp"
#include "gui/widgets/clickable.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "posix.h"

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

namespace {

static const std::string button_up_names[] = {
	"_begin", "_line_up", "_half_page_up", "_page_up" };

static const std::string button_down_names[] = {
	"_end", "_line_down", "_half_page_down", "_page_down" };

/**
 * Returns a map with the names of all buttons and the scrollbar jump they're
 * supposed to execute.
 */
const std::map<std::string, tscrollbar_::tscroll>& scroll_lookup()
{
	static std::map<std::string, tscrollbar_::tscroll> lookup;
	if(lookup.empty()) {
		lookup["_begin"] = tscrollbar_::BEGIN;
		lookup["_line_up"] = tscrollbar_::ITEM_BACKWARDS;
		lookup["_half_page_up"] = tscrollbar_::HALF_JUMP_BACKWARDS;
		lookup["_page_up"] = tscrollbar_::JUMP_BACKWARDS;

		lookup["_end"] = tscrollbar_::END;
		lookup["_line_down"] = tscrollbar_::ITEM_FORWARD;
		lookup["_half_page_down"] = tscrollbar_::HALF_JUMP_FORWARD;
		lookup["_page_down"] = tscrollbar_::JUMP_FORWARD;
	}

	return lookup;
}

} // namespace

tscrollbar_container::tscrollbar_container(const unsigned canvas_count, bool listbox)
	: tcontainer_(canvas_count)
	, listbox_(listbox)
	, state_(ENABLED)
	, vertical_scrollbar_mode_(auto_visible_first_run)
	, horizontal_scrollbar_mode_(auto_visible_first_run)
	, vertical_scrollbar_grid_(NULL)
	, horizontal_scrollbar_grid_(NULL)
	, vertical_scrollbar_(NULL)
	, horizontal_scrollbar_(NULL)
	, content_grid_(NULL)
	, content_(NULL)
	, content_visible_area_()
	, best_size_(0, 0)
	, size_calculated_(false)
	, scroll_to_end_(false)
{
	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&tscrollbar_container::signal_handler_sdl_key_down
				, this, _2, _3, _5, _6));


	connect_signal<event::SDL_WHEEL_UP>(
			boost::bind(
				  &tscrollbar_container::signal_handler_sdl_wheel_up
				, this
				, _2
				, _3)
			, event::tdispatcher::back_post_child);

	connect_signal<event::SDL_WHEEL_DOWN>(
			boost::bind(
				&tscrollbar_container::signal_handler_sdl_wheel_down
				, this
				, _2
				, _3)
			, event::tdispatcher::back_post_child);

	connect_signal<event::SDL_WHEEL_LEFT>(
			boost::bind(
				&tscrollbar_container::signal_handler_sdl_wheel_left
				, this
				, _2
				, _3)
			, event::tdispatcher::back_post_child);

	connect_signal<event::SDL_WHEEL_RIGHT>(
			boost::bind(
				&tscrollbar_container::signal_handler_sdl_wheel_right
				, this
				, _2
				, _3)
			, event::tdispatcher::back_post_child);
}

void tscrollbar_container::layout_init(const bool full_initialization)
{
	size_calculated_ = false;
	// Inherited.
	tcontainer_::layout_init(full_initialization);
	if (full_initialization) {
		switch(vertical_scrollbar_mode_) {
			case always_visible :
				vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				vertical_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}

		switch(horizontal_scrollbar_mode_) {
			case always_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
	}

	assert(content_grid_);
	content_grid_->layout_init(full_initialization);
}

void tscrollbar_container::set_best_size(const tpoint& best_size) 
{ 
	best_size_ = best_size;
}

tpoint tscrollbar_container::scrollbar_size(const tgrid& scrollbar_grid, tscrollbar_mode scrollbar_mode) const
{
	if (scrollbar_mode == auto_visible || scrollbar_grid.get_visible() == twidget::VISIBLE) {
		return scrollbar_grid.get_best_size();
	}
	return tpoint(0, 0);
}

static twidget::tvisible invisible_value(tscrollbar_container::tscrollbar_mode scrollbar_mode)
{
	return scrollbar_mode == tscrollbar_container::auto_visible? twidget::HIDDEN: twidget::INVISIBLE;
}

void tscrollbar_container::request_reduce_height(
		const unsigned maximum_height)
{
	DBG_GUI_L << LOG_HEADER
			<< " requested height " << maximum_height
			<< ".\n";

	if (best_size_ != tpoint(0, 0) && listbox_) {
		// fixed size case, do nothing
		return;
	}
	/*
	 * First ask the content to reduce it's height. This seems to work for now,
	 * but maybe some sizing hints will be required later.
	 */
	/** @todo Evaluate whether sizing hints are required. */
	assert(content_grid_);
	if (!listbox_) {
		const unsigned offset = scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_).y;
		content_grid_->request_reduce_height(maximum_height - offset);
	}

	// Did we manage to achieve the wanted size?
	tpoint size = get_best_size();
	if (static_cast<unsigned>(size.y) <= maximum_height) {
		DBG_GUI_L << LOG_HEADER
				<< " child honoured request, height " << size.y
				<< ".\n";
		return;
	}

	if (vertical_scrollbar_mode_ == always_invisible) {
		DBG_GUI_L << LOG_HEADER << " request failed due to scrollbar mode.\n";
		return;
	}

	assert(vertical_scrollbar_grid_);
	const bool resized = vertical_scrollbar_grid_->get_visible() == twidget::INVISIBLE;

	// Always set the bar visible, is a nop is already visible.
	vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);

	const tpoint vscrollbar_size = vertical_scrollbar_grid_->get_best_size();

	// If showing the scrollbar increased the height, hide and abort.
	if (resized && vscrollbar_size.y > size.y) {
		vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		DBG_GUI_L << LOG_HEADER
				<< " request failed, showing the scrollbar"
			    << " increased the height to " << vscrollbar_size.y
				<< ".\n";
		return;
	}

	if (maximum_height > static_cast<unsigned>(vscrollbar_size.y)) {
		size.y = maximum_height;
	} else {
		size.y = vscrollbar_size.y;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);
	DBG_GUI_L << LOG_HEADER
			<< " resize resulted in " << size.y
			<< ".\n";

	if (resized) {
		DBG_GUI_L << LOG_HEADER
				<< " resize modified the width, throw notification.\n";

		posix_print("id: %s, layout_size_: (%i, %i), width of scrollbar: %i\n", id().c_str(), layout_size().x, layout_size().y, vscrollbar_size.x);
		throw tlayout_exception_width_modified();
	}
}

void tscrollbar_container::request_reduce_width(const unsigned maximum_width)
{
	DBG_GUI_L << LOG_HEADER
			<< " requested width " << maximum_width
			<< ".\n";

	if (best_size_ != tpoint(0, 0) && listbox_) {
		// fixed size case, do nothing
		return;
	}
	// First ask our content, it might be able to wrap which looks better as
	// a scrollbar.
	assert(content_grid_);
	if (!listbox_) {
		const unsigned offset = scrollbar_size(*vertical_scrollbar_grid_, vertical_scrollbar_mode_).x;

		unsigned content_grid_width = best_size_.x? best_size_.x: maximum_width;
		if (content_grid_width < offset) {
			return;
		}
		content_grid_width -= offset;
		content_grid_->request_reduce_width(content_grid_width);
	}

	// Did we manage to achieve the wanted size?
	tpoint size = get_best_size();
	if (static_cast<unsigned>(size.x) <= maximum_width) {
		DBG_GUI_L << LOG_HEADER
				<< " child honoured request, width " << size.x
				<< ".\n";
		return;
	}

	if (horizontal_scrollbar_mode_ == always_invisible) {
		DBG_GUI_L << LOG_HEADER << " request failed due to scrollbar mode.\n";
		return;
	}

	// Always set the bar visible, is a nop when it's already visible.
	assert(horizontal_scrollbar_grid_);
	horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
	size = get_best_size();

	const tpoint hscrollbar_size = scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_);

	// If showing the scrollbar increased the width, hide and abort.
	if (horizontal_scrollbar_mode_ == auto_visible_first_run && hscrollbar_size.x > size.x) {
		horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		DBG_GUI_L << LOG_HEADER
				<< " request failed, showing the scrollbar"
			    << " increased the width to " << hscrollbar_size.x
				<< ".\n";
		return;
	}

	if (maximum_width > static_cast<unsigned>(hscrollbar_size.x)) {
		size.x = maximum_width;
	} else {
		size.x = hscrollbar_size.x;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);
	DBG_GUI_L << LOG_HEADER
			<< " resize resulted in " << size.x
			<< ".\n";
}

tpoint tscrollbar_container::calculate_best_size() const
{
	bool from_best_size = (best_size_ != tpoint(0, 0))? true: false;

	if (from_best_size && listbox_ && size_calculated_) {
		return best_size_;
	}

	/***** get vertical scrollbar size *****/
	const tpoint vertical_scrollbar = scrollbar_size(*vertical_scrollbar_grid_, vertical_scrollbar_mode_);
			
	/***** get horizontal scrollbar size *****/
	const tpoint horizontal_scrollbar = scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_);

	/***** get content size *****/
	assert(content_grid_);
	const tpoint content = content_grid_->get_best_size();

	if (from_best_size && listbox_) {
		bool visiable_horizontal_bar = false;
		if (best_size_.x < content.x) {
			VALIDATE(horizontal_scrollbar_mode_ != always_invisible, "request failed due to scrollbar mode.");
			horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
			visiable_horizontal_bar = true;
		} else {
			horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
		if (best_size_.y - scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_).y < content.y) {
			VALIDATE(vertical_scrollbar_mode_ != always_invisible, "request failed due to scrollbar mode.");
			vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);
		} else {
			vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
		size_calculated_ = true;
		return best_size_;
	} else {
		tpoint result(
			vertical_scrollbar.x +
				std::max(horizontal_scrollbar.x, content.x),
			horizontal_scrollbar.y +
				std::max(vertical_scrollbar.y,  content.y));
		if (from_best_size) {

			if (best_size_.x) {
				if (result.x > best_size_.x) {
					tpoint better_content_grid_size = const_cast<tscrollbar_container*>(this)->pre_request_fix_width(best_size_.x - vertical_scrollbar.x);
					result.x = vertical_scrollbar.x + better_content_grid_size.x;
					if (better_content_grid_size.y) {
						result.y = horizontal_scrollbar.y + better_content_grid_size.y;
					}
				}
				result.x = (result.x >= best_size_.x)? result.x: best_size_.x;
			}
			result.y = (result.y >= best_size_.y)? result.y: best_size_.y;
		}

		return result;
	}
}

tpoint tscrollbar_container::pre_request_fix_width(const unsigned maximum_content_grid_width)
{
	return tpoint(maximum_content_grid_width, 0);
}

static void set_scrollbar_mode(tgrid* scrollbar_grid, tscrollbar_* scrollbar,
		tscrollbar_container::tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items)
{
	assert(scrollbar_grid && scrollbar);

	if(scrollbar_mode == tscrollbar_container::always_invisible) {
		scrollbar_grid->set_visible(twidget::INVISIBLE);
		return;
	}

	scrollbar->set_item_count(items);
	// when layout, keep offset
	// scrollbar->set_item_position(0);
	scrollbar->set_visible_items(visible_items);

	if(scrollbar_mode == tscrollbar_container::auto_visible) {

		const bool scrollbar_needed = items > visible_items;

		scrollbar_grid->set_visible(scrollbar_needed
				? twidget::VISIBLE
				: twidget::HIDDEN);
	}
}

void tscrollbar_container::place(const tpoint& origin, const tpoint& size)
{
	// once place, always this size! BUG!!!
	if (best_size_.x && best_size_.x != size.x) {
		best_size_.x = size.x;
	}
	if (best_size_.y && best_size_.y != size.y) {
		best_size_.y = size.y;
	}

	// Inherited.
	tcontainer_::place(origin, size);

	// Set content size if necessary.
	tpoint content_size = adjust_content_size(content_->get_size());
	if (content_->get_size() != content_size) {
		int diff_y = content_->get_size().y - content_size.y;
		content_->set_size(content_size);
		if (vertical_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
			tpoint vertical_size = vertical_scrollbar_grid_->get_size();
			vertical_size.y -= diff_y;
			vertical_scrollbar_grid_->place(vertical_scrollbar_grid_->get_origin(), vertical_size);
		}
		if (horizontal_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
			tpoint horizontal_origin = horizontal_scrollbar_grid_->get_origin();
			horizontal_origin.y -= diff_y;
			horizontal_scrollbar_grid_->set_origin(horizontal_origin);
		}
	}

	// Set content_grid size
	assert(content_ && content_grid_);

	const tpoint content_origin = content_->get_origin();

	const tpoint best_size = content_grid_->get_best_size();
	// const tpoint content_size(content_->get_width(), content_->get_height());

	const tpoint content_grid_size(
			std::max(best_size.x, content_size.x),
			std::max(best_size.y, content_size.y));

	set_content_size(content_origin, content_grid_size);

	// Set vertical scrollbar
	set_scrollbar_mode(vertical_scrollbar_grid_, vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_->get_height(),
			content_->get_height());

	// Set horizontal scrollbar
	set_scrollbar_mode(horizontal_scrollbar_grid_, horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_->get_width(),
			content_->get_width());

	// Update the buttons.
	set_scrollbar_button_status();

	// Now set the visible part of the content.
	content_visible_area_ = content_->get_rect();
	set_content_grid_visible_area(content_visible_area_);
	// content_grid_->set_visible_area(content_visible_area_);

	if (scroll_to_end_) {
		scroll_vertical_scrollbar(tscrollbar_::END);
	}
}

void tscrollbar_container::set_origin(const tpoint& origin)
{
	// Inherited.
	tcontainer_::set_origin(origin);

	// Set content size
	assert(content_ && content_grid_);

	const tpoint content_origin = content_->get_origin();

	set_content_grid_origin(origin, content_origin);

	// Changing the origin also invalidates the visible area.
	set_content_grid_visible_area(content_visible_area_);
	// content_grid_->set_visible_area(content_visible_area_);
}

tpoint tscrollbar_container::adjust_content_size(const tpoint& size)
{
	return size;
}

void tscrollbar_container::set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	content_grid_->set_origin(content_origin);
}

void tscrollbar_container::set_content_grid_visible_area(const SDL_Rect& area)
{
	content_grid_->set_visible_area(area);
}

void tscrollbar_container::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	tcontainer_::set_visible_area(area);

	// Now get the visible part of the content.
	content_visible_area_ = intersect_rects(area, content_->get_rect());

	content_grid_->set_visible_area(content_visible_area_);
}

twidget* tscrollbar_container::find_at(
		const tpoint& coordinate, const bool must_be_active)
{
	VALIDATE(content_ && content_grid_, null_str);

	twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	if (result == content_) {
		result = content_grid_->find_at(coordinate, must_be_active);
		if (!result) {
			// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
			result = content_grid_;
		}
	}
	return result;
}

const twidget* tscrollbar_container::find_at(const tpoint& coordinate,
		const bool must_be_active) const
{
	VALIDATE(content_ && content_grid_, null_str);

	const twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	if (result == content_) {
		result = content_grid_->find_at(coordinate, must_be_active);
		if (!result) {
			// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
			result = content_grid_;
		}
	}
	return result;
}

twidget* tscrollbar_container::find(
		const std::string& id, const bool must_be_active)
{
	return tscrollbar_container_implementation
			::find<twidget>(*this, id, must_be_active);
}

const twidget* tscrollbar_container::find(
			const std::string& id, const bool must_be_active) const
{
	return tscrollbar_container_implementation
			::find<const twidget>(*this, id, must_be_active);
}

bool tscrollbar_container::disable_click_dismiss() const
{
	assert(content_grid_);
	return tcontainer_::disable_click_dismiss()
			|| content_grid_->disable_click_dismiss();
}

void tscrollbar_container::layout_private(bool first)
{
	const tpoint& size = get_size();
	twindow* window = get_window();
	// in order to clear all widget's layout_size.
	layout_init(first);
	// window->layout_linked_widgets();

	if (first) {

		vertical_scrollbar_->set_item_position(0);
		// vertical_scrollbar_grid_->twidget::set_origin(tpoint(-1, -1));
		// vertical_scrollbar_grid_->twidget::set_size(tpoint(0, 0));

		horizontal_scrollbar_->set_item_position(0);
		// horizontal_scrollbar_grid_->twidget::set_origin(tpoint(-1, -1));
		// horizontal_scrollbar_grid_->twidget::set_size(tpoint(0, 0));
	}

	tpoint best_size = get_best_size();
	if (best_size.x > size.x) {
		request_reduce_width(size.x);
		best_size = get_best_size();
	}
	if (best_size.y > size.y) {
		request_reduce_height(size.y);
	}
}

bool tscrollbar_container::content_resize_request(const bool force_sizing)
{
	const tpoint& size = get_size();
	if (size == tpoint(0, 0)) {
		// initial setup not done, bailing out.
		return false;
	}

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*get_window());

	try {
		layout_private(true);

	} catch (tlayout_exception_width_modified&) {
		layout_private(false);
	}
	place(get_origin(), size);

	// vertical_scrollbar_grid_->set_visible_area(vertical_scrollbar_grid_->get_rect());
	// horizontal_scrollbar_grid_->set_visible_area(horizontal_scrollbar_grid_->get_rect());

	// false: need layout window
	// true: don't require layout window
	return true;
}

bool tscrollbar_container::content_resize_request(
		  const int width_modification
		, const int height_modification)
{
	DBG_GUI_L << LOG_HEADER
			<< " wanted width modification " << width_modification
			<< " wanted height modification " << height_modification
			<< ".\n";

	if(get_size() == tpoint(0, 0)) {
		DBG_GUI_L << LOG_HEADER
				<< " initial setup not done, bailing out.\n";
		return false;
	}

	twindow* window = get_window();
	assert(window);
	if(window->get_need_layout()) {
		DBG_GUI_L << LOG_HEADER
				<< " window already needs a layout phase, bailing out.\n";
		return false;
	}

	assert(content_ && content_grid_);

	const bool result = content_resize_width(width_modification)
			&& content_resize_height(height_modification);

	if(result) {
		/*
		 * The subroutines set the new size of the scrollbar but don't
		 * update the button status.
		 */
		set_scrollbar_button_status();
	}

	DBG_GUI_L << LOG_HEADER << " result " << result << ".\n";
	return result;
}

bool tscrollbar_container::content_resize_width(const int width_modification)
{
	if(width_modification == 0) {
		return true;
	}

	const int new_width = content_grid_->get_width() + width_modification;
	DBG_GUI_L << LOG_HEADER
			<< " current width " << content_grid_->get_width()
			<< " wanted width " << new_width;

	assert(new_width > 0);

	if(static_cast<unsigned>(new_width) <= content_->get_width()) {
		DBG_GUI_L << " width fits in container, test height.\n";
		set_scrollbar_mode(horizontal_scrollbar_grid_
				, horizontal_scrollbar_
				, horizontal_scrollbar_mode_
				, new_width
				, content_->get_width());
		return true;
	}

	assert(horizontal_scrollbar_ && horizontal_scrollbar_grid_);
	if(horizontal_scrollbar_mode_ == always_invisible
			|| (horizontal_scrollbar_mode_ == auto_visible_first_run
				&& horizontal_scrollbar_grid_->get_visible()
					== twidget::INVISIBLE)) {

		DBG_GUI_L << " can't use horizontal scrollbar, ask window.\n";
		twindow* window = get_window();
		assert(window);
		window->invalidate_layout();
		return false;
	}

	DBG_GUI_L << " use the horizontal scrollbar, test height.\n";
	set_scrollbar_mode(horizontal_scrollbar_grid_
			, horizontal_scrollbar_
			, horizontal_scrollbar_mode_
			, new_width
			, content_->get_width());

	return true;
}

bool tscrollbar_container::content_resize_height(const int height_modification)
{
	if(height_modification == 0) {
		return true;
	}

	const int new_height =
			content_grid_->get_height() + height_modification;

	DBG_GUI_L << LOG_HEADER
			<< " current height " << content_grid_->get_height()
			<< " wanted height " << new_height;

	assert(new_height >= 0);

	if(static_cast<unsigned>(new_height) <= content_->get_height()) {
		DBG_GUI_L << " height in container, resize allowed.\n";
		set_scrollbar_mode(vertical_scrollbar_grid_
				, vertical_scrollbar_
				, vertical_scrollbar_mode_
				, new_height
				, content_->get_height());
		return true;
	}

	assert(vertical_scrollbar_ && vertical_scrollbar_grid_);
	if(vertical_scrollbar_mode_ == always_invisible
			|| (vertical_scrollbar_mode_ == auto_visible_first_run
				&& vertical_scrollbar_grid_->get_visible()
					== twidget::INVISIBLE)) {

		DBG_GUI_L << " can't use vertical scrollbar, ask window.\n";
		twindow* window = get_window();
		assert(window);
		window->invalidate_layout();
		return false;
	}

	DBG_GUI_L << " use the vertical scrollbar, resize allowed.\n";
	set_scrollbar_mode(vertical_scrollbar_grid_
			, vertical_scrollbar_
			, vertical_scrollbar_mode_
			, new_height
			, content_->get_height());

	return true;
}

void tscrollbar_container::finalize_setup()
{
	/***** Setup vertical scrollbar *****/

	vertical_scrollbar_grid_ =
		find_widget<tgrid>(this, "_vertical_scrollbar_grid", false, true);

	vertical_scrollbar_ = find_widget<tscrollbar_>(
			vertical_scrollbar_grid_, "_vertical_scrollbar", false, true);

	connect_signal_notify_modified(*vertical_scrollbar_
			, boost::bind(
				  &tscrollbar_container::vertical_scrollbar_moved
				, this));

	/***** Setup horizontal scrollbar *****/
	horizontal_scrollbar_grid_ =
		find_widget<tgrid>(this, "_horizontal_scrollbar_grid", false, true);

	horizontal_scrollbar_ = find_widget<tscrollbar_>(
			horizontal_scrollbar_grid_, "_horizontal_scrollbar", false, true);

	connect_signal_notify_modified(*horizontal_scrollbar_
			, boost::bind(
				  &tscrollbar_container::horizontal_scrollbar_moved
				, this));

	/***** Setup the scrollbar buttons *****/
	typedef std::pair<std::string, tscrollbar_::tscroll> hack;
	BOOST_FOREACH(const hack& item, scroll_lookup()) {

		// Vertical.
		tclickable_* button = find_widget<tclickable_>(
				vertical_scrollbar_grid_, item.first, false, false);

		if(button) {
			button->connect_click_handler(boost::bind(
					  &tscrollbar_container::scroll_vertical_scrollbar
					, this
					, item.second));
		}

		// Horizontal.
		button = find_widget<tclickable_>(
				horizontal_scrollbar_grid_, item.first, false, false);

		if(button) {
			button->connect_click_handler(boost::bind(
					  &tscrollbar_container::scroll_horizontal_scrollbar
					, this
					, item.second));
		}
	}

	/***** Setup the content *****/
	content_ = new tspacer();
	content_->set_definition("default");

	content_grid_ = dynamic_cast<tgrid*>(
			grid().swap_child("_content_grid", content_, true));
	assert(content_grid_);

	content_grid_->set_parent(this);

	/***** Let our subclasses initialize themselves. *****/
	finalize_subclass();
}

void tscrollbar_container::
		set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(vertical_scrollbar_mode_ != scrollbar_mode) {
		vertical_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscrollbar_container::
		set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(horizontal_scrollbar_mode_ != scrollbar_mode) {
		horizontal_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscrollbar_container::impl_draw_children(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	assert(get_visible() == twidget::VISIBLE
			&& content_grid_->get_visible() == twidget::VISIBLE);

	// Inherited.
	tcontainer_::impl_draw_children(frame_buffer, x_offset, y_offset);

	content_grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tscrollbar_container::layout_children()
{
	// Inherited.
	tcontainer_::layout_children();

	assert(content_grid_);
	content_grid_->layout_children();
}

void tscrollbar_container::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tcontainer_::child_populate_dirty_list(caller, call_stack);

	assert(content_grid_);
	std::vector<twidget*> child_call_stack(call_stack);
	content_grid_->populate_dirty_list(caller, child_call_stack);
}

void tscrollbar_container::set_content_size(const tpoint& origin, const tpoint& size)
{
	content_grid_->place(origin, size);
}

void tscrollbar_container::show_content_rect(const SDL_Rect& rect)
{
	assert(content_);
	assert(horizontal_scrollbar_ && vertical_scrollbar_);

	// Set the bottom right location first if it doesn't fit the top left
	// will look good. First calculate the left and top position depending on
	// the current position.

	const int left_position = horizontal_scrollbar_->get_item_position()
			+ (rect.x - content_->get_x());
	const int top_position = vertical_scrollbar_->get_item_position()
			+ (rect.y - content_->get_y());

	// bottom.
	const int wanted_bottom = rect.y + rect.h;
	const int current_bottom = content_->get_y() + content_->get_height();
	int distance = wanted_bottom - current_bottom;
	if(distance > 0) {
		vertical_scrollbar_->set_item_position(
				vertical_scrollbar_->get_item_position() + distance);
	}

	// right.
	const int wanted_right = rect.x + rect.w;
	const int current_right = content_->get_x() + content_->get_width();
	distance = wanted_right - current_right;
	if(distance > 0) {
		horizontal_scrollbar_->set_item_position(
				horizontal_scrollbar_->get_item_position() + distance);
	}

	// top.
	if(top_position < static_cast<int>(
				vertical_scrollbar_->get_item_position())) {

		vertical_scrollbar_->set_item_position(top_position);
	}

	// left.
	if(left_position < static_cast<int>(
				horizontal_scrollbar_->get_item_position())) {

		horizontal_scrollbar_->set_item_position(left_position);
	}

	// Update.
	scrollbar_moved();
}

void tscrollbar_container::set_scrollbar_button_status()
{
	if(true) { /** @todo scrollbar visibility. */
		/***** set scroll up button status *****/
		BOOST_FOREACH(const std::string& name, button_up_names) {
			tcontrol* button = find_widget<tcontrol>(
					vertical_scrollbar_grid_, name, false, false);

			if(button) {
				button->set_active(!vertical_scrollbar_->at_begin());
			}
		}

		/***** set scroll down status *****/
		BOOST_FOREACH(const std::string& name, button_down_names) {
			tcontrol* button = find_widget<tcontrol>(
					vertical_scrollbar_grid_, name, false, false);

			if(button) {
				button->set_active(!vertical_scrollbar_->at_end());
			}
		}

		/***** Set the status if the scrollbars *****/
		vertical_scrollbar_->set_active(
				!vertical_scrollbar_->all_items_visible());
	}

	if(true) { /** @todo scrollbar visibility. */
		/***** Set scroll left button status *****/
		BOOST_FOREACH(const std::string& name, button_up_names) {
			tcontrol* button = find_widget<tcontrol>(
					horizontal_scrollbar_grid_, name, false, false);

			if(button) {
				button->set_active(!horizontal_scrollbar_->at_begin());
			}
		}

		/***** Set scroll right button status *****/
		BOOST_FOREACH(const std::string& name, button_down_names) {
			tcontrol* button = find_widget<tcontrol>(
					horizontal_scrollbar_grid_, name, false, false);

			if(button) {
				button->set_active(!horizontal_scrollbar_->at_end());
			}
		}

		/***** Set the status if the scrollbars *****/
		horizontal_scrollbar_->set_active(
				!horizontal_scrollbar_->all_items_visible());
	}
}

void tscrollbar_container::scroll_vertical_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscrollbar_container::scroll_horizontal_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscrollbar_container::handle_key_home(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::BEGIN);
	horizontal_scrollbar_->scroll(tscrollbar_::BEGIN);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::handle_key_end(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::END);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_page_up(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_page_down(SDLMod /*modifier*/, bool& handled)

{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_up_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::
		handle_key_down_arrow( SDLMod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container
		::handle_key_left_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container
		::handle_key_right_arrow(SDLMod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscrollbar_container::scrollbar_moved()
{
	// Init.
	assert(content_ && content_grid_);
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	/*** Update the content location. ***/
	int x_offset = horizontal_scrollbar_mode_ == always_invisible
			? 0
			: horizontal_scrollbar_->get_item_position() *
			  horizontal_scrollbar_->get_step_size();

	int y_offset = vertical_scrollbar_mode_ == always_invisible
			? 0
			: vertical_scrollbar_->get_item_position() *
			  vertical_scrollbar_->get_step_size();

	adjust_offset(x_offset, y_offset);
	const tpoint content_origin = tpoint(
			content_->get_x() - x_offset, content_->get_y() - y_offset);

	set_content_grid_origin(content_->get_origin(), content_origin);
	set_content_grid_visible_area(content_visible_area_);
	content_grid_->set_dirty();

	// Update scrollbar.
	set_scrollbar_button_status();
}

const std::string& tscrollbar_container::get_control_type() const
{
	static const std::string type = "scrollbar_container";
	return type;
}

void tscrollbar_container::signal_handler_sdl_key_down(
		const event::tevent event
		, bool& handled
		, const SDLKey key
		, SDLMod modifier)
{
	DBG_GUI_E << LOG_HEADER << event << ".\n";

	switch(key) {
		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;


		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;


		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;
		default:
			/* ignore */
			break;
		}
}

void tscrollbar_container::signal_handler_sdl_wheel_up(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << event << ".\n";

	assert(vertical_scrollbar_grid_ && vertical_scrollbar_);

	if(vertical_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
		vertical_scrollbar_->scroll(tscrollbar_::HALF_JUMP_BACKWARDS);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::signal_handler_sdl_wheel_down(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << event << ".\n";

	assert(vertical_scrollbar_grid_ && vertical_scrollbar_);

	if(vertical_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
		vertical_scrollbar_->scroll(tscrollbar_::HALF_JUMP_FORWARD);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::signal_handler_sdl_wheel_left(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << event << ".\n";

	assert(horizontal_scrollbar_grid_ && horizontal_scrollbar_);

	if(horizontal_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
		horizontal_scrollbar_->scroll(tscrollbar_::HALF_JUMP_BACKWARDS);
		scrollbar_moved();
		handled = true;
	}
}

void tscrollbar_container::signal_handler_sdl_wheel_right(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << event << ".\n";

	assert(horizontal_scrollbar_grid_ && horizontal_scrollbar_);

	if(horizontal_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
		horizontal_scrollbar_->scroll(tscrollbar_::HALF_JUMP_FORWARD);
		scrollbar_moved();
		handled = true;
	}

}

} // namespace gui2

