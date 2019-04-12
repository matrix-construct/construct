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

resource
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

static resource::response
get__preview_url(client &client,
                 const resource::request &request)
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

	return resource::response
	{
		client, json::object{ogs}
	};
}

static resource::method
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

	const auto &host
	{
		between(url, "://", "/")  //TODO: grammar / rfc3986.h
	};

	const string_view &service
	{
		split(url, "://").first
	};

	const string_view &path
	{
		end(host), end(url)
	};

	if(empty(host) || empty(path))
		throw m::error
		{
			http::BAD_REQUEST, "M_BAD_URL",
			"Something was wrong with the supplied URL."
		};

	window_buffer wb
	{
		buf + size(url)
	};

	http::request
	{
		wb, host, "GET", path, 0, {},
		{
			{ "User-Agent", info::user_agent },
		}
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	const mutable_buffer in_head
	{
		buf + size(url) + size(out_head)
	};

	const net::hostport remote
	{
		//TODO: fix services translation in net::dns::
		host, {}, service == "https"? uint16_t(443) : uint16_t(80)
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
