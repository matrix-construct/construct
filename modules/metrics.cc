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
	"Prometheus Metrics"
};

resource
metrics_resource
{
	"/metrics",
	{
		"Prometheus Metrics"
	}
};

static resource::response
get__metrics(client &,
             const resource::request &);

resource::method
metrics_get
{
	metrics_resource, "GET", get__metrics
};

resource::response
get__metrics(client &client,
             const resource::request &request)
{
	static const size_t buf_max
	{
		4096
	};

	char buf[buf_max];
	std::stringstream out;
	pubsetbuf(out, buf);

	const time_t ts
	{
		ircd::time<milliseconds>()
	};

	out << "aio_requests_total"
	    << ' ' << fs::aio::stats.requests
	    << ' ' << ts
	    << '\n';

	out << "aio_requests_bytes_total"
	    << ' ' << fs::aio::stats.bytes_requests
	    << ' ' << ts
	    << '\n';

	const string_view output
	{
		view(out, buf)
	};

	return resource::response
	{
		client, output, "text/plain", http::OK
	};
}
