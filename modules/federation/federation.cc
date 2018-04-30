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

	version(const version &) = delete;
	version &operator=(const version &) = delete;
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
feds__version(const m::room::id &room_id, std::ostream &out)
{
	auto reqs
	{
		feds__version(room_id)
	};

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
			out << "+ " << std::setw(40) << std::left << req.origin
			    << " " << string_view{response}
			    << std::endl;
		}
		else cancel(req);
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

	return;
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
			const json::object &response{req};
			out << "+ " << std::setw(40) << std::left << req.origin
			    << " " << string_view{response}
			    << std::endl;
		}
		else cancel(req);
	}
	catch(const std::exception &e)
	{
		out << "- " << req.origin << " " << e.what() << std::endl;
	}

	return;
}
