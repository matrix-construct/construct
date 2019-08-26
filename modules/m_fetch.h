// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// Fetch unit state
namespace ircd::m::fetch
{
	static bool operator<(const request &a, const request &b) noexcept;
	static bool operator<(const request &a, const string_view &b) noexcept;
	static bool operator<(const string_view &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern ctx::mutex requests_mutex;
	extern std::set<request, std::less<>> requests;
	extern std::multimap<room::id, request *> rooms;
	extern ctx::context request_context;
	extern conf::item<size_t> requests_max;
	extern conf::item<seconds> auth_timeout;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;
	extern log::log log;

	static bool timedout(const request &, const time_t &now);
	static string_view select_origin(request &, const string_view &);
	static string_view select_random_origin(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, m::v1::event::opts &);
	static bool start(request &);
	static bool handle(request &);

	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();

	template<class... args>
	static ctx::future<result>
	submit(const event::id &,
	       const room::id &,
	       const size_t &bufsz = 8_KiB,
	       args&&...);

	static void init();
	static void fini();
}
