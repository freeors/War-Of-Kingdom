#include "global.hpp"
#include "hero.hpp"
#include "gettext.hpp"
#include "unit_types.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "help.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "game_preferences.hpp"
#include "gui/dialogs/message.hpp"
#include "unit.hpp"
#include "artifical.hpp"
#include "unit_display.hpp"
#include "game_events.hpp"
#include "team.hpp"
#include "resources.hpp"
#include "game_display.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/hero_selection.hpp"
#include "gui/widgets/window.hpp"
#include "actions.hpp"
#include "multiplayer.hpp"
#include "lobby.hpp"

#include <boost/foreach.hpp>

namespace rpg {
	hero* h = &hero_invalid;
	int stratum = hero_stratum_wander;
	std::set<unit*> humans;
	int forbids = 0;
} // end of namespace rpg

void hero::to_unstage(int reason)
{
	if (official_ == hero_official_mayor) {
		unit_map& units = *resources::units;
		artifical* from = units.city_from_cityno(city_);
		from->select_mayor(&hero_invalid);
	} else if (official_ != HEROS_NO_OFFICIAL) {
		official_ = HEROS_NO_OFFICIAL;
	}
	if (has_nomal_noble()) {
		std::vector<team>& teams = *resources::teams;
		teams[side_].appoint_noble(*this, HEROS_NO_NOBLE, true);
	}
	status_ = hero_status_unstage;
	city_ = HEROS_ROAM_CITY;
	side_ = HEROS_INVALID_SIDE;

	if (reason == UNSTAGE_GROUP) {
		std::string message;
		if (tent::mode != mode_tag::RPG || treasure_ == HEROS_NO_TREASURE) {
			message = _("You had holden same hero as me. I see the light of officialdom and should unstage.");
		} else {
			utils::string_map symbols;
			const ttreasure& treasure = unit_types.treasure(treasure_);
			symbols["treasure"] = tintegrate::generate_format(treasure.name(), "green");
			message = vgettext2("You had holden same hero as me. I see the light of officialdom and should unstage. It is $treasure, and give it to you.", symbols);
			
			team& t = (*resources::teams)[rpg::h->side_];
			std::vector<size_t>& holden = t.holded_treasures();
			holden.push_back(treasure_);
			treasure_ = HEROS_NO_TREASURE;
		}
		game_events::show_hero_message(this, NULL, message, game_events::INCIDENT_WANDER);
	}
}

bool hero::has_nomal_noble() const
{
	if (noble_ == HEROS_NO_NOBLE) return false;
	const tnoble& n = unit_types.noble(noble_);
	return !n.leader();
}

void hero::goto_hate_if(const team& new_team, const hero& original_leader, int random)
{
	if (tent::mode != mode_tag::RPG) {
		return;
	}
	hero& new_leader = *new_team.leader();
	if (&new_leader != rpg::h) {
		return;
	}
	if (base_catalog_ == original_leader.base_catalog_ && base_catalog_ != new_leader.base_catalog_ && !(random % 3)) {
		if (new_leader.can_goto_hate(new_leader)) {
			::do_hate_relation(*this, new_leader, true);
		}
	}
}

void hero::increase_feeling_each(unit_map& units, hero_map& heros, hero& to, int inc)
{
	int carry_to, descent_number;
	std::vector<unit*> adjusted;

	for (int i = 0; i < 2; i ++) {
		if (i == 0) {
			carry_to = increase_feeling(to, inc, descent_number);
		} else {
			carry_to = to.increase_feeling(*this, inc, descent_number);
		}
		unit* u = NULL;
		if (carry_to != FEELING_NONE) {
			game_events::show_relation_message(units, heros, i? to: *this, i? *this: to, carry_to);

			u = units.find_unit(*this);
			if (!u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
				u->adjust();
				adjusted.push_back(u);
			}
		}
		if (descent_number != HEROS_INVALID_NUMBER) {
			u = units.find_unit(heros[descent_number]);
			if (u && !u->is_artifical() && std::find(adjusted.begin(), adjusted.end(), u) == adjusted.end()) {
				u->adjust();
				adjusted.push_back(u);
			}
		}
	}
}

