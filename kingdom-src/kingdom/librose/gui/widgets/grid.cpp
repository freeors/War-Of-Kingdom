/* $Id: grid.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/grid_private.hpp"

#include "gui/auxiliary/iterator/walker_grid.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/layout_exception.hpp"
#include "gui/widgets/control.hpp"
#include "gui/widgets/spacer.hpp"

#include <boost/foreach.hpp>

#include <numeric>

#define LOG_SCOPE_HEADER "tgrid [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'
#define LOG_IMPL_HEADER "tgrid [" + grid.id() + "] " + __func__ + ':'

#define LOG_CHILD_SCOPE_HEADER "tgrid::tchild [" \
		+ (widget_ ? widget_->id() : "-") + "] " + __func__
#define LOG_CHILD_HEADER LOG_CHILD_SCOPE_HEADER + ':'

namespace gui2 {

tgrid::tgrid(const unsigned rows, const unsigned cols)
	: rows_(rows)
	, cols_(cols)
	, row_height_()
	, col_width_()
	, row_grow_factor_(rows)
	, col_grow_factor_(cols)
	, children_(NULL)
	, children_size_(rows * cols)
	, children_vsize_(rows * cols)
	, stuff_widget_()
	, stuff_size_(0)
	, last_draw_end_(0, 0)
{
	if (children_size_) {
		children_ = (tchild*)malloc(children_size_ * sizeof(tchild));
		memset(children_, 0, children_size_ * sizeof(tchild)); 
	}
}

tgrid::~tgrid()
{
	if (children_) {
		for (int n = 0; n < children_vsize_ - stuff_size_; n ++) {
			delete children_[n].widget_;
			children_[n].widget_ = NULL;
		}
		for (std::vector<tspacer*>::const_iterator it = stuff_widget_.begin(); it != stuff_widget_.end(); ++ it) {
			delete *it;
		}
		free(children_);
	}
}

unsigned tgrid::add_row(const unsigned count)
{
	assert(count);

	//FIXME the warning in set_rows_cols should be killed.

	unsigned result = rows_;
	set_rows_cols(rows_ + count, cols_);
	return result;
}

void tgrid::set_child(twidget* widget, const unsigned row,
		const unsigned col, const unsigned flags, const unsigned border_size)
{
	assert(row < rows_ && col < cols_);
	assert(flags & VERTICAL_MASK);
	assert(flags & HORIZONTAL_MASK);

	tchild& cell = child(row, col);

	// clear old child if any
	if (cell.widget_) {
		// free a child when overwriting it
		WRN_GUI_G << LOG_HEADER
				<< " child '" << cell.widget_->id()
				<< "' at cell '" << row << ',' << col
				<< "' will be replaced.\n";
		delete cell.widget_;
	}

	// copy data
	cell.flags_ = flags;
	cell.border_size_ = border_size;
	cell.widget_ = widget;
	if (cell.widget_) {
		// make sure the new child is valid before deferring
		cell.widget_->set_parent(this);
	}
}

twidget* tgrid::swap_child(
		const std::string& id, twidget* widget, const bool recurse,
		twidget* new_parent)
{
	assert(widget);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];
		if (child.widget_->id() != id) {
			if (recurse) {
				// decent in the nested grids.
				tgrid* grid = dynamic_cast<tgrid*>(child.widget_);
				if (grid) {

					twidget* old = grid->swap_child(id, widget, true);
					if (old) {
						return old;
					}
				}
			}

			continue;
		}

		// When find the widget there should be a widget.
		twidget* old = child.widget_;
		assert(old);
		old->set_parent(new_parent);

		widget->set_parent(this);
		child.widget_ = widget;

		return old;
	}

	return NULL;
}

void tgrid::remove_child(const unsigned row, const unsigned col)
{
	assert(row < rows_ && col < cols_);

	tchild& cell = child(row, col);

	if (cell.widget_) {
		delete cell.widget_;
	}
	cell.widget_ = NULL;
}

void tgrid::remove_child(const std::string& id, const bool find_all)
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		if (child.widget_->id() == id) {
			delete child.widget_;
			child.widget_ = NULL;

			if (!find_all) {
				break;
			}
		}
	}
}

void tgrid::resize_children(int size)
{
	if (size > children_size_) {
		do {
			children_size_ += children_size_? children_size_ * 2: 10;
		} while (size > children_size_);
		tchild* tmp = (tchild*)malloc(children_size_ * sizeof(tchild));
		memset(tmp, 0, children_size_ * sizeof(tchild)); 
		if (children_) {
			if (children_vsize_) {
				memcpy(tmp, children_, children_vsize_ * sizeof(tchild));
			}
			free(children_);
		}
		children_ = tmp;
	}
}

void tgrid::init_report(int unit_w, int unit_h, int gap, bool extendable)
{
	static const std::string report_stuff_id = "_stuff";

	VALIDATE(!children_vsize_, "Must no child in report!");
	VALIDATE(rows_ == 1, "Must one row in report!");

	std::stringstream ss;

	int cols = 1;
	if (extendable) {
		VALIDATE(unit_w, "Multiline only support fixed size!");
		/*
		 * width >= n * unit_w + (n - 1) * gap_w;
		 * width >= n * unit_w + n * gap_w - gap_w;
		 * width >= n * (unit_w + gap_w) - gap_w;
		 * n < width + gap_w / (unit_w + gap_w);
		*/
		cols = (w_ + gap) / (unit_w + gap);
	}

	if (!unit_w && row_height_.empty()) {
		row_height_.push_back(0);
	}
	row_height_[0] = unit_h;
	for (int n = 0; n < cols; n ++) {
		ss.str("");
		ss << report_stuff_id << n;
		tspacer* spacer = create_spacer(ss.str());
		insert_child(unit_w, unit_h, *spacer, -1, false);
		stuff_widget_.push_back(spacer);
	}
	stuff_size_ = cols_;
}

