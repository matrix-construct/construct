// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_RESOURCE_RESOURCE_H

namespace ircd
{
	struct client;
	struct resource;
}

/// The target of an HTTP request specified by clients with a path.
///
struct ircd::resource
{
	IRCD_EXCEPTION(ircd::error, error)

	enum flag :uint;
	struct opts;
	struct method;
	struct request;
	struct response;
	struct redirect;

	static log::log log;
	static std::map<string_view, resource *, iless> resources;

	string_view path;
	std::unique_ptr<const struct opts> opts;
	std::map<string_view, method *> methods;
	unique_const_iterator<decltype(resources)> resources_it;
	std::unique_ptr<method> default_method_head;
	std::unique_ptr<method> default_method_options;

	using method_closure = std::function<bool (const method &)>;
	string_view method_list(const mutable_buffer &buf, const method_closure &) const;
	string_view method_list(const mutable_buffer &buf) const;

	response handle_options(client &, const request &) const;
	response handle_head(client &, const request &) const;

  public:
	method &operator[](const string_view &name) const;

	resource(const string_view &path, struct opts);
	resource(const string_view &path);
	resource() = default;
	~resource() noexcept;

	static resource &find(const string_view &path);
};

#include "method.h"
#include "request.h"
#include "response.h"
#include "redirect.h"

enum ircd::resource::flag
:uint
{
	DIRECTORY          = 0x01,
	OVERRIDE_HEAD      = 0x02,
	OVERRIDE_OPTIONS   = 0x04,
};

struct ircd::resource::opts
{
	/// developer's literal description of the resource
	string_view description
	{
		"no description"
	};

	/// flags for the resource
	flag flags
	{
		flag(0)
	};

	/// parameter count limits (DIRECTORY only)
	std::pair<short, short> parc
	{
		0,   // minimum params
		15   // maximum params
	};
};
