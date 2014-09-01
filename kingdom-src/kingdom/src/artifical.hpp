#ifndef __ARTIFICAL_H_
#define __ARTIFICAL_H_

#include "config.hpp"
#include "map_location.hpp"
#include "unit.hpp"

class game_display;
class card;
#define unit_2_artifical(unitp)			(dynamic_cast<artifical*>((unitp)))
#define const_unit_2_artifical(unitp)	(dynamic_cast<const artifical*>((unitp)))

#define unit_is_city(unitp)				((unitp)->is_artifical() && (unitp)->can_reside())
#define artifical_is_city(artp)			((artp)->can_reside())

#define this_is_city()					(artifical_ && can_reside_)

struct artifical_fields_t
{
	UNIT_FIELDS;
	int32_t mayor_;
	int32_t fronts_;
	int32_t police_;
	int32_t soldiers_;
	int32_t gold_bonus_;
	int32_t technology_bonus_;
	int32_t max_commoners_;
	int32_t decree_;
	int32_t research_turns_;
	int32_t max_level_;
	unit_segment fresh_heros_;
	unit_segment wander_heros_;
	unit_segment finish_heros_;
	unit_segment economy_area_;
	unit_segment district_;
	unit_segment not_recruit_;
	unit_segment reside_troops_;
	unit_segment reside_commoners_;
};

class tartifical_
{
public:
	tartifical_();

	void reset_decree_effect();

	int police_speed_;
	int generate_speed_;
	int trade_speed_;
	int business_speed_;
	int business_income_ratio_;
	int technology_speed_;
	int technology_income_ratio_;
	int revenue_income_;
	int destruct_hp_;
	int heal_hp_;
};

class artifical : public unit, public tartifical_
{
public:
	artifical(const artifical& cobj);
	artifical(game_state& state, const config &cfg);
	artifical(const uint8_t* mem);
	artifical(unit_map& units, hero_map& heros, std::vector<team>& teams, game_state& state, type_heros_pair& t, int cityno, bool use_traits);
	~artifical();

	const SDL_Rect& alert_rect() const { return alert_rect_; }

	std::vector<unit*>& reside_troops() {return reside_troops_;}
	const std::vector<unit*>& reside_troops() const {return reside_troops_;}

	std::vector<unit*>& field_troops() {return field_troops_;}
	const std::vector<unit*>& field_troops() const {return field_troops_;}

	std::vector<unit*>& reside_commoners() {return reside_commoners_;}
	const std::vector<unit*>& reside_commoners() const {return reside_commoners_;}

	std::vector<unit*>& field_commoners() {return field_commoners_;}
	const std::vector<unit*>& field_commoners() const {return field_commoners_;}

	std::vector<artifical*>& field_arts() {return field_arts_;}
	const std::vector<artifical*>& field_arts() const {return field_arts_;}

	std::vector<hero*>& fresh_heros() {return fresh_heros_;}
	const std::vector<hero*>& fresh_heros() const {return fresh_heros_;}

	std::vector<hero*>& finish_heros() {return finish_heros_;}
	const std::vector<hero*>& finish_heros() const {return finish_heros_;}

	std::vector<hero*>& wander_heros() {return wander_heros_;}
	const std::vector<hero*>& wander_heros() const {return wander_heros_;}

	std::vector<map_location>& economy_area() {return economy_area_;}
	const std::vector<map_location>& economy_area() const {return economy_area_;}

	std::vector<SDL_Rect>& district() {return district_;}
	const std::vector<SDL_Rect>& district() const {return district_;}
	const std::set<map_location>& district_locs() const;
	
	bool terrain_matches(t_translation::t_terrain tcode) const
		{ return terrain_types_list_.empty()? false: t_translation::terrain_matches(tcode, terrain_types_list_); }

	t_translation::t_list& terrain_types_list() { return terrain_types_list_; }

	const t_translation::t_terrain& terrain() const { return terrain_; }

	bool troop_come_into(unit* troop, int pos = -1);
	bool troop_come_into2(unit* troop, int pos = -1);

	// 一支野外部队/建筑物将归属本城市
	void unit_belong_to(unit* troop, bool loyalty = true, bool to_recorder = false);
	// 一支城内部队出城
	void troop_go_out(const unit& u, bool del = true);

	void commoner_go_out(const unit& u, bool del);

	// add unit to field
	void field_troops_add(unit* troop);
	void field_commoners_add(unit* commoner);
	void field_arts_add(artifical* troop);

	// add unit to reside
	void field_troops_erase(const unit* troop);
	void field_commoners_erase(const unit* commoner);
	void field_arts_erase(const artifical* art);

	// 数个武将出城
	void heros_go_out(const std::vector<hero*>& heros);
	// 一个武将出城
	void hero_go_out(const hero& h);
	// 解散城内一支部队
	void disband(const int index_of_army);
	void decrease_level();
	void fallen(int a_side, unit* attacker = NULL);

	void fort_fallen(int a_side);

	void independence(bool independenced, team& to_team, artifical* rpg_city, team& from_team, artifical* aggressing, hero* from_leader, unit* from_leader_unit);
	void change_to_special_unit(game_display& gui, int attacker_side, int random);
	// whether surrounded or not
	bool is_surrounded() const;
	bool is_front(bool must_enemy = false) const;
	std::vector<artifical*> adjacent_other_cities(bool must_enemy = false) const;

