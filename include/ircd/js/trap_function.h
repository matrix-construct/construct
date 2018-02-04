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
#define HAVE_IRCD_JS_TRAP_FUNCTION

namespace ircd {
namespace js   {

struct trap::function
{
	friend struct trap;
	using closure = std::function<value (object::handle, value::handle, const args &)>;

	struct trap *trap;
	std::string name;
	closure lambda;
	JSFunctionSpec spec;

  protected:
	virtual value on_call(object::handle callee, value::handle that, const args &);
	virtual value on_new(object::handle callee, const args &);

  private:
	static function &from(JSObject *const &);

	static bool handle_call(JSContext *, unsigned, JS::Value *) noexcept;

  public:
	js::function operator()(const object::handle &) const;

	function(struct trap &,
	         const std::string &name,
	         const uint16_t &flags = 0,
	         const uint16_t &arity = 0,
	         const closure & = {});

	function(function &&) = delete;
	function(const function &) = delete;
	virtual ~function() noexcept;
};

} // namespace js
} // namespace ircd
