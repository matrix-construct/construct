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

class trap
{
	const std::string _name;                     // don't touch
	std::unique_ptr<JSClass> _class;

	// Override these to define JS objects in C
	virtual JS::Value on_add(const JSObject &, const jsid &, const JS::Value &);
	virtual JS::Value on_set(const JSObject &, const jsid &, const JS::Value &);
	virtual JS::Value on_get(const JSObject &, const jsid &, const JS::Value &);
	virtual bool on_del(const JSObject &, const jsid &);
	virtual bool on_has(const JSObject &, const jsid &);
	virtual bool on_enu(const JSObject &);
	virtual value on_call(const JSObject &, const JS::CallArgs &);
	virtual void on_ctor(object &, const JS::CallArgs &);

  private:
	void host_exception(const char *fmt, ...) const AFP(2, 3);
	void debug(const char *fmt, ...) const AFP(2, 3);

	static trap &from(const JSObject &);
	static trap &from(const JS::HandleObject &);

	// Internal callback interface
	static void handle_trace(JSTracer *, JSObject *) noexcept;
	static bool handle_inst(JSContext *, JS::HandleObject, JS::MutableHandleValue, bool *yesno) noexcept;
	static bool handle_add(JSContext *, JS::HandleObject, JS::HandleId, JS::HandleValue) noexcept;
	static bool handle_set(JSContext *, JS::HandleObject, JS::HandleId, JS::MutableHandleValue, JS::ObjectOpResult &) noexcept;
	static bool handle_get(JSContext *, JS::HandleObject, JS::HandleId, JS::MutableHandleValue) noexcept;
	static bool handle_del(JSContext *, JS::HandleObject, JS::HandleId, JS::ObjectOpResult &) noexcept;
	static bool handle_has(JSContext *, JS::HandleObject, JS::HandleId, bool *resolved) noexcept;
	static bool handle_enu(JSContext *, JS::HandleObject) noexcept;
	static bool handle_call(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_ctor(JSContext *, unsigned argc, JS::Value *argv) noexcept;
	static void handle_dtor(JSFreeOp *, JSObject *) noexcept;

  public:
	auto &name() const                           { return _name;                                   }
	auto &jsclass() const                        { return *_class;                                 }

	operator const JSClass &() const             { return jsclass();                               }
	operator const JSClass *() const             { return &jsclass();                              }

	object operator()(const object &proto);
	object operator()();

	trap(std::string name, const uint32_t &flags = 0);
	trap(trap &&) = delete;
	trap(const trap &) = delete;
	virtual ~trap() noexcept;
};

} // namespace js
} // namespace ircd