int tgrid::children_vsize() const
{
	return children_vsize_ - stuff_size_;
}

void tgrid::insert_child(int unit_w, int unit_h, twidget& widget, int at, bool extendable)
{
	size_t max_cols = stuff_widget_.size();

	// make sure the new child is valid before deferring
	widget.set_parent(this);
	if (at == npos || at > children_vsize_ - stuff_size_) {
		at = children_vsize_ - stuff_size_;
	}

	// below memmove require children_size_ are large than children_vsize_!
	// resize_children require large than exactly rows_*cols_. because children_vsize_ maybe equal it.
	if (extendable) {
		if (stuff_size_ != 1) {
			memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
			stuff_size_ --;

		} else {
			rows_ ++;
			row_height_.push_back(unit_h);
			// append all stuff widget
			resize_children((rows_ * cols_) + 1);
			for (std::vector<tspacer*>::const_iterator it = stuff_widget_.begin(); it != stuff_widget_.end(); ++ it) {
				children_[children_vsize_ ++].widget_ = *it;
			}
			stuff_size_ = max_cols;
		}
	} else {
		// i think, all grow factor is 0. except last stuff.
		if (cols_ > 0) {
			// last is stuff. it's grow factor is 1.
			col_grow_factor_[cols_ - 1] = 0;
		}
		col_grow_factor_.push_back(0);
		cols_ ++;
		col_width_.push_back(unit_w);
	
		resize_children((rows_ * cols_) + 1);
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	if (!extendable) {
		children_vsize_ ++;
	}
}

void tgrid::erase_child(int at, bool extendable)
{
	if (stuff_widget_.empty()) {
		return;
	}

	if (at == npos || at >= children_vsize_ - stuff_size_) {
		return;
	}
	delete children_[at].widget_;
	children_[at].widget_ = NULL;
	if (at < children_vsize_ - 1) {
		memcpy(&(children_[at]), &(children_[at + 1]), (children_vsize_ - at - 1) * sizeof(tchild));
	}
	children_[children_vsize_ - 1].widget_ = NULL;

	size_t max_cols = stuff_widget_.size();
	if (extendable) {
		if (stuff_size_ != max_cols) {
			children_[children_vsize_ - 1].widget_ = stuff_widget_[stuff_size_];
			stuff_size_ ++;

		} else {
			row_height_.pop_back();
			rows_ --;
			children_vsize_ -= max_cols;
			stuff_size_ = 1;
		}

	} else {
		children_vsize_ --;

		cols_ = children_vsize_;
		col_grow_factor_.resize(cols_);
		col_width_.pop_back();
	}
}

void tgrid::replacement_children(int unit_w, int unit_h, int gap, bool extendable)
{
	if (stuff_widget_.empty()) {
		return;
	}
	
	tpoint origin(x_, y_);
	tpoint size(unit_w, unit_h);

	// for variable-size, should recalculte size. 
	row_height_[0] = unit_h;

	size_t max_cols = stuff_widget_.size();
	size_t n = 0, row = 0, col = 0;
	size_t end;
	if (extendable) {
		end = children_vsize_;
	} else {
		end = children_vsize_ - stuff_size_;
	}
	for (n = 0; n < end; n ++) {
		tchild& child = children_[n];
		twidget* widget2 = child.widget_;
		twidget::tvisible visible = widget2->get_visible();
		if (visible == twidget::INVISIBLE) {
			continue;
		}

		tpoint origin2 = widget2->get_origin();
		if (extendable) {
			if (col) {
				child.flags_ |= tgrid::BORDER_LEFT;
				origin.x += gap;
			}
			if (row && !col) {
				child.flags_ |= tgrid::BORDER_TOP;
				origin.y += gap;
			}
		} else if (col) {
			child.flags_ |= tgrid::BORDER_LEFT;
		}
		if (child.flags_) {
			child.border_size_ = gap;
		}

		child.flags_ |= VERTICAL_ALIGN_CENTER | HORIZONTAL_ALIGN_CENTER;


		if (!unit_w) {
			size = widget2->get_best_size();
			col_width_[col] = size.x;
			if (size.y > static_cast<int>(row_height_[0])) {
				row_height_[0] = size.y;
			}
		}
		if (w_ && visible == twidget::VISIBLE && (origin2 != origin || widget2->get_size() != size)) {
			if (origin2.x != -1 && origin2.y != -1) {
				widget2->twidget::place(origin, size);
			} else {
				widget2->place(origin, size);
			}
		}
		origin.x += size.x;

		col ++;
		if (extendable) {
			if (col == max_cols) {
				origin.x = x_;
				origin.y += unit_h;
				row ++;
				col = 0;
			}
		}
	}

	if (extendable) {
		if (rows_ > 1 && stuff_size_ == max_cols) {
			origin.y -= unit_h + gap;
			size.y = 0;
			for (std::vector<tspacer*>::const_iterator it = stuff_widget_.begin(); it != stuff_widget_.end(); ++ it) {
				twidget* widget = *it;
				widget->set_size(size);
			}
		}

		int h = parent()->get_height();
		if (origin.y - y_ > h) {
			h = origin.y - y_;
		}
		set_fix_rect(::create_rect(x_, y_, w_, h));
		h_ = h;

	} else {
		children_[children_vsize_ - 1].flags_ = VERTICAL_GROW_SEND_TO_CLIENT | HORIZONTAL_GROW_SEND_TO_CLIENT;
		// place stuff. stuff is always visible.
		if (origin.x < last_draw_end_.x) {
			size.x = last_draw_end_.x - origin.x;
		} else {
			size.x = 0;
		}
		tspacer* stuff = stuff_widget_[0];
		col_grow_factor_[col_grow_factor_.size() - 1] = 1;
		if (w_) {
			col_width_[col] = size.x;

			size.y = row_height_[0];
			stuff->set_best_size(size);
		}
		stuff->place(origin, size);
		stuff->set_visible_area(get_rect());
	}
}

void tgrid::erase_children(int unit_w, int unit_h, int gap, bool extendable)
{
	if (stuff_widget_.empty()) {
		return;
	}

	int n = 0;
	for (; n < children_vsize_ - stuff_size_; n ++) {
		delete children_[n].widget_;
		children_[n].widget_ = NULL;
	}
	for (int n2 = 0; n < children_vsize_; n ++, n2 ++) {
		children_[n2] = children_[n];
	}
	children_vsize_ = stuff_size_;

	rows_ = 1;
	cols_ = children_vsize_;
	col_grow_factor_.resize(cols_);
	if (!extendable) {
		col_width_.resize(cols_);
	}
	replacement_children(unit_w, unit_h, gap, extendable);

	set_dirty();
}

void tgrid::hide_children(int unit_w, int unit_h, int gap, bool extendable)
{
	if (stuff_widget_.empty()) {
		return;
	}

	for (int n = 0; n < children_vsize_ - stuff_size_; n ++) {
		children_[n].widget_->set_visible(twidget::INVISIBLE);
	}
	replacement_children(unit_w, unit_h, gap, extendable);
}

void tgrid::update_last_draw_end()
{
	if (stuff_widget_.empty()) {
		return;
	}
	last_draw_end_ = stuff_widget_[stuff_size_ - 1]->get_origin();
}

void tgrid::set_active(const bool active)
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		if (!widget) {
			continue;
		}

		tgrid* grid = dynamic_cast<tgrid*>(widget);
		if (grid) {
			grid->set_active(active);
			continue;
		}

		tcontrol* control =  dynamic_cast<tcontrol*>(widget);
		if(control) {
			control->set_active(active);
		}
	}
}

