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
	static conf::item<bool> enable;
	static conf::item<bool> autorun;
	static conf::item<std::string> path;
	static std::set<std::string> bin;

	m::event::idx event_idx;
	std::string feature;
	json::object config;
	json::array arg;
	std::string binpath;
	std::vector<json::string> argv;
	unique_mutable_buffer outbuf;
	exec child;
	id::user::buf user_id;
	id::room::buf room_id;
	id::event::buf event_id;
	hookfn<vm::eval &> room_hook;
	context worker_context;

	void handle_stdin(const event &, const string_view &);
	void handle_room_message(const event &, vm::eval &);
	bool handle_stdout();
	void worker();

	app(const m::event::idx &);
	~app() noexcept;

	static void init(), fini();
};
