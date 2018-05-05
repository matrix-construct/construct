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
	"Federation :General Library and Utils"
};

namespace ircd::m::feds
{
	struct version;
}

struct ircd::m::feds::version
:m::v1::version
{
	using closure = std::function<bool (const string_view &, std::exception_ptr, const json::object &)>;

	char origin[256];
	char buf[16_KiB];

	version(const string_view &origin)
	:m::v1::version{[&]
	{
		m::v1::version::opts opts;
		opts.dynamic = false;
		opts.remote = string_view{this->origin, strlcpy(this->origin, origin)};
		return m::v1::version{mutable_buffer{buf}, std::move(opts)};
	}()}
	{}

	version(version &&) = delete;
	version(const version &) = delete;
};

std::list<m::feds::version>
feds__version(const m::room::id &room_id)
{
	std::list<m::feds::version> reqs;
	m::room::origins{room_id}.for_each([&reqs]
	(const string_view &origin)
	{
		const auto emsg
		{
			ircd::server::errmsg(origin)
		};

		if(!emsg) try
		{
			reqs.emplace_back(origin);
		}
		catch(const std::exception &)
		{
			return;
		}
	});

	return std::move(reqs);
}

extern "C" void
feds__version(const m::room::id &room_id,
              const milliseconds &timeout,
              const m::feds::version::closure &closure)
{
	auto reqs
	{
		feds__version(room_id)
	};

	auto when
	{
		now<steady_point>() + timeout
	};

	while(!reqs.empty())
	{
		auto next
		{
			ctx::when_any(begin(reqs), end(reqs))
		};

		if(!next.wait_until(when, std::nothrow))
			break;

		const auto it
		{
			next.get()
		};

		auto &req{*it}; try
		{
			const auto code{req.get()};
			const json::object &response{req};
			if(!closure(req.origin, {}, response))
				break;
		}
		catch(const std::exception &)
		{
			if(!closure(req.origin, std::current_exception(), {}))
				break;
		}

		reqs.erase(it);
	}
	catch(const std::exception &e)
	{
		out << "- " << std::setw(40) << std::left << req.origin
		    << " " << e.what()
		    << std::endl;
	}
}