void tgrid::layout_init(const bool full_initialization)
{
	// Inherited.
	twidget::layout_init(full_initialization);

	// Clear child caches.
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		cell_layout_init(child, full_initialization);

	}
}

void tgrid::reduce_width(const unsigned maximum_width)
{
	/***** ***** ***** ***** INIT ***** ***** ***** *****/
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);
	DBG_GUI_L << LOG_HEADER << " maximum width " << maximum_width << ".\n";

	tpoint size = get_best_size();
	if(size.x <= static_cast<int>(maximum_width)) {
		DBG_GUI_L << LOG_HEADER << " Already fits.\n";
		return;
	}

	/***** ***** ***** ***** Request resize ***** ***** ***** *****/

	request_reduce_width(maximum_width);

	size = get_best_size();
	if(size.x <= static_cast<int>(maximum_width)) {
		DBG_GUI_L << LOG_HEADER << " Resize request honoured.\n";
		return;
	}

	/***** ***** ***** ***** Demand resize ***** ***** ***** *****/

	/** @todo Implement. */

	/***** ***** ***** ***** Acknowlegde failure ***** ***** ***** *****/

	DBG_GUI_L << LOG_HEADER << " Resizing failed.\n";

	throw tlayout_exception_width_resize_failed();
}

void tgrid::request_reduce_width(const unsigned maximum_width)
{
	tpoint size = get_best_size();
	if(size.x <= static_cast<int>(maximum_width)) {
		/** @todo this point shouldn't be reached, find out why it does. */
		return;
	}

	const unsigned too_wide = size.x - maximum_width;
	unsigned reduced = 0;
	for(size_t col = 0; col < cols_; ++col) {
		if(too_wide - reduced >=  col_width_[col]) {
			DBG_GUI_L << LOG_HEADER
					<< " column " << col
					<< " is too small to be reduced.\n";
			continue;
		}

		const unsigned wanted_width = col_width_[col] - (too_wide - reduced);
		const unsigned width = tgrid_implementation::
				column_request_reduce_width(*this, col, wanted_width);

		if(width < col_width_[col]) {
			DBG_GUI_L << LOG_HEADER
					<< " reduced " << col_width_[col] - width
					<< " pixels for column " << col
					<< ".\n";

			size.x -= col_width_[col] - width;
			col_width_[col] = width;
		}

		if(size.x <= static_cast<int>(maximum_width)) {
			break;
		}
	}

	set_layout_size(calculate_best_size());
}