void hero::add_modification(unit_map& units, hero_map& heros, std::vector<team>& teams, const config& mod, unit* u, hero* leader)
{
	BOOST_FOREACH (const config &effect, mod.child_range("effect")) {
		const std::string &apply_to = effect["apply_to"];

		if (apply_to == "loyalty") {
			int increase = effect["increase"].to_int();

			if (increase) {
				increase_catalog(-1 * increase, *leader);
				if (!u->is_artifical()) {
					u->adjust();
				}
				// play animation
				std::stringstream str;
				str << _("Loyalty") << "\n";
				std::vector<unit*> touchers;
				if (artifical* city = units.city_from_loc(u->get_location())) {
					touchers.push_back((unit*)(city));
				} else {
					touchers.push_back(u);
				}
				unit_display::unit_touching(*u, touchers, increase, str.str());
				
				hero& leader = *teams[u->side() - 1].leader();
				if (base_catalog_ == leader.base_catalog_ && is_hate(leader)) {
					::do_hate_relation(leader, *this, false);
				}
			}			
		} else if (apply_to == "office") {
			artifical* selected_city = unit_2_artifical(u);
			bool city_team_is_rpg = teams[selected_city->side() - 1].leader() == rpg::h;

			std::string message;
			if (city_team_is_rpg && runtime_groups::exist_member(*this, *rpg::h)) {
				this->to_unstage(UNSTAGE_GROUP);
			} else if (!tent::tower_mode()) {
				selected_city->finish_into(*this, hero_status_backing);
			} else {
				selected_city->fresh_into(*this);
			}
			std::vector<hero*>& wander_heros = selected_city->wander_heros();
			wander_heros.erase(std::find(wander_heros.begin(), wander_heros.end(), this));

			if (status_ != hero_status_unstage) {
				message = _("Let me join in. I will do my best to maintenance our honor.");
				join_anim(this, selected_city, message);
			}
			
			map_location loc2(MAGIC_HERO, number_);
			game_events::fire("post_recommend", selected_city->get_location(), loc2);

		} else if (apply_to == "wander") {
			std::vector<hero*> captains;
			if (artifical* city = units.city_from_loc(u->get_location())) {
				// fresh/finish/reside troop
				if (u->is_city()) {
					for (int type = 0; type < 3; type ++) { 
						std::vector<hero*>* list;
						if (type == 0) {
							list = &city->fresh_heros();
						} else if (type == 1) {
							list = &city->finish_heros();
						} else {
							list = &city->wander_heros();
						}
						std::vector<hero*>::iterator i2 = std::find(list->begin(), list->end(), this);
						if (i2 != list->end()) {
							list->erase(i2);
							break;
						}
					}
				} else {
					std::vector<unit*>& reside_troops = city->reside_troops();
					int index = 0;
					for (std::vector<unit*>::iterator i2 = reside_troops.begin(); i2 != reside_troops.end(); ++ i2, index ++) {
						if (*i2 != u) {
							continue;
						}
						u->replace_captains_internal(*this, captains);
						if (captains.empty()) {
							city->troop_go_out(*u);
						} else {
							u->replace_captains(captains);
						}
						break;
					}
				}
			} else {
				// field troop
				u->replace_captains_internal(*this, captains);
				if (captains.empty()) {
					units.erase2(u);
				} else {
					u->replace_captains(captains);
				}
			}
			// select one city in myself side
			artifical* to_city = units.city_from_cityno(leader->city_);
			to_city->wander_into(*this);

		} else if (apply_to == "communicate") {
			int increase = effect["increase"].to_int();
			std::vector<std::pair<int, unit*> > pairs;

			for (hero_map::iterator i = heros.begin(); i != heros.end(); ++ i) {
				const hero& h = *i;
				if (h.side_ != side_ || h.number_ == number_) {
					continue;
				}
				unit* u = units.find_unit(h);
				if (u->is_city() && h.number_ == u->master().number_) {
					continue;
				}
				pairs.push_back(std::make_pair(h.number_, u));
			}
			if (increase && !pairs.empty()) {
				// display hero selection dialog
				gui2::thero_selection dlg(&teams, &units, heros, pairs, side_ + 1, "");
				try {
					dlg.show(resources::screen->video());
				} catch(twml_exception& e) {
					e.show(*resources::screen);
					return;
				}
				const std::set<size_t>& checked_pairs = dlg.checked_pairs();
				if (dlg.get_retval() != gui2::twindow::OK || checked_pairs.empty()) {
					return;
				}
				hero& to = heros[pairs[*checked_pairs.begin()].first];
				utils::string_map symbols;
				symbols["first"] = name();
				symbols["second"] = to.name();
				game_events::show_hero_message(&heros[hero::number_scout], NULL, vgettext2("Feeling between $first and $second is increased.", symbols), game_events::INCIDENT_INVALID);
				
				// adjust troop if needed.
				increase_feeling_each(units, heros, to, increase);
			}
		}
	}
}

bool hero::internal_matches_filter(const vconfig& cfg)
{
	if (!cfg["number"].blank()) {
		if (cfg["number"].to_int() != number_) {
			return false;
		}
	}

	return true;
}

