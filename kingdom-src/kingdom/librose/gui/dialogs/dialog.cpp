/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/dialog.hpp"

#include "gui/dialogs/field.hpp"
#include "gui/widgets/integer_selector.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/visual_layout.hpp"
#include "video.hpp"
#include "gettext.hpp"
#include "display.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

tdialog::~tdialog()
{
	BOOST_FOREACH (tfield_* field, fields_) {
		delete field;
	}
	if (async_window_) {
		delete async_window_;
	}
}

bool tdialog::show(CVideo& video, const unsigned auto_close_time)
{
	// hide unit tip if necessary.
	if (display::get_singleton()) {
		display::get_singleton()->hide_tip();
	}

	std::vector<twindow*> connected = gui2::connectd_window();
	if (!connected.empty()) {
		connected.back()->remove_tooltip();
	}

	{
		std::auto_ptr<twindow> window(build_window(video));
		VALIDATE(window.get(), null_str);

		try {
			post_build(video, *window);

			window->set_owner(this);

			init_fields(*window);

			pre_show(video, *window);

			window->set_transition(video.getSurface(), SDL_GetTicks());

			retval_ = window->show(restore_, auto_close_time);

			events::discard(DRAW_EVENT, DRAW_EVENT);

			finalize_fields(*window, (retval_ ==  twindow::OK || always_save_fields_));

			post_show(*window);

		} catch (twindow::tlayout_exception& e) {
			if (window->id() != twindow::visual_layout_id) {
				gui2::tvisual_layout dlg(*display::get_singleton(), e.target, e.reason);
				dlg.show(video);
				throw CVideo::quit();

			} else {
				throw twml_exception(null_str, e.reason);
			}
		}
	}
	connected = gui2::connectd_window();
	if (!connected.empty() && connected.back()->is_theme()) {
		display::get_singleton()->invalidate_all();
	}

	return retval_ == twindow::OK;
}

void tdialog::asyn_show(CVideo& video, const SDL_Rect& map_area)
{
	twindow* window = build_window(video);
	// below code mybe exception, destruct can relase, evalue async_window at first.
	async_window_ = window;

	post_build(video, *window);

	window->set_owner(this);
	volatiles_ = window->set_fix_coordinate(map_area);

	pre_show(video, *window);

	// window->set_transition(twindow::last_frame_buffer, SDL_GetTicks());
	retval_ = window->asyn_show();

	window->layout();
}

void tdialog::async_draw()
{
	async_window_->draw();
}

tfield_bool* tdialog::register_bool(
		  const std::string& id
		, const bool mandatory
		, bool (*callback_load_value) ()
		, void (*callback_save_value) (const bool value)
		, void (*callback_change) (twidget* widget))
{
	tfield_bool* field =  new tfield_bool(
			  id
			, mandatory
			, callback_load_value
			, callback_save_value, callback_change);

	fields_.push_back(field);
	return field;
}

tfield_bool* tdialog::register_bool(const std::string& id
		, const bool mandatory
		, bool& linked_variable
		, void (*callback_change) (twidget* widget))
{
	tfield_bool* field =  new tfield_bool(
			  id
			, mandatory
			, linked_variable
			, callback_change);

	fields_.push_back(field);
	return field;
}

tfield_integer* tdialog::register_integer(
		  const std::string& id
		, const bool mandatory
		, int (*callback_load_value) ()
		, void (*callback_save_value) (const int value))
{
	tfield_integer* field =  new tfield_integer(
			  id
			, mandatory
			, callback_load_value
			, callback_save_value);

	fields_.push_back(field);
	return field;
}

tfield_integer* tdialog::register_integer(const std::string& id
		, const bool mandatory
		, int& linked_variable)
{
	tfield_integer* field =  new tfield_integer(
			  id
			, mandatory
			, linked_variable);

	fields_.push_back(field);
	return field;
}

tfield_text* tdialog::register_text(
		  const std::string& id
		, const bool mandatory
		, std::string (*callback_load_value) ()
		, void (*callback_save_value) (const std::string& value)
		, const bool capture_focus)
{
	tfield_text* field =  new tfield_text(
			  id
			, mandatory
			, callback_load_value
			, callback_save_value);

	if(capture_focus) {
		focus_ = id;
	}

	fields_.push_back(field);
	return field;
}

tfield_text* tdialog::register_text(const std::string& id
		, const bool mandatory
		, std::string& linked_variable
		, const bool capture_focus)
{
	tfield_text* field =  new tfield_text(
			  id
			, mandatory
			, linked_variable);

	if(capture_focus) {
		focus_ = id;
	}

	fields_.push_back(field);
	return field;
}

tfield_label* tdialog::register_label(const std::string& id
		, const bool mandatory
		, const std::string& text
		, const bool use_markup)
{
	tfield_label* field =  new tfield_label(
			  id
			, mandatory
			, text
			, use_markup);

	fields_.push_back(field);
	return field;
}

twindow* tdialog::build_window(CVideo& video) const
{
	return build(video, window_id());
}

void tdialog::init_fields(twindow& window)
{
	BOOST_FOREACH (tfield_* field, fields_) {
		field->attach_to_window(window);
		field->widget_init(window);
	}

	if(!focus_.empty()) {
		if(twidget* widget = window.find(focus_, false)) {
			window.keyboard_capture(widget);
		}
	}
}

void tdialog::finalize_fields(twindow& window, const bool save_fields)
{
	BOOST_FOREACH (tfield_* field, fields_) {
		if(save_fields) {
			field->widget_finalize(window);
		}
		field->detach_from_window();
	}
}

void tdialog::toggle_report(twidget* widget)
{
	treport* bar = treport::get_report(widget);
	bar->select(widget);
}

} // namespace gui2


/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 1
 *
 * {{Autogenerated}}
 *
 * = Window definition =
 *
 * The window definition define how the windows shown in the dialog look.
 */

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 */

