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
	struct request;

	static bool operator<(const request &a, const request &b) noexcept;
	static bool operator<(const request &a, const string_view &b) noexcept;
	static bool operator<(const string_view &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern std::set<request, std::less<>> requests;
	extern std::multimap<m::room::id, request *> rooms;
	extern std::deque<decltype(requests)::iterator> complete;
	extern ctx::context eval_context;
	extern ctx::context request_context;
	extern hookfn<vm::eval &> hook;
	extern conf::item<bool> enable;

	template<class... args> static void start(const m::event::id &, const m::room::id &, args&&...);
	static void eval_handle(const decltype(requests)::iterator &);
	static void eval_handle();
	static void eval_worker();
	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static void request_cleanup();
	static void request_worker();
	static void hook_handler(const event &, vm::eval &);
	static void init();
	static void fini();
}

/// Fetch entity state
struct ircd::m::fetch::request
:m::v1::event
{
	using is_transparent = void;

	m::room::id::buf room_id;
	m::event::id::buf event_id;
	unique_buffer<mutable_buffer> _buf;
	mutable_buffer buf;
	std::set<std::string, std::less<>> attempted;
	string_view origin;
	time_t started {0};
	time_t last {0};
	time_t finished {0};
	std::exception_ptr eptr;

	void finish();
	void retry();
	bool handle();

	string_view select_origin(const string_view &);
	string_view select_random_origin();
	void start(m::v1::event::opts &);
	void start();

	request(const m::room::id &room_id,
	        const m::event::id &event_id,
	        const mutable_buffer & = {});

	request() = default;
	request(request &&) = delete;
	request(const request &) = delete;
};

template<class... args>
void
ircd::m::fetch::start(const m::event::id &event_id,
                      const m::room::id &room_id,
                      args&&... a)
{
	auto it
	{
		requests.lower_bound(string_view(event_id))
	};

	if(it == end(requests) || it->event_id != event_id) try
	{
		it = requests.emplace_hint(it, room_id, event_id, std::forward<args>(a)...);
		const_cast<request &>(*it).start();
	}
	catch(const std::exception &e)
	{
		log::error
		{
			m::log, "Failed to start fetch for %s in %s :%s",
			string_view{event_id},
			string_view{room_id},
			e.what(),
		};

		requests.erase(it);
		return;
	};

	assert(it->room_id == room_id);
}