int hero::calculate_used_point() const
{
	const int least_field = 50;
	const int point_per_adaptability = 10;
	const int point_per_feature_level = 25;
	const int point_per_tactic_level = 25;
	const int point_character_used = 50;
	int used_point = 0;

	// 5 field
	if (fxptoi9(leadership_) > least_field) {
		used_point += fxptoi9(leadership_) - least_field;
	}
	if (fxptoi9(force_) > least_field) {
		used_point += fxptoi9(force_) - least_field;
	}
	if (fxptoi9(intellect_) > least_field) {
		used_point += fxptoi9(intellect_) - least_field;
	}
	if (fxptoi9(spirit_) > least_field) {
		used_point += fxptoi9(spirit_) - least_field;
	}
	if (fxptoi9(charm_) > least_field) {
		used_point += fxptoi9(charm_) - least_field;
	}

	// arms adaptability
	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		used_point += fxptoi12(arms_[i]) * point_per_adaptability;
	}
		
	// skill adaptability
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		used_point += fxptoi12(skill_[i]) * point_per_adaptability;
	}

	// feature
	if (feature_ != HEROS_NO_FEATURE) {
		used_point += (1 + unit_types.feature_level(feature_)) * point_per_feature_level;
	}
	if (side_feature_ != HEROS_NO_FEATURE) {
		used_point += (1 + unit_types.feature_level(side_feature_)) * 2 * point_per_feature_level;
	}

	// tactic
	if (tactic_ != HEROS_NO_TACTIC) {
		used_point += (1 + unit_types.tactic(tactic_).level()) * point_per_tactic_level;
	}

	// character
	if (character_ != HEROS_NO_CHARACTER) {
		used_point += point_character_used;
	}

	return used_point;
}

bool hero::check_valid() const
{
	return true;
}

bool hero::confirm_carry_to(hero& h2, int carry_to) 
{ 
	return game_events::confirm_carry_to(*this, h2, carry_to);
}

namespace increase_xp {

ublock& attack_ublock(const unit& attack, bool opp_is_artifical) 
{
	memset(&ub, 0, sizeof(ublock));
	ub.xp = true;
	if (!attack.is_commoner()) {
		ub.leadership = ub.force = ub.intellect = ub.charm = true;
		ub.arms = true;
		if (opp_is_artifical) {
			ub.skill[hero_skill_demolish] = true;
		}
		// effect: character
		int level = attack.character_level(apply_to_tag::INDUSTRIOUS);
		if (level) {
			int bonus = level / 2;
			ub.leadership_speed = ub.force_speed = ub.intellect_speed = ub.charm_speed = bonus;
		}
		level = attack.character_level(apply_to_tag::EXPANSIVE);
		if (level) {
			ub.arms_speed = level / 2;
		}
		level = attack.character_level(apply_to_tag::CREATIVE);
		if (level) {
			ub.skill_speed[hero_skill_demolish] = level / 2;
		}
		
		// effect: feature
		if (unit_feature_val2(attack, hero_feature_guide)) {
			ub.abilityx2 = true;
		}
		if (unit_feature_val2(attack, hero_feature_arms)) {
			ub.armsx2 = true;
		}
		if (unit_feature_val2(attack, hero_feature_skill)) {
			ub.skillx2 = true;
		}
		ub.meritorious = true;
	}
	return ub;
}

ublock& turn_ublock(const unit& u)
{
	memset(&ub, 0, sizeof(ublock));
	ub.skill[hero_skill_encourage] = true;
	ub.skill[hero_skill_formation] = true;
	ub.skill_speed[hero_skill_formation] = -50;
	int level = u.character_level(apply_to_tag::CREATIVE);
	if (level) {
		ub.skill_speed[hero_skill_encourage] += level / 2;
		ub.skill_speed[hero_skill_formation] += level / 2;
	}
	if (unit_feature_val2(u, hero_feature_skill)) {
		ub.skillx2 = true;
	}
	return ub;
}

ublock& duel_ublock(const unit& u)
{
	memset(&ub, 0, sizeof(ublock));
	ub.skill[hero_skill_hero] = true;
	if (unit_feature_val2(u, hero_feature_skill)) {
		ub.skillx2 = true;
	}
	return ub;
}

}

bool tgroup::upgrade_internal(display& disp, hero_map& heros, tmember* m, std::map<int, http::temployee>* employees)
{
	utils::string_map symbols;
	std::stringstream err;
	
	const upgrade::trequire* require_ptr;
	if (m) {
		if (m->level >= game_config::max_member_level) {
			return false;
		}
		require_ptr = &upgrade::member_require(m->level);

	} else {
		int noble = preferences::noble();
		if (noble >= unit_types.max_noble_level()) {
			return false;
		}
		require_ptr = &upgrade::noble_require(noble);
	}

	const upgrade::trequire& require = *require_ptr;
	if (sum_score(coin_, score_) < require.score) {
		symbols["coin"] = tintegrate::generate_format(require.coin, "red");
		symbols["score"] = tintegrate::generate_format(require.score, "red");
		err << vgettext2("Repertory is not enough to pay $coin coin and $score score. If lack one only, can exchange between coin and score.", symbols);
		gui2::show_message(disp.video(), "", err.str());
		return false;
	}

	http::membership m2;
	if (!m) {
		m2 = http::upgrade(disp, heros, HEROS_INVALID_NUMBER, require.coin, require.score);

	} else if (!m->base->get_flag(hero_flag_employee)) {
		m2 = http::upgrade(disp, heros, m->base->number_, require.coin, require.score);

	} else {
		std::set<int> number;
		number.insert(m->base->number_);
		m2 = http::employee_common(disp, heros, http::employee_tag_upgrade, number, -1 * require.coin, -1 * require.score, employees);
	}
	if (m2.uid < 0) {
		return false;
	}
	from_local_membership(disp, heros, m2, false);

	return true;
}

