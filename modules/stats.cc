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
	static const_buffer print_item(stats::item<void> &item, const mutable_buffer &, const time_t &);
	static void each_item(resource::response::chunked &, stats::item<void> &, window_buffer &, const time_t &);
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

	window_buffer buf
	{
		response.buf
	};

	const time_t ts
	{
		ircd::time<milliseconds>()
	};

	for(const auto &item : items)
		each_item(response, *item, buf, ts);

	const auto flushed
	{
		response.flush(buf.completed())
	};

	buf.shift(size(flushed));
	assert(!buf.consumed());
	return response;
}

void
ircd::stats::each_item(resource::response::chunked &response,
                       stats::item<void> &item,
                       window_buffer &buf,
                       const time_t &ts)
{
	buf([&item, &ts]
	(const mutable_buffer &buf)
	{
		return print_item(item, buf, ts);
	});

	if(buf.remaining() >= 1_KiB)
		return;

	const auto completed
	{
		buf.completed()
	};

	const auto flushed
	{
		response.flush(completed)
	};

	buf.shift(size(flushed));
}

ircd::const_buffer
ircd::stats::print_item(stats::item<void> &item,
                        const mutable_buffer &buf,
                        const time_t &ts)
{
	char name[2][128], val[64];
	const string_view _name
	{
		replace(name[0], item.name, '.', '_')
	};

	return string_view
	{
		data(buf), size_t(::snprintf
		(
			data(buf), size(buf), "%s %s %lu\n",
			data(strlcpy(name[1], _name)),
			data(string(val, item)),
			ts
		))
	};
}
