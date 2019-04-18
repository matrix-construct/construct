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

	template<class T>
	struct request;

	template<class T>
	static bool
	handler(const opts &, const closure &, std::list<std::unique_ptr<request_base>> &);

	template<class T>
	static std::list<std::unique_ptr<request_base>>
	creator(const opts &, const std::function<T (request<T> &, const string_view &origin)> &);

	static bool head(const opts &, const closure &);
	static bool auth(const opts &, const closure &);
	static bool event(const opts &, const closure &);
	static bool state(const opts &, const closure &);
	static bool backfill(const opts &, const closure &);
	static bool version(const opts &, const closure &);
	static bool keys(const opts &, const closure &);

	bool execute(const vector_view<const opts> &opts, const closure &closure);
}

//
// request_base
//

/// Polymorphic non-template base
struct ircd::m::feds::request_base
{
	const feds::opts *opts {nullptr};

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
	char origin[256];
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
	for(const auto &opts : optsv) switch(opts.op)
	{
		case op::head:         return head(opts, closure);
		case op::auth:         return auth(opts, closure);
		case op::event:        return event(opts, closure);
		case op::state:        return state(opts, closure);
		case op::backfill:     return backfill(opts, closure);
		case op::version:      return version(opts, closure);
		case op::keys:         return keys(opts, closure);
		case op::noop:         break;
	}

	return true;
}

bool
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

	auto requests
	{
		creator<m::v1::key::query>(opts, make_request)
	};

	return handler<m::v1::key::query>(opts, closure, requests);
}

bool
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

	return handler<m::v1::version>(opts, closure, requests);
}

bool
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

	return handler<m::v1::backfill>(opts, closure, requests);
}

bool
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

	return handler<m::v1::state>(opts, closure, requests);
}

bool
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

	return handler<m::v1::event>(opts, closure, requests);
}

bool
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

	auto requests
	{
		creator<m::v1::event_auth>(opts, make_request)
	};

	return handler<m::v1::event_auth>(opts, closure, requests);
}

bool
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

	return handler<m::v1::make_join>(opts, closure, requests);
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal)
//

template<class T>
std::list<std::unique_ptr<m::feds::request_base>>
ircd::m::feds::creator(const opts &opts,
                       const std::function<T (request<T> &, const string_view &origin)> &closure)
{
	assert(opts.room_id);
	const m::room::origins origins
	{
		opts.room_id
	};

	std::list<std::unique_ptr<request_base>> ret;
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

template<class T>
bool
ircd::m::feds::handler(const opts &opts,
                       const closure &closure,
                       std::list<std::unique_ptr<request_base>> &reqs)
{
	const auto when
	{
		now<steady_point>() + opts.timeout
	};

	while(!reqs.empty())
	{
		static const auto dereferencer{[]
		(auto &iterator) -> T &
		{
			return dynamic_cast<T &>(**iterator);
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

		auto &req(dynamic_cast<feds::request<T> &>(**it)); try
		{
			const auto code{req.get()};
			const json::array &array{req.in.content};
			const json::object &object{req.in.content};
			if(!closure({&opts, req.origin, {}, object, array}))
				return false;
		}
		catch(const std::exception &)
		{
			const ctx::exception_handler eptr;
			const std::exception_ptr &eptr_(eptr);
			if(!closure({&opts, req.origin, eptr_}))
				return false;
		}

		reqs.erase(it);
	}

	return true;
}
