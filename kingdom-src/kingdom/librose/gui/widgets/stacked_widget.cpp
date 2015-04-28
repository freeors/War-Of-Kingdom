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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/stacked_widget.hpp"

#include "gui/auxiliary/widget_definition/stacked_widget.hpp"
#include "gui/auxiliary/window_builder/stacked_widget.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_WIDGET(stacked_widget)

tstacked_widget::tstacked_widget()
	: tcontainer_(1)
	, grid2_(NULL)
	, float_(false)
{
}

void tstacked_widget::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> layers;
	bool dirtied = false;
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	int normals = float_? childs - 1: childs;
	std::vector<tdirty_list>& dirty_list = caller.dirty_list();

	for (int n = 0; n < normals; ++ n) {
		size_t dirty_size = dirty_list.size();

		// tgrid <==> [layer]
		tgrid& grid = *(dynamic_cast<tgrid*>(children[n].widget_));
		if (grid.get_visible() != twidget::VISIBLE) {
			continue;
		}
		std::vector<twidget*> child_call_stack = call_stack;
		if (n > 0) {
			child_call_stack.insert(child_call_stack.end(), layers.begin(), layers.end());
		}
		grid.populate_dirty_list(caller, child_call_stack);
	
		if (dirty_list.size() != dirty_size) {
			dirtied = true;
			if (n > 0) {
				std::vector<tdirty_list>::iterator it = dirty_list.begin();
				for (std::advance(it, dirty_size); it != dirty_list.end(); ++ it) {
					it->children = layers;
				}
			}
		}
		layers.push_back(&grid);
	}

	if (float_) {
		tgrid& grid = *(dynamic_cast<tgrid*>(children[normals].widget_));
		std::vector<twidget*> child_call_stack = call_stack;
		child_call_stack.insert(child_call_stack.end(), layers.begin(), layers.end());
		child_call_stack.push_back(&grid);

		size_t dirty_size = dirty_list.size();
		if (dirtied) {
			tadd_to_dirty_list_lock lock;
			grid.child_populate_dirty_list(caller, child_call_stack);
		} else {
			// normal layer hasn't dirty. use normal populate dirty list.
			grid.child_populate_dirty_list(caller, child_call_stack);
		}
		if (dirty_list.size() != dirty_size) {
			std::vector<tdirty_list>::iterator it = dirty_list.begin();
			for (std::advance(it, dirty_size); it != dirty_list.end(); ++ it) {
				it->children = layers;
			}
		}
	}
}

void tstacked_widget::layout_children()
{
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	for (int n = childs - 1; n >= 0; n --) {
		tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
		grid->layout_children();
	}
}


twidget* tstacked_widget::find_at(const tpoint& coordinate, const bool must_be_active)
{ 
	// find max tow layer. 1)float layer, 2)current layer.
	int bonus_layers = float_? 1: 0;
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	for (int n = childs - 1; n >= 0; n --) {
		tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
		if (grid->get_visible() != twidget::VISIBLE) {
			if (float_ && n == childs - 1) {
				bonus_layers --;
			}
			continue;
		}
		twidget* ret = grid->find_at(coordinate, must_be_active);
		if (ret || !bonus_layers) {
			return ret;
		}
		bonus_layers --;
	}
	return NULL;
}

/** Inherited from tcontrol. */
const twidget* tstacked_widget::find_at(const tpoint& coordinate, const bool must_be_active) const
{ 
	int bonus_layers = float_? 1: 0;
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	for (int n = childs - 1; n >= 0; n --) {
		const tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
		if (grid->get_visible() != twidget::VISIBLE) {
			if (float_ && n == childs - 1) {
				bonus_layers --;
			}
			continue;
		}
		const twidget* ret = grid->find_at(coordinate, must_be_active);
		if (ret || !bonus_layers) {
			return ret;
		}
		bonus_layers --;
	}
	return NULL;
}

