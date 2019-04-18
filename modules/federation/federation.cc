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
	template<class T>
	struct request;

	template<class T>
	static bool
	handler(const opts &, const closure &, std::list<request<T>> &);

	template<class T>
	static std::list<request<T>>
	creator(const opts &, const std::function<T (request<T> &, const string_view &origin)> &);
}

template<class T>
struct ircd::m::feds::request
:T
{
	char origin[256];
	char buf[8_KiB];

	request(const std::function<T (request &)> &closure)
	:T(closure(*this))
	{}

	request(request &&) = delete;
	request(const request &) = delete;
	~request() noexcept
	{
		if(this->valid())
		{
			server::cancel(*this);
			this->wait();
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
//
// m/feds.h
//

bool
IRCD_MODULE_EXPORT
ircd::m::feds::perspective(const opts &opts,
                           const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::key::query::opts v1opts;
		v1opts.dynamic = false;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		const m::v1::key::server_key server_key
		{
			opts.arg[0], opts.arg[1]
		};

		return m::v1::key::query
		{
			{&server_key, 1}, request.buf, std::move(v1opts)
		};
	}};

	auto requests
	{
		creator<m::v1::key::query>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

bool
IRCD_MODULE_EXPORT
ircd::m::feds::version(const opts &opts,
                       const closure &closure)
{
	static const auto make_request{[]
	(auto &request, const auto &origin)
	{
		m::v1::version::opts opts;
		opts.dynamic = false;
		opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::version
		{
			request.buf, std::move(opts)
		};
	}};

	auto requests
	{
		creator<m::v1::version>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

bool
IRCD_MODULE_EXPORT
ircd::m::feds::backfill(const opts &opts,
                        const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::backfill::opts v1opts;
		v1opts.dynamic = true;
		v1opts.event_id = opts.event_id;
		v1opts.limit = opts.argi[0];
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::backfill
		{
			opts.room_id, request.buf, std::move(v1opts)
		};
	}};

	auto requests
	{
		creator<m::v1::backfill>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

bool
IRCD_MODULE_EXPORT
ircd::m::feds::state(const opts &opts,
                     const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::state::opts v1opts;
		v1opts.dynamic = true;
		v1opts.ids_only = opts.arg[0] == "ids";
		v1opts.event_id = opts.event_id;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::state
		{
			opts.room_id, request.buf, std::move(v1opts)
		};
	}};

	auto requests
	{
		creator<m::v1::state>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

bool
IRCD_MODULE_EXPORT
ircd::m::feds::event(const opts &opts,
                     const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::event::opts v1opts;
		v1opts.dynamic = true;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::event
		{
			opts.event_id, request.buf, std::move(v1opts)
		};
	}};

	auto requests
	{
		creator<m::v1::event>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

bool
IRCD_MODULE_EXPORT
ircd::m::feds::head(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::make_join::opts v1opts;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::make_join
		{
			opts.room_id, opts.user_id, request.buf, std::move(v1opts)
		};
	}};

	auto requests
	{
		creator<m::v1::make_join>(opts, make_request)
	};

	return handler(opts, closure, requests);
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal)
//

template<class T>
std::list<m::feds::request<T>>
ircd::m::feds::creator(const opts &opts,
                       const std::function<T (request<T> &, const string_view &origin)> &closure)
{
	assert(opts.room_id);
	const m::room::origins origins
	{
		opts.room_id
	};

	std::list<m::feds::request<T>> ret;
	origins.for_each([&ret, &closure]
	(const string_view &origin)
	{
		if(!server::errmsg(origin)) try
		{
			ret.emplace_back([&closure, &origin]
			(auto &request)
			{
				return closure(request, origin);
			});
		}
		catch(const std::exception &)
		{
			return;
		}
	});

	return ret;
}

template<class T>
bool
ircd::m::feds::handler(const opts &opts,
                       const closure &closure,
                       std::list<request<T>> &reqs)
{
	const auto when
	{
		now<steady_point>() + opts.timeout
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

		assert(it != end(reqs));
		auto &req{*it}; try
		{
			const auto code{req.get()};
			const json::array &array{req.in.content};
			const json::object &object{req.in.content};
			if(!closure({req.origin, {}, object, array}))
				return false;
		}
		catch(const std::exception &)
		{
			const ctx::exception_handler eptr;
			const std::exception_ptr &eptr_(eptr);
			if(!closure({req.origin, eptr_}))
				return false;
		}

		reqs.erase(it);
	}

	return true;
}
