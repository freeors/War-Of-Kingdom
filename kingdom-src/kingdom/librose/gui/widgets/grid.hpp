/* $Id: grid.hpp 54906 2012-07-29 19:52:01Z mordante $ */
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

#ifndef GUI_WIDGETS_GRID_HPP_INCLUDED
#define GUI_WIDGETS_GRID_HPP_INCLUDED

#include "gui/widgets/widget.hpp"

namespace gui2 {

class tspacer;

/**
 * Base container class.
 *
 * This class holds a number of widgets and their wanted layout parameters. It
 * also layouts the items in the grid and hanldes their drawing.
 */
class tgrid : public virtual twidget
{
	friend class tdebug_layout_graph;
	friend struct tgrid_implementation;

public:

	explicit tgrid(const unsigned rows = 0, const unsigned cols = 0);

	virtual ~tgrid();

	/***** ***** ***** ***** LAYOUT FLAGS ***** ***** ***** *****/
	enum {
		VERTICAL_GROW_SEND_TO_CLIENT   = 1,
		VERTICAL_ALIGN_TOP             = 2,
		VERTICAL_ALIGN_CENTER          = 3,
		VERTICAL_ALIGN_BOTTOM          = 4,
		VERTICAL_MASK                  = 7,
    
	
		HORIZONTAL_GROW_SEND_TO_CLIENT = 1 << 3,
		HORIZONTAL_ALIGN_LEFT          = 2 << 3,
		HORIZONTAL_ALIGN_CENTER        = 3 << 3,
		HORIZONTAL_ALIGN_RIGHT         = 4 << 3,
		HORIZONTAL_MASK                = 7 << 3,
    
		BORDER_TOP                     = 1 << 6,
		BORDER_BOTTOM                  = 1 << 7,
		BORDER_LEFT                    = 1 << 8,
		BORDER_RIGHT                   = 1 << 9
	};

	static const unsigned VERTICAL_SHIFT                 = 0;
	static const unsigned HORIZONTAL_SHIFT               = 3;
	static const unsigned BORDER_ALL                     =
		BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT;

	/***** ***** ***** ***** ROW COLUMN MANIPULATION ***** ***** ***** *****/

	/**
	 * Addes a row to end of the grid.
	 *
	 * @param count               Number of rows to add, should be > 0.
	 *
	 * @returns                   The row number of the first row added.
	 */
	unsigned add_row(const unsigned count = 1);

	/**
	 * Sets the grow factor for a row.
	 *
	 * @todo refer to a page with the layout manipulation info.
	 *
	 * @param row                 The row to modify.
	 * @param factor              The grow factor.
	 */
	void set_row_grow_factor(const unsigned row, const unsigned factor)
	{
		assert(row < row_grow_factor_.size());
		row_grow_factor_[row] = factor;
		set_dirty();
	}

	/**
	 * Sets the grow factor for a column.
	 *
	 * @todo refer to a page with the layout manipulation info.
	 *
	 * @param column              The column to modify.
	 * @param factor              The grow factor.
	 */
	void set_column_grow_factor(const unsigned column, const unsigned factor)
	{
		assert(column< col_grow_factor_.size());
		col_grow_factor_[column] = factor;
		set_dirty();
	}

	/***** ***** ***** ***** CHILD MANIPULATION ***** ***** ***** *****/

	/**
	 * Sets a child in the grid.
	 *
	 * When the child is added to the grid the grid 'owns' the widget.
	 * The widget is put in a cell, and every cell can only contain 1 widget if
	 * the wanted cell already contains a widget, that widget is freed and
	 * removed.
	 *
	 * @param widget              The widget to put in the grid.
	 * @param row                 The row of the cell.
	 * @param col                 The columnof the cell.
	 * @param flags               The flags for the widget in the cell.
	 * @param border_size         The size of the border for the cell, how the
	 *                            border is applied depends on the flags.
	 */
	void set_child(twidget* widget, const unsigned row,
		const unsigned col, const unsigned flags, const unsigned border_size);

