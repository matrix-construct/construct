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
#define HAVE_IRCD_JS_TRAP_H

namespace ircd {
namespace js   {

struct trap
{
	struct property;            // Properties (trap_property.h)
	struct function;            // Functions (trap_function.h)
	struct constint;            // Constant integers (trap_constint.h)

	std::string name;                  // classp name
	trap *prototrap;                   // Parent trap

	// Static specifications
	std::map<std::string, property *> sprop;
	std::map<std::string, function *> sfunc;

	// Member specifications
	std::map<std::string, property *> memprop;
	std::map<std::string, function *> memfunc;

  protected:
	void debug(const void *const &that, const char *fmt, ...) const AFP(3, 4);

	// Override these to define JS objects in C
	virtual value on_call(object::handle, value::handle, const args &);
	virtual value on_set(object::handle, id::handle, value::handle);
	virtual value on_get(object::handle, id::handle, value::handle);
	virtual void on_add(object::handle, id::handle, value::handle);
	virtual bool on_del(object::handle, id::handle);
	virtual bool on_has(object::handle, id::handle);
	virtual void on_enu(object::handle);
	virtual void on_new(object::handle, object &, const args &);
	virtual void on_trace(const JSObject *const &);
	virtual void on_gc(JSObject *const &);

  private: protected:
	void host_exception(const void *const &that, const char *fmt, ...) const AFP(3, 4);

	// Internal callback interface
	static void handle_trace(JSTracer *, JSObject *) noexcept;
	static bool handle_inst(JSContext *, JS::HandleObject, JS::MutableHandleValue, bool *yesno) noexcept;
	static bool handle_add(JSContext *, JS::HandleObject, JS::HandleId, JS::HandleValue) noexcept;
	static bool handle_set(JSContext *, JS::HandleObject, JS::HandleId, JS::MutableHandleValue, JS::ObjectOpResult &) noexcept;
	static bool handle_get(JSContext *, JS::HandleObject, JS::HandleId, JS::MutableHandleValue) noexcept;
	static bool handle_del(JSContext *, JS::HandleObject, JS::HandleId, JS::ObjectOpResult &) noexcept;
	static bool handle_has(JSContext *, JS::HandleObject, JS::HandleId, bool *resolved) noexcept;
	static bool handle_enu(JSContext *, JS::HandleObject) noexcept;
	static bool handle_setter(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_getter(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_call(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_ctor(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static void handle_dtor(JSFreeOp *, JSObject *) noexcept;

	// Aggregate structure specifying internal callback surface
	static const JSClassOps cops;      // Used for regular objects
	static const JSClassOps gcops;     // Used for global objects
	const JSClass classp;              // Instance uses one of the above JSClassOps

  public:
	static trap &from(const JSObject *const &);
	static trap &from(const JSObject &);

	auto &jsclass() const                        { return classp;                                  }
	auto &jsclass()                              { return classp;                                  }

	operator const JSClass &() const             { return jsclass();                               }
	operator const JSClass *() const             { return &jsclass();                              }

	object prototype(const object::handle &globals);
	object construct(const object::handle &globals, const vector<value>::handle &argv = {});
	object construct(const vector<value>::handle &argv = {});
	template<class... args> object operator()(args&&...);

	trap(const std::string &name, const uint &flags = 0, const uint &prop_flags = 0);
	trap(trap &&) = delete;
	trap(const trap &) = delete;
	virtual ~trap() noexcept;
};

} // namespace js
} // namespace ircd

template<class... args>
ircd::js::object
ircd::js::trap::operator()(args&&... a)
{
	const vector<value> argv
	{
		std::forward<args>(a)...
	};

	return construct(argv);
}
