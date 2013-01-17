#ifndef __ARTIFICAL_H_
#define __ARTIFICAL_H_

#include "config.hpp"
#include "map_location.hpp"
#include "unit.hpp"

class game_display;

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
	unit_segment fresh_heros_;
	unit_segment wander_heros_;
	unit_segment finish_heros_;
	unit_segment economy_area_;
	unit_segment district_;
	unit_segment not_recruit_;
	unit_segment reside_troops_;	
};

class artifical : public unit
{
public:
	artifical(const artifical& cobj);
	artifical(const config &cfg);
	artifical(const uint8_t* mem);
	artifical(unit_map& units, hero_map& heros, type_heros_pair& t, int cityno, bool use_traits);
	~artifical();

	const SDL_Rect& alert_rect() const { return alert_rect_; }

	std::vector<unit*>& reside_troops() {return reside_troops_;}
	const std::vector<unit*>& reside_troops() const {return reside_troops_;}

	std::vector<unit*>& field_troops() {return field_troops_;}
	const std::vector<unit*>& field_troops() const {return field_troops_;}

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

	// 一支部队进城
	// @disband: 是否要自动解散该部队
	void troop_come_into(unit* troop, int pos = -1, bool create = true);
	// 一支野外部队/建筑物将归属本城市
	void unit_belong_to(unit* troop, bool loyalty = true, bool to_recorder = false);
	// 一支城内部队出城
	void troop_go_out(const int index_of_army, bool del = true);

	// 向野外部队add一部队
	void field_troops_add(unit* troop);
	// 向野外建筑物add一建筑物
	void field_arts_add(artifical* troop);

	// 从野外部队中erase一部队
	void field_troops_erase(const unit* troop);
	// 从野外建筑物中erase一部队
	void field_arts_erase(const artifical* art);

	// 数个武将出城
	void heros_go_out(const std::vector<hero*>& heros);
	// 一个武将出城
	void hero_go_out(const hero& h);
	// 解散城内一支部队
	void disband(const int index_of_army);
	// 沦陷
	void fallen(int a_side, unit* attacker = NULL);
	void independence(bool independenced, team& to_team, artifical* rpg_city, team& from_team, artifical* aggressing, hero* from_leader, unit* from_leader_unit);
	// 判断是否被包围
	bool is_surrounded() const;

	bool independence_vote(const artifical* aggressing) const;

	void fresh_into(hero& h);
	void fresh_into(const unit* troop);
	void wander_into(const unit* troop, bool dialog = true);
	void wander_into(hero& h, bool dialog = true);
	void move_into(hero& h);

	void select_mayor(hero* commend = NULL, bool dialog = true);
	// must not return NULL, should &hero_invalid.
	hero* mayor() const { return mayor_; }

	void calculate_exploiture();
	int commercial_exploiture() const { return commercial_exploiture_; }
	int technology_exploiture() const { return technology_exploiture_; }
	void active_exploiture();
	std::pair<bool, bool> calculate_feature() const;

	void calculate_ea_tiles(std::vector<const map_location*>& ea_vacants, int& markets, int& technologies);
	//
	// override
	//
	/** draw a unit.  */
	void redraw_unit();
	// 告知下一回轮到它了
	bool new_turn();

	void read(const config& cfg, bool use_traits=true, game_state* state = 0);
	void write(config& cfg) const;

	void set_location(const map_location &loc);

	void set_resting(bool rest);
	int upkeep() const;
	void get_experience(const increase_xp::ublock& ub, int xp, bool master = true, bool second = true, bool third = true);

	void extract_heros_number();
	void recalculate_heros_pointer();

	int office_heros() const;

	int fronts() const { return fronts_; }
	void inching_fronts(bool increase);
	void set_fronts(int fronts);

	void write(uint8_t* mem) const;
	void read(const uint8_t* mem);

	const std::set<map_location>& villages() const;

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

	hero* mayor_;
	int fronts_;

	int commercial_exploiture_;
	int technology_exploiture_;

	// recruit
	std::set<const unit_type*> not_recruit_;
	std::map<int, std::vector<const unit_type*> > can_recruit_map_;
	int max_recruit_cost_;
};

#endif // __ARTIFICAL_H_