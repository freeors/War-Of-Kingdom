#ifndef __RECTANGLE_HPP_
#define __RECTANGLE_HPP_

#include <windows.h>
#include <map>
#include "hero.hpp"

class tzoom_rect
{
public:
	enum {NORMAL = 0, MAX, COUNT};

	enum {status, ddesc, sync, hero, core, visual, campaign};

	static void calculate_maxs(HWND hwnd);

	tzoom_rect(int id, const HWND hwnd);
	virtual void calculate_max(const HWND hwnd);
	virtual void placement(const HWND hwnd, bool max);
protected:
	void place_rect(const HWND hwnd, const RECT& rc);

public:
	int id_;
	RECT normal_;
	RECT max_;
};

class tstatus_rect : public tzoom_rect
{
public:
	tstatus_rect(const HWND hwnd);
};

class tddesc_rect : public tzoom_rect
{
public:
	tddesc_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);

public:
	RECT explorer_[COUNT];
	RECT subarea_[COUNT];
};

class tsync_rect : public tzoom_rect
{
public:
	tsync_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);

public:
	RECT explorer_[COUNT];
	RECT subarea_[COUNT];
};

class thero_rect : public tzoom_rect
{
public:
	thero_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);
public:
	RECT explorer_[COUNT];
};


class tcore_rect : public tzoom_rect
{
public:
	tcore_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);
public:
	RECT explorer_[COUNT];
};

class tvisual_rect : public tzoom_rect
{
public:
	tvisual_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);
public:
	RECT explorer_[COUNT];
};

class tcampaign_rect : public tzoom_rect
{
public:
	tcampaign_rect(const HWND hwnd);
	void calculate_max(const HWND hwnd);
	void placement(const HWND hwnd, bool max);
public:
	RECT explorer_[COUNT];
};

extern std::map<int, tzoom_rect*> zoom_rects;

//
// candidate hero
//
namespace candidate_hero {
extern LPARAM lParam;

void fill_header(HWND hwnd);
void fill_row(HWND hctl, hero& h);
bool on_command(HWND hwnd, int id, UINT codeNotify);
typedef bool (*is_enable_cb)(int id);
bool notify_handler_rclick(const std::map<int, std::string>& menu, HWND hwnd, int id, LPNMHDR lpNMHdr, is_enable_cb fn = NULL);
}

#endif // __RECTANGLE_HPP_