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
#define HAVE_IRCD_JS_GLOBAL_H

namespace ircd {
namespace js   {

class global
:public object
{
	static void handle_trace(JSTracer *, JSObject *) noexcept;

	std::map<std::string, module *> imports;
	object import(module &importer, const string &requesting, const object &that);
	std::unique_ptr<function::native> module_resolve_hook;

  public:
	global(trap &,
	       JSPrincipals *const & = nullptr,
	       JS::CompartmentCreationOptions = {},
	       JS::CompartmentBehaviors = {});

	global(global &&) = default;
	global(const global &) = delete;
	~global() noexcept;
};

} // namespace js
} // namespace ircd
