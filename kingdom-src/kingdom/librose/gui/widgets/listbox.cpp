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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(listbox)

namespace {

bool callback_list_item_clicked(twidget* caller, const int type)
{
	return get_parent<tlistbox>(caller)->list_item_clicked(caller, type);
}

} // namespace

tlistbox::tlistbox()
	: tscrollbar_container(2, true) // FIXME magic number
	, list_builder_(NULL)
	, callback_value_changed_(NULL)
	, dynamic_(false)
	, list_grid_(NULL)
	, cursel_(npos)
	, drag_at_(npos)
	, left_drag_grid_(NULL)
	, left_drag_grid_size_(0, 0)
	, row_align_(true)
{
}

tlistbox::~tlistbox()
{
	if (left_drag_grid_) {
		delete left_drag_grid_;
	}
}

void tlistbox::add_row(const std::map<std::string /* widget id */, string_map>& data, const int index)
{
	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(list_builder_->widgets[0]->build());
	widget->set_callback_state_pre_change(callback_list_item_clicked);
	if (left_drag_grid_) {
		widget->set_callback_control_drag_detect(boost::bind(&tlistbox::callback_control_drag_detect, this, _1, _2, _3));
		widget->set_callback_pre_impl_draw_children(boost::bind(&tlistbox::callback_pre_impl_draw_children, this, _1, _2, _3, _4));
		widget->set_callback_set_drag_coordinate(boost::bind(&tlistbox::callback_set_drag_coordinate, this, _1, _2, _3));
	}
	widget->set_child_members(data);
	widget->at_ = list_grid_->listbox_insert_child(*widget, index);

	if (cursel_ == npos) {
		select_row(widget);
	}

	// don't call invalidate_layout.
	// caller maybe call add_row continue, will result to large effect burden
}

bool tlistbox::callback_control_drag_detect(tcontrol* control, bool start, const tdrag_direction type)
{
	// set_visible spend a lot of time.
	twindow::tinvalidate_layout_blocker block(*this->get_window());

	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(control);
	if (start) {
		if (cursel_ != npos && widget->at_ != cursel_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
			list_grid_->child(cursel_).widget_->set_dirty();
		}
		left_drag_grid_->place(tpoint(content_->get_x() + content_->get_width() - left_drag_grid_size_.x, widget->get_y()),
			tpoint(left_drag_grid_size_.x, widget->get_height()));
		left_drag_grid_->set_visible(twidget::VISIBLE);
		left_drag_grid_->set_visible_area(empty_rect);

		drag_at_ = widget->at_;

	} else if (type == drag_none) {
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		widget->set_draw_offset(0, 0);

		drag_at_ = npos;

	} else if (type == drag_left) {
		widget->set_draw_offset(-1 * (content_->get_x() + content_->get_width() - left_drag_grid_->get_x()), 0);
		left_drag_grid_->set_visible_area(left_drag_grid_->get_rect());
	}

	return true;
}

void tlistbox::callback_pre_impl_draw_children(tcontrol* control, surface& frame_buffer, int x_offset, int y_offset)
{
	if (left_drag_grid_->get_visible() == twidget::VISIBLE) {
		left_drag_grid_->impl_draw_children(frame_buffer, x_offset, y_offset);
	}
}

bool tlistbox::callback_set_drag_coordinate(tcontrol* control, const tpoint& first, const tpoint& last)
{
	if (left_drag_grid_->get_visible() == twidget::VISIBLE) {
		int diff = first.x - last.x;
		if (diff > left_drag_grid_size_.x) {
			diff = left_drag_grid_size_.x;
		} else if (diff < 0) {
			diff = 0;
		}
		SDL_Rect area = ::create_rect(left_drag_grid_->get_x() + left_drag_grid_size_.x - diff, left_drag_grid_->get_y(), 
			diff, left_drag_grid_->get_height());
		left_drag_grid_->set_visible_area(area);
	}

	return true;
}

