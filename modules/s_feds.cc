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
	struct request_base;
	template<class T> struct request;
	using request_list = std::list<std::unique_ptr<request_base>>;
	template<class T> using create_closure = std::function<T (request<T> &, const string_view &origin)>;

	template<class T> static request_list creator(const opts &, const create_closure<T> &);
	static bool handler(request_list &, const milliseconds &, const closure &);

	static request_list head(const opts &, const closure &);
	static request_list auth(const opts &, const closure &);
	static request_list event(const opts &, const closure &);
	static request_list state(const opts &, const closure &);
	static request_list backfill(const opts &, const closure &);
	static request_list version(const opts &, const closure &);
	static request_list keys(const opts &, const closure &);

	bool execute(const vector_view<const opts> &opts, const closure &closure);
}

//
// request_base
//

/// Polymorphic non-template base
struct ircd::m::feds::request_base
{
	const feds::opts *opts {nullptr};
	char origin[256];

	request_base(const feds::opts &opts)
	:opts{&opts}
	{}

	request_base() = default;
	virtual ~request_base() noexcept;
};

ircd::m::feds::request_base::~request_base()
noexcept
{
}

//
// request
//

template<class T>
struct ircd::m::feds::request
:request_base
,T
{
	char buf[8_KiB];

	request(const feds::opts &opts, const std::function<T (request &)> &closure)
	:request_base{opts}
	,T(closure(*this))
	{}

	request(request &&) = delete;
	request(const request &) = delete;
	~request() noexcept;
};

template<class T>
ircd::m::feds::request<T>::~request()
noexcept
{
	if(this->valid())
	{
		server::cancel(*this);
		this->wait();
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// m/feds.h
//

bool
IRCD_MODULE_EXPORT
ircd::m::feds::execute(const vector_view<const opts> &optsv,
                       const closure &closure)
{
	request_list list;
	for(const auto &opts : optsv) switch(opts.op)
	{
		case op::head:
			list.splice(list.end(), head(opts, closure));
			continue;

		case op::auth:
			list.splice(list.end(), auth(opts, closure));
			continue;

		case op::event:
			list.splice(list.end(), event(opts, closure));
			continue;

		case op::state:
			list.splice(list.end(), state(opts, closure));
			continue;

		case op::backfill:
			list.splice(list.end(), backfill(opts, closure));
			continue;

		case op::version:
			list.splice(list.end(), version(opts, closure));
			continue;

		case op::keys:
			list.splice(list.end(), keys(opts, closure));
			continue;

		case op::noop:
			continue;
	}

	milliseconds timeout {0};
	for(const auto &opts : optsv)
		if(opts.timeout > timeout)
			timeout = opts.timeout;

	return handler(list, timeout, closure);
}

ircd::m::feds::request_list
ircd::m::feds::keys(const opts &opts,
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

	return creator<m::v1::key::query>(opts, make_request);
}

ircd::m::feds::request_list
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

	return creator<m::v1::version>(opts, make_request);
}

ircd::m::feds::request_list
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

	return creator<m::v1::backfill>(opts, make_request);
}

ircd::m::feds::request_list
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

	return creator<m::v1::state>(opts, make_request);
}

ircd::m::feds::request_list
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

	return creator<m::v1::event>(opts, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::auth(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::v1::event_auth::opts v1opts;
		v1opts.dynamic = true;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::v1::event_auth
		{
			opts.room_id, opts.user_id, request.buf, std::move(v1opts)
		};
	}};

	return creator<m::v1::event_auth>(opts, make_request);
}

ircd::m::feds::request_list
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

	return creator<m::v1::make_join>(opts, make_request);
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal)
//

bool
ircd::m::feds::handler(request_list &reqs,
                       const milliseconds &timeout,
                       const closure &closure)
{
	const auto when
	{
		now<steady_point>() + timeout
	};

	while(!reqs.empty())
	{
		static const auto dereferencer{[]
		(auto &iterator) -> server::request &
		{
			return dynamic_cast<server::request &>(**iterator);
		}};

		auto next
		{
			ctx::when_any(begin(reqs), end(reqs), dereferencer)
		};

		if(!next.wait_until(when, std::nothrow))
			break;

		const auto it
		{
			next.get()
		};

		assert(it != end(reqs));

		request_base &req(**it);
		server::request &sreq(dynamic_cast<server::request &>(req)); try
		{
			const auto code{sreq.get()};
			const json::array &array{sreq.in.content};
			const json::object &object{sreq.in.content};
			if(!closure({req.opts, req.origin, {}, object, array}))
				return false;
		}
		catch(const std::exception &)
		{
			const ctx::exception_handler eptr;
			const std::exception_ptr &eptr_(eptr);
			if(!closure({req.opts, req.origin, eptr_}))
				return false;
		}

		reqs.erase(it);
	}

	return true;
}

template<class T>
ircd::m::feds::request_list
ircd::m::feds::creator(const opts &opts,
                       const std::function<T (request<T> &, const string_view &origin)> &closure)
{
	assert(opts.room_id);
	const m::room::origins origins
	{
		opts.room_id
	};

	request_list ret;
	origins.for_each([&opts, &ret, &closure]
	(const string_view &origin)
	{
		if(!server::errmsg(origin)) try
		{
			ret.emplace_back(std::make_unique<request<T>>(opts, [&closure, &origin]
			(auto &request)
			{
				return closure(request, origin);
			}));
		}
		catch(const std::exception &)
		{
			return;
		}
	});

	return ret;
}
