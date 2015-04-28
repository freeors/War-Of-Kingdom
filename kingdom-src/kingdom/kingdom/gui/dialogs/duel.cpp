/* $Id: message.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/duel.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "resources.hpp"
#include "play_controller.hpp"

#include "sound.hpp"
#include "game_config.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(duel)

int tduel::min_hp_ = 50;

tduel::tduel(hero& left, hero& right)
	: left_(left)
	, right_(right)
	, turn_(0)
	, endturn_(NULL)
	, lskill0_(NULL)
	, lskill1_(NULL)
	, hp_(TOTAL_HP / 2)
	, finished_(false)
{
	int larger_force, force_distance;

	setting_[0].force_ = fxptoi9(left.force_);
	setting_[1].force_ = fxptoi9(right.force_);
	
	if (setting_[0].force_ >= setting_[1].force_) {
		larger_force = 0;
		force_distance = setting_[0].force_ - setting_[1].force_;
	} else {
		larger_force = 1;
		force_distance = setting_[1].force_ - setting_[0].force_;
	}

	setting_[larger_force].count_ = MAX_CARDS;
	setting_[larger_force].min_point_ = LARGER_MIN_POINT;
	setting_[larger_force].max_point_ = LARGER_MAX_POINT;

	setting* t = setting_ + (larger_force ^ 1);
	if (force_distance == 0) {
		t->count_ = MAX_CARDS;
		t->min_point_ = LARGER_MIN_POINT;
		t->max_point_ = LARGER_MAX_POINT;
	} else {
		t->count_ = std::max(MIN_CARDS, MAX_CARDS - force_distance / 5);
		t->min_point_ = std::max(SMALLER_MIN_POINT, LARGER_MIN_POINT - (force_distance + 4) / 5);
		t->max_point_ = std::max(SMALLER_MAX_POINT, LARGER_MAX_POINT - (force_distance + 4) / 5);
	}

	for (int index = 0; index < 2; index ++) {
		if (index == 0) {
			setting_[index].skill_ = fxptoi12(left.skill_[hero_skill_hero]);
		} else {
			setting_[index].skill_ = fxptoi12(right.skill_[hero_skill_hero]);
		}
		if (!setting_[index].skill_) {
			setting_[index].skill_ = 1;
		}
		setting_[index].deadfight_ = false;
		setting_[index].fightback_ = false;
	}
}

static int nb_items = 3;
static const char* menu_items[] = {
	"misc/stone.png",
	"misc/forfex.png",
	"misc/cloth.png"
};

void tduel::reset_turn(twindow& window)
{
	std::stringstream str;

	str.str("");
	str << "misc/digit-big.png~CROP(" << 30 * (turn_ + 1) << ", 0, 30, 45)";
	find_widget<tcontrol>(&window, "turn", false, false)->set_label(str.str());

	// set up card
	for (int index = 0; index < 2; index ++) {
		setting* t = setting_ + index;
		t->cards_.clear();
		if (!t->deadfight_) {
			t->deadfight_ = (rand() % 16) < t->skill_;
		}
		if (!t->fightback_) {
			t->fightback_ = (rand() % 64) < t->skill_;
		}

		char c = index? 'r': 'l';
		for (int index2 = 4; index2 >= 0; index2 --) {
			str.str("");
			str << c << "card" << index2;
			ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
			if (t->count_ <= index2) {
				toggle->set_visible(twidget::INVISIBLE);
				continue;
			}
			if (index == 1) {
				toggle->set_active(false);
			}

			int type = rand() % nb_items;
			int point = t->min_point_ + rand() % (t->max_point_ - t->min_point_ + 1);
			t->cards_.insert(std::pair<int, int>(type, point));
		}
		int index2 = 0;
		for (std::multimap<int, int>::const_iterator c_itor = t->cards_.begin(); c_itor != t->cards_.end(); ++ c_itor, index2 ++) {
			str.str("");
			str << c << "card" << index2;
			ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
			if (index == 0) {
				connect_signal_mouse_left_click(
					 *toggle
					, boost::bind(
					  &tduel::on_card
					, this
					, boost::ref(window)
					, index2));
			}
			for (int i = 0; i < 6; i ++) {
				toggle->canvas()[i].set_variable("foreground", variant(menu_items[c_itor->first]));
			}
			str.str("");
			str << c_itor->second;
			toggle->set_label(str.str());
			// reset lcard/rcard.
			toggle->set_value(false);
			toggle->set_dirty();
		}

		// total card
		str.str("");
		str << c << "total";
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		for (int i = 0; i < 6; i ++) {
			toggle->canvas()[i].set_variable("foreground", variant(""));
		}
		toggle->set_label("0");
		toggle->set_active(false);

		// skill
		str.str("");
		str << c << "skill";
		find_widget<tcontrol>(&window, str.str(), false, false)->set_label("");

		// deadfight
		str.str("");
		str << c << "skill0";
		toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_value(false);
		for (int i = 0; i < 6; i ++) {
			if (t->deadfight_) {
				toggle->canvas()[i].set_variable("image", variant("misc/deadfight.png"));
			} else {
				toggle->canvas()[i].set_variable("image", variant(""));
			}
		}
		if (index == 0) {
			toggle->set_active(t->deadfight_);
		} else {
			toggle->set_active(false);
		}

		// fightback
		str.str("");
		str << c << "skill1";
		toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_value(false);
		for (int i = 0; i < 6; i ++) {
			if (t->fightback_) {
				toggle->canvas()[i].set_variable("image", variant("misc/fightback.png"));
			} else {
				toggle->canvas()[i].set_variable("image", variant(""));
			}
		}
		if (index == 0) {
			toggle->set_active(t->fightback_);
		} else {
			toggle->set_active(false);
		}
	}
	endturn_->set_active(false);
}

void tduel::pre_show(CVideo& /*video*/, twindow& window)
{
	// Override the user value, to make sure it's set properly.
	window.set_click_dismiss(false);

	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	window.canvas(1).set_variable("left_image", variant(left_.image(true)));
	window.canvas(1).set_variable("right_image", variant(right_.image(true)));
	window.canvas(1).set_variable("center_image", variant(""));
	
	window.canvas(1).set_variable("hp_left", variant("misc/hp-blue.png"));
	window.canvas(1).set_variable("hp_right", variant("misc/hp-red.png"));
	window.canvas(1).set_variable("percentage", variant((hp_ * 100) / TOTAL_HP));

	std::stringstream str;

	tbutton* b = find_widget<tbutton>(&window, "endturn", false, false);
	for (int i = 0; i < 4; i ++) {
		b->canvas()[i].set_variable("image", variant("misc/ok.png"));
	}

	find_widget<tcontrol>(&window, "lname", false, false)->set_label(left_.name());
	find_widget<tcontrol>(&window, "rname", false, false)->set_label(right_.name());

	tcontrol* control = find_widget<tcontrol>(&window, "lforce", false, false);
	str.str("");
	str << setting_[0].force_ << "(" << hero::adaptability_str2(left_.skill_[hero_skill_hero]).c_str() << ")";
	control->set_label(str.str());

	control = find_widget<tcontrol>(&window, "rforce", false, false);
	str.str("");
	str << setting_[1].force_ << "(" << hero::adaptability_str2(right_.skill_[hero_skill_hero]).c_str() << ")";;
	control->set_label(str.str());

	endturn_ = find_widget<tbutton>(&window, "endturn", false, false);
	connect_signal_mouse_left_click(
		*endturn_
		, boost::bind(
			&tduel::end_turn
			, this
			, boost::ref(window)));
	endturn_->set_sound_button_click("hatchet.wav");
	
	lskill0_ = find_widget<ttoggle_button>(&window, "lskill0", false, false);
	connect_signal_mouse_left_click(
		 *lskill0_
		, boost::bind(
		  &tduel::on_deadfight
		, this
		, boost::ref(window)));

	lskill1_ = find_widget<ttoggle_button>(&window, "lskill1", false, false);
	connect_signal_mouse_left_click(
		 *lskill1_
		, boost::bind(
		  &tduel::on_fightback
		, this
		, boost::ref(window)));
	
	find_widget<ttoggle_button>(&window, "lturn0", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn0_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<tlabel>(&window, "turn0", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn0", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn0_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn1", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn1_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<tlabel>(&window, "turn1", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn1", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn1_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn2", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn2_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<tlabel>(&window, "turn2", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn2", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn2_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn3", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn3_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<tlabel>(&window, "turn3", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn3", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn3_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn4", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "lturn4_skill", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<tlabel>(&window, "turn4", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn4", false, false)->set_visible(twidget::INVISIBLE);
	find_widget<ttoggle_button>(&window, "rturn4_skill", false, false)->set_visible(twidget::INVISIBLE);

	reset_turn(window);
}

void tduel::post_show(twindow& /*window*/)
{
}

void tduel::on_card(twindow& window, int index)
{
	std::stringstream str;

	int index2 = 0;
	std::multimap<int, int>::const_iterator current_itor;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[0].cards_.begin(); c_itor != setting_[0].cards_.end(); ++ c_itor, index2 ++) {
		if (index2 == index) {
			current_itor = c_itor;
			break;
		}
	}

	// disable all non-type card
	int total_point = 0;
	index2 = 0;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[0].cards_.begin(); c_itor != setting_[0].cards_.end(); ++ c_itor, index2 ++) {
		str.str("");
		str << "lcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		if (c_itor->first != current_itor->first) {
			toggle->set_value(false);
			continue;
		}
		if (toggle->get_value()) {
			total_point += c_itor->second;
		}
	}

	// update total point to ltotal/rtotal.
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "ltotal", false, false);

	for (int i = 0; i < 6; i ++) {
		if (total_point) {
			toggle->canvas()[i].set_variable("foreground", variant(menu_items[current_itor->first]));
		} else {
			toggle->canvas()[i].set_variable("foreground", variant(""));
		}
	}
	str.str("");
	if (lskill1_->get_value()) {
		str << "0";
	} else {
		str << total_point * (lskill0_->get_value()? 2: 1);
	}
	toggle->set_label(str.str());
	toggle->set_dirty();

	endturn_->set_active(total_point || toggle->get_value());
}

void tduel::on_deadfight(twindow& window)
{
	std::stringstream str;

	int total_point = 0;
	int index2 = 0;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[0].cards_.begin(); c_itor != setting_[0].cards_.end(); ++ c_itor, index2 ++) {
		str.str("");
		str << "lcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);

		str.str("");
		str << c_itor->second * (lskill0_->get_value()? 2: 1);
		toggle->set_label(str.str());

		if (toggle->get_value()) {
			total_point += c_itor->second;
		}
	}
	if (setting_[0].fightback_) {
		lskill1_->set_active(!lskill0_->get_value());
	}

	// update skill to rskill
	if (lskill0_->get_value()) {
		find_widget<tcontrol>(&window, "lskill", false, false)->set_label("misc/deadfight.png");
	} else {
		find_widget<tcontrol>(&window, "lskill", false, false)->set_label("");
	}

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "ltotal", false, false);
	str.str("");
	str << total_point * (lskill0_->get_value()? 2: 1);
	toggle->set_label(str.str());
}