void tlistbox::cancel_drag()
{
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		// set_visible spend a lot of time.
		twindow::tinvalidate_layout_blocker block(*this->get_window());

		VALIDATE(drag_at_ >= 0 && drag_at_ < list_grid_->children_vsize(), null_str);
		dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_)->set_draw_offset(0, 0);
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		drag_at_ = npos;
	}
}

// if count equal 0, remove all.
void tlistbox::remove_row(int row, int count)
{
	if (row < 0 || row >= get_item_count()) {
		return;
	}
	// cursel_ maybe npos. when all item are invisible!

	if (!count) {
		row = 0;
		count = get_item_count();

	} else if (row + count > get_item_count()) {
		count = get_item_count() - row;
	}
	
	bool remove_all = !row && count == get_item_count();
	bool cursel_is_remove = cursel_ != npos && (cursel_ >= row && cursel_ < row + count);
	bool drag_is_remove = drag_at_ != npos && (drag_at_ >= row && drag_at_ < row + count);

	for (; count; -- count) {
		list_grid_->listbox_erase_child(row);
	}

	// update subsequent panel's at_.
	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	for (int at = row; at < childs; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
		widget->at_ = at;
	}

	if (!remove_all) {
		if (cursel_is_remove) {
			int original_cursel = cursel_;
			cursel_ = npos;
			ttoggle_panel* widget = next_selectable_row(original_cursel, true);
			if (widget) {
				select_row(widget);
			}
		}
	} else {
		cursel_ = npos;
	}

	if (drag_is_remove) {
		// set_visible spend a lot of time.
		twindow::tinvalidate_layout_blocker block(*this->get_window());

		VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		drag_at_ = npos;
	}

	// don't call invalidate_layout(true).
	// caller maybe call remove_row continue, will result to large effect burden
	invalidate_layout(false);
}

void tlistbox::clear()
{
	// Due to the removing from the linked group, don't use
	remove_row(0, 0);
}

class sort_func
{
public:
	sort_func(void* caller, bool (*callback)(void*, twidget&, twidget&)) : caller_(caller), callback_(callback)
	{}

	bool operator()(twidget* a, twidget* b) const
	{
		return callback_(caller_, *a, *b);
	}

private:
	void* caller_;
	bool (*callback_)(void*, twidget&, twidget&);
};

void tlistbox::sort(void* caller, bool (*callback)(void*, twidget&, twidget&))
{
	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	if (!childs) {
		return;
	}

	int original_cursel = cursel_;
	bool check_cursel = cursel_ != npos;

	std::vector<twidget*> tmp;
	tmp.resize(childs, NULL);
	for (int n = 0; n < childs; n ++) {
		tmp[n] = children[n].widget_;
	}
	std::stable_sort(tmp.begin(), tmp.end(), sort_func(caller, callback));
	for (int n = 0; n < childs; n ++) {
		children[n].widget_ = tmp[n];
		ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		if (check_cursel && panel->at_ == cursel_) {
			cursel_ = n;
			check_cursel = false;
		}
		panel->at_ = n;
	}
}

int tlistbox::get_item_count() const
{
	return list_grid_->children_vsize();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(0, 0).widget_);
	widget->set_active(active);
}

void tlistbox::set_row_shown(const int row, const bool visible)
{
	if (row < 0 || row >= get_item_count()) {
		return;
	}
	twidget* widget = list_grid_->child(row, 0).widget_;
	if (visible) {
		if (widget->get_visible() == twidget::VISIBLE) {
			return;
		}
	} else if (widget->get_visible() == twidget::INVISIBLE) {
		return;
	}

	const int original_selected_row = get_selected_row();

	{
		twindow *window = get_window();
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);
		
		widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
		if (!visible) {
			int original_cursel = cursel_;
			cursel_ = npos;
			ttoggle_panel* widget = next_selectable_row(original_cursel, true);
			if (widget) {
				select_row(widget);
			}
		}
	}

	if (original_selected_row != get_selected_row() && cursel_ != npos && callback_value_changed_) {
		callback_value_changed_(this, drag_none);
	}
	invalidate_layout(false);

}

