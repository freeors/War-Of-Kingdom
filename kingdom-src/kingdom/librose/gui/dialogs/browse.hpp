/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_BROWSE_HPP_INCLUDED
#define GUI_DIALOGS_BROWSE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"
#include <vector>
#include <set>

class display;

namespace gui2 {

class tbutton;
class tlistbox;
class treport;
class ttext_box;

class tbrowse: public tdialog
{
public:
	enum {METHOD_ALPHA, METHOD_DATE};
	enum {TYPE_NONE, TYPE_FILE = 0x1, TYPE_DIR = 0x2};

	struct tentry {
		tentry(const std::string& path, const std::string& label, const std::string& icon, const std::string& id = null_str)
			: path(path)
			, label(label)
			, icon(icon)
			, id(id)
		{}

		bool valid() const { return !path.empty(); }

		std::string path;
		std::string label;
		std::string icon;
		std::string id;
	};

	struct tparam {
		tparam(int flags, bool readonly, const std::string& initial = null_str, 
			const std::string& title = null_str, const std::string& open = null_str)
			: flags(flags)
			, readonly(readonly)
			, initial(initial)
			, title(title)
			, open(open)
			, extra(null_str, null_str, null_str)
		{}

		int flags;
		bool readonly;
		std::string initial;
		std::string title;
		std::string open;
		std::string result;
		tentry extra;
	};

	struct tfile {
		tfile(const std::string& name);

		bool operator<(const tfile& that) const;

		std::string name;
		std::string lower;
		int method;
	};

	static const char path_delim;
	static const std::string path_delim_str;

	explicit tbrowse(display& disp, tparam& param);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

	void init_entry(twindow& window);
	void set_ok_active(twindow& window, tristate dir);

	void item_selected(twindow& window);
	void goto_higher(twindow& window);
	void click_navigate(twindow& window, bool& handled, bool& halt, int index);

	tbutton* create_navigate_button(twindow& window, const std::string& label, int index);
	void reload_navigate(twindow& window, bool first);
	void reload_file_table(twindow& window, int cursel);
	void add_row(twindow& window, tlistbox& list, const std::string& name, bool dir);
	void open(twindow& window, bool& handled, bool& halt, bool dir, int index);
	void update_file_lists(twindow& window);
	std::string get_path(const std::string& file_or_dir) const;
	void goto_entry(twindow& window, int index);

	void text_changed_callback(ttext_box* widget);
	void set_filename(twindow& window);

private:
	display& disp_;
	tparam& param_;

	treport* navigate_;
	tbutton* goto_higher_;
	ttext_box* filename_;
	tbutton* ok_;

	std::vector<tentry> entries_;
	std::string current_dir_;
	std::string chosen_file_;
	std::set<tfile> files_in_current_dir_;
	std::set<tfile> dirs_in_current_dir_;
	std::vector<std::string> cookie_paths_;
	std::string selected_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
