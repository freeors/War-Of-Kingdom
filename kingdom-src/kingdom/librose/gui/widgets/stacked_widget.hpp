/* $Id: stacked_widget.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_STACKED_WIDGET_HPP_INCLUDED
#define GUI_WIDGETS_STACKED_WIDGET_HPP_INCLUDED

#include "gui/widgets/container.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_stacked_widget;
}

class tstacked_widget
		: public tcontainer_
{
	friend struct implementation::tbuilder_stacked_widget;
	friend class tdebug_layout_graph;

public:
	class tgrid2: public tgrid
	{
	public:
		tgrid2(tstacked_widget& stacked)
			: stacked_(stacked)
		{}

		tpoint calculate_best_size() const;

		void set_visible_area(const SDL_Rect& area);

		void set_origin(const tpoint& origin);
		void place(const tpoint& origin, const tpoint& size);

		twidget* find(const std::string& id, const bool must_be_active);
		const twidget* find(const std::string& id, const bool must_be_active) const;

	private:
		tstacked_widget& stacked_;
	};

	tstacked_widget();

	/***** ***** ***** inherited ***** ****** *****/
	/** Inherited from tcontainer_ */
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack);

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/** Inherited from tscrollbar_container. */
	void layout_children();

	/** Inherited from tcontrol. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active);

	/** Inherited from tcontrol. */
	const twidget* find_at(const tpoint& coordinate, const bool must_be_active) const;

	void set_float(bool val);

	void set_radio_layer(int layer);

	std::string generate_layout_str(const int level) const;

	tgrid* layer(int at) const;

private:

	/**
	 * Finishes the building initialization of the widget.
	 *
	 * @param widget_builder      The builder to build the contents of the
	 *                            widget.
	 */
	void finalize(std::vector<tbuilder_grid_const_ptr> widget_builder);

	/**
	 * Contains a pointer to the generator.
	 *
	 * The pointer is not owned by this class, it's stored in the content_grid_
	 * of the tscrollbar_container super class and freed when it's grid is
	 * freed.
	 */
	tgrid2* grid2_;

	bool float_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/) {}
};

} // namespace gui2

#endif