void tlistbox::set_row_shown(const std::vector<bool>& shown)
{
	if (!get_item_count()) {
		return;
	}

	VALIDATE(false, "Don't support!");
}

twidget* tlistbox::get_row_panel(const unsigned row) const
{
	const tgrid::tchild* children = list_grid_->children();
	return children[row].widget_;
}

void tlistbox::select_row(const unsigned row)
{
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	if ((int)row >= childs) {
		return;
	}
	select_row(children[row].widget_);
}

void tlistbox::select_row(twidget* widget)
{
	ttoggle_panel* desire_panel = dynamic_cast<ttoggle_panel*>(widget);
	if (desire_panel->at_ == cursel_) {
		return;
	}
	if (cursel_ != npos) {
		const tgrid::tchild& child = list_grid_->child(cursel_, 0);
		ttoggle_panel* that = dynamic_cast<ttoggle_panel*>(child.widget_);

		VALIDATE(that->get_value(), "Previous toggle panel must be selected!");
		that->set_value(false);
		that->set_dirty();
	}

	desire_panel->set_value(true);
	cursel_ = desire_panel->at_;
	desire_panel->set_dirty();
}

int tlistbox::get_selected_row() const
{
	return cursel_;
}

bool tlistbox::list_item_clicked(twidget* caller, const int type)
{
	/** @todo Hack to capture the keyboard focus. */
	get_window()->keyboard_capture(this);

	ttoggle_panel* clicked_panel = dynamic_cast<ttoggle_panel*>(caller);
	if (clicked_panel->at_ != cursel_) {
		if (cursel_ != npos) {
			dynamic_cast<tcontrol*>(list_grid_->child(cursel_).widget_)->set_draw_offset(0, 0);
		}
		if (type != drag_none && left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
			// clicked_panel->set_draw_offset(-1 * (content_->get_x() + content_->get_width() - left_drag_grid_->get_x()), 0);
		}
		select_row(caller);
	}
	if (callback_value_changed_) {
		callback_value_changed_(this, type);
	}
	return true;
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
	const int selected_row = get_selected_row();
	if (selected_row != npos) {
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = list_grid_->child(0, selected_row).widget_->get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		// show_content_rect(rect);
		scrollbar_moved();
	}
}

tpoint tlistbox::list_calculate_best_size() const
{
	if (!dynamic_) {
		return list_grid_->tgrid::calculate_best_size();
	}

	// The best size is the sum of the heights and the greatest width.
	tpoint result(0, 0);
	return result;

	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	for (int n = 0; n < childs; n ++) {
		tcontrol* widget = dynamic_cast<tcontrol*>(children[n].widget_);
		if (widget->get_visible() == twidget::INVISIBLE || !widget->get_active()) {
			continue;
		}

		const tpoint best_size = widget->get_best_size();
		if (best_size.x > result.x) {
			result.x = best_size.x;
		}
		result.y += best_size.y;
	}
	return result;
}

void tlistbox::list_place(const tpoint& origin, const tpoint& size)
{
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	tpoint current_origin = origin;
	tpoint best_size(0, 0);

	if (!dynamic_) {
		list_grid_->tgrid::place(origin, size);
/*
		list_grid_->twidget::place(origin, size);

		for (int n = 0; n < childs; ++ n) {
			tcontrol* widget = dynamic_cast<tcontrol*>(children[n].widget_);
			if (widget->get_visible() != twidget::VISIBLE) {
				continue;
			}

			best_size = widget->get_best_size();
			best_size.x = size.x;

			widget->place(current_origin, best_size);
			current_origin.y += best_size.y;
		}
*/
		return;
	}

	/*
	 * - Set every item to its best size.
	 * - The origin gets increased with the height of the last item.
	 * - No item should be wider as the size.
	 * - In the end the origin should be the sum or the origin and the wanted
	 *   height.
	 */

	for (int n = 0; n < childs; ++ n) {
		tcontrol* widget = dynamic_cast<tcontrol*>(children[n].widget_);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		{
			treduce_width_lock lock;

			best_size = widget->get_best_size();
			if (best_size.x > size.x) {
				// require reduce width
				best_size = widget->request_reduce_width(size.x);
			}
		
			// FIXME should we look at grow factors???
			best_size.x = size.x;

			widget->place(current_origin, best_size);
		}

		current_origin.y += best_size.y;
	}

	tpoint list_grid_size = size;
	if (current_origin.y - origin.y > size.y) {
		list_grid_size.y = current_origin.y - origin.y;
	}

	list_grid_->set_layout_size(list_grid_size);

	tpoint actual_size = content_grid_->get_best_size();
	calculate_scrollbar(actual_size, size);
/*
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
*/
	list_grid_->twidget::place(origin, list_grid_size);
	content_grid_->twidget::set_size(actual_size);
}

