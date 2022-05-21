// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "media.h"

using namespace ircd;

m::resource
preview_url_resource
{
	"/_matrix/media/r0/preview_url",
	{
		"(11.7.1.5) Get information about a URL for a client"
	}
};

static unique_buffer<mutable_buffer>
request_url(const string_view &urle);

static json::strung
parse_og(const string_view &content);

static m::resource::response
get__preview_url(client &client,
                 const m::resource::request &request)
{
	const auto &url
	{
		request.query.at("url")
	};

	const auto &ts
	{
		request.query.get("ts", milliseconds(-1))
	};

	const unique_buffer<mutable_buffer> content_buffer
	{
		//request_url(url)
	};

	const string_view content
	{
		content_buffer
	};

	const json::strung ogs
	{
		parse_og(content)
	};

	return m::resource::response
	{
		client, json::object{ogs}
	};
}

static m::resource::method
method_get
{
	preview_url_resource, "GET", get__preview_url
};

json::strung
parse_og(const string_view &content)
{
	std::vector<json::member> og;

	return
	{
		og.data(), og.data() + og.size()
	};
}

unique_buffer<mutable_buffer>
request_url(const string_view &urle)
{
	const unique_buffer<mutable_buffer> buf
	{
		24_KiB
	};

	const auto url
	{
		url::decode(buf, urle)
	};

	const rfc3986::uri uri
	{
		url
	};

	const net::hostport remote
	{
		uri
	};

	if(empty(host(remote)) || empty(uri.path))
		throw m::error
		{
			http::BAD_REQUEST, "M_BAD_URL",
			"Required elements are missing from the supplied URL."
		};

	window_buffer window
	{
		buf + size(url)
	};

	http::request
	{
		window, host(remote), "GET", uri.path, 0, {},
		{
			{ "User-Agent", info::user_agent },
		}
	};

	const const_buffer out_head
	{
		window.completed()
	};

	const mutable_buffer in_head
	{
		buf + size(url) + size(out_head)
	};

	server::request::opts sopts;
	sopts.http_exceptions = false;
	server::request request
	{
		remote, { out_head }, { in_head, mutable_buffer{} }, &sopts
	};

	request.wait(seconds(10)); //TODO: conf

	const auto code
	{
		request.get()
	};

	if(code != http::OK)
		return {};

	assert(data(request.in.content) == data(request.in.dynamic));
	return unique_buffer<mutable_buffer>
	{
		std::move(request.in.dynamic)
	};
}
