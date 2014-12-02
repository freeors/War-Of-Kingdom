/* $Id: control.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "control.hpp"

#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/auxiliary/iterator/walker_widget.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/dialogs/tip.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "marked-up_text.hpp"
#include "display.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <iomanip>

#define LOG_SCOPE_HEADER "tcontrol(" + get_control_type() + ") [" \
		+ id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

tcontrol::tcontrol(const unsigned canvas_count)
	: definition_("default")
	, label_()
	, label_size_(std::make_pair(0, tpoint(0, 0)))
	, text_editable_(false)
	, pre_anims_()
	, post_anims_()
	, integrate_(NULL)
	, use_tooltip_on_label_overflow_(true)
	, tooltip_()
	, help_message_()
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, text_alignment_(PANGO_ALIGN_LEFT)
	, shrunken_(false)
{
	connect_signal<event::SHOW_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::SHOW_HELPTIP>(boost::bind(
			  &tcontrol::signal_handler_show_helptip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::tcontrol(
		  const implementation::tbuilder_control& builder
		, const unsigned canvas_count
		, const std::string& control_type)
	: twidget(builder)
	, definition_(builder.definition)
	, label_(builder.label)
	, label_size_(std::make_pair(0, tpoint(0, 0)))
	, text_editable_(false)
	, pre_anims_()
	, post_anims_()
	, integrate_(NULL)
	, use_tooltip_on_label_overflow_(builder.use_tooltip_on_label_overflow)
	, tooltip_(builder.tooltip)
	, help_message_(builder.help)
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, text_alignment_(PANGO_ALIGN_LEFT)
	, shrunken_(false)
{
	definition_load_configuration(control_type);

	connect_signal<event::SHOW_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::SHOW_HELPTIP>(boost::bind(
			  &tcontrol::signal_handler_show_helptip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::~tcontrol()
{
	while (!pre_anims_.empty()) {
		erase_animation(pre_anims_.front());
	}
	while (!post_anims_.empty()) {
		erase_animation(post_anims_.front());
	}
	if (integrate_) {
		delete integrate_;
		integrate_ = NULL;
	}
}

void tcontrol::set_members(const string_map& data)
{
	/** @todo document this feature on the wiki. */
	/** @todo do we need to add the debug colors here as well? */
	string_map::const_iterator itor = data.find("id");
	if(itor != data.end()) {
		set_id(itor->second);
	}

	itor = data.find("linked_group");
	if(itor != data.end()) {
		set_linked_group(itor->second);
	}

	itor = data.find("label");
	if(itor != data.end()) {
		set_label(itor->second);
	}

	itor = data.find("tooltip");
	if(itor != data.end()) {
		set_tooltip(itor->second);
	}

	itor = data.find("help");
	if(itor != data.end()) {
		set_help_message(itor->second);
	}

	itor = data.find("editable");
	if (itor != data.end()) {
		set_text_editable(utils::string_bool(itor->second));
	}
}

bool tcontrol::disable_click_dismiss() const
{
	return get_visible() == twidget::VISIBLE && get_active();
}

iterator::twalker_* tcontrol::create_walker()
{
	return new iterator::walker::twidget(*this);
}

tpoint tcontrol::get_config_minimum_size() const
{
	assert(config_);

	tpoint result(config_->min_width, config_->min_height);

	DBG_GUI_L << LOG_HEADER << " result " << result << ".\n";
	return result;
}

tpoint tcontrol::get_config_default_size() const
{
	assert(config_);

	tpoint result(config_->default_width, config_->default_height);

	DBG_GUI_L << LOG_HEADER << " result " << result << ".\n";
	return result;
}

tpoint tcontrol::get_config_maximum_size() const
{
	assert(config_);

	tpoint result(config_->max_width, config_->max_height);
	if (!result.x) {
		result.x = settings::screen_width;
	}
	if (!result.y) {
		result.y = settings::screen_height;
	}

	DBG_GUI_L << LOG_HEADER << " result " << result << ".\n";
	return result;
}

unsigned tcontrol::get_characters_per_line() const
{
	return 0;
}

void tcontrol::layout_init(const bool full_initialization)
{
	// Inherited.
	twidget::layout_init(full_initialization);

	if(full_initialization) {
		shrunken_ = false;
	}
}

