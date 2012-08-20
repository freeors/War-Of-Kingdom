/* $Id: message.hpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#ifndef GUI_DIALOGS_DUEL_HPP_INCLUDED
#define GUI_DIALOGS_DUEL_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"

class hero;

namespace gui2 {

class tbutton;
class ttoggle_button;

#define TOTAL_HP	160
#define MAX_TURNS	5
#define MAX_CARDS	5
#define MIN_CARDS	3
#define LARGER_MIN_POINT 7
#define LARGER_MAX_POINT 9
#define SMALLER_MIN_POINT 4
#define SMALLER_MAX_POINT 6

typedef struct {
	int force_;
	int skill_;
	// count of card.
	int count_;
	// card type/point in this turn.
	std::multimap<int, int> cards_;
	// whether or no deadfight in this turn.
	bool deadfight_;
	// whether or no fightback in this turn.
	bool fightback_;

	int min_point_;
	int max_point_;
} setting;

class analysis
{
public:
	analysis(int type = -1, bool deadfight = false, bool fightback = false, int total_point = 0);

	int type_;
	bool deadfight_;
	bool fightback_;
	int total_point_;
	int score_;

	size_t equals_;
	size_t mores_;
	size_t lesses_;
};

/**
 * Main class to show messages to the user.
 *
 * It can be used to show a message or ask a result from the user. For the
 * most common usage cases there are helper functions defined.
 */
class tduel : public tdialog
{
public:
	static int min_hp_;

	tduel(hero& left, hero& right);

	int hp_percentage() const { return hp_ * 100 / TOTAL_HP; }
protected:
	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window);

	/** Inherited from tdialog. */
	void post_show(twindow& window);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void reset_turn(twindow& window);
	void do_duel(twindow& window);
	int duel(const analysis& l, const analysis& r); 

	void on_card(twindow& window, int index);
	void on_deadfight(twindow& window);
	void on_fightback(twindow& window);
	void end_turn(twindow& window);

	void finish_duel(twindow& window);

	hero& left_;
	hero& right_;
	setting setting_[2];
	int turn_;
	tbutton* endturn_;
	ttoggle_button* lskill0_;
	ttoggle_button* lskill1_;
	int hp_;
	bool finished_;
};

} // namespace gui2

#endif