void tgrid::demand_reduce_width(const unsigned /*maximum_width*/)
{
	/** @todo Implement. */
}

void tgrid::reduce_height(const unsigned maximum_height)
{
	/***** ***** ***** ***** INIT ***** ***** ***** *****/
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);
	DBG_GUI_L << LOG_HEADER << " maximum height " << maximum_height << ".\n";

	tpoint size = get_best_size();
	if(size.y <= static_cast<int>(maximum_height)) {
		DBG_GUI_L << LOG_HEADER << " Already fits.\n";
		return;
	}

	/***** ***** ***** ***** Request resize ***** ***** ***** *****/

	request_reduce_height(maximum_height);

	size = get_best_size();
	if(size.y <= static_cast<int>(maximum_height)) {
		DBG_GUI_L << LOG_HEADER << " Resize request honoured.\n";
		return;
	}

	/***** ***** ***** ***** Demand resize ***** ***** ***** *****/

	/** @todo Implement. */

	/***** ***** ***** ***** Acknowlegde failure ***** ***** ***** *****/

	DBG_GUI_L << LOG_HEADER << " Resizing failed.\n";

	throw tlayout_exception_height_resize_failed();
}

void tgrid::request_reduce_height(const unsigned maximum_height)
{
	tpoint size = get_best_size();
	if(size.y <= static_cast<int>(maximum_height)) {
		/** @todo this point shouldn't be reached, find out why it does. */
		return;
	}

	const unsigned too_high = size.y - maximum_height;
	unsigned reduced = 0;
	for(size_t row = 0; row < rows_; ++row) {
		unsigned wanted_height = row_height_[row] - (too_high - reduced);
		/**
		 * @todo Improve this code.
		 *
		 * Now we try every item to be reduced, maybe items need a flag whether
		 * or not to try to reduce and also eveluate whether the force
		 * reduction is still needed.
		 */
		if(too_high - reduced >=  row_height_[row]) {
			DBG_GUI_L << LOG_HEADER
					<< " row " << row
					<< " height " << row_height_[row]
					<< " want to reduce " << too_high
					<< " is too small to be reduced fully try 1 pixel.\n";

			wanted_height = 1;
		}

		const unsigned height = tgrid_implementation::
				row_request_reduce_height(*this, row, wanted_height);

		if(height < row_height_[row]) {
			DBG_GUI_L << LOG_HEADER
					<< " row " << row
					<< " height " << row_height_[row]
					<< " want to reduce " << too_high
					<< " reduced " << row_height_[row] - height
					<< " pixels.\n";

			size.y -= row_height_[row] - height;
			row_height_[row] = height;
		}

		if(size.y <= static_cast<int>(maximum_height)) {
			break;
		}
	}

	size = calculate_best_size();

	DBG_GUI_L << LOG_HEADER
			<< " Requested maximum " << maximum_height
			<< " resulting height " << size.y
			<< ".\n";

	set_layout_size(size);
}

void tgrid::demand_reduce_height(const unsigned /*maximum_height*/)
{
	/** @todo Implement. */
}

tpoint tgrid::calculate_best_size() const
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);

	// Reset the cached values.
	row_height_.clear();
	row_height_.resize(rows_, 0);
	col_width_.clear();
	col_width_.resize(cols_, 0);

	// First get the sizes for all items.
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			const tpoint size = cell_get_best_size(child(row, col));

			if(size.x > static_cast<int>(col_width_[col])) {
				col_width_[col] = size.x;
			}

			if(size.y > static_cast<int>(row_height_[row])) {
				row_height_[row] = size.y;
			}

		}
	}

	for(unsigned row = 0; row < rows_; ++row) {
		DBG_GUI_L << LOG_HEADER
				<< " the row_height_ for row " << row
				<< " will be " << row_height_[row]
				<< ".\n";
	}

	for(unsigned col = 0; col < cols_; ++col) {
		DBG_GUI_L << LOG_HEADER
				<< " the col_width_ for column " << col
				<< " will be " << col_width_[col]
				<< ".\n";
	}

	const tpoint result(
		std::accumulate(col_width_.begin(), col_width_.end(), 0),
		std::accumulate(row_height_.begin(), row_height_.end(), 0));

	DBG_GUI_L << LOG_HEADER << " returning " << result << ".\n";
	return result;
}

tpoint tgrid::calculate_best_size_fix() const
{
	// Reset the cached values.
	row_height_.clear();
	row_height_.resize(rows_, 0);
	col_width_.clear();
	col_width_.resize(cols_, 0);

	assert(rows_ == 1);

	// First get the sizes for all items.
	for (unsigned col = 0; col < cols_; ++col) {

		const tpoint size = cell_get_best_size(child(0, col));

		if (size.x > static_cast<int>(col_width_[col])) {
			col_width_[col] = size.x;
		}

		if (size.y > static_cast<int>(row_height_[0])) {
			row_height_[0] = size.y;
		}
	}

	const tpoint result(
		std::accumulate(col_width_.begin(), col_width_.end(), 0),
		std::accumulate(row_height_.begin(), row_height_.end(), 0));

	return result;
}