bool tgroup::upgrade_leader(display& disp, hero_map& heros)
{
	return upgrade_internal(disp, heros, NULL, NULL);
}

bool tgroup::upgrade_member(display& disp, hero_map& heros, hero& h)
{
	tmember& m = member(h);
	return upgrade_internal(disp, heros, &m, NULL);
}

int tgroup::calculate_total_point() const
{
	const int allocatable[] = {300, 350, 400, 450, 500};
	int noble = preferences::noble();

	int bonus = preferences::vip2()? 150: 0;
	return allocatable[noble] + bonus;
}

void tgroup::from_membership(hero_map& heros, const http::membership& m, int city)
{
	reset();

	heros.add(hero(heros.size()));
	hero& leader = heros[heros.size() - 1];
	leader.set_name(m.name);
	leader.set_uid(m.uid);
	leader.field_from_str(m.field);
	leader.translatable_from_str(m.translatable);

	set_leader(leader);
	noble_ = m.noble;
	coin_ = m.coin;
	score_ = m.score;
	tax_ = m.tax;
	interior_from_str(m.interior);
	signin_from_str(m.signin);
	message_from_str(m.message);
	member_from_str(heros, m.member, false);
	exile_from_str(heros, m.exile);
	associate_from_str(m.associate);
	set_layout(m.layout);
	set_map(m.map);

	int number = city != HEROS_INVALID_NUMBER? city: hero::number_local_player_city + 1 + (int)other_group.size();
	if (number > hero::number_network_player_city_max) {
		number = hero::number_network_player_city_max;
	}
	hero& h = heros[number];
	set_city(h);
	city_->set_name(m.city);
}

// this function can only use for local player!
void tgroup::from_local_membership(display& disp, hero_map& heros, const http::membership& m, bool enhance)
{
	if (m.noble != noble()) {
		set_noble(m.noble);
		preferences::set_noble(m.noble);
	}

	if (m.coin != coin()) {
		set_coin(m.coin);
		preferences::set_coin(m.coin);
	}
	if (m.score != score()) {
		set_score(m.score);
		preferences::set_score(m.score);
	}
	preferences::set_vip_expire(m.expire);

	if (m.uid != leader().uid()) {
		leader().set_uid(m.uid);
		preferences::set_uid(m.uid);
	}

	if (m.city != city().name()) {
		city().set_name(m.city);
		preferences::set_city(m.city);
	}

	if (m.interior != interior_to_str()) {
		interior_from_str(m.interior);
		preferences::set_interior(m.interior);
	}

	if (m.signin != signin_to_str()) {
		signin_from_str(m.signin);
		preferences::set_signin(m.signin);
	}

	if (m.member != member_to_str() || m.exile != exile_to_str()) {
		if (group.valid()) {
			// if in runtime, don't update heros!
			reload_heros_from_string(heros, m.member, m.exile);
		}
		preferences::set_member(m.member);
		preferences::set_exile(m.exile);
	}

	if (m.associate != associate_to_str()) {
		associate_from_str(m.associate);
		preferences::set_associate(m.associate);
	}

	if (enhance) {
		// below code maybe send http command, must validate timestamp.
		game_config::timestamp = m.timestamp;
	}

	// check other's request according to new associate.
	http::membership m2;
	for (std::vector<tgroup::tassociate>::const_iterator it = associates_.begin(); it != associates_.end(); ++ it) {
		const tgroup::tassociate& a = *it;
		if (a.agreement == tgroup::tassociate::requestally && !a.t) {
			m2 = http::affirm_ally(disp, heros, a.username, true, false);
		} else if (a.agreement == tgroup::tassociate::requestterminate && !a.t) {
			m2 = http::affirm_terminate(disp, heros, a.username, true);
		}
	}
	if (m2.uid >= 0 && m2.associate != associate_to_str()) {
		associate_from_str(m2.associate);
		preferences::set_associate(m2.associate);
	}
	
	bool hero_data_dirty = false;
	if (m.field != leader().field_to_str()) {
		leader().field_from_str(m.field);
		hero_data_dirty = true;
	}

	if (m.translatable != leader().translatable_to_str()) {
		leader().translatable_from_str(m.translatable);
		hero_data_dirty = true;
	}

	if (hero_data_dirty) {
		preferences::set_hero(heros, leader());
	}

	if (enhance) {
		if (m.tax != tax()) {
			set_tax(m.tax);
		}
		if (m.layout != layout()) {
			set_layout(m.layout);
			preferences::set_layout(m.layout);
		}

		if (m.map != map()) {
			set_map(m.map);
			preferences::set_map(m.map);
		}

		message_from_str(m.message);
	}
}

