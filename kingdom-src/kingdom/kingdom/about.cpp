/* $Id: about.cpp 47831 2010-12-05 18:09:26Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
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
 * Show screen with scrolling credits.
 */

#include "about.hpp"

#include "display.hpp"
#include "gettext.hpp"
#include "marked-up_text.hpp"
#include "game_config.hpp"
#include "cursor.hpp"

#include <boost/foreach.hpp>

/**
 * @namespace about
 * Display credits %about all contributors.
 *
 * This module is used from the startup screen. \n
 * When show_about() is called, a list of contributors
 * to the game will be presented to the user.
 */
namespace about
{
	static config about_list = config();
	static std::map<std::string , std::string> images;
	static std::string images_default;

/**
 * Given a vector of strings, and a config representing an [about] section,
 * add all the credits lines from the about section to the list of strings.
 */
static void add_lines(std::vector<std::string> &res, config const &c) {
	std::string title = c["title"];
	if (!title.empty()) {
		title = "+" + title;
		res.push_back(title);
	}

	std::vector<std::string> lines = utils::split(c["text"], '\n');
	BOOST_FOREACH (std::string &line, lines)
	{
		if (line.size() > 1 && line[0] == '+')
			line = "+  " + line.substr(1);
		else
			line = "-  " + line;

		if (!line.empty())
		{
			if (line[0] == '_')
				line = _(line.substr(1).c_str());
			res.push_back(line);
		}
	}

	BOOST_FOREACH (const config &entry, c.child_range("entry")) {
		res.push_back("-  "+ entry["name"].str());
	}
}


std::vector<std::string> get_text(const std::string &campaign)
{
	std::vector< std::string > res;

	config::child_itors about_entries = about_list.child_range("about");

	if (!campaign.empty()) {
		BOOST_FOREACH (const config &about, about_entries) {
			// just finished a particular campaign
			if (campaign == about["id"]) {
				add_lines(res, about);
			}
		}
	}

	BOOST_FOREACH (const config &about, about_entries) {
		add_lines(res, about);
	}

	return res;
}

void set_about(const config &cfg)
{
	BOOST_FOREACH (const config &about, cfg.child_range("about"))
	{
		about_list.add_child("about", about);
		const std::string &im = about["images"];
		if (!images.empty())
		{
			if (images_default.empty())
				images_default = im;
			else
				images_default += ',' + im;
		}
	}

	BOOST_FOREACH (const config &campaign, cfg.child_range("campaign"))
	{
		config::const_child_itors abouts = campaign.child_range("about");
		if (abouts.first == abouts.second) continue;

		config temp;
		std::ostringstream text;
		const std::string &id = campaign["id"];
		temp["title"] = campaign["name"];
		temp["id"] = id;
		std::string campaign_images;

		BOOST_FOREACH (const config &about, abouts)
		{
			const std::string &subtitle = about["title"];
			if (!subtitle.empty())
			{
				text << '+';
				if (subtitle[0] == '_')
					text << _(subtitle.substr(1, subtitle.size() - 1).c_str());
				else
					text << subtitle;
				text << '\n';
			}

			BOOST_FOREACH (const std::string &line, utils::split(about["text"], '\n'))
			{
				text << "    " << line << '\n';
			}

			BOOST_FOREACH (const config &entry, about.child_range("entry"))
			{
				text << "    " << entry["name"] << '\n';
			}

			const std::string &im = about["images"];
			if (!im.empty())
			{
				if (campaign_images.empty())
					campaign_images = im;
				else
					campaign_images += ',' + im;
			}
		}

		images[id] = campaign_images;
		temp["text"] = text.str();
		about_list.add_child("about",temp);
	}
}

/**
 * Show credits with list of contributors.
 *
 * Names of people are shown scrolling up like in movie-credits.\n
 * Uses map from wesnoth or campaign as background.
 */
void show_about(display &disp, const std::string &campaign)
{
	return;
}

} // end namespace about