bool tgrid::can_wrap() const
{
	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		if (child.widget_ && child.widget_->can_wrap()) {
			return true;
		}
	}

	// Inherited.
	return twidget::can_wrap();
}

void tgrid::place(const tpoint& origin, const tpoint& size)
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);

	/***** INIT *****/

	twidget::place(origin, size);

	if (!rows_ || !cols_) {
		return;
	}

	// call the calculate so the size cache gets updated.
	const tpoint best_size = calculate_best_size();

	assert(row_height_.size() == rows_);
	assert(col_width_.size() == cols_);
	assert(row_grow_factor_.size() == rows_);
	assert(col_grow_factor_.size() == cols_);

	DBG_GUI_L << LOG_HEADER
		<< " best size " << best_size
		<< " available size " << size
		<< ".\n";

	/***** BEST_SIZE *****/

	if(best_size == size) {
		layout(origin);
		return;
	}

	/***** GROW *****/
	if(best_size.x <= size.x && best_size.y <= size.y) {

		// expand it.
		if(size.x > best_size.x) {
			const unsigned w = size.x - best_size.x;
			unsigned w_size = std::accumulate(
					col_grow_factor_.begin(), col_grow_factor_.end(), 0);

			DBG_GUI_L << LOG_HEADER
					<< " extra width " << w
					<< " will be divided amount " << w_size
					<< " units in " << cols_
					<< " columns.\n";

			if(w_size == 0) {
				// If all sizes are 0 reset them to 1
				BOOST_FOREACH(unsigned& val, col_grow_factor_) {
					val = 1;
				}
				w_size = cols_;
			}
			// We might have a bit 'extra' if the division doesn't fix exactly
			// but we ignore that part for now.
			const unsigned w_normal = w / w_size;
			for(unsigned i = 0; i < cols_; ++i) {
				col_width_[i] += w_normal * col_grow_factor_[i];
				DBG_GUI_L << LOG_HEADER
						<< " column " << i
						<< " with grow factor " << col_grow_factor_[i]
						<< " set width to " << col_width_[i]
						<< ".\n";
			}

		}

		if(size.y > best_size.y) {
			const unsigned h = size.y - best_size.y;
			unsigned h_size = std::accumulate(
					row_grow_factor_.begin(), row_grow_factor_.end(), 0);
			DBG_GUI_L << LOG_HEADER
					<< " extra height " << h
					<< " will be divided amount " << h_size
					<< " units in " << rows_
					<< " rows.\n";

			if(h_size == 0) {
				// If all sizes are 0 reset them to 1
				BOOST_FOREACH(unsigned& val, row_grow_factor_) {
					val = 1;
				}
				h_size = rows_;
			}
			// We might have a bit 'extra' if the division doesn't fix exactly
			// but we ignore that part for now.
			const unsigned h_normal = h / h_size;
			for(unsigned i = 0; i < rows_; ++i) {
				row_height_[i] += h_normal * row_grow_factor_[i];
				DBG_GUI_L << LOG_HEADER
						<< " row " << i
						<< " with grow factor " << row_grow_factor_[i]
						<< " set height to " << row_height_[i]
						<< ".\n";
			}
		}

		layout(origin);
		return;
	}

	// This shouldn't be possible...
	std::stringstream str;
	str << "tgrid::place, undesired best_size(" << best_size.x << ", " << best_size.y << ") vs size(" << size.x << ", " << size.y << ").";
	VALIDATE(false, str.str());
}

void tgrid::place_fix(const tpoint& origin, const tpoint& size)
{
	/***** INIT *****/
	twidget::place(origin, size);

	if (!rows_ || !cols_) {
		return;
	}
	assert(rows_ == 1);

	for (unsigned col = 0; col < cols_; ++ col) {
		twidget* widget = child(0, col).widget_;
		const SDL_Rect& fix_rect = widget->fix_rect();
		cell_place(child(0, col), tpoint(fix_rect.x, fix_rect.y), tpoint(fix_rect.w, fix_rect.h));
	}
}

void tgrid::set_origin(const tpoint& origin)
{
	const tpoint movement = tpoint(
			origin.x - get_x(),
			origin.y - get_y());

	// Inherited.
	twidget::set_origin(origin);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		assert(widget);

		widget->set_origin(tpoint(
				widget->get_x() + movement.x,
				widget->get_y() + movement.y));
	}
}

void tgrid::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		assert(widget);

		widget->set_visible_area(area);
	}
}

void tgrid::layout_children()
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];
		assert(child.widget_);
		child.widget_->layout_children();
	}
}

void tgrid::child_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack)
{
	assert(!call_stack.empty() && call_stack.back() == this);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		assert(child.widget_);

		std::vector<twidget*> child_call_stack = call_stack;
		child.widget_->populate_dirty_list(caller, child_call_stack);
	}
}