void tlistbox::list_set_origin(const tpoint& origin)
{
	list_grid_->tgrid::set_origin(origin);
/*
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	tpoint current_origin = origin;
	for (int n = 0; n < childs; ++ n) {
		tcontrol* widget = dynamic_cast<tcontrol*>(children[n].widget_);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		widget->set_origin(current_origin);
		current_origin.y += widget->get_height();
	}
*/
}

void tlistbox::list_set_visible_area(const SDL_Rect& area)
{
	list_grid_->tgrid::set_visible_area(area);
}

void tlistbox::list_impl_draw_children(surface& frame_buffer, int x_offset, int y_offset)
{
	list_grid_->tgrid::impl_draw_children(frame_buffer, x_offset, y_offset);
}

void tlistbox::adjust_offset(int& x_offset, int& y_offset)
{
	if (dynamic_ || !row_align_) {
		return;
	}

	unsigned items = list_grid_->children_vsize();
	if (!items || !y_offset) {
		return;
	}
	int height = list_grid_->child(0, 0).widget_->get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
	}
}

void tlistbox::set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	list_grid_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));

	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		const twidget* parent = list_grid_->child(drag_at_).widget_;
		left_drag_grid_->set_origin(tpoint(left_drag_grid_->get_x(), parent->get_y()));	
	}
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
	list_grid_->set_visible_area(list_area);
}

twidget* tlistbox::find_at(const tpoint& coordinate, const bool must_be_active)
{
	twidget* result = NULL;
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_);
		if (!widget->drag_detect_started()) {
			result = left_drag_grid_->find_at(coordinate, must_be_active);
		}
	}
	if (!result) {
		result = tscrollbar_container::find_at(coordinate, must_be_active);
	}
	return result;
}

const twidget* tlistbox::find_at(const tpoint& coordinate, const bool must_be_active) const
{
	const twidget* result = NULL;
	if (left_drag_grid_->get_visible() == twidget::VISIBLE) {
		tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_);
		if (!widget->drag_detect_started()) {
			result = left_drag_grid_->find_at(coordinate, must_be_active);
		}
	}
	if (!result) {
		result = tscrollbar_container::find_at(coordinate, must_be_active);
	}
	return result;
}

void tlistbox::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		std::vector<twidget*> child_call_stack(call_stack);
		child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
		left_drag_grid_->populate_dirty_list(caller, child_call_stack);
	}

	tscrollbar_container::child_populate_dirty_list(caller, call_stack);
}

void tlistbox::scroll_to_row(const unsigned row)
{
	//
	// This function only call list is complete! that mean no modify.
	//

	if ((int)row >= get_item_count()) {
		return;
	}

	const SDL_Rect& visible = content_visible_area();
	SDL_Rect rect = list_grid_->child(0, row).widget_->get_rect();

	rect.x = visible.x;
	if (rect.y < visible.y) {
		tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
		int header_height = header->get_best_size().y;
		rect.y -= header_height;
	}
	rect.w = visible.w;

	show_content_rect(rect);
}