void tcontrol::request_reduce_width(const unsigned maximum_width)
{
	assert(config_);

	if(!label_.empty() && can_wrap()) {

		tpoint size = get_best_text_size(
				tpoint(0,0),
				tpoint(maximum_width - config_->text_extra_width, 0));

		size.x += config_->text_extra_width;
		size.y += config_->text_extra_height;

		set_layout_size(size);

		DBG_GUI_L << LOG_HEADER
				<< " label '" << debug_truncate(label_)
				<< "' maximum_width " << maximum_width
				<< " result " << size
				<< ".\n";

	} else {
		DBG_GUI_L << LOG_HEADER
				<< " label '" << debug_truncate(label_)
				<< "' failed; either no label or wrapping not allowed.\n";
	}
}

tpoint tcontrol::calculate_best_size() const
{
	assert(config_);
	if(label_.empty()) {
		DBG_GUI_L << LOG_HEADER << " empty label return default.\n";
		return get_config_default_size();
	}

	const tpoint minimum = get_config_default_size();
	tpoint maximum = get_config_maximum_size();
	if (text_maximum_width_ && maximum.x > text_maximum_width_) {
		maximum.x = text_maximum_width_;
	}

	/**
	 * @todo The value send should subtract the border size
	 * and read it after calculation to get the proper result.
	 */
	tpoint result = get_best_text_size(minimum, maximum);
	DBG_GUI_L << LOG_HEADER
			<< " label '" << debug_truncate(label_)
			<< "' result " << result
			<< ".\n";
	return result;
}

void tcontrol::calculate_integrate()
{
	if (!text_editable_) {
		return;
	}
	if (integrate_) {
		delete integrate_;
	}
	if (get_text_maximum_width() > 0) {
		// before place, not ready.
		integrate_ = new tintegrate(label_, get_text_maximum_width(), -1, config()->text_font_size, font::NORMAL_COLOR, text_editable_);
		if (!locator_.empty()) {
			integrate_->fill_locator_rect(locator_, true);
		}
	}
}

void tcontrol::refresh_locator_anim(std::vector<tintegrate::tlocator>& locator)
{
	if (!text_editable_) {
		return;
	}
	locator_.clear();
	if (integrate_) {
		integrate_->fill_locator_rect(locator, true);
	} else {
		locator_ = locator;
	}
}

void tcontrol::place(const tpoint& origin, const tpoint& size)
{
	// resize canvasses
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_width(size.x);
		canvas.set_height(size.y);
	}

	// inherited
	twidget::place(origin, size);

	calculate_integrate();

	// update the state of the canvas after the sizes have been set.
	update_canvas();
}

void tcontrol::load_config()
{
	if(!config()) {

		definition_load_configuration(get_control_type());

		load_config_extra();
	}
}

void tcontrol::set_definition(const std::string& definition)
{
	assert(!config());
	definition_ = definition;
	load_config();
	assert(config());

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	init();
#endif
}

void tcontrol::set_label(const std::string& label)
{
	if (label == label_) {
		return;
	}

	label_ = label;
	label_size_.second.x = 0;
	set_layout_size(tpoint(0, 0));
	update_canvas();
	set_dirty();

	calculate_integrate();
}

void tcontrol::set_text_editable(bool editable)
{
	if (editable == text_editable_) {
		return;
	}

	text_editable_ = editable;
	update_canvas();
	set_dirty();
}

void tcontrol::set_text_alignment(const PangoAlignment text_alignment)
{
	if(text_alignment_ == text_alignment) {
		return;
	}

	text_alignment_ = text_alignment;
	update_canvas();
	set_dirty();
}

void tcontrol::update_canvas()
{
	const int max_width = get_text_maximum_width();
	const int max_height = get_text_maximum_height();

	// set label in canvases
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable("text", variant(label_));
		canvas.set_variable("text_alignment", variant(encode_text_alignment(text_alignment_)));
		canvas.set_variable("text_maximum_width", variant(max_width));
		canvas.set_variable("text_maximum_height", variant(max_height));
		canvas.set_variable("text_wrap_mode", variant(can_wrap()? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END));
		canvas.set_variable("text_characters_per_line", variant(get_characters_per_line()));
	}
}

int tcontrol::get_text_maximum_width() const
{
	assert(config_);

	return get_width() - config_->text_extra_width;
}

int tcontrol::get_text_maximum_height() const
{
	assert(config_);
	return get_height() - config_->text_extra_height;
}

void tcontrol::set_text_maximum_width(int maximum)
{
	text_maximum_width_ = maximum;
}

bool tcontrol::exist_anim() const
{
	if (!pre_anims_.empty() || !post_anims_.empty()) {
		return true;
	}
	if (integrate_ && integrate_->exist_anim()) {
		return true;
	}
	return !canvas_.empty() && canvas_[get_state()].exist_anim();
}