twidget* tgrid::find_at(const tpoint& coordinate,
		const bool must_be_active)
{
	return tgrid_implementation::find_at<twidget>(
		*this, coordinate, must_be_active);
}

const twidget* tgrid::find_at(const tpoint& coordinate,
		const bool must_be_active) const
{
	return tgrid_implementation::find_at<const twidget>(
		*this, coordinate, must_be_active);
}

twidget* tgrid::find(const std::string& id, const bool must_be_active)
{
	return tgrid_implementation::find<twidget>(
			*this, id, must_be_active);
}

const twidget* tgrid::find(const std::string& id,
		const bool must_be_active) const
{
	return tgrid_implementation::find<const twidget>(
			*this, id, must_be_active);
}

bool tgrid::has_widget(const twidget* widget) const
{
	if(twidget::has_widget(widget)) {
		return true;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		if (child.widget_->has_widget(widget)) {
			return true;
		}
	}
	return false;
}

bool tgrid::disable_click_dismiss() const
{
	if(get_visible() != twidget::VISIBLE) {
		return false;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		const twidget* widget = child.widget_;
		assert(widget);

		if(widget->disable_click_dismiss()) {
			return true;
		}
	}
	return false;
}

iterator::twalker_* tgrid::create_walker()
{
	return new gui2::iterator::tgrid(*this);
}

void tgrid::set_rows(const unsigned rows)
{
	if(rows == rows_) {
		return;
	}

	set_rows_cols(rows, cols_);
}

void tgrid::set_cols(const unsigned cols)
{
	if(cols == cols_) {
		return;
	}

	set_rows_cols(rows_, cols);
}

void tgrid::set_rows_cols(const unsigned rows, const unsigned cols)
{
	if (rows == rows_ && cols == cols_) {
		return;
	}

	if (children_) {
		WRN_GUI_G << LOG_HEADER << " resizing a non-empty grid "
				<< " may give unexpected problems.\n";
	}

	rows_ = rows;
	cols_ = cols;
	row_grow_factor_.resize(rows);
	col_grow_factor_.resize(cols);
	resize_children((rows * cols) + 1);
	children_vsize_ = rows * cols;
}

tpoint cell_get_best_size(const tgrid::tchild& cell)
{
	if (!cell.widget_) {
		return cell_border_space(cell);
	}

	if (cell.widget_->get_visible() == twidget::INVISIBLE) {
		return tpoint(0, 0);
	}

	const tpoint best_size = cell.widget_->get_best_size() + cell_border_space(cell);
	return best_size;
}

void cell_place(tgrid::tchild& cell, tpoint origin, tpoint size)
{
	if (cell.widget_->get_visible() == twidget::INVISIBLE) {
		return;
	}

	if (cell.border_size_) {
		if(cell.flags_ & tgrid::BORDER_TOP) {
			origin.y += cell.border_size_;
			size.y -= cell.border_size_;
		}
		if(cell.flags_ & tgrid::BORDER_BOTTOM) {
			size.y -= cell.border_size_;
		}

		if(cell.flags_ & tgrid::BORDER_LEFT) {
			origin.x += cell.border_size_;
			size.x -= cell.border_size_;
		}
		if(cell.flags_ & tgrid::BORDER_RIGHT) {
			size.x -= cell.border_size_;
		}
	}

	// If size smaller or equal to best size set that size.
	// No need to check > min size since this is what we got.
	const tpoint best_size = cell.widget_->get_best_size();
	if (size <= best_size) {
		cell.widget_->place(origin, size);
		return;
	}

	const tcontrol* control = dynamic_cast<const tcontrol*>(cell.widget_);
	const tpoint maximum_size = control
		? control->get_config_maximum_size()
		: tpoint(0, 0);

	if ((cell.flags_ & (tgrid::HORIZONTAL_MASK | tgrid::VERTICAL_MASK))
			== (tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT | tgrid::VERTICAL_GROW_SEND_TO_CLIENT)) {

		if (maximum_size == tpoint(0,0) || size <= maximum_size) {
			cell.widget_->place(origin, size);
			return;

		}
	}

	tpoint widget_size = tpoint(
		std::min(size.x, best_size.x),
		std::min(size.y, best_size.y));
	tpoint widget_orig = origin;

	const unsigned v_flag = cell.flags_ & tgrid::VERTICAL_MASK;

	if (v_flag == tgrid::VERTICAL_GROW_SEND_TO_CLIENT) {
		if(maximum_size.y) {
			widget_size.y = std::min(size.y, maximum_size.y);
		} else {
			widget_size.y = size.y;
		}

	} else if (v_flag == tgrid::VERTICAL_ALIGN_TOP) {
		// Do nothing.

	} else if (v_flag == tgrid::VERTICAL_ALIGN_CENTER) {

		widget_orig.y += (size.y - widget_size.y) / 2;

	} else if (v_flag == tgrid::VERTICAL_ALIGN_BOTTOM) {

		widget_orig.y += (size.y - widget_size.y);

	} else {
		// err << " Invalid vertical alignment '" << v_flag << "' specified."
		VALIDATE(false, null_str);
	}

	
	const unsigned h_flag = cell.flags_ & tgrid::HORIZONTAL_MASK;

	if (h_flag == tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT) {
		if(maximum_size.x) {
			widget_size.x = std::min(size.x, maximum_size.x);
		} else {
			widget_size.x = size.x;
		}

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_LEFT) {
		// Do nothing.

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_CENTER) {

		widget_orig.x += (size.x - widget_size.x) / 2;

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_RIGHT) {

		widget_orig.x += (size.x - widget_size.x);

	} else {
		// err << " No horizontal alignment '" << h_flag << "' specified.";
		VALIDATE(false, null_str);
	}

	cell.widget_->place(widget_orig, widget_size);
}

