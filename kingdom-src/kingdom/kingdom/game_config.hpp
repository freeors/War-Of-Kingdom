#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

#include "rose_config.hpp"

//basic game configuration information is here.
namespace game_config
{
extern std::vector<std::string> foot_speed_prefix;
extern std::string foot_teleport_enter;
extern std::string foot_teleport_exit;

/** observer team name used for observer team chat */
extern const std::string observer_team_name;

/**
	* Return a color corresponding to the value val
	* red for val=0 to green for val=100, passing by yellow.
	* Colors are defined by [game_config] keys
	* red_green_scale and red_green_scale_text
	*/
Uint32 red_to_green(int val, bool for_text = true);
}

#define sum_score(coin, score)	((coin) * game_config::coin_score_rate + (score))

#endif
