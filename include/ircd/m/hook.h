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

namespace ircd::m
{
	struct hook;
}

struct ircd::m::hook
{
	struct site;
	struct list;

	IRCD_EXCEPTION(ircd::error, error)

	struct list static list;

	json::strung _feature;
	json::object feature;
	m::event matching;
	std::function<void (const m::event &)> function;
	bool registered;

	string_view site_name() const;
	bool match(const m::event &) const;

 public:
	hook(const json::members &, decltype(function));
	hook(decltype(function), const json::members &);
	hook(hook &&) = delete;
	hook(const hook &) = delete;
	virtual ~hook() noexcept;
};

/// The hook::site is the call-site for a hook. Each hook site is named
/// and registers itself with the master extern hook::list. Each hook
/// then registers itself with a hook::site. The site contains internal
/// state to manage the efficient calling of the participating hooks.
struct ircd::m::hook::site
{
	json::strung _feature;
	json::object feature;
	bool registered;
	size_t count {0};

	string_view name() const;

	std::multimap<string_view, hook *> origin;
	std::multimap<string_view, hook *> room_id;
	std::multimap<string_view, hook *> sender;
	std::multimap<string_view, hook *> state_key;
	std::multimap<string_view, hook *> type;

	friend class hook;
	bool add(hook &);
	bool del(hook &);

	void call(hook &, const event &);

  public:
	void operator()(const event &);

	site(const json::members &);
	site(site &&) = delete;
	site(const site &) = delete;
	~site() noexcept;
};

/// The hook list is the registry of all of the hook sites. This is a singleton
/// class with an extern instance. Each hook::site will register itself here
/// by human readable name.
struct ircd::m::hook::list
:std::map<string_view, hook::site *>
{
	friend class site;
	bool add(site &);
	bool del(site &);

	friend class hook;
	bool add(hook &);
	bool del(hook &);

  public:
	list() = default;
	list(list &&) = delete;
	list(const list &) = delete;
};
