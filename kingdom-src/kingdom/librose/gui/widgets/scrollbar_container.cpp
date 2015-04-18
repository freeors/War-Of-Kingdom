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
#include "gui/widgets/settings.hpp"

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
	, best_width_("(0)")
	, best_height_("(0)")
	, size_calculated_(false)
	, scroll_to_end_(false)
	, calculate_reduce_(false)
	, need_layout_(false)
	, keep_content_grid_origin_(tpoint(-1, -1))
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

void tscrollbar_container::set_best_size(const std::string& width, const std::string& height) 
{ 
	best_width_ = tformula<unsigned>(width);
	best_height_ = tformula<unsigned>(height);
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
}

tpoint tscrollbar_container::calculate_best_size() const
{
	tpoint result(0, 0);

	const twindow* window = get_window();
	unsigned w = best_width_(window->variables());
	unsigned h = best_height_(window->variables());

	if (w >= settings::screen_width || h >= settings::screen_height) {
		result = content_grid_->calculate_best_size();

		const tpoint vertical_scrollbar = scrollbar_size(*vertical_scrollbar_grid_, vertical_scrollbar_mode_);
		const tpoint horizontal_scrollbar = scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_);
		if (result.x < horizontal_scrollbar.x) {
			result.x = horizontal_scrollbar.x;
		}
		if (result.y < vertical_scrollbar.y) {
			result.y = vertical_scrollbar.y;
		}
		result.x += vertical_scrollbar.x;
		result.y += horizontal_scrollbar.y;

		if (w >= settings::screen_width) {
			w -= settings::screen_width;
			if (!w || result.x < (int)w) {
				w = result.x;
			}
		}

		if (h >= settings::screen_height) {
			h -= settings::screen_height;
			if (!h || result.y < (int)h) {
				h = result.y;
			}
		}
	}

	return tpoint(w, h);
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
	need_layout_ = false;
	
	if (!size.x && !size.y && content_empty()) {
		return;
	}

	// Inherited.
	tcontainer_::place(origin, size);

	tpoint content_grid_origin = content_->get_origin();
	if (keep_content_grid_origin_.x >= 0) {
		// it is replace, should keep content_grid_ rectangle to original position.
		content_grid_origin = content_grid_->get_origin();
	}
	set_content_size(content_grid_origin, content_->get_size());

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

void tscrollbar_container::set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin)
{
	content_grid_->set_origin(content_grid_origin);
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
	if (need_layout_) {
		keep_content_grid_origin_ = content_grid_->get_origin();
		tset_point_lock lock(keep_content_grid_origin_, -1);

		place(get_origin(), get_size());

		set_dirty();
	} else {
		// Inherited.
		tcontainer_::layout_children();

		content_grid_->layout_children();
	}
}

void tscrollbar_container::invalidate_layout(bool calculate_linked_group)
{ 
	if (calculate_linked_group) {
		layout_init(true);
		get_window()->layout_linked_widgets(this);
	}

	need_layout_ = true;
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

bool tscrollbar_container::calculate_scrollbar(const tpoint& actual_size, const tpoint& desire_size)
{
	// change scrollbar visible/invislbe don't effect layout.
	// to prevent window from layouting, disable layout_window.
	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*get_window());

	std::stringstream err;
	tvisible horizontal_visible = horizontal_scrollbar_grid_->get_visible();
	tvisible vertical_visible = vertical_scrollbar_grid_->get_visible();

	if (actual_size.x > desire_size.x) {
		err << tintegrate::generate_format(id(), "yellow");
		err << " request reduce width failed due to horizontal scrollbar mode;"
			<< " wanted size " << actual_size
			<< " available size " << desire_size
			<< '.';
		VALIDATE(horizontal_scrollbar_mode_ != always_invisible, err.str());
		horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);

	} else if (horizontal_scrollbar_mode_ != always_invisible) {
		horizontal_scrollbar_grid_->set_visible(horizontal_scrollbar_mode_ == auto_visible_first_run? twidget::VISIBLE: twidget::HIDDEN);
	}

	if (actual_size.y > desire_size.y) {
		err.str("");
		err << tintegrate::generate_format(id(), "yellow");
		err << " request reduce height failed due to vertical scrollbar mode;"
			<< " wanted size " << actual_size
			<< " available size " << desire_size
			<< '.';
		VALIDATE(vertical_scrollbar_mode_ != always_invisible, err.str());
		vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);

	} else if (vertical_scrollbar_mode_ != always_invisible) {
		vertical_scrollbar_grid_->set_visible(vertical_scrollbar_mode_ == auto_visible_first_run? twidget::VISIBLE: twidget::HIDDEN);
	}

	return horizontal_visible != horizontal_scrollbar_grid_->get_visible() || vertical_visible != vertical_scrollbar_grid_->get_visible();
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
	const tpoint content_grid_origin = tpoint(
			content_->get_x() - x_offset, content_->get_y() - y_offset);

	set_content_grid_origin(content_->get_origin(), content_grid_origin);
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

