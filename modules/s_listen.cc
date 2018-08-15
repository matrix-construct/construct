// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void create_listener(const m::event &);
static void init_listener(const string_view &, const json::object &);
static void init_listeners();
static void on_load();

mapi::header
IRCD_MODULE
{
	"Server listeners", on_load
};

std::list<net::listener>
listeners;

//
// init
//

void
on_load()
{
	if(ircd::nolisten)
	{
		log::warning
		{
			"Not listening on any addresses because nolisten flag is set."
		};

		return;
	}

	init_listeners();
}

void
init_listeners()
{
	m::room::state{m::my_room}.for_each("ircd.listen", []
	(const m::event &event)
	{
		const string_view &name
		{
			at<"state_key"_>(event)
		};

		const json::object &opts
		{
			json::get<"content"_>(event)
		};

		init_listener(name, opts);
	});

	if(listeners.empty())
		log::warning
		{
			"No listening sockets configured; can't hear anyone."
		};
}

void
init_listener(const string_view &name,
              const json::object &opts)
{
	if(!opts.has("tmp_dh_path"))
		throw user_error
		{
			"Listener %s requires a 'tmp_dh_path' in the config. We do not"
			" create this yet. Try `openssl dhparam -outform PEM -out dh512.pem 512`",
			name
		};

	listeners.emplace_back(name, opts, []
	(const auto &sock)
	{
		ircd::add_client(sock);
	});
}

//
//
//

const m::hookfn<>
create_listener_hook
{
	create_listener,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};

void
create_listener(const m::event &)
{

}
