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
#define HAVE_IRCD_JS_FUNCTION_NATIVE

namespace ircd {
namespace js   {

struct function::native
:root<JSFunction *>
{
	using closure = std::function<value (object::handle, value::handle, const args &)>;

	const char *_name;
	closure lambda;

  protected:
	virtual value on_call(object::handle callee, value::handle that, const args &);
	virtual value on_new(object::handle callee, const args &);

  private:
	static function::native &from(JSObject *const &);
	static bool handle_call(JSContext *, unsigned, JS::Value *) noexcept;

  public:
	uint16_t arity() const;
	string display_name() const;
	string name() const;

	value operator()(const object::handle &, const vector<value>::handle &) const;
	template<class... args> value operator()(const object::handle &, args&&...) const;

	native(const char *const &name   = "<native>",
	       const uint &flags         = 0,
	       const uint &arity         = 0,
	       const closure &           = {});

	native(native &&) = delete;
	native(const native &) = delete;
	virtual ~native() noexcept;
};

template<class... args>
value
function::native::operator()(const object::handle &that,
                             args&&... a)
const
{
	const vector<value> argv
	{
		std::forward<args>(a)...
	};

	const vector<value>::handle handle(argv);
	return operator()(that, handle);
}

} // namespace js
} // namespace ircd
