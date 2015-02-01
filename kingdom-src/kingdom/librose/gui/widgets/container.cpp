/* $Id: container.cpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#include "gui/widgets/container.hpp"

#include "formula_string_utils.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/log.hpp"

#define LOG_SCOPE_HEADER "tcontainer(" + get_control_type() + ") [" \
		+ id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

void tradio_page::parse_cfg(const config& cfg, std::vector<tradio_page::tpage>& result)
{
	if (!cfg) {
		// it isn't radio-style.
		return;
	}

	BOOST_FOREACH(const config& page_cfg, cfg.child_range("page")) {
		result.push_back(tpage());
		tpage& page = result.back();

		if (page_cfg.child("linked_group")) {
			BOOST_FOREACH(const config& lg, page_cfg.child_range("linked_group")) {
				tlinked_group linked_group;
				linked_group.id = lg["id"].str();
				linked_group.fixed_width = lg["fixed_width"].to_bool();
				linked_group.fixed_height = lg["fixed_height"].to_bool();

				VALIDATE(!linked_group.id.empty()
						, missing_mandatory_wml_key("linked_group", "id"));

				if(!(linked_group.fixed_width || linked_group.fixed_height)) {
					utils::string_map symbols;
					symbols["id"] = linked_group.id;
					t_string msg = vgettext(
							  "Linked '$id' group needs a 'fixed_width' or "
								"'fixed_height' key."
							, symbols);

					VALIDATE(false, msg);
				}

				page.linked_groups.push_back(linked_group);
			}
		}

		if (page_cfg.child("list_definition")) {
			page.header = new tbuilder_grid(page_cfg.child("header"));
			page.row = new tbuilder_grid(page_cfg.child("list_definition"));
		} else {
			page.header = new tbuilder_grid(page_cfg.child("grid"));
		}
	}
}

tradio_page::tradio_page(const std::vector<tpage>& pages, twidget* widget)
	: pages_(pages)
	, widget_(widget)
	, current_page_(NO_PAGE)
	, swaping_page_(NO_PAGE)
{}

bool tradio_page::swap_uh(twindow& window, int page)
{
	VALIDATE(swaping_page_ == NO_PAGE, "Program require call swap_bh before it!");
	VALIDATE(page >= 0 && page < (int)pages_.size(), "Invalid page number!");

	if (page == current_page_) {
		return false;
	}

	bool first = current_page_ == NO_PAGE;
	window.radio_page_swap_uh(pages_[page], widget_, first);
	
	if (!first) {
		swaping_page_ = page;

	} else {
		// first is fill data. needn't call swap_bh.
		current_page_ = page;
	}
	return true;
}

void tradio_page::swap_bh(twindow& window)
{
	VALIDATE(swaping_page_ != NO_PAGE, "Program require to call swap_uh before it!");
	VALIDATE(current_page_ != NO_PAGE, "Don't call swap_bh when fill data!");

	window.radio_page_swap_bh(pages_[swaping_page_], widget_);
	
	current_page_ = swaping_page_;
	swaping_page_ = NO_PAGE;
}

void tcontainer_::layout_init(const bool full_initialization)
{
	// Inherited.
	tcontrol::layout_init(full_initialization);

	grid_.layout_init(full_initialization);
}

void tcontainer_::reduce_width(const unsigned maximum_width)
{
	grid_.reduce_width(maximum_width - border_space().x);
}

void tcontainer_::request_reduce_width(const unsigned maximum_width)
{
	grid_.request_reduce_width(maximum_width - border_space().x);
}

void tcontainer_::demand_reduce_width(const unsigned maximum_width)
{
	grid_.demand_reduce_width(maximum_width - border_space().x);
}

void tcontainer_::reduce_height(const unsigned maximum_height)
{
	grid_.reduce_height(maximum_height - border_space().y);
}

void tcontainer_::request_reduce_height(const unsigned maximum_height)
{
	grid_.request_reduce_height(maximum_height - border_space().y);
}

void tcontainer_::demand_reduce_height(const unsigned maximum_height)
{
	grid_.demand_reduce_height(maximum_height - border_space().y);
}

void tcontainer_::place(const tpoint& origin, const tpoint& size)
{
	tcontrol::place(origin, size);

	const SDL_Rect rect = get_client_rect();
	const tpoint client_size(rect.w, rect.h);
	const tpoint client_position(rect.x, rect.y);
	grid_.place(client_position, client_size);
}

tpoint tcontainer_::calculate_best_size() const
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);

	tpoint result(grid_.get_best_size());
	const tpoint border_size = border_space();

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	if(result.x) {
		result.x += border_size.x;
	}

	if(result.y) {
		result.y += border_size.y;
	}

	DBG_GUI_L << LOG_HEADER
			<< " border size " << border_size
			<< " returning " << result
			<< ".\n";

	return result;
}

void tcontainer_::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	const SDL_Rect rect = get_client_rect();
	const tpoint client_position(rect.x, rect.y);
	grid_.set_origin(client_position);
}

void tcontainer_::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	grid_.set_visible_area(area);
}

void tcontainer_::impl_draw_children(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	assert(get_visible() == twidget::VISIBLE
			&& grid_.get_visible() == twidget::VISIBLE);

	grid_.draw_children(frame_buffer, x_offset, y_offset);
}

void tcontainer_::layout_children()
{
	grid_.layout_children();
}

void tcontainer_::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> child_call_stack = call_stack;
	grid_.populate_dirty_list(caller, child_call_stack);
}

void tcontainer_::set_active(const bool active)
{
	// Not all our children might have the proper state so let them run
	// unconditionally.
	grid_.set_active(active);

	if(active == get_active()) {
		return;
	}

	set_dirty();

	set_self_active(active);
}

bool tcontainer_::disable_click_dismiss() const
{
	return tcontrol::disable_click_dismiss() && grid_.disable_click_dismiss();
}

void tcontainer_::init_grid(
		const boost::intrusive_ptr<tbuilder_grid>& grid_builder)
{
	log_scope2(log_gui_general, LOG_SCOPE_HEADER);

	assert(initial_grid().get_rows() == 0 && initial_grid().get_cols() == 0);

	grid_builder->build(&initial_grid());
}

} // namespace gui2

