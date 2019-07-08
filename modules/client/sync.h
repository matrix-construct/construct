// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	struct args;
	struct stats;
	struct data;
	struct response;

	extern const string_view description;
	extern resource resource;
	extern resource::method method_get;

	extern conf::item<size_t> flush_hiwat;
	extern conf::item<size_t> buffer_size;
	extern conf::item<size_t> linear_buffer_size;
	extern conf::item<size_t> linear_delta_max;
	extern conf::item<bool> longpoll_enable;
	extern conf::item<bool> polylog_phased;
	extern conf::item<bool> polylog_only;

	static const_buffer flush(data &, resource::response::chunked &, const const_buffer &);
	static void empty_response(data &, const uint64_t &next_batch);
	static bool linear_handle(data &);
	static bool polylog_handle(data &);
	static bool longpoll_handle(data &);
	static resource::response handle_get(client &, const resource::request &);
}

#include "sync/args.h"
