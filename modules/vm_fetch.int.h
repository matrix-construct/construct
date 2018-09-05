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

static void _init();
static void _fini();

mapi::header
IRCD_MODULE
{
	"Matrix Virtual Machine: Fetch Unit", _init, _fini
};

// Fetch unit state
namespace ircd::m::vm::fetch
{
	struct request;

	extern ctx::dock dock;
	extern std::set<request, request> fetching;
	extern std::deque<request *> fetched;
	extern ctx::context context;
	extern "C" vm::phase phase;

	// worker stack
	static bool requesting();
	static bool handle();
	static void worker();

	// interface stack
	static request &_fetch(const m::room::id &, const m::event::id &);
	static bool remove(const m::event::id &);
	extern "C" bool prefetch(const m::room::id &, const m::event::id &);
	extern "C" json::object acquire(const m::room::id &, const m::event::id &, const mutable_buffer &);

	// phase stack
	static void enter(m::vm::eval &);
}

/// Fetch entity state
struct ircd::m::vm::fetch::request
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
	ctx::dock dock;

	bool operator()(const request &a, const request &b) const;
	bool operator()(const request &a, const string_view &b) const;
	bool operator()(const string_view &a, const request &b) const;

	void finish();
	void retry();
	void handle();

	string_view select_origin(const string_view &);
	string_view select_random_origin();
	void start(m::v1::event::opts &&);
	void start();

	request(const m::room::id &room_id,
	        const m::event::id &event_id,
	        const mutable_buffer & = {});

	request() = default;
	request(request &&) = delete;
	request(const request &) = delete;
};