void cell_layout_init(tgrid::tchild& cell, const bool full_initialization)
{
	assert(cell.widget_);

	if(cell.widget_->get_visible() != twidget::INVISIBLE) {
		cell.widget_->layout_init(full_initialization);
	}
}

tpoint cell_border_space(const tgrid::tchild& cell)
{
	tpoint result(0, 0);

	if(cell.border_size_) {

		if(cell.flags_ & tgrid::BORDER_TOP) result.y += cell.border_size_;
		if(cell.flags_ & tgrid::BORDER_BOTTOM) result.y += cell.border_size_;

		if(cell.flags_ & tgrid::BORDER_LEFT) result.x += cell.border_size_;
		if(cell.flags_ & tgrid::BORDER_RIGHT) result.x += cell.border_size_;
	}

	return result;
}

void tgrid::layout(const tpoint& origin)
{
	tpoint orig = origin;
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			const tpoint size(col_width_[col], row_height_[row]);
			DBG_GUI_L << LOG_HEADER
					<< " set widget at " << row << ',' << col
					<< " at origin " << orig
					<< " with size " << size
					<< ".\n";

			if (child(row, col).widget_) {
				cell_place(child(row, col), orig, size);
			}

			orig.x += col_width_[col];
		}
		orig.y += row_height_[row];
		orig.x = origin.x;
	}
}

void tgrid::impl_draw_children(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	/*
	 * The call to SDL_PumpEvents seems a bit like black magic.
	 * With the call the resizing doesn't seem to lose resize events.
	 * But when added the queue still only contains one resize event and the
	 * internal SDL queue doesn't seem to overflow (rarely more than 50 pending
	 * events).
	 * Without the call when resizing larger a black area of remains, this is
	 * the area not used for resizing the screen, this call `fixes' that.
	 */
	SDL_PumpEvents();

	assert(get_visible() == twidget::VISIBLE);
	set_dirty(false);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		assert(widget);

		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		if(widget->get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		widget->draw_background(frame_buffer, x_offset, y_offset);
		widget->draw_children(frame_buffer, x_offset, y_offset);
		widget->draw_foreground(frame_buffer, x_offset, y_offset);
		widget->set_dirty(false);
	}
}

unsigned tgrid_implementation::row_request_reduce_height(tgrid& grid,
		const unsigned row, const unsigned maximum_height)
{
	// The minimum height required.
	unsigned required_height = 0;

	for(size_t x = 0; x < grid.cols_; ++x) {
		tgrid::tchild& cell = grid.child(row, x);
		cell_request_reduce_height(cell, maximum_height);

		const tpoint size(cell_get_best_size(cell));

		if(required_height == 0
				|| static_cast<size_t>(size.y) > required_height) {

			required_height = size.y;
		}
	}

	DBG_GUI_L << LOG_IMPL_HEADER
			<< " maximum row height " << maximum_height
			<< " returning " << required_height
			<< ".\n";

	return required_height;
}

unsigned tgrid_implementation::column_request_reduce_width(tgrid& grid,
		const unsigned column, const unsigned maximum_width)
{
	// The minimum width required.
	unsigned required_width = 0;

	for(size_t y = 0; y < grid.rows_; ++y) {
		tgrid::tchild& cell = grid.child(y, column);
		cell_request_reduce_width(cell, maximum_width);
		const tpoint size(cell_get_best_size(cell));

		if(required_width == 0
				|| static_cast<size_t>(size.x) > required_width) {

			required_width = size.x;
		}
	}

	DBG_GUI_L << LOG_IMPL_HEADER
			<< " maximum column width " << maximum_width
			<< " returning " << required_width
			<< ".\n";

	return required_width;
}

void tgrid_implementation::cell_request_reduce_height(
		tgrid::tchild& child, const unsigned maximum_height)
{
	assert(child.widget_);

	if(child.widget_->get_visible() == twidget::INVISIBLE) {
		return;
	}

	child.widget_->request_reduce_height(
			maximum_height - cell_border_space(child).y);
}

void tgrid_implementation::cell_request_reduce_width(
		tgrid::tchild& child, const unsigned maximum_width)
{
	assert(child.widget_);

	if(child.widget_->get_visible() == twidget::INVISIBLE) {
		return;
	}

	child.widget_->request_reduce_width(
			maximum_width - cell_border_space(child).x);
}

void set_single_child(tgrid& grid, twidget* widget)
{
	grid.set_rows_cols(1, 1);
	grid.set_child(widget
			, 0
			, 0
			, tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT
				| tgrid::VERTICAL_GROW_SEND_TO_CLIENT
			, 0);
}

} // namespace gui2


