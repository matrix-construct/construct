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
  protected:
	const std::string parent;
	const std::string _name;                     // don't touch
	JSPropertySpec ps[2];
	JSFunctionSpec fs[2];
	std::unique_ptr<JSClass> _class;
	std::map<persist_string, trap *, persist_string::less> children;

	// Override these to define JS objects in C
	virtual value on_call(object::handle, value::handle, const args &);
	virtual value on_set(object::handle, id::handle, value::handle);
	virtual value on_get(object::handle, id::handle, value::handle);
	virtual void on_add(object::handle, id::handle, value::handle);
	virtual bool on_del(object::handle, id::handle);
	virtual bool on_has(object::handle, id::handle);
	virtual void on_enu(object::handle);
	virtual void on_new(object::handle, object &, const args &);
	virtual void on_gc(JSObject &);

  private:
	void host_exception(const char *fmt, ...) const AFP(2, 3);
	void debug(const char *fmt, ...) const AFP(2, 3);
	void add_this();
	void del_this();

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

	 // Get child by name (NOT PATH)
	const trap &child(const string &name) const;
	trap &child(const string &name);

	// Path is absolute to root
	static trap &find(const string::handle &path);
	static trap &find(const std::string &path);

	operator const JSClass &() const             { return jsclass();                               }
	operator const JSClass *() const             { return &jsclass();                              }

	IRCD_OVERLOAD(prototyped)
	template<class... args> object operator()(prototyped_t, const object &parent, const object &proto, args&&...);
	template<class... args> object operator()(const object &parent, args&&...);

	trap(const std::string &path, const uint &flags = 0, const uint &prop_flags = 0);
	trap(trap &&) = delete;
	trap(const trap &) = delete;
	virtual ~trap() noexcept;
};

extern __thread trap *tree;

template<class... args>
object
trap::operator()(const object &parent,
                 args&&... a)
{
	return operator()(prototyped, parent, object{}, std::forward<args>(a)...);
}

template<class... args>
object
trap::operator()(prototyped_t,
                 const object &parent,
                 const object &parent_proto,
                 args&&... a)
{
	object proto(JS_InitClass(*cx,
	                          parent,
	                          parent_proto,
	                          _class.get(),
	                          nullptr,
	                          0,
	                          ps,
	                          fs,
	                          nullptr,
	                          nullptr));

	return JS_New(*cx, proto, vector<value>{std::forward<args>(a)...});
}

} // namespace js
} // namespace ircd