void tduel::on_fightback(twindow& window)
{
	std::stringstream str;

	int total_point = 0;
	int index2 = 0;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[0].cards_.begin(); c_itor != setting_[0].cards_.end(); ++ c_itor, index2 ++) {
		str.str("");
		str << "lcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);

		str.str("");
		str << c_itor->second * (lskill1_->get_value()? 0: 1);
		toggle->set_label(str.str());

		if (toggle->get_value()) {
			total_point += c_itor->second;
		}
	}
	if (setting_[0].deadfight_) {
		lskill0_->set_active(!lskill1_->get_value());
	}

	// update skill to rskill
	if (lskill1_->get_value()) {
		find_widget<tcontrol>(&window, "lskill", false, false)->set_label("misc/fightback.png");
	} else {
		find_widget<tcontrol>(&window, "lskill", false, false)->set_label("");
	}

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "ltotal", false, false);
	str.str("");
	str << (lskill1_->get_value()? 0: total_point);
	toggle->set_label(str.str());
}

void tduel::finish_duel(twindow& window)
{
	std::stringstream str;

	// disable all button/toggle_button
	for (int index2 = 0; index2 < setting_[0].count_; index2 ++) {
		str.str("");
		str << "lcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_value(false);
		toggle->set_active(false);
		for (int i = 0; i < 6; i ++) {
			toggle->canvas()[i].set_variable("foreground", variant(""));
		}
		toggle->set_label("");
	}
	lskill0_->set_active(false);
	lskill0_->set_value(false);
	for (int i = 0; i < 6; i ++) {
		lskill0_->canvas()[i].set_variable("image", variant(""));
	}
	lskill1_->set_active(false);
	lskill1_->set_value(false);
	for (int i = 0; i < 6; i ++) {
		lskill1_->canvas()[i].set_variable("image", variant(""));
	}

	if (hp_ < TOTAL_HP / 2) {
		window.canvas(1).set_variable("center_image", variant(right_.image(true)));
		if (hp_ == 0) {
			window.canvas(1).set_variable("left_image", variant(std::string(left_.image(true)) + "~GS()"));
		}
		window.canvas(1).set_variable("right_image", variant(""));

		sound::play_music_once("defeat.ogg");

	} else if (hp_ > TOTAL_HP / 2) {
		window.canvas(1).set_variable("center_image", variant(left_.image(true)));
		window.canvas(1).set_variable("left_image", variant(""));
		if (hp_ == TOTAL_HP) {
			window.canvas(1).set_variable("right_image", variant(std::string(right_.image(true)) + "~GS()"));
		}
		sound::play_music_once("victory.ogg");
	}

	endturn_->set_sound_button_click("");
	
	finished_ = true;
}

