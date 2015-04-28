#ifndef __INTEGRATE_HPP_
#define __INTEGRATE_HPP_

#include "global.hpp"
#include "stdafx.h"
#include <windowsx.h>
#include "config.hpp"
#include "editor.hpp"
#include "help.hpp"

class tformater
{
public:
	tformater(const std::string& name, const std::string& example)
		: name(name)
		, example(example)
	{}

public:
	std::string name;
	std::string example;
};

class tintegrate2
{
public:
	tintegrate2();
	
	HWND init_toolbar(HINSTANCE hinst, HWND hdlgP);
	void refresh(HWND hdlgP);
	bool save(HWND hdlgP);

	void fill_formaters();
	void insert_formater(const std::string& name, const std::string& example);

public:
	HWND htb_integrate;
	std::vector<tformater> formaters;
};

#endif // __INTEGRATE_HPP_
