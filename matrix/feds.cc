// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::feds
{
	struct request_base;
	template<class T> struct request;
	using request_list = std::list<std::unique_ptr<request_base>>;
	template<class T> using create_closure = std::function<T (request<T> &, const string_view &origin)>;

	template<class T> static request_list for_one(const string_view &origin, const opts &, const closure &, const create_closure<T> &);
	template<class T> static request_list for_each_in_room(const opts &, const closure &, const create_closure<T> &);

	static bool call_user(const closure &closure, const result &result);
	static bool handler(request_list &, const milliseconds &, const closure &);

	static request_list head(const opts &, const closure &);
	static request_list auth(const opts &, const closure &);
	static request_list event(const opts &, const closure &);
	static request_list state(const opts &, const closure &);
	static request_list backfill(const opts &, const closure &);
	static request_list version(const opts &, const closure &);
	static request_list keys(const opts &, const closure &);
	static request_list send(const opts &, const closure &);
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

ircd::m::feds::execute::execute(const vector_view<const opts> &optsv,
                                const closure &closure)
:boolean{true}
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

		case op::send:
			list.splice(list.end(), send(opts, closure));
			continue;

		case op::noop:
			continue;
	}

	milliseconds timeout {0};
	for(const auto &opts : optsv)
		timeout = std::max(opts.timeout, timeout);

	this->boolean::val = handler(list, timeout, closure);
}

ircd::m::feds::request_list
ircd::m::feds::send(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::send::opts v1opts;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::send
		{
			opts.arg[0], opts.arg[1], request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::send>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::keys(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::key::query::opts v1opts;
		v1opts.dynamic = false;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		const m::fed::key::server_key server_key
		{
			opts.arg[0], opts.arg[1]
		};

		return m::fed::key::query
		{
			{&server_key, 1}, request.buf, std::move(v1opts)
		};
	}};

	return opts.room_id?
		for_each_in_room<m::fed::key::query>(opts, closure, make_request):
		for_one<m::fed::key::query>(opts.arg[0], opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::version(const opts &opts,
                       const closure &closure)
{
	static const auto make_request{[]
	(auto &request, const auto &origin)
	{
		m::fed::version::opts opts;
		opts.dynamic = false;
		opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::version
		{
			request.buf, std::move(opts)
		};
	}};

	return for_each_in_room<m::fed::version>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::backfill(const opts &opts,
                        const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::backfill::opts v1opts;
		v1opts.event_id = opts.event_id;
		v1opts.limit = opts.argi[0];
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::backfill
		{
			opts.room_id, request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::backfill>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::state(const opts &opts,
                     const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::state::opts v1opts;
		v1opts.ids_only = opts.arg[0] == "ids";
		v1opts.event_id = opts.event_id;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::state
		{
			opts.room_id, request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::state>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::event(const opts &opts,
                     const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::event::opts v1opts;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::event
		{
			opts.event_id, request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::event>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::auth(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::event_auth::opts v1opts;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::event_auth
		{
			opts.room_id, opts.event_id, request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::event_auth>(opts, closure, make_request);
}

ircd::m::feds::request_list
ircd::m::feds::head(const opts &opts,
                    const closure &closure)
{
	const auto make_request{[&opts]
	(auto &request, const auto &origin)
	{
		m::fed::make_join::opts v1opts;
		v1opts.remote = string_view
		{
			strlcpy{request.origin, origin}
		};

		return m::fed::make_join
		{
			opts.room_id, opts.user_id, request.buf, std::move(v1opts)
		};
	}};

	return for_each_in_room<m::fed::make_join>(opts, closure, make_request);
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
		now<system_point>() + timeout
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
		const unwind remove{[&reqs, &it]
		{
			reqs.erase(it);
		}};

		request_base &req(**it);
		server::request &sreq(dynamic_cast<server::request &>(req)); try
		{
			const auto code{sreq.get()};
			const json::array &array{sreq.in.content};
			const json::object &object{sreq.in.content};
			const result result
			{
				req.opts, req.origin, {}, object, array
			};

			if(!call_user(closure, result))
				return false;
		}
		catch(const std::exception &)
		{
			if(!req.opts->closure_errors && !req.opts->nothrow_closure_retval)
				return false;

			if(!req.opts->closure_errors)
				continue;

			const ctx::exception_handler eptr;
			const std::exception_ptr &eptr_(eptr);
			const result result
			{
				req.opts, req.origin, eptr_
			};

			if(!call_user(closure, result))
				return false;
		}
	}

	return true;
}

bool
ircd::m::feds::call_user(const closure &closure,
                         const result &result)
try
{
	return closure(result);
}
catch(const std::exception &)
{
	assert(result.request);
	if(result.request->nothrow_closure)
		return result.request->nothrow_closure_retval;

	throw;
}


template<class T>
ircd::m::feds::request_list
ircd::m::feds::for_each_in_room(const opts &opts,
                                const feds::closure &closure,
                                const std::function<T (request<T> &, const string_view &origin)> &create_closure)
{
	request_list ret;
	if(!opts.room_id)
		return ret;

	const m::room::origins origins
	{
		opts.room_id
	};

	// Prelink loop
	if(opts.prelink)
		origins.for_each([&opts]
		(const string_view &origin)
		{
			if(opts.exclude_myself && my_host(origin))
				return;

			fed::prelink(origin);
		});

	// Request loop
	origins.for_each([&opts, &ret, &closure, &create_closure]
	(const string_view &origin)
	{
		if(opts.exclude_myself && my_host(origin))
			return;

		const auto errant
		{
			fed::errant(origin)
		};

		if(opts.closure_cached_errors || !errant) try
		{
			ret.emplace_back(std::make_unique<request<T>>(opts, [&create_closure, &origin]
			(auto &request)
			{
				return create_closure(request, origin);
			}));
		}
		catch(const std::exception &)
		{
			if(!opts.closure_cached_errors)
				return;

			feds::result result;
			result.request = &opts;
			result.origin = origin;
			result.eptr = std::current_exception();
			const ctx::exception_handler eh;
			m::feds::call_user(closure, result);
		}
	});

	return ret;
}

template<class T>
ircd::m::feds::request_list
ircd::m::feds::for_one(const string_view &origin,
                       const opts &opts,
                       const feds::closure &closure,
                       const std::function<T (request<T> &, const string_view &origin)> &create_closure)
{
	request_list ret;
	if(opts.exclude_myself && my_host(origin))
		return ret;

	const auto errant
	{
		fed::errant(origin)
	};

	if(opts.closure_cached_errors || !errant) try
	{
		ret.emplace_back(std::make_unique<request<T>>(opts, [&create_closure, &origin]
		(auto &request)
		{
			return create_closure(request, origin);
		}));
	}
	catch(const std::exception &e)
	{
		if(!opts.closure_cached_errors)
			return ret;

		feds::result result;
		result.request = &opts;
		result.origin = origin;
		result.eptr = std::current_exception();
		const ctx::exception_handler eh;
		m::feds::call_user(closure, result);
	}

	return ret;
}