http::membership tgroup::to_membership() const
{
	http::membership m;

	m.uid = leader().uid();
	m.vip = vip();
	m.name = leader().name();
	m.expire = expire();
	m.noble = noble();
	m.member = member_to_str();
	m.exile = exile_to_str();
	m.associate = associate_to_str();
	m.layout = layout();
	m.field = leader().field_to_str();
	m.translatable = leader().translatable_to_str();
	m.score = score();
	m.coin = coin();
	m.city = city().name();
	m.map = map();
	m.interior = interior_to_str();
	m.signin = signin_to_str();

	return m;
}

void tgroup::set_layout(const std::string& str)
{
	layout_ = str;
}

const char layout_chapter_marker = '$';
const char layout_section_marker = '|';
const std::string layout_base_prefix = "base";
const std::string layout_troop_prefix = "troop";
const std::string layout_art_prefix = "art";
const std::string layout_noble_prefix = "noble";

int tgroup::layout_number_from_hero(const hero& h) const
{
	if (h.player_ != HEROS_INVALID_NUMBER) {
		return member(h).base->number_;
	}
	return h.number_;
}

bool compare_unit_location(const unit* l, const unit* r)
{
	return l->get_location() < r->get_location();
}

std::string tgroup::layout_from_team_internal(team& t, const tgroup& g)
{
	// map_location, hero | & NOBLE_MAGIC, hero, & map_lcation, wall2
	const std::vector<artifical*>& cities = t.holden_cities();
	const artifical& city = *cities[0];

	std::stringstream strstr;

	// base
	strstr << layout_base_prefix;
	strstr << layout_section_marker;
	strstr << hero_map::map_size_from_dat;
	strstr << layout_section_marker;

	int stratagem = t.current_stratagem();
	if (stratagem == stratagem_tag::none) {
		stratagem = stratagem_tag::min;
	}
	strstr << stratagem;

	strstr << layout_chapter_marker;

	// filed troop
	strstr << layout_troop_prefix;
	std::vector<unit*> fields = city.field_troops();
	std::sort(fields.begin(), fields.end(), compare_unit_location);
	for (std::vector<unit*>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
		strstr << layout_section_marker;
		const unit& u = **it;
		strstr << u.get_location().x << "," << u.get_location().y << ",";
		strstr << g.layout_number_from_hero(u.master());
		if (u.second().valid()) {
			strstr << "," << g.layout_number_from_hero(u.second());
		}
		if (u.third().valid()) {
			strstr << "," << g.layout_number_from_hero(u.third());
		}
	}
	strstr << layout_chapter_marker;

	// field art
	strstr << layout_art_prefix;
	std::vector<artifical*> arts = city.field_arts();
	std::sort(arts.begin(), arts.end(), compare_unit_location);
	for (std::vector<artifical*>::const_iterator it = arts.begin(); it != arts.end(); ++ it) {
		strstr << layout_section_marker;
		const artifical& art = **it;
		strstr << art.get_location().x << "," << art.get_location().y << ",";
		strstr << art.type()->id();
	}
	strstr << layout_chapter_marker;
	
	// noble
	strstr << layout_noble_prefix;
	const std::map<int, hero*>& appointed_nobles = t.appointed_nobles();
	for (std::map<int, hero*>::const_iterator it = appointed_nobles.begin(); it != appointed_nobles.end(); ++ it) {
		strstr << layout_section_marker;
		strstr << it->first << "," << g.layout_number_from_hero(*it->second);
	}

	return strstr.str();
}

void tgroup::layout_from_team(game_display& disp, team& t)
{
	layout_ = layout_from_team_internal(t, *this);
}

hero& tgroup::hero_from_layout_number(hero_map& heros, int number, int layout_leader_number) const
{
	hero& h = heros[number];
	if (number == layout_leader_number) {
		return *leader_;
	} else if (h.player_ == HEROS_INVALID_NUMBER) {
		hero* h2 = exist_member(number);
		return h2? *h2: hero_invalid;
	}
	return hero_invalid;
}

