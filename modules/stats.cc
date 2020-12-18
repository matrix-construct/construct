// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::stats
{
	static resource::response get_stats(client &, const resource::request &);

	extern resource::method method_get;
	extern resource stats_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Prometheus Metrics"
};

decltype(ircd::stats::stats_resource)
ircd::stats::stats_resource
{
	"/stats",
	{
		"Prometheus Metrics"
	}
};

decltype(ircd::stats::method_get)
ircd::stats::method_get
{
	stats_resource, "GET", get_stats
};

ircd::resource::response
ircd::stats::get_stats(client &client,
                       const resource::request &request)
{
	resource::response::chunked response
	{
		client, http::OK, "text/plain"
	};

	const time_t ts
	{
		ircd::time<milliseconds>()
	};

	for(const auto &item : items)
	{
		char buf[256], name[2][128], val[64];
		const string_view _name
		{
			replace(name[0], item->name, '.', '_')
		};

		const string_view line
		{
			buf,
			::snprintf
			(
				buf, sizeof(buf), "%s %s %lu\n",
				data(strlcpy(name[1], _name)),
				data(string(val, *item)),
				ts
			)
		};

		response.write(line);
	}

	return std::move(response);
}
