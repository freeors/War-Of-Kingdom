/* $Id: leader_list.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License 2
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Manage the selection of a leader, and select his/her gender.
 */

#include "global.hpp"

#include "gettext.hpp"
#include "leader_list.hpp"
#include "wml_separators.hpp"
#include "widgets/combo.hpp"
#include "hero.hpp"

const std::string force_list_manager::random_enemy_picture("units/random-dice.png");

force_list_manager::force_list_manager(int index, hero_map& heros, config& era_cfg, 
		std::vector<std::pair<const config *, int> >& force_list, gui::combo* faction_combo, gui::combo* gender_combo):
	heros_(heros),
	index_(index),
	leaders_(),
	genders_(),
	gender_ids_(),
	era_cfg_(era_cfg),
	force_list_(force_list),
	faction_combo_(faction_combo),
	gender_combo_(NULL),
	// gender_combo_(gender_combo),
	color_(0)
{
}

force_list_manager& force_list_manager::operator=(const force_list_manager& that)
{
	heros_ = that.heros_;
	index_ = that.index_;
	leaders_ = that.leaders_;
	genders_ = that.genders_;
	gender_ids_ = that.gender_ids_;
	era_cfg_ = that.era_cfg_;
	force_list_ = that.force_list_;
	faction_combo_ = that.faction_combo_;
	gender_combo_ = that.gender_combo_;
	color_ = that.color_;

	return *this;
}

void force_list_manager::set_leader_combo(gui::combo* combo)
{
	int selected = faction_combo_ != NULL ? faction_combo_->selected() : 0;
	faction_combo_ = combo;

	if(faction_combo_ != NULL) {
		if(leaders_.empty()) {
			update_leader_list(0);
		} else {
			populate_leader_combo(selected);
		}
	}
}

void force_list_manager::set_gender_combo(gui::combo* combo)
{
	return;
/*
	gender_combo_ = combo;

	if(gender_combo_ != NULL) {
		if(!leaders_.empty()) {
			update_gender_list(get_leader());
		}
	}
*/
}

void force_list_manager::faction_changed()
{
	leaders_.clear();
}

void force_list_manager::update_leader_list(int old_faction_combo_index)
{
	// const config& side = *side_list_[side_index];

	int selected_index_in_forces = (leaders_.empty() || !old_faction_combo_index)? -1: leaders_[old_faction_combo_index];
	int modified_faction_combo_index = old_faction_combo_index;

	leaders_.clear();
/*
	if (side["random_faction"].to_bool()) {
		if(faction_combo_ != NULL) {
			std::vector<std::string> dummy;
			dummy.push_back("-");
			faction_combo_->enable(false);
			faction_combo_->set_items(dummy);
			faction_combo_->set_selected(0);
		}
		return;
	} else {
		if(faction_combo_ != NULL)
			faction_combo_->enable(true);
		if(gender_combo_ != NULL)
			gender_combo_->enable(true);
	}
*/
	std::vector<std::string> force_ids;
	if (!era_cfg_["factions"].empty()) {
		force_ids = utils::split(era_cfg_["factions"]);
	}

	std::vector<std::string>::const_iterator itor;

	leaders_.push_back(RANDOM_FORCE);

	int partial_index = 1;
	for (itor = force_ids.begin(); itor != force_ids.end(); ++itor) {
		int index = 0;
		for (std::vector<std::pair<const config *, int> >::iterator v = force_list_.begin(); v != force_list_.end(); ++ v, index ++) {
			if (v->second >= 0 && v->second != index_) {
				continue;
			}
			const config& force = *(v->first);
			if (*itor == force["id"]) {
				leaders_.push_back(index); 
				if (selected_index_in_forces == index) {
					modified_faction_combo_index = partial_index;
				}
				partial_index ++;
			}
		}
	}

	populate_leader_combo(modified_faction_combo_index);
}

void force_list_manager::update_gender_list(const std::string& leader)
{
}

void force_list_manager::populate_leader_combo(int selected_index) 
{
	std::vector<int>::const_iterator itor;
	std::vector<std::string> leader_strings;
	for (itor = leaders_.begin(); itor != leaders_.end(); ++itor) {
		if (*itor != RANDOM_FORCE) {
			hero& leader = heros_[force_list_[*itor].first->get("leader")->to_int()];
			leader_strings.push_back(IMAGE_PREFIX + std::string(leader.image()) + std::string("~CROP(36, 30, 48, 60)~SCALE(32, 40)") + COLUMN_SEPARATOR + leader.name());
		} else {
			leader_strings.push_back(IMAGE_PREFIX + random_enemy_picture + std::string("~SCALE(32, 40)") + COLUMN_SEPARATOR + _("Random"));
		}
	}

	if (faction_combo_ != NULL) {
		faction_combo_->set_items(leader_strings);
		faction_combo_->set_selected(selected_index);
	}
}

void force_list_manager::set_leader(const std::string& leader)
{
	return;
/*
	if(faction_combo_ == NULL)
		return;

	int leader_index = 0;
	for(std::vector<int>::const_iterator itor = leaders_.begin(); itor != leaders_.end(); ++itor) {
		if(leader == *itor) {
			faction_combo_->set_selected(leader_index);
			return;
		}
		++leader_index;
	}
*/
}

void force_list_manager::set_gender(const std::string& gender)
{
}

int force_list_manager::get_leader() const
{
	if (faction_combo_ == NULL)
		return RANDOM_FORCE;

	if (leaders_.empty())
		return RANDOM_FORCE;

	if (size_t(faction_combo_->selected()) >= leaders_.size())
		return RANDOM_FORCE;

	return leaders_[faction_combo_->selected()];
}

std::string force_list_manager::get_gender() const
{
	if(gender_combo_ == NULL || genders_.empty() || size_t(gender_combo_->selected()) >= genders_.size())
		return "null";
	return gender_ids_[gender_combo_->selected()];
}

#ifdef LOW_MEM
std::string force_list_manager::get_RC_suffix(const std::string& /*unit_color*/) const {
	return "";
}
#else
std::string force_list_manager::get_RC_suffix(const std::string& unit_color) const {
	return "~RC("+unit_color+">"+lexical_cast<std::string>(color_+1) +")";
}
#endif