bool tgroup::layout_to_team_internal(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, const std::string& from, team& t, bool mirror, bool conside_stratagem, bool wall, const tgroup& g)
{
	int relative = 0;
	const std::vector<artifical*>& cities = t.holden_cities();
	artifical& city = *cities[0];
	int layout_leader_number = HEROS_INVALID_NUMBER;
	// int stratagem = stratagem_tag::min;

	const std::vector<std::string> chapters = utils::split(from, layout_chapter_marker, utils::STRIP_SPACES);
	if (chapters.size() != 4) {
		return false;
	}
	for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
		const std::string& chapter = *it;
		const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
		if (sections.empty()) {
			return false;
		}
		const std::string& name = sections[0];
		std::vector<std::string> fields;
		if (name == layout_base_prefix) {
			if (sections.size() < 3) {
				return false;
			}
			layout_leader_number = lexical_cast_default<int>(sections[1]);
			if (conside_stratagem) {
				int stratagem = lexical_cast_default<int>(sections[2]);
				if (stratagem < stratagem_tag::min || stratagem > stratagem_tag::max) {
					stratagem = stratagem_tag::min;
				}
				t.insert_stratagem(stratagem_tag::technology(stratagem), false);
			}

		} else if (name == layout_troop_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				if (layout_leader_number == HEROS_INVALID_NUMBER) {
					continue;
				}
				if (t.max_troops && t.holden_troops() >= t.max_troops) {
					break;
				}

				fields = utils::split(sections[n]);
				if (fields.size() < 3 || fields.size() > 5) {
					continue;
				}
				map_location at(lexical_cast_default<int>(fields[0]), lexical_cast_default<int>(fields[1]));
				if (at.x >= game_config::siege_map_w || at.y >= game_config::siege_map_h) {
					continue;
				}
				if (mirror) {
					at.x = game_config::siege_map_w * 2 - at.x;
				}
				hero& master = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[2]), layout_leader_number); 
				if (!master.valid()) {
					continue;
				}
				const unit_type* ut = unit_types.keytypes().begin()->second;
				if (master.utype_ != HEROS_NO_UTYPE) {
					ut = unit_types.keytype(master.utype_);
				}

				std::vector<const hero*> v;
				v.push_back(&master);
				for (size_t n1 = 3; n1 < fields.size(); n1 ++) {
					hero& h = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[n1]), layout_leader_number);
					if (h.valid()) {
						v.push_back(&h);
					}
				}
	
				int cost = 0;
				do_recruit(units, heros, teams, t, ut, v, city, cost, true, false, state);
				do_direct_expedite(teams, units, city, city.reside_troops().size() - 1, at, false);
			}
		} else if (name == layout_art_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				fields = utils::split(sections[n]);
				if (fields.size() != 3) {
					continue;
				}
				map_location at(lexical_cast_default<int>(fields[0]), lexical_cast_default<int>(fields[1]));
				if (mirror) {
					at.x = game_config::siege_map_w * 2 - at.x;
				}
				const unit_type* ut = unit_types.find(fields[2]);
				if (!ut) {
					continue;
				}
				VALIDATE(ut->master() != HEROS_INVALID_NUMBER, "to_team, invalid art type");

				if (!wall && ut->master() == hero::number_wall) {
					continue;
				}

				std::vector<const hero*> v;
				v.push_back(&heros[ut->master()]);
				
				type_heros_pair pair(ut, v);
				artifical new_art(units, heros, teams, state, pair, city.cityno(), true);
				units.add(at, &new_art);
			}

		} else if (name == layout_noble_prefix) {
			for (size_t n = 1; n < sections.size(); n ++) {
				if (layout_leader_number == HEROS_INVALID_NUMBER) {
					continue;
				}
				fields = utils::split(sections[n]);
				if (fields.size() != 2) {
					continue;
				}
				int noble = lexical_cast_default<int>(fields[0]);
				hero& h = g.hero_from_layout_number(heros, lexical_cast_default<int>(fields[1]), layout_leader_number); 
				if (!h.valid()) {
					continue;
				}
				t.appoint_noble(h, noble, false);
			}
		}
	}
	return true;
}

bool tgroup::layout_to_team(game_display& disp, unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, team& t, bool mirror, bool conside_stratagem, bool wall)
{
	if (layout_.empty()) {
		return true;
	}
	try {
		return layout_to_team_internal(units, heros, teams, state, layout_, t, mirror, conside_stratagem, wall, *this);
	} catch (twml_exception& e) {
		layout_.clear();
		e.show(disp);
	}
	return layout_to_team_internal(units, heros, teams, state, layout_, t, mirror, conside_stratagem, wall, *this);
}

const ttechnology& tgroup::stratagem_from_layout_str_internal(const std::string& layout)
{
	int stratagem = stratagem_tag::min;

	do {
		const std::vector<std::string> chapters = utils::split(layout, layout_chapter_marker, utils::STRIP_SPACES);
		if (chapters.size() != 4) {
			break;
		}
		for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
			const std::string& chapter = *it;
			const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
			if (sections.empty()) {
				break;
			}
			const std::string& name = sections[0];
			std::vector<std::string> fields;
			if (name == layout_base_prefix) {
				if (sections.size() >= 3) {
					stratagem = lexical_cast_default<int>(sections[2]);
					if (stratagem < stratagem_tag::min || stratagem > stratagem_tag::max) {
						stratagem = stratagem_tag::min;
					}
				}
			}
		}
	} while (false);

	return stratagem_tag::technology(stratagem);
}

const ttechnology& tgroup::stratagem_from_layout_str() const
{
	return stratagem_from_layout_str_internal(layout_);
}

