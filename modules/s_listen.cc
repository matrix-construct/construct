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

mapi::header
IRCD_MODULE
{
	"Server listeners"
};

namespace ircd::m
{
	void init_listener(const json::object &config);
}

static void
create_listener(const m::event &)
{
	std::cout << "hi" << std::endl;
}

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
ircd::m::init_listener(const json::object &config)
{
	if(ircd::nolisten)
	{
		log::warning
		{
			"Not listening on any addresses because nolisten flag is set."
		};

		return;
	}

	const json::array listeners
	{
		config[{"ircd", "listen"}]
	};

	for(const auto &name : listeners) try
	{
		const json::object &opts
		{
			config.at({"listen", unquote(name)})
		};

		if(!opts.has("tmp_dh_path"))
			throw user_error
			{
				"Listener %s requires a 'tmp_dh_path' in the config. We do not"
				" create this yet. Try `openssl dhparam -outform PEM -out dh512.pem 512`",
				name
			};
/*
		m::listeners.emplace_back(unquote(name), opts, []
		(const auto &sock)
		{
			add_client(sock);
		});
*/
	}
	catch(const json::not_found &e)
	{
		throw ircd::user_error
		{
			"Failed to find configuration block for listener %s", name
		};
	}
}
