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
#define HAVE_IRCD_JS_COMPARTMENT_H

namespace ircd {
namespace js   {

struct compartment
{
	using closure_our = std::function<void (compartment &)>;
	using closure = std::function<void (JSCompartment *)>;

  private:
	static void handle_iterate(JSContext *, void *, JSCompartment *) noexcept;

	context *c;
	JSCompartment *prev;
	JSCompartment *ours;
	compartment *cprev;

  public:
	explicit operator const context &() const    { return *c;                                      }
	operator const JSCompartment *() const       { return ours;                                    }
	operator const JSCompartment &() const       { return *ours;                                   }

	explicit operator context &()                { return *c;                                      }
	operator JSCompartment *()                   { return ours;                                    }
	operator JSCompartment &()                   { return *ours;                                   }

	compartment(JSObject *const &, context &, const JSVersion & = JSVERSION_LATEST);
	compartment(JSObject *const &, const JSVersion & = JSVERSION_LATEST);
	compartment(context &, const JSVersion & = JSVERSION_LATEST);
	compartment(const JSVersion & = JSVERSION_LATEST);
	compartment(compartment &&) noexcept;
	compartment(const compartment &) = delete;
	~compartment() noexcept;

	friend void for_each_compartment_our(const closure_our &);
	friend void for_each_compartment(const closure &);

	static compartment &get(context &c);
	static compartment &get();
};

// Get our structure from JSCompartment. Returns null when not ours.
const compartment *our(const JSCompartment *const &);
compartment *our(JSCompartment *const &);

// Compartment iterations
void for_each_compartment_our(const compartment::closure_our &);  // iterate our compartments only
void for_each_compartment(const compartment::closure &);          // iterate all compartments

// Get the compartmentalized `this` object.
JSObject *current_global(compartment &);


inline compartment &
compartment::get()
{
	assert(cx);
	return get(*cx);
}

inline compartment &
compartment::get(context &c)
{
	const auto cp(current_compartment(c));
	if(unlikely(!cp))
		throw error("No current compartment on context(%p)", (const void *)&c);

	const auto ret(our(cp));
	if(unlikely(!ret))
		throw error("Current compartment on context(%p) not ours", (const void *)&c);

	return *ret;
}

inline JSObject *
current_global(compartment &c)
{
	return JS_GetGlobalForCompartmentOrNull(static_cast<context &>(c), c);
}

inline compartment *
our(JSCompartment *const &cp)
{
	return static_cast<compartment *>(JS_GetCompartmentPrivate(cp));
}

inline
const compartment *
our(const JSCompartment *const &cp)
{
	return static_cast<const compartment *>(JS_GetCompartmentPrivate(const_cast<JSCompartment *>(cp)));
}

} // namespace js
} // namespace ircd