std::vector<int> tgroup::interior_increase_from_layout_str_internal(const std::string& layout)
{
	std::vector<int> increase(interior_count);

	do {
		const std::vector<std::string> chapters = utils::split(layout, layout_chapter_marker, utils::STRIP_SPACES);
		if (chapters.size() != 4) {
			break;
		}
		for (std::vector<std::string>::const_iterator it = chapters.begin(); it != chapters.end(); ++ it) {
			const std::string& chapter = *it;
			const std::vector<std::string> sections = utils::split(chapter, layout_section_marker, utils::STRIP_SPACES);
			if (sections.empty()) {
				break;
			}
			const std::string& name = sections[0];
			std::vector<std::string> fields;
			if (name == layout_art_prefix) {
				for (size_t n = 1; n < sections.size(); n ++) {
					fields = utils::split(sections[n]);
					if (fields.size() != 3) {
						continue;
					}
					const unit_type* ut = unit_types.find(fields[2]);
					if (!ut) {
						continue;
					}
					if (ut->master() == hero::number_tactic) {
						increase[tgroup::interior_military] += ut->level();

					} else if (ut->master() == hero::number_school) {
						increase[tgroup::interior_culture] += ut->level();

					} else if (ut->master() == hero::number_market) {
						increase[tgroup::interior_economy] += ut->level();

					} else if (ut->master() == hero::number_technology) {
						increase[tgroup::interior_science] += ut->level();
					}
				}
			}
		}
	} while (false);

	return increase;
}

std::vector<int> tgroup::interior_increase_from_layout_str() const
{
	return interior_increase_from_layout_str_internal(layout_);
}

std::vector<const ttechnology*> tgroup::calculate_allocatable_technology(int mode, std::vector<const ttechnology*>& exclude, int random) const
{
	std::vector<const ttechnology*> ret;
	int science = interiors_[interior_science];

	// 10--->625
	int allocatable = pow(1.0 * science * 39062.5, 0.5);

	std::vector<const ttechnology*> candidate;
	const std::set<const ttechnology*>& selectable = unit_types.selectable_technologies();
	const std::map<std::string, ttechnology>& technologies = unit_types.technologies();
	for (std::map<std::string, ttechnology>::const_iterator it = technologies.begin(); it != technologies.end(); ++ it) {
		const ttechnology& t = it->second;
		if (std::find(exclude.begin(), exclude.end(), &t) != exclude.end()) {
			continue;
		}
		if (t.stratagem() || selectable.find(&t) == selectable.end() || !t.filter_mode(mode)) {
			continue;
		}
		if (allocatable >= t.max_experience()) {
			candidate.push_back(&t);
		}
	}
	while (!candidate.empty()) {
		const ttechnology* t = candidate[random % candidate.size()];
		ret.push_back(t);
		allocatable -= t->max_experience();
		candidate.erase(std::find(candidate.begin(), candidate.end(), t));
		for (std::vector<const ttechnology*>::iterator it = candidate.begin(); it != candidate.end(); ) {
			const ttechnology* t = *it;
			if (allocatable < t->max_experience()) {
				it = candidate.erase(it);
			} else {
				++ it;
			}
		}
		random = random * 1103515245 + 12345;
		random = (static_cast<unsigned>(random / 65536) % 32768);
	}

	return ret;
}

std::string tgroup::increase_allocatable_technology(int mode, const std::string& exclude_str, int random) const
{
	const std::vector<std::string> vstr = utils::split(exclude_str);
	std::vector<const ttechnology*> exclude;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		exclude.push_back(&unit_types.technology(*it));
	}
	std::vector<const ttechnology*> allocated = calculate_allocatable_technology(mode, exclude, random);
	// std::copy(allocated.begin(), allocated.end(), std::back_inserter(exclude));

	std::stringstream ret;
	ret << exclude_str;
	for (std::vector<const ttechnology*>::const_iterator it = allocated.begin(); it != allocated.end(); ++ it) {
		if (!ret.str().empty()) {
			ret << ",";
		}
		ret << (*it)->id();
	}
	return ret.str();
}

bool compare_charm_leadership_diff(const hero* l, const hero* r)
{
	int ldiff = 1 * l->charm_ - l->leadership_;
	if (l->feature_ != HEROS_NO_FEATURE) {
		ldiff += unit_types.feature_level(l->feature_) * ftofxp9(10);
	}
	if (l->tactic_ != HEROS_NO_TACTIC) {
		ldiff += unit_types.tactic(l->tactic_).level() * ftofxp9(10);
	}
	int rdiff = 1 * r->charm_ - r->leadership_;
	if (r->feature_ != HEROS_NO_FEATURE) {
		rdiff += unit_types.feature_level(r->feature_) * ftofxp9(10);
	}
	if (r->tactic_ != HEROS_NO_TACTIC) {
		rdiff += unit_types.tactic(r->tactic_).level() * ftofxp9(10);
	}
	return ldiff > rdiff || (ldiff == rdiff && l->number_ < r->number_);
}