int tlistbox::list_grid_handle_key_up_arrow(SDLMod /*modifier*/, bool& handled)
{
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	if (!childs) {
		return npos;
	}

	// NOTE maybe this should only work if we can select only one item...
	handled = true;

	for (int i = get_selected_row() - 1; i >= 0; -- i) {
		// NOTE we check the first widget to be active since grids have no
		// active flag. This method might not be entirely reliable.
		ttoggle_panel* control = dynamic_cast<ttoggle_panel*>(children[i].widget_);
		if (control->can_selectable()) {
			select_row(control);
			return i;
		}
	}
	return npos;
}

ttoggle_panel* tlistbox::next_selectable_row(int start, bool invert) const
{
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	if (!childs) {
		return NULL;
	}

	for (int i = start; i < childs; i ++) {
		ttoggle_panel* control = dynamic_cast<ttoggle_panel*>(children[i].widget_);
		if (control->get_active() && control->get_visible() == twidget::VISIBLE) {
			return control;
		}
	}
	if (!invert) {
		return NULL;
	}
	for (int i = start - 1; i >= 0; i --) {
		ttoggle_panel* control = dynamic_cast<ttoggle_panel*>(children[i].widget_);
		if (control->can_selectable()) {
			return control;
		}
	}
	return NULL;
}

int tlistbox::list_grid_handle_key_down_arrow(SDLMod /*modifier*/, bool& handled)
{
	ttoggle_panel* valid = next_selectable_row(get_selected_row() + 1, false);
	if (valid) {
		select_row(valid);

		// NOTE maybe this should only work if we can select only one item...
		handled = true;
		return valid->at_;
	}
	return npos;
}

void tlistbox::handle_key_up_arrow(SDLMod modifier, bool& handled)
{
	int cursel = list_grid_handle_key_up_arrow(modifier, handled);

	if (handled && cursel != npos) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = list_grid_->child(0, cursel).widget_->get_rect();

		rect.x = visible.x;
		if (rect.y < visible.y) {
			tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
			int header_height = header->get_best_size().y;
			rect.y -= header_height;
		}
		rect.w = visible.w;

		show_content_rect(rect);

		if (callback_value_changed_) {
			callback_value_changed_(this, drag_none);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_down_arrow(SDLMod modifier, bool& handled)
{
	int cursel = list_grid_handle_key_down_arrow(modifier, handled);

	if (handled && cursel != npos) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = list_grid_->child(0, cursel).widget_->get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if (callback_value_changed_) {
			callback_value_changed_(this, drag_none);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
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

	if (header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if (footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	list_grid_ = new tgrid2(*this);
	list_grid_->listbox_init();
	swap_grid(NULL, content_grid(), list_grid_, "_list_grid");
}

void tlistbox::place_content_grid(const tpoint& content_origin, const tpoint& desire_size, const tpoint& origin)
{
	if (dynamic_) {
		list_grid_->set_layout_size(tpoint(0, 0));
		content_grid_->place(origin, desire_size);
		return;
	}

	tpoint size = desire_size;
	unsigned items = list_grid_->children_vsize();
	if (row_align_ && items) {
		tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
		// by this time, hasn't called place(), cannot use get_size().
		int header_height = header->get_best_size().y;
		int height = list_grid_->child(0, 0).widget_->get_best_size().y;
		if (header_height + height <= size.y) {
			int list_height = size.y - header_height;
			list_height = list_height / height * height;

			// reduce hight if allow height > header_height + get_best_size().y
			height = list_grid_->get_best_size().y;
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
	content_grid_->place(origin, size);
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	VALIDATE(!list_builder_.get(), null_str);

	list_builder_ = list_builder;
	if (list_builder_->widgets.size() >= 2) {
		left_drag_grid_ = dynamic_cast<tgrid*>(list_builder_->widgets[1]->build());
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		left_drag_grid_size_ = left_drag_grid_->get_best_size();

		// don't set row(toggle_panel) to it's parent.
		// grid mayb containt "erase" button, it will erase row(toggle_panel). if it is parent, thine became complex.
		left_drag_grid_->set_parent(this);
	}
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}

} // namespace gui2