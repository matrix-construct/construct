// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::rest::request::request(const rfc3986::uri &uri,
                             opts opts)
{
	if(!opts.remote)
		opts.remote = net::hostport{uri};

	const bool need_alloc
	{
		empty(opts.buf)
		&& (empty(opts.sout.head) || empty(opts.sin.head))
	};

	const unique_mutable_buffer _buf
	{
		need_alloc? 16_KiB: 0_KiB
	};

	if(!empty(_buf))
		opts.buf = _buf;

	window_buffer window
	{
		opts.buf
	};

	if(empty(opts.sout.head))
	{
		assert(opts.remote);
		assert(opts.method);
		http::request
		{
			window,
			host(opts.remote),
			opts.method,
			uri.resource(),
			size(opts.content),
			opts.content_type,
			opts.headers
		};

		opts.sout.head = window.completed();
	}

	if(empty(opts.sout.content))
		opts.sout.content = opts.content;

	if(empty(opts.sin.head))
		opts.sin.head = mutable_buffer{window};

	// server::request will allocate dynamic content
	opts.sin.content = mutable_buffer{};

	assert(opts.remote);
	server::request request
	{
		opts.remote,
		std::move(opts.sout),
		std::move(opts.sin),
		&opts.sopts,
	};

	const auto code
	{
		request.get(opts.timeout)
	};

	if(opts.code)
		*opts.code = code;

	if(opts.out)
		*opts.out = std::move(request.in.dynamic);
	else
		this->out = std::move(request.in.dynamic);

	ret = request.in.content;
}

ircd::rest::request::request(const mutable_buffer &out,
                             const rfc3986::uri &uri,
                             opts opts)
{
	if(!opts.remote)
		opts.remote = net::hostport{uri};

	const bool need_alloc
	{
		empty(opts.buf)
		&& (empty(opts.sout.head) || empty(opts.sin.head))
	};

	const unique_mutable_buffer _buf
	{
		need_alloc? 16_KiB: 0_KiB
	};

	if(!empty(_buf))
		opts.buf = _buf;

	window_buffer window
	{
		opts.buf
	};

	if(empty(opts.sout.head))
	{
		assert(opts.remote);
		assert(opts.method);
		http::request
		{
			window,
			host(opts.remote),
			opts.method,
			uri.resource(),
			size(opts.content),
			opts.content_type,
			opts.headers
		};

		opts.sout.head = window.completed();
	}

	if(empty(opts.sout.content))
		opts.sout.content = opts.content;

	if(empty(opts.sin.head))
		opts.sin.head = mutable_buffer{window};

	if(empty(opts.sin.content))
		opts.sin.content = out;

	if(empty(opts.sin.content))
		opts.sin.content = opts.sin.head;

	assert(opts.remote);
	server::request request
	{
		opts.remote,
		std::move(opts.sout),
		std::move(opts.sin),
		&opts.sopts,
	};

	const auto code
	{
		request.get(opts.timeout)
	};

	if(opts.code)
		*opts.code = code;

	ret = request.in.content;
}
