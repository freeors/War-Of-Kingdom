/* $Id: listbox.cpp 54521 2012-06-30 17:46:54Z mordante $ */
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

#ifndef GUI2_EXPERIMENTAL_LISTBOX

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/layout_exception.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/auxiliary/window_builder/horizontal_listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/spacer.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(listbox)

namespace {
// in separate namespace to avoid name classes
REGISTER_WIDGET3(tlistbox_definition, horizontal_listbox, _4)

void callback_list_item_clicked(twidget* caller)
{
	get_parent<tlistbox>(caller)->list_item_clicked(caller);
}

} // namespace

tlistbox::tlistbox(const std::vector<tradio_page::tpage>& pages, const bool has_minimum, const bool has_maximum,
		const tgenerator_::tplacement placement, const bool select)
	: tscrollbar_container(2, true) // FIXME magic number
	, tradio_page(pages, this)
	, generator_(
			tgenerator_::build(has_minimum, has_maximum, placement, select))
	, list_builder_(NULL)
	, callback_value_changed_(NULL)
{
}

void tlistbox::add_row(const string_map& item, const int index)
{
	assert(generator_);
	generator_->create_item(
			index, list_builder_, item, callback_list_item_clicked);
}

void tlistbox::add_row(
		  const std::map<std::string /* widget id */, string_map>& data
		, const int index)
{
	assert(generator_);
	generator_->create_item(
			index, list_builder_, data, callback_list_item_clicked);
}

void tlistbox::remove_row(const unsigned row, unsigned count)
{
	assert(generator_);

	if (row >= get_item_count()) {
		return;
	}

	if (!count || count > get_item_count()) {
		count = get_item_count();
	}

	const unsigned selected_row = get_selected_row();
	bool change_select = selected_row >= row && selected_row < row + count;

	for (; count; -- count) {
		generator_->delete_item(row);
	}

	invalidate_layout(false);
}

void tlistbox::clear()
{
	// Due to the removing from the linked group, don't use
	// generator_->clear() directly.
	remove_row(0, 0);
}

void tlistbox::sort(void* caller, bool (*callback)(void*, tgrid&, tgrid&))
{
	if (!generator_) return;
	generator_->sort(caller, callback);
}

unsigned tlistbox::get_item_count() const
{
	assert(generator_);
	return generator_->get_item_count();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	assert(generator_);
	generator_->item(row).set_active(active);
}

void tlistbox::set_row_shown(const unsigned row, const bool shown)
{
	assert(generator_);

	twindow *window = get_window();
	assert(window);

	const int selected_row = get_selected_row();

	{
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);
		generator_->set_item_shown(row, shown);
	}
	if (selected_row != get_selected_row() && callback_value_changed_) {
		callback_value_changed_(this);
	}
	invalidate_layout(false);
}

void tlistbox::set_row_shown(const std::vector<bool>& shown)
{
	assert(generator_);
	assert(shown.size() == get_item_count());

	twindow *window = get_window();
	assert(window);

	const int selected_row = get_selected_row();

	{
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);
		for (size_t i = 0; i < shown.size(); ++i) {
			generator_->set_item_shown(i, shown[i]);
		}
	}
	if (selected_row != get_selected_row() && callback_value_changed_) {
		callback_value_changed_(this);
	}
	invalidate_layout(false);
}

const tgrid* tlistbox::get_row_grid(const unsigned row) const
{
	assert(generator_);
	// rename this function and can we return a reference??
	return &generator_->item(row);
}

tgrid* tlistbox::get_row_grid(const unsigned row)
{
	assert(generator_);
	return &generator_->item(row);
}

bool tlistbox::select_row(const unsigned row, const bool select)
{
	assert(generator_);

	generator_->select_item(row, select);

	return true; // FIXME test what result should have been!!!
}

int tlistbox::get_selected_row() const
{
	assert(generator_);

	return generator_->get_selected_item();
}

void tlistbox::list_item_clicked(twidget* caller)
{
	assert(caller);
	assert(generator_);

	/** @todo Hack to capture the keyboard focus. */
	get_window()->keyboard_capture(this);

	for(size_t i = 0; i < generator_->get_item_count(); ++i) {

		if(generator_->item(i).has_widget(caller)) {
			generator_->toggle_item(i);
			if(callback_value_changed_) {
				callback_value_changed_(this);
			}
			return;
		}
	}
	assert(false);
}