bool compare_leadership_charm_diff(const hero* l, const hero* r)
{
	int ldiff = 1 * l->leadership_ - l->charm_;
	if (l->feature_ != HEROS_NO_FEATURE) {
		ldiff += unit_types.feature_level(l->feature_) * ftofxp9(10);
	}
	if (l->tactic_ != HEROS_NO_TACTIC) {
		ldiff += unit_types.tactic(l->tactic_).level() * ftofxp9(10);
	}
	int rdiff = 1 * r->leadership_ - r->charm_;
	if (r->feature_ != HEROS_NO_FEATURE) {
		rdiff += unit_types.feature_level(r->feature_) * ftofxp9(10);
	}
	if (r->tactic_ != HEROS_NO_TACTIC) {
		rdiff += unit_types.tactic(r->tactic_).level() * ftofxp9(10);
	}
	return ldiff > rdiff || (ldiff == rdiff && l->number_ < r->number_);
}

std::map<int, int> tgroup::adjust_hero_field(int field)
{
	VALIDATE(field == hero_field_leadership || field == hero_field_charm, "adjust_hero_field, only support leadership, charm!");

	std::map<int, int> ret;
	int interior_index = interior_count;
	std::vector<hero*> candidate;
	candidate.push_back(leader_);

	for (std::vector<tgroup::tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		candidate.push_back(it->h);
	}
	if (field == hero_field_leadership) {
		interior_index = interior_military;
		std::sort(candidate.begin(), candidate.end(), compare_charm_leadership_diff);

	} else if (field == hero_field_charm) {
		interior_index = interior_culture;
		std::sort(candidate.begin(), candidate.end(), compare_leadership_charm_diff);
	}

	std::vector<int> numbers;
	for (std::vector<hero*>::const_iterator it = candidate.begin(); it != candidate.end(); ++ it) {
		numbers.push_back((*it)->number_);
	}

	int value = interiors_[interior_index];

	// 10--->15
	int allocatable = pow(1.0 * value * 22.5, 0.5);

	const int interior_max_hero_field = 110;
	while (!candidate.empty() && allocatable > 0) {
		hero& h = *candidate.front();
		if (field == hero_field_leadership) {
			value = fxptoi9(h.leadership_);
		} else if (field == hero_field_charm) {
			value = fxptoi9(h.charm_);
		}
		if (value < interior_max_hero_field) {
			int bonus = interior_max_hero_field - value;
			if (allocatable < bonus) {
				bonus = allocatable;
			}
			allocatable -= bonus;
			if (field == hero_field_leadership) {
				h.leadership_ = ftofxp9(value + bonus);
			} else if (field == hero_field_charm) {
				h.charm_ = ftofxp9(value + bonus);
			}
			ret.insert(std::make_pair(h.number_, bonus));
		}
		candidate.erase(candidate.begin());
	}
	return ret;
}

std::vector<size_t> tgroup::increase_card(team& t, int mode)
{
	std::vector<size_t> ret;
	int economy = interiors_[interior_economy];

	// 10--->20
	int allocatable = pow(1.0 * economy * 40, 0.5);

	const std::vector<size_t>& candidate_cards = t.candidate_cards();
	const int card_cost = (mode == mode_tag::RPG)? 30: 40;
	int index = 0;
	while (!candidate_cards.empty() && allocatable >= card_cost) {
		ret.push_back(candidate_cards[index % candidate_cards.size()]);
		t.add_card(ret.back(), true);
		index ++;
		allocatable -= card_cost;
	}
	return ret;
}

const config& tgroup::to_faction_cfg(bool exile, bool multigrid)
{
	if (!faction_cfg_.empty()) {
		return faction_cfg_;
	}

	config& cfg = faction_cfg_;
	cfg["leader"] = leader_->number_;
	
	std::stringstream strstr, tower_strstr;
	int number = leader_->number_;
	strstr << number;
	tower_strstr << number;
	int count = 1;
	for (std::vector<tmember>::const_iterator it = members_.begin(); it != members_.end(); ++ it) {
		number = it->h->number_;
		strstr << "," << number;
		if (count < game_config::tower_fix_heros) {
			tower_strstr << "," << number;
		}
		count ++;
	}
	if (exile) {
		std::set<int> numbers;
		for (std::vector<tmember>::const_iterator it = exiles_.begin(); it != exiles_.end(); ++ it) {
			numbers.insert(it->h->number_);
		}
		int require_extract = numbers.size() / 2;
		while (require_extract > 0) {
			std::set<int>::iterator it = numbers.begin();
			std::advance(it, rand() % numbers.size());
			number = *it;
			strstr << "," << number;
			if (count < game_config::tower_fix_heros) {
				tower_strstr << "," << number;
			}
			count ++;
			numbers.erase(it);
			require_extract --;
		}
	}
	cfg["service_heros"] = strstr.str();
	cfg["tower_heros"] = tower_strstr.str();
	cfg["wander_heros"] = "";
	config& sub = cfg.add_child("artifical");
	sub["heros_army"] = city_->number_;
	if (multigrid) {
		sub["type"] = unit_types.find_city(1)->id();
	} else {
		sub["type"] = "forest1";
	}

	return faction_cfg_;
}

