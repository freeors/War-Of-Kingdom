/* $Id: settings.hpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * This file contains the settings handling of the widget library.
 */

#ifndef GUI_WIDGETS_SETTING_HPP_INCLUDED
#define GUI_WIDGETS_SETTING_HPP_INCLUDED

#include "gui/auxiliary/widget_definition/window.hpp"
#include "gui/auxiliary/tips.hpp"

#include <boost/function.hpp>
#include <boost/foreach.hpp>

#include <string>
#include <vector>

namespace gui2 {


struct tgui_definition;
class ttip;

/** Do we wish to use the new library or not. */
extern bool new_widgets;

/**
 * Registers a window.
 *
 * This function registers the available windows defined in WML. All windows
 * need to register themselves before @ref gui2::init) is called.
 *
 * @warning This function runs before @ref main() so needs to be careful
 * regarding the static initialization problem.
 *
 * @note Double registering a window can't hurt, but no way to probe for it,
 * this can be added if needed. The same for an unregister function.
 *
 * @param id                      The id of the window to register.
 */
void register_window(const std::string& id);

/**
 * Special helper class to get the list of registered windows.
 *
 * This is used in the unit tests, but these implementation details shouldn't
 * be used in the normal code.
 */
class tunit_test_access_only
{
	friend std::vector<std::string>& unit_test_registered_window_list();

	/** Returns a copy of the list of registered windows. */
	static std::vector<std::string> get_registered_window_list();
};

/**
 * Registers a widgets.
 *
 * This function registers the available widgets defined in WML. All widgets
 * need to register themselves before @ref gui2::init) is called.
 *
 * @warning This function runs before @ref main() so needs to be careful
 * regarding the static initialization problem.
 *
 * @param id                      The id of the widget to register.
 * @param functor                 The function to load the definitions.
 */
void register_widget(const std::string& id
		, boost::function<void(
			  tgui_definition& gui_definition
			, const std::string& definition_type
			, const config& cfg
			, const char *key)> functor);

/**
 * Loads the definitions of a widget.
 *
 * @param gui_definition          The gui definition the widget definition
 *                                belongs to.
 * @param definition_type         The type of the widget whose definitions are
 *                                to be loaded.
 * @param definitions             The definitions serialized from a config
 *                                object.
 */
void load_widget_definitions(
	  tgui_definition& gui_definition
	, const std::string& definition_type
	, const std::vector<tcontrol_definition_ptr>& definitions);

/**
 * Loads the definitions of a widget.
 *
 * This function is templated and kept small so only loads the definitions from
 * the config and then lets the real job be done by the @ref
 * load_widget_definitions above.
 *
 * @param gui_definition          The gui definition the widget definition
 *                                belongs to.
 * @param definition_type         The type of the widget whose definitions are
 *                                to be loaded.
 * @param config                  The config to serialiaze the definitions
 *                                from.
 * @param key                     Optional id of the definition to load.
 */
template<class T>
void load_widget_definitions(
	  tgui_definition& gui_definition
	, const std::string& definition_type
	, const config& cfg
	, const char *key)
{
	std::vector<tcontrol_definition_ptr> definitions;

	BOOST_FOREACH(const config& definition, cfg.child_range(key ? key : definition_type + "_definition")) {
		definitions.push_back(new T(definition));
	}

	const std::string dpi_wml = twidget::hdpi? "widget_hdpi": "widget_mdpi";
	if (const config& widget_dpi = cfg.child(dpi_wml)) {
		BOOST_FOREACH(const config& definition, widget_dpi.child_range(key ? key : definition_type + "_definition")) {
			definitions.push_back(new T(definition));
		}
	}

	load_widget_definitions(gui_definition, definition_type, definitions);
}


tresolution_definition_ptr get_control(
	const std::string& control_type, const std::string& definition);

/** Helper struct to signal that get_window_builder failed. */
struct twindow_builder_invalid_id {};

/**
 * Returns an interator to the requested builder.
 *
 * The builder is determined by the @p type and the current screen
 * resolution.
 *
 * @pre                       There is a valid builder for @p type at the
 *                            current resolution.
 *
 * @throw twindow_builder_invalid_id
 *                            When the precondition is violated.
 *
 * @param type                The type of builder window to get.
 *
 * @returns                   An iterator to the requested builder.
 */
std::vector<twindow_builder::tresolution>::const_iterator
	get_window_builder(const std::string& type);

const config& get_theme(const std::string& id);
void reload_window_builder(const std::string& type, const config& cfg, const std::set<std::string>& reserve_wml_tag);
void reload_test_window(const std::string& type, const config& cfg);
bool valid_control_definition(const std::string& type, const std::string& definition);
std::string scale_with_size(const std::string& file, int width, int height);
std::string scale_with_screen_size(const std::string& file);

/** Loads the setting for the theme. */
void load_settings();

struct ttip_definition
{
	void read(const config& _cfg);

	int text_extra_width;
	int text_extra_height;
	int text_font_size;
	int vertical_gap;

	config cfg;
};

/** This namespace contains the 'global' settings. */
namespace settings {

	/**
	 * The screen resolution should be available for all widgets since
	 * their drawing method will depend on it.
	 */
	extern unsigned screen_width;
	extern unsigned screen_height;

	extern unsigned keyboard_height;

	/** These are copied from the active gui. */
	extern unsigned double_click_time;

	extern std::string sound_button_click;
	extern std::string sound_toggle_button_click;
	extern std::string sound_toggle_panel_click;
	extern std::string sound_slider_adjust;

	extern t_string has_helptip_message;

	extern std::map<std::string, tbuilder_widget_ptr> portraits;
	extern std::map<std::string, ttip_definition> tip_cfgs;
	extern std::vector<config> theme_cfgs;

	extern bool actived;
}

struct tgui_definition
{
	tgui_definition()
		: id()
		, description()
		, control_definition()
		, windows()
		, window_types()
		, double_click_time_(0)
		, sound_button_click_()
		, sound_toggle_button_click_()
		, sound_toggle_panel_click_()
		, sound_slider_adjust_()
		, has_helptip_message_()
	{
	}

	std::string id;
	t_string description;

	const std::string& read(const config& cfg);

	/** Activates a gui. */
	void activate() const;

	typedef std::map <std::string /*control type*/,
		std::map<std::string /*id*/, tcontrol_definition_ptr> >
		tcontrol_definition_map;

	tcontrol_definition_map control_definition;

	std::map<std::string, twindow_definition> windows;

	std::map<std::string, twindow_builder> window_types;

	std::map<std::string, config> theme_cfgs;

	void load_widget_definitions(
			  const std::string& definition_type
			, const std::vector<tcontrol_definition_ptr>& definitions);
private:

	unsigned double_click_time_;

	std::string sound_button_click_;
	std::string sound_toggle_button_click_;
	std::string sound_toggle_panel_click_;
	std::string sound_slider_adjust_;

	t_string has_helptip_message_;

	std::map<std::string, tbuilder_widget_ptr> portraits_;
	std::map<std::string, ttip_definition> tip_cfgs_;
};

extern std::map<std::string, tgui_definition>::const_iterator current_gui;

} // namespace gui2

#endif

