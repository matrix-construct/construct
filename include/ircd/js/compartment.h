/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_JS_COMPARTMENT_H

namespace ircd {
namespace js   {

struct compartment
{
	using closure_our = std::function<void (compartment &)>;
	using closure = std::function<void (JSCompartment *)>;

  private:
	static void handle_iterate(JSRuntime *, void *, JSCompartment *) noexcept;

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

const compartment *our(const JSCompartment *const &);
compartment *our(JSCompartment *const &);

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