	/**
	 * Exchangs a child in the grid.
	 *
	 * It replaced the child with a certain id with the new widget but doesn't
	 * touch the other settings of the child.
	 *
	 * @param id                  The id of the widget to free.
	 * @param widget              The widget to put in the grid.
	 * @param recurse             Do we want to decent into the child grids.
	 * @param new_parent          The new parent for the swapped out widget.
	 *
	 * returns                    The widget which got removed (the parent of
	 *                            the widget is cleared). If no widget found
	 *                            and thus not replace NULL will returned.
	 */
	twidget* swap_child(
		const std::string& id, twidget* widget, const bool recurse,
		twidget* new_parent = NULL);

	/**
	 * Removes and frees a widget in a cell.
	 *
	 * @param row                 The row of the cell.
	 * @param col                 The columnof the cell.
	 */
	void remove_child(const unsigned row, const unsigned col);

	/**
	 * Removes and frees a widget in a cell by it's id.
	 *
	 * @param id                  The id of the widget to free.
	 * @param find_all            If true if removes all items with the id,
	 *                            otherwise it stops after removing the first
	 *                            item (or once all children have been tested).
	 */
	void remove_child(const std::string& id, const bool find_all = false);

	/**
	 * Activates all children.
	 *
	 * If a child inherits from tcontrol or is a tgrid it will call
	 * set_active() for the child otherwise it ignores the widget.
	 *
	 * @param active              Parameter for set_active.
	 */
	void set_active(const bool active);


	/** Returns the widget in the selected cell. */
	const twidget* widget(const unsigned row, const unsigned col) const
		{ return child(row, col).widget_; }

	/** Returns the widget in the selected cell. */
	twidget* widget(const unsigned row, const unsigned col)
		{ return child(row, col).widget_; }

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/** Inherited from twidget. */
	void layout_init(const bool full_initialization);

	/**
	 * Tries to reduce the width of a container.
	 *
	 * @see @ref layout_algorithm for more information.
	 *
	 * @param maximum_width       The wanted maximum width.
	 */
	void reduce_width(const unsigned maximum_width);

	/** Inherited from twidget. */
	void request_reduce_width(const unsigned maximum_width);

	/** Inherited from twidget. */
	void demand_reduce_width(const unsigned maximum_width);

	/**
	 * Tries to reduce the height of a container.
	 *
	 * @see @ref layout_algorithm for more information.
	 *
	 * @param maximum_height      The wanted maximum height.
	 */
	void reduce_height(const unsigned maximum_height);

	/** Inherited from twidget. */
	void request_reduce_height(const unsigned maximum_height);

	/** Inherited from twidget. */
	void demand_reduce_height(const unsigned maximum_height);

	/** Inherited from twidget. */
	tpoint calculate_best_size() const;

	tpoint calculate_best_size_fix() const;

	/** Inherited from twidget. */
	bool can_wrap() const;

public:
	/** Inherited from twidget. */
	void place(const tpoint& origin, const tpoint& size);

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from twidget. */
	void set_origin(const tpoint& origin);

	/** Inherited from twidget. */
	void set_visible_area(const SDL_Rect& area);

	/** Inherited from twidget. */
	void layout_children();

	/** Inherited from twidget. */
	void child_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack);

	/** Inherited from twidget. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active);

	/** Inherited from twidget. */
	const twidget* find_at(const tpoint& coordinate,
			const bool must_be_active) const;

	/** Inherited from twidget.*/
	twidget* find(const std::string& id, const bool must_be_active);

	/** Inherited from twidget.*/
	const twidget* find(const std::string& id,
			const bool must_be_active) const;

	/** Inherited from twidget.*/
	bool has_widget(const twidget* widget) const;

	/** Inherited from tcontrol. */
	bool disable_click_dismiss() const;

	/** Inherited from twidget. */
	virtual iterator::twalker_* create_walker();

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_rows(const unsigned rows);
	unsigned int get_rows() const { return rows_; }

	void set_cols(const unsigned cols);
	unsigned int get_cols() const { return cols_; }

	/**
	 * Wrapper to set_rows and set_cols.
	 *
	 * @param rows                Parameter to call set_rows with.
	 * @param cols                Parameter to call set_cols with.
	 */
	void set_rows_cols(const unsigned rows, const unsigned cols);

	void place_fix(const tpoint& origin, const tpoint& size);

