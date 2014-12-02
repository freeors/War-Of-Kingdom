/* $Id: stacked_widget.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/stacked_widget.hpp"

#include "gui/auxiliary/widget_definition/stacked_widget.hpp"
#include "gui/auxiliary/window_builder/stacked_widget.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/generator.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_WIDGET(stacked_widget)

tstacked_widget::tstacked_widget()
	: tcontainer_(1)
	, generator_(
			tgenerator_::build(false, false, tgenerator_::independent, false))
{
}

void tstacked_widget::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<std::vector<twidget*> >& dirty_list = caller.dirty_list();
	size_t dirty_size = dirty_list.size();

	for(size_t i = 0; i < generator_->get_item_count(); ++i) {
		std::vector<twidget*> child_call_stack = call_stack;
		generator_->item(i).populate_dirty_list(caller, child_call_stack);
		
		if (dirty_list.size() != dirty_size) {
			std::vector<std::vector<twidget*> >::iterator it = dirty_list.begin();
			for (std::advance(it, dirty_size); it != dirty_list.end(); ) {
				it = dirty_list.erase(it);
			}
			caller.add_to_dirty_list(call_stack);
			return;
		}
	}
}

void tstacked_widget::layout_children()
{
	assert(generator_);
	for(unsigned i = 0; i < generator_->get_item_count(); ++i) {
		generator_->item(i).layout_children();
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

void tstacked_widget::finalize(
		std::vector<tbuilder_grid_const_ptr> widget_builder)
{
	assert(generator_);
	string_map empty_data;
	BOOST_FOREACH(const tbuilder_grid_const_ptr& builder, widget_builder) {
		generator_->create_item(-1, builder, empty_data, NULL);
	}
	swap_grid(NULL, &grid(), generator_, "_content_grid");

	for(size_t i = 0; i < generator_->get_item_count(); ++i) {
		generator_->select_item(i, true);
	}
}

const std::string& tstacked_widget::get_control_type() const
{
    static const std::string type = "stacked_widget";
    return type;
}

} // namespace gui2