void tduel::end_turn(twindow& window)
{
	if (finished_) {
		// resume [scenario] music list
		sound::empty_playlist();
		const config& level = resources::controller->level();
		BOOST_FOREACH (const config &m, level.child_range("music")) {
			sound::play_music_config(m);
		}
		sound::commit_music_changes();

		window.set_retval(twindow::OK);

	} else if (turn_ < MAX_TURNS - 1) {
		find_widget<tcontrol>(&window, "duel_header", false, false)->set_dirty();
		do_duel(window);
		if (hp_ > 0 && hp_ < TOTAL_HP) {
			turn_ ++;
			reset_turn(window);
		} else {
			finish_duel(window);
		}

	} else if (turn_ == MAX_TURNS - 1) {
		do_duel(window);

		finish_duel(window);
	}
}

analysis::analysis(int type, bool deadfight, bool fightback, int total_point)
	: type_(type)
	, deadfight_(deadfight)
	, fightback_(fightback)
	, total_point_(total_point)
	, score_(0)
	, equals_(0)
	, mores_(0)
	, lesses_(0)
{
}

// score is for right.
int tduel::duel(const analysis& l, const analysis& r)
{
	int score;

	if (r.type_ == l.type_) {
		// equel.
		if (!r.fightback_ && !l.fightback_) {
			score = r.total_point_ * (r.deadfight_? 2: 1) - l.total_point_ * (l.deadfight_? 2: 1);
		} else if (r.fightback_ && !l.fightback_) {
			score = l.total_point_ * (l.deadfight_? 2: 1);
		} else if (!r.fightback_ && l.fightback_) {
			score = -1 * r.total_point_ * (r.deadfight_? 2: 1);
		} else {
			score = 0;
		}
	} else if ((r.type_ + nb_items - l.type_) % nb_items == 2) {
		// right is more.
		if (!r.fightback_ && !l.fightback_) {
			score = r.total_point_ * (r.deadfight_? 2: 1);
		} else if (r.fightback_ && !l.fightback_) {
			score = 0;
		} else if (!r.fightback_ && l.fightback_) {
			score = -1 * r.total_point_ * (r.deadfight_? 2: 1);
		} else {
			score = 0;
		}
	} else {
		// right is less.
		if (!r.fightback_ && !l.fightback_) {
			score = -1 * l.total_point_ * (l.deadfight_? 2: 1);
		} else if (r.fightback_ && !l.fightback_) {
			score = l.total_point_ * (l.deadfight_? 2: 1);
		} else if (!r.fightback_ && l.fightback_) {
			score = 0;
		} else {
			score = 0;
		}
	}
	return score;
}

