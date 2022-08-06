// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_DBS_INIT_H

struct ircd::m::dbs::init
{
	std::string our_dbpath;
	std::string their_dbpath;

  private:
	static bool (*update[])(void);
	void apply_updates();
	void init_columns();
	void init_database();

  public:
	init(const string_view &servername, std::string dbopts = {});
	~init() noexcept;
};