int tcontrol::insert_animation(const ::config& cfg, bool pre)
{
	int id = start_cycle_float_anim(*display::get_singleton(), cfg);
	if (id != INVALID_ANIM_ID) {
		std::vector<int>& anims = pre? pre_anims_: post_anims_;
		anims.push_back(id);
	}
	return id;
}

void tcontrol::erase_animation(int id)
{
	bool found = false;
	std::vector<int>::iterator find = std::find(pre_anims_.begin(), pre_anims_.end(), id);
	if (find != pre_anims_.end()) {
		pre_anims_.erase(find);
		found = true;

	} else {
		find = std::find(post_anims_.begin(), post_anims_.end(), id);
		if (find != post_anims_.end()) {
			post_anims_.erase(find);
			found = true;
		}
	}
	if (found) {
		display& disp = *display::get_singleton();
		disp.erase_area_anim(id);
		set_dirty();
	}
}

class tshare_canvas_integrate_lock
{
public:
	tshare_canvas_integrate_lock(tintegrate* integrate)
		: integrate_(share_canvas_integrate)
	{
		share_canvas_integrate = integrate;
	}
	~tshare_canvas_integrate_lock()
	{
		share_canvas_integrate = integrate_;
	}

private:
	tintegrate* integrate_;
};

void tcontrol::impl_draw_background(surface& frame_buffer)
{
	DBG_GUI_D << LOG_HEADER
			<< " label '" << debug_truncate(label_)
			<< "' size " << get_rect()
			<< ".\n";

	tshare_canvas_integrate_lock lock(integrate_);
	canvas(get_state()).blit(frame_buffer, get_rect(), get_dirty(), pre_anims_, post_anims_);
}

void tcontrol::impl_draw_background(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	DBG_GUI_D << LOG_HEADER
			<< " label '" << debug_truncate(label_)
			<< "' size " << get_rect()
			<< ".\n";

	tshare_canvas_integrate_lock lock(integrate_);
	canvas(get_state()).blit(frame_buffer, calculate_blitting_rectangle(x_offset, y_offset), get_dirty(), pre_anims_, post_anims_);
}

void tcontrol::definition_load_configuration(const std::string& control_type)
{
	assert(!config());

	set_config(get_control(control_type, definition_));

	assert(canvas().size() == config()->state.size());
	for (size_t i = 0; i < canvas().size(); ++i) {
		canvas(i) = config()->state[i].canvas;
		canvas(i).start_animation();
	}

	update_canvas();
}

tpoint tcontrol::get_best_text_size(
		  const tpoint& minimum_size
		, const tpoint& maximum_size) const
{
	log_scope2(log_gui_layout, LOG_SCOPE_HEADER);

	assert(!label_.empty());

	const tpoint border(config_->text_extra_width, config_->text_extra_height);
	tpoint size = minimum_size - border;

	if (!label_size_.second.x || (maximum_size.x != label_size_.first)) {
		// Try with the minimum wanted size.
		label_size_.first = maximum_size.x;
		label_size_.second = font::get_rendered_text_size(label_, maximum_size.x, config_->text_font_size, font::NORMAL_COLOR, text_editable_);
	}

	size = label_size_.second + border;

	if (size.x < minimum_size.x) {
		size.x = minimum_size.x;
	}

	if (size.y < minimum_size.y) {
		size.y = minimum_size.y;
	}

	DBG_GUI_L << LOG_HEADER
			<< " label '" << debug_truncate(label_)
			<< "' result " << size
			<< ".\n";
	return size;
}

void tcontrol::signal_handler_show_tooltip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	if(!tooltip_.empty()) {
		std::string tip = tooltip_;
		if(!help_message_.empty()) {
			utils::string_map symbols;
			symbols["hotkey"] =
					hotkey::get_hotkey(hotkey::GLOBAL__HELPTIP).get_name();

			tip = tooltip_ + utils::interpolate_variables_into_string(
					  settings::has_helptip_message
					, &symbols);
		}

		event::tmessage_show_tooltip message(tip, location);
		handled = fire(event::MESSAGE_SHOW_TOOLTIP, *this, message);
	}
}

void tcontrol::signal_handler_show_helptip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	if(!help_message_.empty()) {
		event::tmessage_show_helptip message(help_message_, location);
		handled = fire(event::MESSAGE_SHOW_HELPTIP, *this, message);
	}
}

void tcontrol::signal_handler_notify_remove_tooltip(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	/*
	 * This makes the class know the tip code rather intimately. An
	 * alternative is to add a message to the window to remove the tip.
	 * Might be done later.
	 */
	tip::remove();

	handled = true;
}

} // namespace gui2

