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
#define HAVE_IRCD_JS_TRAP_PROPERTY

namespace ircd {
namespace js   {

struct trap::property
{
	friend struct trap;
	using function = js::function;

	struct trap *trap;
	std::string name;
	JSPropertySpec spec;

	virtual value on_set(function::handle, object::handle, value::handle);
	virtual value on_get(function::handle, object::handle);

  private:
	static bool handle_set(JSContext *c, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_get(JSContext *c, unsigned argc, JS::Value *argv) noexcept;

  public:
	property(struct trap &, const std::string &name, const uint &flags = 0);
	property(property &&) = delete;
	property(const property &) = delete;
	virtual ~property() noexcept;
};

} // namespace js
} // namespace ircd