extern "C" void
feds__event(const m::event::id &event_id, std::ostream &out)
{
	const m::room::id::buf room_id{[&event_id]
	{
		const m::event::fetch event
		{
			event_id
		};

		return m::room::id::buf{at<"room_id"_>(event)};
	}()};

	struct req
	:m::v1::event
	{
		char origin[256];
		char buf[96_KiB];

		req(const m::event::id &event_id, const string_view &origin)
		:m::v1::event{[&]
		{
			m::v1::event::opts opts;
			opts.dynamic = false;
			opts.remote = string_view{this->origin, strlcpy(this->origin, origin)};
			return m::v1::event{event_id, mutable_buffer{buf}, std::move(opts)};
		}()}
		{}
	};

	std::list<req> reqs;
	const m::room::origins origins{room_id};
	origins.for_each([&out, &event_id, &reqs]
	(const string_view &origin)
	{
		const auto emsg
		{
			ircd::server::errmsg(origin)
		};

		if(emsg)
		{
			out << "! " << origin << " " << emsg << std::endl;
			return;
		}

		try
		{
			reqs.emplace_back(event_id, origin);
		}
		catch(const std::exception &e)
		{
			out << "! " << origin << " " << e.what() << std::endl;
			return;
		}
	});

	auto all
	{
		ctx::when_all(begin(reqs), end(reqs))
	};

	all.wait(30s, std::nothrow);

	for(auto &req : reqs) try
	{
		if(req.wait(1ms, std::nothrow))
		{
			const auto code{req.get()};
			out << "+ " << req.origin << " " << http::status(code) << std::endl;
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << req.origin << " " << e.what() << std::endl;
	}
}

extern "C" void
feds__head(const m::room::id &room_id,
           const m::user::id &user_id,
           std::ostream &out)
{
	struct req
	:m::v1::make_join
	{
		char origin[256];
		char buf[16_KiB];

		req(const m::room::id &room_id, const m::user::id &user_id, const string_view &origin)
		:m::v1::make_join{[this, &room_id, &user_id, &origin]
		{
			m::v1::make_join::opts opts;
			opts.remote = string_view{this->origin, strlcpy(this->origin, origin)};
			return m::v1::make_join{room_id, user_id, mutable_buffer{buf}, std::move(opts)};
		}()}
		{}
	};

	std::list<req> reqs;
	const m::room::origins origins{room_id};
	origins.for_each([&out, &room_id, &user_id, &reqs]
	(const string_view &origin)
	{
		const auto emsg
		{
			ircd::server::errmsg(origin)
		};

		if(emsg)
		{
			out << "! " << origin << " " << emsg << std::endl;
			return;
		}

		try
		{
			reqs.emplace_back(room_id, user_id, origin);
		}
		catch(const std::exception &e)
		{
			out << "! " << origin << " " << e.what() << std::endl;
		}
	});

	auto all
	{
		ctx::when_all(begin(reqs), end(reqs))
	};

	all.wait(30s, std::nothrow);

	for(auto &req : reqs) try
	{
		if(req.wait(1ms, std::nothrow))
		{
			const auto code{req.get()};
			const json::object &response{req};
			const json::object &event{response.at("event")};
			const json::array prev_events
			{
				event.at("prev_events")
			};

			out << "+ " << std::setw(40) << std::left << req.origin;
			out << " " << event.at("depth");
			for(const json::array prev_event : prev_events)
			{
				const auto &prev_event_id
				{
					unquote(prev_event.at(0))
				};

				out << " " << string_view{prev_event_id};
			};
			out << std::endl;
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << req.origin << " " << e.what() << std::endl;
	}
}

extern "C" void
feds__backfill(const m::room::id &room_id,
               const m::event::id &event_id,
               const size_t &limit,
               std::ostream &out)
{
	struct req
	:m::v1::backfill
	{
		char origin[256];
		char buf[16_KiB];

		req(const m::room::id &room_id, const m::event::id &event_id, const string_view &origin, const size_t &limit)
		:m::v1::backfill{[&]
		{
			m::v1::backfill::opts opts;
			opts.remote = string_view{this->origin, strlcpy(this->origin, origin)};
			opts.event_id = event_id;
			opts.limit = limit;
			opts.dynamic = true;
			return m::v1::backfill{room_id, mutable_buffer{buf}, std::move(opts)};
		}()}
		{}
	};

	std::list<req> reqs;
	const m::room::origins origins{room_id};
	origins.for_each([&out, &room_id, &event_id, &limit, &reqs]
	(const string_view &origin)
	{
		const auto emsg
		{
			ircd::server::errmsg(origin)
		};

		if(!emsg) try
		{
			reqs.emplace_back(room_id, event_id, origin, limit);
		}
		catch(const std::exception &e)
		{
			return;
		}
	});

	auto all
	{
		ctx::when_all(begin(reqs), end(reqs))
	};

	all.wait(30s, std::nothrow);

	std::map<string_view, std::set<const req *>> grid;

	for(auto &req : reqs) try
	{
		if(req.wait(1ms, std::nothrow))
		{
			const auto code{req.get()};
			const json::object &response{req};
			const json::array pdus
			{
				response.at("pdus")
			};

			for(const json::object &pdu : pdus)
			{
				const auto &event_id
				{
					unquote(pdu.at("event_id"))
				};

				grid[event_id].emplace(&req);
			}
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << req.origin << " " << e.what() << std::endl;
	}

	size_t i(0);
	for(const auto &p : grid)
		out << i++ << " " << p.first << std::endl;

	for(size_t j(0); j < i; ++j)
		out << "| " << std::left << std::setw(2) << j;
	out << "|" << std::endl;

	for(const auto &req : reqs)
	{
		for(const auto &p : grid)
			out << "| " << (p.second.count(&req)? '+' : ' ') << " ";
		out << "| " << req.origin << std::endl;
	}
}