tpoint tstacked_widget::tgrid2::calculate_best_size() const
{
	/*
	 * The best size is the combination of the greatest width and greatest
	 * height.
	 */
	int childs = children_vsize_;

	tpoint result(0, 0);
	for (int i = 0; i < childs; ++ i) {
		const tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		const tpoint best_size = grid->get_best_size();

		if (best_size.x > result.x) {
			result.x = best_size.x;
		}

		row_height_[i] = best_size.y;
		if (best_size.y > result.y) {
			result.y = best_size.y;
		}
	}
	col_width_[0] = result.x;

	return result;
}

void tstacked_widget::tgrid2::place(const tpoint& origin, const tpoint& size)
{
	int childs = children_vsize_;

	twidget::place(origin, size);

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);

		if (grid->get_visible() != VISIBLE) {
			// normal, it should continue.
			// but set_radio_layer don't trigger place, if continue here, will result place fail.
			// of course, don't continue will increase cpu load.
			// continue;
		}
		grid->place(origin, size);
	}
}

void tstacked_widget::tgrid2::set_origin(const tpoint& origin)
{
	/*
	 * Set the origin for every item.
	 *
	 * @todo evaluate whether setting it only for the visible item is better
	 * and what the consequences are.
	 */
	twidget::set_origin(origin);

	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		
		grid->set_origin(origin);
	}
}

twidget* tstacked_widget::tgrid2::find(const std::string& id, const bool must_be_active)
{
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		if (twidget* widget = grid->find(id, must_be_active)) {
			return widget;
		}
	}
	return NULL;
}

const twidget* tstacked_widget::tgrid2::find(const std::string& id, const bool must_be_active) const
{
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		if (const twidget* widget = grid->find(id, must_be_active)) {
			return widget;
		}
	}
	return NULL;
}

void tstacked_widget::tgrid2::set_visible_area(const SDL_Rect& area)
{
	/*
	 * Set the visible area for every item.
	 *
	 * @todo evaluate whether setting it only for the visible item is better
	 * and what the consequences are.
	 */
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		grid->set_visible_area(area);
	}
}

void tstacked_widget::set_float(bool val)
{
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	VALIDATE(!val || childs >= 2, "if overlay, must not less than 2 layers!");
	float_ = val;
}

void tstacked_widget::set_radio_layer(int layer)
{
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();
	int count = childs - (float_? 1: 0);

	VALIDATE(layer < count, "layer is invalid!");

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*get_window());

	bool changed = false;
	for (int n = 0; n < count; ++ n) {
		tgrid& grid = *(dynamic_cast<tgrid*>(children[n].widget_));
		if (n != layer) {
			if (grid.get_visible() != twidget::HIDDEN) {
				changed = true;
				grid.set_visible(twidget::HIDDEN);
			}
		} else if (grid.get_visible() != twidget::VISIBLE) {
			changed = true;
			grid.set_visible(twidget::VISIBLE);
		}
	}
}

std::string tstacked_widget::generate_layout_str(const int level) const
{
	tgrid::tchild* children = grid2_->children();
	int childs = grid2_->children_vsize();

	std::stringstream ss;
	for (int n = 0; n < childs; ++ n) {
		tgrid& grid = *(dynamic_cast<tgrid*>(children[n].widget_));
		if (!ss.str().empty()) {
			ss << "| ";
		}
		ss << grid.generate_layout_str(level);
	}
	return ss.str();
}

tgrid* tstacked_widget::layer(int at) const 
{ 
	return dynamic_cast<tgrid*>(grid2_->child(at).widget_); 
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
	grid2_ = new tgrid2(*this);
	grid2_->stacked_init();

	BOOST_FOREACH(const tbuilder_grid_const_ptr& builder, widget_builder) {
		tgrid* grid = new tgrid();
		builder->build(grid);
		grid2_->stacked_insert_child(*grid, npos);
	}

	swap_grid(NULL, &grid(), grid2_, "_content_grid");
}

const std::string& tstacked_widget::get_control_type() const
{
    static const std::string type = "stacked_widget";
    return type;
}

} // namespace gui2

