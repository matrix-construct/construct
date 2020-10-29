// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_HOOK_H

/// Hooks allow dynamic functionality to be invoked as a result of an event
/// matching some criteria. Hooks are comprised of two interfacing components:
/// the hook function (callee) and the hook site (caller); these components
/// link and delink to each other during initialization. This hook system is
/// oriented around the m::event structure; every hook function has an m::event
/// as its first argument. An optional second argument can be specified with
/// the template to convey additional payload and options.
///
/// Hook functions and hook sites are constructed out of json::members (pairs
/// of json::value in an initializer list). We refer to this as the "feature."
/// Each member with a name directly corresponding to an m::event property is
/// a match parameter. The hook function is not called if a matching parameter
/// is specified in the feature, but the event input at the hook::site does not
/// match it. Undefined features match everything.
///
/// One can create a hook pair anywhere, simply create a m::hook::site with a
/// feature `{ "name", "myname" }` and a hook::hook with a similar feature
/// `{ "_site", "myname" }` matching the site's name; these objects must have
/// matching templates.
///
namespace ircd::m::hook
{
	IRCD_EXCEPTION(ircd::error, error)

	struct base;
	struct maps;

	template<class data = void> struct hook;
	template<class data = void> struct site;

	template<> struct hook<void>;
	template<> struct site<void>;
}

namespace ircd::m
{
	template<class data = void> using hookfn = m::hook::hook<data>;
}

/// Non-template base class for all hook functions. This is the handler
/// (or callee) component of the hook
///
struct ircd::m::hook::base
:instance_list<base>
{
	struct site;

	json::strung _feature;
	json::object feature;
	m::event matching;
	bool registered {false};
	size_t matchers {0};
	size_t calls {0};
	size_t calling {0};

  public:
	uint id() const;
	site *find_site() const;
	string_view site_name() const;

 protected:
	base(const json::members &);
	base(base &&) = delete;
	base(const base &) = delete;
	virtual ~base() noexcept;
};

/// Non-template base class for all hook sites (dispatcher/caller component)
///
struct ircd::m::hook::base::site
:instance_list<site>
{
	json::strung _feature;
	json::object feature;
	size_t count {0};
	std::unique_ptr<struct maps> maps;
	std::set<base *> hooks;
	size_t matchers {0};
	bool exceptions {true};
	bool interrupts {true};
	size_t calls {0};
	size_t calling {0};

  public:
	uint id() const;
	string_view name() const;

  protected:
	friend class base;
	bool add(base &);
	bool del(base &);

	void match(const event &, const std::function<bool (base &)> &);

	site(const json::members &);
	site(site &&) = delete;
	site(const site &) = delete;
	virtual ~site() noexcept;
};

/// Hook function with no payload; only an m::event argument
template<>
struct ircd::m::hook::hook<void>
final
:base
{
	std::function<void (const m::event &)> function;

 public:
	hook(const json::members &feature, decltype(function) function);
	hook(decltype(function) function, const json::members &feature);
};

/// Hook site for functions with no payload.
template<>
struct ircd::m::hook::site<void>
final
:base::site
{
	void call(hook<void> &, const event &);

  public:
	void operator()(base **const &, const event &);
	void operator()(const event &);

	site(const json::members &feature);
};

/// Hook function with a template class as the payload
template<class data_type>
struct ircd::m::hook::hook
:base
{
	std::function<void (const m::event &, data_type)> function;

 public:
	hook(const json::members &feature, decltype(function) function)
	:base{feature}
	,function{std::move(function)}
	{}

	hook(decltype(function) function, const json::members &feature)
	:base{feature}
	,function{std::move(function)}
	{}
};

/// Hook site for functions with a template class as the payload
template<class data_type>
struct ircd::m::hook::site
:base::site
{
	void call(hook<data_type> &hfn, const event &event, data_type d);

  public:
	void operator()(base **const &, const event &event, data_type d);
	void operator()(const event &event, data_type d);

	site(const json::members &feature)
	:base::site{feature}
	{}
};

template<class data>
void
ircd::m::hook::site<data>::operator()(const event &event,
                                      data d)
{
	base *cur {nullptr};
	operator()(&cur, event, std::forward<data>(d));
}

template<class data>
void
ircd::m::hook::site<data>::operator()(base **const &cur,
                                      const event &event,
                                      data d)
{
	const ctx::uninterruptible::nothrow ui
	{
		!interrupts
	};

	// Iterate all matching hooks
	match(event, [this, &cur, &event, &d]
	(base &base)
	{
		// Indicate which hook we're entering
		const scope_restore entered
		{
			*cur, std::addressof(base)
		};

		auto &hfn
		{
			dynamic_cast<hook<data> &>(base)
		};

		call(hfn, event, d);
		return true;
	});
}

template<class data>
void
ircd::m::hook::site<data>::call(hook<data> &hfn,
                                const event &event,
                                data d)
try
{
	// stats for site
	++calls;
	const scope_count site_calling
	{
		calling
	};

	// stats for hook
	++hfn.calls;
	const scope_count hook_calling
	{
		hfn.calling
	};

	// call hook
	hfn.function(event, d);
}
catch(const ctx::interrupted &e)
{
	if(exceptions && interrupts)
		throw;

	log::logf
	{
		log, interrupts? log::DERROR: log::ERROR,
		"site:%u hook:%u %s error :%s",
		id(),
		hfn.id(),
		string_view{hfn.feature},
		e.what(),
	};
}
catch(const std::exception &e)
{
	if(exceptions)
		throw;

	log::critical
	{
		log, "Unhandled site:%u hook:%u %s error :%s",
		id(),
		hfn.id(),
		string_view{hfn.feature},
		e.what(),
	};
}