	bool independence_vote(const artifical* aggressing) const;

	void fresh_into(hero& h);
	void fresh_into(const unit* troop);
	void finish_into(hero& h, uint8_t status);
	void wander_into(const unit* troop, bool dialog = true);
	void wander_into(hero& h, bool dialog = true);
	void move_into(hero& h);

	void finish_2_fresh();

	bool select_mayor(hero* commend = NULL, bool dialog = true);
	// must not return NULL, should &hero_invalid.
	hero* mayor() const { return mayor_; }

	void calculate_exploiture();
	int commercial_exploiture() const { return commercial_exploiture_; }
	int technology_exploiture() const { return technology_exploiture_; }
	void active_exploiture();
	std::pair<bool, bool> calculate_feature() const;

	int gold_bonus() const { return gold_bonus_; }
	void add_gold_bonus(int bonus) { gold_bonus_ += bonus; }
	int technology_bonus() const { return technology_bonus_; }
	void add_technology_bonus(int bonus) { technology_bonus_ += bonus; }
	int max_commoners() const { return max_commoners_; }

	void calculate_ea_tiles(std::vector<const map_location*>& ea_vacants, int& markets, int& technologies, int& tactics) const;
	void calculate_ea_tiles(int& markets, int& technologies, int& tactics) const;
	const unit_type* next_ea_utype(std::vector<const map_location*>& ea_vacants, const std::set<const unit_type*>& can_build_ea) const;
	artifical* tactic_on_ea() const;
	void demolish_ea(const std::set<const unit_type*>& can_build_ea);

	//
	// override
	//
	/** draw a unit.  */
	void redraw_unit();
	bool new_turn(play_controller& controller, int random);
	void advance_to(const unit_type *t, bool use_traits = false, game_state *state = 0);

	void fill_ai_hero(std::set<int>& candidate, int count, int& random);
	void fill_human_hero(const hero& leader, std::set<int>& candidate, int fill_freshes, int fill_wanders, int& random);
	hero* select_leader(int random) const;

	void read(game_state& state, const config& cfg, bool use_traits=true);
	void write(config& cfg) const;

	void set_location(const map_location &loc);
	void issue_decree(const config& effect);
	void apply_issur_decree(const tdecree& d, bool anim);
	void clear_decree();

	void set_resting(bool rest);
	int upkeep() const;
	void get_experience(const increase_xp::ublock& ub, int xp, bool master = true, bool second = true, bool third = true);

	void extract_heros_number();
	void recalculate_heros_pointer();

	int office_heros() const;

	int fronts() const { return fronts_; }
	void inching_fronts(bool increase);
	void set_fronts(int fronts);

	int police() const { return police_; }
	void increase_police(int increase);
	void set_police(int val);

	int soldiers() const { return soldiers_; }
	void set_soldiers(int val);
	int current_soldiers() const;

	const tdecree* decree() const { return decree_; }
	int max_level() const { return max_level_; }
	
	void write(uint8_t* mem) const;
	void read(const uint8_t* mem);

	const std::set<map_location>& villages() const;
	const std::set<int>& roaded_cities() const;
	void set_roaded_cities(const std::set<int>& cities);

	int total_gold_income(int market_increase) const;
	int total_technology_income(int technology_increase) const;

	void demolish_to(artifical& to, int damage, int random);
	void ally_terminate_adjust(team& adjusting_team);

	// recruit
	int max_recruit_cost();
	void reset_max_recruit_cost();
	const std::vector<const unit_type*>& recruits(int level);
	void insert_level_unit_type(int level, const unit_type* from, std::vector<const unit_type*>& to);
private:
	// notice: loading game, gui_ isn't ready when artifical::artifical.
	gamemap& map_;
	
	t_translation::t_list terrain_types_list_;
	
	std::vector<unit*> reside_troops_;
	std::vector<unit*> field_troops_;
	std::vector<unit*> reside_commoners_;
	std::vector<unit*> field_commoners_;
	std::vector<artifical*> field_arts_;
	std::vector<hero*> fresh_heros_;
	std::vector<hero*> finish_heros_;
	std::vector<hero*> wander_heros_;
	std::vector<map_location> economy_area_;
	std::vector<SDL_Rect> district_;
	std::set<map_location> district_locs_;
	SDL_Rect alert_rect_;

	std::vector<uint16_t> fresh_heros_number_;
	std::vector<uint16_t> finish_heros_number_;

	std::set<map_location> villages_;
	std::set<int> roaded_cities_;

	hero* mayor_;
	int fronts_;
	int police_;
	int soldiers_;
	const tdecree* decree_;
	int research_turns_;
	int max_level_;

	int commercial_exploiture_;
	int technology_exploiture_;
	int gold_bonus_;
	int technology_bonus_;
	int max_commoners_;

	// recruit
	std::set<const unit_type*> not_recruit_;
	std::map<int, std::vector<const unit_type*> > can_recruit_map_;
	int max_recruit_cost_;
};

#endif // __ARTIFICAL_H_