void tlistbox::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tscrollbar_container::place(origin, size);

	/**
	 * @todo Work-around to set the selected item visible again.
	 *
	 * At the moment the listboxes and dialogs in general are resized a lot as
	 * work-around for sizing. So this function makes the selected item in view
	 * again. It doesn't work great in all cases but the proper fix is to avoid
	 * resizing dialogs a lot. Need more work later on.
	 */
	const int selected_item = generator_->get_selected_item();
	if (selected_item != -1) {
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(selected_item).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		// show_content_rect(rect);
		scrollbar_moved();
	}
}

void tlistbox::adjust_offset(int& x_offset, int& y_offset)
{
	unsigned items = generator_->get_item_count();
	if (!items || !y_offset) {
		return;
	}
	int height = generator_->item(0).get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
		// y_offset = y_offset / height * height;
	}
}

void tlistbox::set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	generator_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));
}

void tlistbox::set_content_grid_visible_area(const SDL_Rect& area)
{
	// Here need call twidget::set_visible_area in conent_gri_ only.
	content_grid()->twidget::set_visible_area(area);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();
	SDL_Rect header_area = intersect_rects(area, header->get_rect());
	header->set_visible_area(header_area);
	
	SDL_Rect list_area = area;
	list_area.y = area.y + size.y;
	list_area.h = area.h - size.y;
	generator_->set_visible_area(list_area);
}

void tlistbox::handle_key_up_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_up_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_down_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_down_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_left_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_left_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_right_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_right_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

namespace {

/**
 * Swaps an item in a grid for another one.*/
void swap_grid(tgrid* grid,
		tgrid* content_grid, twidget* widget, const std::string& id)
{
	assert(content_grid);
	assert(widget);

	// Make sure the new child has same id.
	widget->set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = NULL;
	if(grid) {
		parent_grid = find_widget<tgrid>(grid, id, false, false);
	}
	if(!parent_grid) {
		parent_grid = find_widget<tgrid>(content_grid, id, true, false);
	}
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	assert(parent_grid);

	// Replace the child.
	widget = parent_grid->swap_child(id, widget, false);
	assert(widget);

	delete widget;
}

} // namespace

void tlistbox::finalize(
		tbuilder_grid_const_ptr header,
		tbuilder_grid_const_ptr footer,
		const std::vector<string_map>& list_data)
{
	// "Inherited."
	tscrollbar_container::finalize_setup();

	assert(generator_);

	if(header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if(footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	generator_->create_items(
			-1, list_builder_, list_data, callback_list_item_clicked);
	swap_grid(NULL, content_grid(), generator_, "_list_grid");

}

void tlistbox::set_content_size(const tpoint& origin, const tpoint& desire_size)
{
	tpoint size = desire_size;
	unsigned items = generator_->get_item_count();
	if (items) {
		tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
		// by this time, hasn't called place(), cannot use get_size().
		int header_height = header->get_best_size().y;
		int height = generator_->item(0).get_best_size().y;
		if (header_height + height <= size.y) {
			int list_height = size.y - header_height;
			list_height = list_height / height * height;

			// reduce hight if allow height > header_height + get_best_size().y
			height = generator_->get_best_size().y;
			if (list_height > height) {
				list_height = height;
			}

			size.y = header_height + list_height;

			if (size.y != desire_size.y) {
				content_->set_size(size);
			}
		}
	}

	const tpoint actual_size = content_grid_->get_best_size();
	calculate_scrollbar(actual_size, size);

	if (size.y != desire_size.y) {
		int diff_y = desire_size.y - size.y;
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

	size.x = std::max(actual_size.x, size.x);
	size.y = std::max(actual_size.y, size.y);
	tscrollbar_container::set_content_size(origin, size);
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	if (!list_builder_) {
		list_builder_ = list_builder; 
		return;
	}

	// generator_->clear();
	unsigned count = get_item_count();

	int height_reduced = 0;
	for (; count; --count) {
		if (generator_->item(0).get_visible() != INVISIBLE) {
			height_reduced += generator_->item(0).get_height();
		}
		generator_->delete_item(0);
	}

	list_builder_ = list_builder; 

	invalidate_layout(false);
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}
} // namespace gui2

#endif