bool strategy_compare(const analysis& a, const analysis& b)
{
	return a.score_ > b.score_;
}

void tduel::do_duel(twindow& window)
{
	// Enumerate one analysis, and calculate it's score. select it that most score.
	std::vector<analysis> strategies, left_strategies;

	typedef std::multimap<int, int>::const_iterator Itor;
	for (int i = 0; i < nb_items; i ++) {
		if (!setting_[1].cards_.count(i)) {
			continue;
		}
		strategies.push_back(analysis(i));
		analysis& s = strategies.back();

		std::pair<Itor, Itor> range = setting_[1].cards_.equal_range(i);
		while (range.first != range.second) {
			s.total_point_ += range.first->second;
			++ range.first;
		}
		if (setting_[1].deadfight_) {
			strategies.push_back(analysis(i, true, false, s.total_point_));
		} 
		if (setting_[1].fightback_) {
			strategies.push_back(analysis(i, false, true, s.total_point_));
		}
	}

	for (int i = 0; i < nb_items; i ++) {
		if (!setting_[0].cards_.count(i)) {
			continue;
		}
		left_strategies.push_back(analysis(i));
		analysis& s = left_strategies.back();

		std::pair<Itor, Itor> range = setting_[0].cards_.equal_range(i);
		while (range.first != range.second) {
			s.total_point_ += range.first->second;
			++ range.first;
		}
		if (setting_[0].deadfight_) {
			left_strategies.push_back(analysis(i, true, false, s.total_point_));
		} 
		if (setting_[0].fightback_) {
			left_strategies.push_back(analysis(i, false, true, s.total_point_));
		}
	}

	for (std::vector<analysis>::iterator r_itor = strategies.begin(); r_itor != strategies.end();) {
		int score = 0;
		for (std::vector<analysis>::const_iterator l_itor = left_strategies.begin(); l_itor != left_strategies.end(); ++ l_itor) {
			score = duel(*l_itor, *r_itor);
			if (score > 0) {
				r_itor->mores_ ++;
			} else if (score < 0) {
				r_itor->lesses_ ++;
			} else {
				r_itor->equals_ ++;
			}
			r_itor->score_ += score;
		}

		// get rid off full less strategies
		if (r_itor->lesses_ + r_itor->equals_ == left_strategies.size() && strategies.size() > 1) {
			r_itor = strategies.erase(r_itor);
		} else {
			++ r_itor;
		}
	}
	std::sort(strategies.begin(), strategies.end(), strategy_compare);

	std::vector<analysis>::iterator choice_it = strategies.end();
	if (strategies.front().mores_ == left_strategies.size() || strategies.size() == 1) {
		choice_it = strategies.begin();
	} else {
		// why -1?
		// strategies may be some items, and those items are same score!
		// if not -1, all items's score are 0.
		int least_socre = strategies.back().score_ - 1;
		int adjusted_total = 0;

		for (std::vector<analysis>::iterator r_itor = strategies.begin(); r_itor != strategies.end(); ++ r_itor) {
			r_itor->score_ -= least_socre;
			adjusted_total += r_itor->score_;
		}
		int random = rand() %  adjusted_total;
		for (std::vector<analysis>::iterator r_itor = strategies.begin(); r_itor != strategies.end(); ++ r_itor) {
			if (random <= r_itor->score_) {
				choice_it = r_itor;
				break;
			}
			random -= r_itor->score_;
		}
	}
	analysis& choice = *choice_it;

	std::stringstream str;

	// select first analysis
	int index2 = 0;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[1].cards_.begin(); c_itor != setting_[1].cards_.end(); ++ c_itor, index2 ++) {
		if (c_itor->first != choice.type_) {
			continue;
		}
		str.str("");
		str << "rcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_value(true);
		if (choice.fightback_) {
			toggle->set_label("0");
		}
	}
	if (choice.deadfight_) {
		find_widget<ttoggle_button>(&window, "rskill0", false, false)->set_value(true);
	} else if (choice.fightback_) {
		find_widget<ttoggle_button>(&window, "rskill1", false, false)->set_value(true);
	}
	
	// update total point to rtotal.
	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "rtotal", false, false);
	for (int i = 0; i < 6; i ++) {
		toggle->canvas()[i].set_variable("foreground", variant(menu_items[choice.type_]));
	}
	str.str("");
	if (choice.fightback_) {
		str << "0";
	} else {
		str << choice.total_point_ * (choice.deadfight_? 2: 1);
	}
	toggle->set_label(str.str());
	toggle->set_dirty();

	// update skill to rskill
	if (choice.deadfight_) {
		find_widget<tcontrol>(&window, "rskill", false, false)->set_label("misc/deadfight.png");
	} else if (choice.fightback_) {
		find_widget<tcontrol>(&window, "rskill", false, false)->set_label("misc/fightback.png");
	} else {
		find_widget<tcontrol>(&window, "rskill", false, false)->set_label("");
	}

	// form left analysis
	analysis left;

	index2 = 0;
	for (std::multimap<int, int>::const_iterator c_itor = setting_[0].cards_.begin(); c_itor != setting_[0].cards_.end(); ++ c_itor, index2 ++) {
		str.str("");
		str << "lcard" << index2;
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		if (toggle->get_value()) {
			if (left.type_ == -1) {
				left.type_ = c_itor->first;
			}
			left.total_point_ += c_itor->second;
		}
	}
	left.deadfight_ = lskill0_->get_value();
	left.fightback_ = lskill1_->get_value();
	
	// refresh to turn result
	for (int index = 0; index < 2; index ++) {
		analysis* t = &left;
		if (index == 1) {
			t = &choice;
		}
		char c = index? 'r': 'l';

		// card 
		str.str("");
		str << c << "turn" << turn_; 
		toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_visible(twidget::VISIBLE);
		toggle->set_active(false);
		for (int i = 0; i < 6; i ++) {
			toggle->canvas()[i].set_variable("foreground", variant(menu_items[t->type_]));
		}
		str.str("");
		if (t->fightback_) {
			str << "0";
		} else {
			str << t->total_point_ * (t->deadfight_? 2: 1);
		}
		toggle->set_label(str.str());

		// skill
		str.str("");
		str << c << "turn" << turn_ << "_skill"; 
		toggle = find_widget<ttoggle_button>(&window, str.str(), false, false);
		toggle->set_visible(twidget::VISIBLE);
		toggle->set_active(false);
		for (int i = 0; i < 6; i ++) {
			if (t->deadfight_) {
				toggle->canvas()[i].set_variable("image", variant("misc/deadfight.png"));
			} else if (t->fightback_) {
				toggle->canvas()[i].set_variable("image", variant("misc/fightback.png"));
			} else {
				toggle->canvas()[i].set_variable("image", variant(""));
			}
		}
	}

	int score = duel(choice, left);
	str.str("");
	str << "turn" << turn_; 
	tlabel* label = find_widget<tlabel>(&window, str.str(), false, false);
	label->set_visible(twidget::VISIBLE);
	str.str("");
	str << score;
	label->set_label(str.str());

	hp_ += score;
	if (hp_ <= 0) {
		hp_ = 0;
		window.canvas(1).set_variable("hp_left", variant(""));
	} else if (hp_ >= TOTAL_HP) {
		hp_ = TOTAL_HP;
		window.canvas(1).set_variable("hp_right", variant(""));
	}
	window.canvas(1).set_variable("percentage", variant((hp_ * 100) / TOTAL_HP));

	// update flag: deadfight, fightback
	if (left.deadfight_) {
		setting_[0].deadfight_ = false;
	}
	if (left.fightback_) {
		setting_[0].fightback_ = false;
	}
	if (choice.deadfight_) {
		setting_[1].deadfight_ = false;
	}
	if (choice.fightback_) {
		setting_[1].fightback_ = false;
	}

	// sound::play_UI_sound("join.wav");
}

} // namespace gui2

