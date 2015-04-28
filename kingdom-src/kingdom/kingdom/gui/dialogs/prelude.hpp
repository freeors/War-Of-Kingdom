/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#ifndef GUI_DIALOGS_PRELUDE_HPP_INCLUDED
#define GUI_DIALOGS_PRELUDE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"

class display;
class hero_map;

namespace gui2 {

class tbutton;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tprelude : public tdialog
{
public:
	enum ttype {NONE, MESSAGE, ANIM};
	struct tcommand
	{
		tcommand(ttype t, const config& cfg)
			: type(t)
			, cfg(cfg)
		{}

		ttype type;
		config cfg;
	};
	static std::map<const std::string, ttype> tags;
	static void fill_tags();
	static ttype find(const std::string& tag);

	tprelude(display& disp, hero_map& heros, const config& game_config, const config& part);

	~tprelude();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	virtual void post_build(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/**
	 * Updates the tip of day widget.
	 *
	 * @param window              The window being shown.
	 * @param previous            Show the previous tip, else shows the next
	 *                            one.
	 */
	void previous(twindow& window);
	void next(twindow& window);

	void form_commands(const config& cfg);
	void execute(twindow& window);
	void timer_handler();

private:
	display& disp_;
	hero_map& heros_;
	const config& game_config_;

	const config& cfg_;
	tbutton* next_;
	std::vector<tcommand> commands_;
	size_t current_;
	std::map<int, const config*> anims_;
	unsigned long ing_timer_;
	int active_anim_;
};

} // namespace gui2

#endif
