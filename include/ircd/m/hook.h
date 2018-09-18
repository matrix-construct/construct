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

	string_view site_name() const;
	site *find_site() const;

 protected:
	base(const json::members &);
	base(base &&) = delete;
	base(const base &) = delete;
	virtual ~base() noexcept;
};

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

	friend class base;
	string_view name() const;
	bool add(base &);
	bool del(base &);

	void match(const event &, const std::function<bool (base &)> &);

  protected:
	site(const json::members &);
	site(site &&) = delete;
	site(const site &) = delete;
	virtual ~site() noexcept;
};

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

template<>
struct ircd::m::hook::site<void>
final
:base::site
{
	void call(hook<void> &, const event &);

  public:
	void operator()(const event &);

	site(const json::members &feature);
};

template<class data>
struct ircd::m::hook::hook
:base
{
	std::function<void (const m::event &, data)> function;

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

template<class data>
struct ircd::m::hook::site
:base::site
{
	void call(hook<data> &hfn, const event &event, data d);

  public:
	void operator()(const event &event, data d);

	site(const json::members &feature)
	:base::site{feature}
	{}
};

template<class data>
void
ircd::m::hook::site<data>::operator()(const event &event,
                                      data d)
{
	match(event, [this, &event, &d]
	(base &base)
	{
		call(dynamic_cast<hook<data> &>(base), event, d);
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
	++hfn.calls;
	hfn.function(event, d);
}
catch(const std::exception &e)
{
	if(exceptions)
		throw;

	log::critical
	{
		"Unhandled hookfn(%p) %s error :%s",
		&hfn,
		string_view{hfn.feature},
		e.what()
	};
}