/*WIKI
 * @page = GUILayout
 *
 * {{Autogenerated}}
 *
 * = Abstract =
 *
 * In the widget library the placement and sizes of elements is determined by
 * a grid. Therefore most widgets have no fixed size.
 *
 *
 * = Theory =
 *
 * We have two examples for the addon dialog, the first example the lower
 * buttons are in one grid, that means if the remove button gets wider
 * (due to translations) the connect button (4.1 - 2.2) will be aligned
 * to the left of the remove button. In the second example the connect
 * button will be partial underneath the remove button.
 *
 * A grid exists of x rows and y columns for all rows the number of columns
 * needs to be the same, there is no column (nor row) span. If spanning is
 * required place a nested grid to do so. In the examples every row has 1 column
 * but rows 3, 4 (and in the second 5) have a nested grid to add more elements
 * per row.
 *
 * In the grid every cell needs to have a widget, if no widget is wanted place
 * the special widget ''spacer''. This is a non-visible item which normally
 * shouldn't have a size. It is possible to give a spacer a size as well but
 * that is discussed elsewhere.
 *
 * Every row and column has a ''grow_factor'', since all columns in a grid are
 * aligned only the columns in the first row need to define their grow factor.
 * The grow factor is used to determine with the extra size available in a
 * dialog. The algorithm determines the extra size work like this:
 *
 * * determine the extra size
 * * determine the sum of the grow factors
 * * if this sum is 0 set the grow factor for every item to 1 and sum to sum of items.
 * * divide the extra size with the sum of grow factors
 * * for every item multiply the grow factor with the division value
 *
 * eg
 *  extra size 100
 *  grow factors 1, 1, 2, 1
 *  sum 5
 *  division 100 / 5 = 20
 *  extra sizes 20, 20, 40, 20
 *
 * Since we force the factors to 1 if all zero it's not possible to have non
 * growing cells. This can be solved by adding an extra cell with a spacer and a
 * grow factor of 1. This is used for the buttons in the examples.
 *
 * Every cell has a ''border_size'' and ''border'' the ''border_size'' is the
 * number of pixels in the cell which aren't available for the widget. This is
 * used to make sure the items in different cells aren't put side to side. With
 * ''border'' it can be determined which sides get the border. So a border is
 * either 0 or ''border_size''.
 *
 * If the widget doesn't grow when there's more space available the alignment
 * determines where in the cell the widget is placed.
 *
 * == Examples ==
 *
 *  |---------------------------------------|
 *  | 1.1                                   |
 *  |---------------------------------------|
 *  | 2.1                                   |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 3.1 - 1.1          | 3.1 - 1.2    | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 4.1 - 1.1 | 4.1 - 1.2 | 4.1 - 1.3 | |
 *  | |-----------------------------------| |
 *  | | 4.1 - 2.1 | 4.1 - 2.2 | 4.1 - 2.3 | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *
 *
 *  1.1       label : title
 *  2.1       label : description
 *  3.1 - 1.1 label : server
 *  3.1 - 1.2 text box : server to connect to
 *  4.1 - 1.1 spacer
 *  4.1 - 1.2 spacer
 *  4.1 - 1.3 button : remove addon
 *  4.2 - 2.1 spacer
 *  4.2 - 2.2 button : connect
 *  4.2 - 2.3 button : cancel
 *
 *
 *  |---------------------------------------|
 *  | 1.1                                   |
 *  |---------------------------------------|
 *  | 2.1                                   |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 3.1 - 1.1          | 3.1 - 1.2    | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 4.1 - 1.1         | 4.1 - 1.2     | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 5.1 - 1.1 | 5.1 - 1.2 | 5.1 - 2.3 | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *
 *
 *  1.1       label : title
 *  2.1       label : description
 *  3.1 - 1.1 label : server
 *  3.1 - 1.2 text box : server to connect to
 *  4.1 - 1.1 spacer
 *  4.1 - 1.2 button : remove addon
 *  5.2 - 1.1 spacer
 *  5.2 - 1.2 button : connect
 *  5.2 - 1.3 button : cancel
 *
 * = Praxis =
 *
 * This is the code needed to create the skeleton for the structure the extra
 * flags are ommitted.
 *
 *  	[grid]
 *  		[row]
 *  			[column]
 *  				[label]
 *  					# 1.1
 *  				[/label]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[label]
 *  					# 2.1
 *  				[/label]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[grid]
 *  					[row]
 *  						[column]
 *  							[label]
 *  								# 3.1 - 1.1
 *  							[/label]
 *  						[/column]
 *  						[column]
 *  							[text_box]
 *  								# 3.1 - 1.2
 *  							[/text_box]
 *  						[/column]
 *  					[/row]
 *  				[/grid]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[grid]
 *  					[row]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 1.1
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 1.2
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 1.3
 *  							[/button]
 *  						[/column]
 *  					[/row]
 *  					[row]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 2.1
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 2.2
 *  							[/button]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 2.3
 *  							[/button]
 *  						[/column]
 *  					[/row]
 *  				[/grid]
 *  			[/column]
 *  		[/row]
 *  	[/grid]
 *
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 */
