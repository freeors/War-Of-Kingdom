#ifndef __MULTIPLAYER_HPP_
#define __MULTIPLAYER_HPP_

#include "dlgcampaignproc.hpp"

class tmultiplayer_
{
public:
	tmultiplayer_()
	{
	}

	bool dirty() const;
};

class tmultiplayer: public tmultiplayer_
{
public:
	tmultiplayer()
		: multiplayer_from_cfg_()
	{}

	void from_config(const config& event_cfg);
	void from_ui(HWND hdlgP);
	
	void update_to_ui(HWND hdlgP, int index = -1) const;
	void update_to_ui_event_edit(HWND htv, HTREEITEM branch) const;

	void generate() const;
	void clear();
public:
	tmultiplayer_ multiplayer_from_cfg_;
};

#endif // __MULTIPLAYER_HPP_
