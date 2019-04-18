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
	struct request; // m/fetch.h

	static bool operator<(const request &a, const request &b) noexcept;
	static bool operator<(const request &a, const string_view &b) noexcept;
	static bool operator<(const string_view &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern std::set<request, std::less<>> requests;
	extern std::multimap<room::id, request *> rooms;
	extern std::deque<decltype(requests)::iterator> complete;
	extern ctx::context eval_context;
	extern ctx::context request_context;
	extern hookfn<vm::eval &> hook;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;

	static bool timedout(const request &, const time_t &now);
	static string_view select_origin(request &, const string_view &);
	static string_view select_random_origin(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, m::v1::event::opts &);
	static bool start(request &);
	static bool handle(request &);

	template<class... args> static bool submit(const event::id &, const room::id &, const size_t &bufsz = 8_KiB, args&&...);
	static void eval_handle(const decltype(requests)::iterator &);
	static void eval_handle();
	static void eval_worker();
	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();
	static void hook_handler(const event &, vm::eval &);
	static void init();
	static void fini();
}

template<class... args>
bool
ircd::m::fetch::submit(const m::event::id &event_id,
                       const m::room::id &room_id,
                       const size_t &bufsz,
                       args&&... a)
{
	auto it
	{
		requests.lower_bound(string_view(event_id))
	};

	if(it == end(requests) || it->event_id != event_id) try
	{
		it = requests.emplace_hint(it, room_id, event_id, bufsz, std::forward<args>(a)...);
		while(!start(const_cast<request &>(*it)));
		return true;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			m::log, "Failed to start any fetch for %s in %s :%s",
			string_view{event_id},
			string_view{room_id},
			e.what(),
		};

		requests.erase(it);
		return false;
	};

	assert(it->room_id == room_id);
	return false;
}