	void init_report(int unit_w, int unit_y, int gap, bool extendable);
	void insert_child(int unit_w, int unit_h, twidget& widget, size_t at, bool extendable);
	void erase_child(size_t at, bool extendable);
	void replacement_children(int unit_w, int unit_h, int gap, bool extendable);
	void erase_children(int unit_w, int unit_h, int gap, bool extendable);
	void hide_children(int unit_w, int unit_h, int gap, bool extendable);

	void resize_children(size_t size);

	/** Child item of the grid. */
	struct tchild {
		/** The flags for the border and cell setup. */
		unsigned flags_;

		/**
		 * The size of the border, the actual configuration of the border
		 * is determined by the flags.
		 */
		unsigned border_size_;

		/**
		 * Pointer to the widget.
		 *
		 * Once the widget is assigned to the grid we own the widget and are
		 * responsible for it's destruction.
		 */
		twidget* widget_;

	}; // class tchild

	const tchild* children() const { return children_; }
	int children_vsize() const;

public:
	/** Iterator for the tchild items. */
	class iterator
	{

	public:
		iterator(tchild* children, int n)
			: children_(children)
			, n_(n)
			{}

		iterator operator++() { return iterator(children_, ++ n_); }
		iterator operator--() { return iterator(children_, -- n_); }
		twidget* operator->() { return children_[n_].widget_; }
		twidget* operator*() { return children_[n_].widget_; }

		bool operator==(const iterator& i) const
			{ return i.children_ == this->children_ && i.n_ == this->n_; }

		bool operator!=(const iterator& i) const
			{ return i.children_ != this->children_ || i.n_ != this->n_; }

	private:
		tchild* children_;
		int n_;
	};

	iterator begin() { return iterator(children_, 0); }
	iterator end() { return iterator(children_, children_vsize_); }

	void update_last_draw_end();

private:
	/** The number of grid rows. */
	unsigned rows_;

	/** The number of grid columns. */
	unsigned cols_;

	/***** ***** ***** ***** size caching ***** ***** ***** *****/

	/** The row heights in the grid. */
	mutable std::vector<unsigned> row_height_;

	/** The column widths in the grid. */
	mutable std::vector<unsigned> col_width_;

	/** The grow factor for all rows. */
	std::vector<unsigned> row_grow_factor_;

	/** The grow factor for all columns. */
	std::vector<unsigned> col_grow_factor_;

	/**
	 * The child items.
	 *
	 * All children are stored in a 1D vector and the formula to access a cell
	 * is: row * cols_ + col. All other vectors use the same access formula.
	 */
	tchild* children_;
	size_t children_size_;
	size_t children_vsize_;

	std::vector<tspacer*> stuff_widget_;
	size_t stuff_size_;
	tpoint last_draw_end_;

	const tchild& child(const unsigned row, const unsigned col) const
		{ return children_[row * cols_ + col]; }
	tchild& child(const unsigned row, const unsigned col)
		{ return children_[row * cols_ + col]; }

	/** Layouts the children in the grid. */
	void layout(const tpoint& origin);

	/** Inherited from twidget. */
	void impl_draw_children(surface& frame_buffer, int x_offset, int y_offset);

};

/** Returns the best size for the cell. */
tpoint cell_get_best_size(const tgrid::tchild& cell);

/**
 * Places the widget in the cell.
 *
 * @param origin          The origin (x, y) for the widget.
 * @param size            The size for the widget.
 */
void cell_place(tgrid::tchild& cell, tpoint origin, tpoint size);

/** Forwards layout_init() to the cell. */
void cell_layout_init(tgrid::tchild& cell, const bool full_initialization);

/** Returns the space needed for the border. */
tpoint cell_border_space(const tgrid::tchild& cell);

/** Returns the space needed for the border. */
tpoint cell_border_space(const tgrid::tchild& cell);


/**
 * Sets the single child in a grid.
 *
 * The function initializes the grid to 1 x 1 and adds the widget with the grow
 * to client flags.
 *
 * @param grid                    The grid to add the child to.
 * @param widget                  The widget to add as child to the grid.
 */
void set_single_child(tgrid& grid, twidget* widget);

} // namespace gui2

#endif

