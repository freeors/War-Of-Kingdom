/* $Id: tree_view.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2010 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/tree_view.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/tree_view.hpp"
#include "gui/auxiliary/window_builder/tree_view.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/tree_view_node.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/selectable.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(tree_view)

ttree_view::ttree_view(const std::vector<tnode_definition>& node_definitions)
	: tscrollbar_container(2)
	, node_definitions_(node_definitions)
	, indention_step_size_(0)
	, need_layout_(false)
	, root_node_(new ttree_view_node(
		  "root"
		, node_definitions_
		, NULL
		, *this
		, std::map<std::string, string_map>()))
	, selected_item_(NULL)
	, selection_change_callback_()
	, left_align_(false)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &ttree_view::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

ttree_view_node& ttree_view::add_node(const std::string& id
		, const std::map<std::string /* widget id */, string_map>& data)
{
	return get_root_node().add_child(id, data);
}

void ttree_view::remove_node(ttree_view_node* node)
{
	assert(node && node != root_node_ && node->parent_node_);
	const tpoint node_size = node->get_size();

	boost::ptr_vector<ttree_view_node>::iterator itor =
				  node->parent_node_->children_.begin();

	for( ; itor != node->parent_node_->children_.end(); ++itor) {
		if(&*itor == node) {
			break;
		}
	}

	assert(itor != node->parent_node_->children_.end());

	node->parent_node_->children_.erase(itor);

	if(get_size() == tpoint(0, 0)) {
		return;
	}

	// Don't shrink the width, need to think about a good algorithm to do so.
	resize_content(0, -node_size.y);
}

void ttree_view::child_populate_dirty_list(twindow& caller
		, const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tscrollbar_container::child_populate_dirty_list(caller, call_stack);

	assert(root_node_);
	root_node_->impl_populate_dirty_list(caller, call_stack);
}

bool ttree_view::empty() const
{
	return root_node_->empty();
}

void ttree_view::layout_children()
{
	layout_children(false);
}

void ttree_view::set_select_item(ttree_view_node* node)
{
	if (selected_item_ == node) {
		return;
	}

	if (selected_item_ && selected_item_->label()) {
		selected_item_->label()->set_value(false);
	}

	selected_item_ = node;
	selected_item_->label()->set_value(true);

	if (selection_change_callback_) {
		selection_change_callback_();
	}
}

tpoint ttree_view::adjust_content_size(const tpoint& size)
{
	if (!left_align_ || empty()) {
		return size;
	}
	tgrid& item = root_node_->children_.begin()->grid_;
	// by this time, hasn't called place(), cannot use get_size().
	int height = item.get_best_size().y;
	if (height > size.y) {
		return size;
	}
	int list_height = size.y / height * height;

	// reduce hight if necessary.
	height = root_node_->get_best_size().y;
	if (list_height > height) {
		list_height = height;
	}
	return tpoint(size.x, list_height);
}

void ttree_view::adjust_offset(int& x_offset, int& y_offset)
{
	if (!left_align_ || empty() || !y_offset) {
		return;
	}
	tgrid& item = root_node_->children_.begin()->grid_;
	int height = item.get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
	}
}

void ttree_view::resize_content(
		  const int width_modification
		, const int height_modification)
{
	DBG_GUI_L << LOG_HEADER << " current size " << content_grid()->get_size()
			<< " width_modification " << width_modification
			<< " height_modification " << height_modification
			<< ".\n";

	if(content_resize_request(width_modification, height_modification)) {

		// Calculate new size.
		tpoint size = content_grid()->get_size();
		size.x += width_modification;
		size.y += height_modification;

		// Set new size.
		content_grid()->set_size(size);

		// Set status.
		need_layout_ = true;
		// If the content grows assume it "overwrites" the old content.
		if(width_modification < 0 || height_modification < 0) {
			set_dirty();
		}
		DBG_GUI_L << LOG_HEADER << " succeeded.\n";
	} else {
		DBG_GUI_L << LOG_HEADER << " failed.\n";
	}
}

void ttree_view::layout_children(const bool force)
{
	assert(root_node_ && content_grid());

	if(need_layout_ || force) {
		root_node_->place(indention_step_size_
			, get_origin()
			, content_grid()->get_size().x);
		root_node_->set_visible_area(content_visible_area_);

		need_layout_ = false;
	}
}

void ttree_view::finalize_setup()
{
	// Inherited.
	tscrollbar_container::finalize_setup();

	assert(content_grid());
	content_grid()->set_rows_cols(1, 1);
	content_grid()->set_child(
			  root_node_
			, 0
			, 0
			, tgrid::VERTICAL_GROW_SEND_TO_CLIENT
				| tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT
			, 0);
}
/*
twidget* ttree_view::find_at(
		const tpoint& coordinate, const bool must_be_active)
{
	twidget* w = tscrollbar_container::find_at(coordinate, must_be_active);
	if (!w && selected_item_) {
		// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
		// as find, function is called in event chain, so finded must be child, here select selected_item_ as child.
		w = twidget::find_at(coordinate, must_be_active);
		if (w) {
			w = selected_item_;
		}
	}
	return w;
}
*/
const std::string& ttree_view::get_control_type() const
{
	static const std::string type = "tree_view";
	return type;
}

void ttree_view::signal_handler_left_button_down(const event::tevent event)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	get_window()->keyboard_capture(this);
}

} // namespace gui2

