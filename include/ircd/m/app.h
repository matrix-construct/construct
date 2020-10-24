// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	struct app;
};

struct ircd::m::app
:instance_list<ircd::m::app>
{
	static log::log log;
	static conf::item<std::string> path;
	static std::set<std::string> bin;

	m::event::idx event_idx;
	std::string feature;
	json::object config;
	json::array arg;
	std::string binpath;
	std::vector<json::string> argv;
	exec child;
	context worker_context;

	void worker();

	app(const m::event::idx &);
	~app() noexcept;

	static void init(), fini();
};
