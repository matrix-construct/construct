/*
 * charybdis: standing on the shoulders of giant build times
 *
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

#include <js/Initialization.h>                   // JS_Init() / JS_ShutDown()
#include <ircd/js/js.h>

namespace ircd {
namespace js   {

// Location of the thread_local runtext. externs exist in js/runtime.h and js/context.h.
// If these are null, js is not available on your thread.
__thread runtime *rt;
__thread context *cx;

// Location of the main JSRuntime instance. An extern reference to this exists in js/runtime.h.
// It is null until js::init manually constructs (and later destructs) it.
runtime main;

// Location of the default/main JSContext instance. An extern reference to this exists in js/context.h.
// Lifetime similar to main runtime
context mc;

// Logging facility for this submodule with SNOMASK.
struct log::log log
{
	"js", 'J'
};

} // namespace js
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js.h - Without 3rd party (JSAPI) symbols
//

ircd::js::init::init()
{
	log.info("Initializing the JS engine [%s: %s]",
	         "SpiderMonkey",
	         version(ver::IMPLEMENTATION));

	if(!JS_Init())
		throw error("JS_Init(): failure");

	struct runtime::opts runtime_opts;
	struct context::opts context_opts;
	log.info("Initializing the main JS Runtime (main_maxbytes: %zu)",
	         runtime_opts.maxbytes);

	main = runtime(runtime_opts);
	mc = context(main, context_opts);
	log.info("Initialized main JS Runtime and context (version: '%s')",
	         version(mc));
}

ircd::js::init::~init()
noexcept
{
	log.info("Terminating the main JS Runtime");

	// Assign empty objects to free and reset
	mc = context{};
	main = runtime{};

	log.info("Terminating the JS engine");
	JS_ShutDown();
}

const char *
ircd::js::version(const ver &type)
{
	switch(type)
	{
		case ver::IMPLEMENTATION:
			return JS_GetImplementationVersion();

		default:
			throw error("version(): Unknown version type requested");
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/js.h - With 3rd party (JSAPI) symbols
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/trap.h
//

ircd::js::trap::trap(std::string name,
                     const uint32_t &flags)
:_name{std::move(name)}
,_class
{
	this->_name.c_str(),
	flags,
	handle_add,
	handle_del,
	handle_get,
	handle_set,
	handle_enu,
	handle_has,
	nullptr,           // JSConvertOp - Obsolete since SpiderMonkey 44  // 45 = mayResolve?
	handle_dtor,
	handle_call,
	handle_inst,
	handle_ctor,
	handle_trace,
	{ this }           // reserved[0] TODO: ?????????
}
{
}

ircd::js::trap::~trap()
noexcept
{
	// Must run GC here to force reclamation of objects before
	// the JSClass hosted by this trap destructs.
	run_gc(*rt);
}

JSObject *
ircd::js::trap::operator()()
{
	return JS_NewObject(*cx, &_class);
}

JSObject *
ircd::js::trap::operator()(JS::HandleObject proto)
{
	return JS_NewObjectWithGivenProto(*cx, &_class, proto);
}

void
ircd::js::trap::handle_dtor(JSFreeOp *const op,
                            JSObject *const obj)
noexcept try
{
	auto &trap(from(*obj));
	trap.debug("dtor");
}
catch(const jserror &e)
{
	e.set_pending();
	return;
}
catch(const std::exception &e)
{
	auto &trap(from(*obj));
	trap.host_exception("dtor: %s", e.what());
	return;
}

bool
ircd::js::trap::handle_ctor(JSContext *const c,
                            unsigned argc,
                            JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);
	//debug("ctor");

	return false;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	//auto &trap(from(obj));
	//trap.host_exception("call: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_call(JSContext *const c,
                            unsigned argc,
                            JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);
	//debug("call");

	return false;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	//auto &trap(from(obj));
	//trap.host_exception("call: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_enu(JSContext *const c,
                           JS::HandleObject obj)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("enu");
	return trap.on_enu(*obj.get());
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("enu: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_has(JSContext *const c,
                           JS::HandleObject obj,
                           JS::HandleId id,
                           bool *const resolved)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("has: %s", string(id).c_str());
	*resolved = trap.on_has(*obj.get(), id.get());
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("has: '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_del(JSContext *const c,
                           JS::HandleObject obj,
                           JS::HandleId id,
                           JS::ObjectOpResult &res)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("del: %s", string(id).c_str());
	if(trap.on_del(*obj.get(), id.get()))
		res.succeed();

	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("del '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_get(JSContext *const c,
                           JS::HandleObject obj,
                           JS::HandleId id,
                           JS::MutableHandleValue val)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("get: %s", string(id).c_str());
	val.set(trap.on_get(*obj.get(), id.get(), val));
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("get: '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_set(JSContext *const c,
                           JS::HandleObject obj,
                           JS::HandleId id,
                           JS::MutableHandleValue val,
                           JS::ObjectOpResult &res)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("set: %s", string(id).c_str());
	val.set(trap.on_set(*obj.get(), id.get(), val));
	if(!val.isUndefined())
		res.succeed();

	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("get: '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_add(JSContext *const c,
                           JS::HandleObject obj,
                           JS::HandleId id,
                           JS::HandleValue val)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("add: %s", string(id).c_str());
	trap.on_add(*obj.get(), id.get(), val.get());
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("add: '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_inst(JSContext *const c,
                            JS::HandleObject obj,
                            JS::MutableHandleValue val,
                            bool *const has_instance)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug("inst");

	return false;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto &trap(from(obj));
	trap.host_exception("inst: %s", e.what());
	return false;
}

void
ircd::js::trap::handle_trace(JSTracer *const tracer,
                             JSObject *const obj)
noexcept try
{
	auto &trap(from(*obj));
	trap.debug("trace");
}
catch(const jserror &e)
{
	e.set_pending();
	return;
}
catch(const std::exception &e)
{
	auto &trap(from(*obj));
	trap.host_exception("trace: %s", e.what());
	return;
}

ircd::js::trap &
ircd::js::trap::from(const JS::HandleObject &o)
{
	return from(*o.get());
}

ircd::js::trap &
ircd::js::trap::from(const JSObject &o)
{
	auto *const c(JS_GetClass(const_cast<JSObject *>(&o)));
	if(!c)
		std::terminate(); //TODO: exception

	if(!c->reserved[0])
		std::terminate(); //TODO: exception

	return *static_cast<trap *>(c->reserved[0]);  //TODO: ???
}

void
ircd::js::trap::debug(const char *const fmt,
                      ...)
const
{
	va_list ap;
	va_start(ap, fmt);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	log.debug("trap(%p) \"%s\": %s",
	          reinterpret_cast<const void *>(this),
	          name().c_str(),
	          buf);

	va_end(ap);
}

void
ircd::js::trap::host_exception(const char *const fmt,
                               ...)
const
{
	va_list ap;
	va_start(ap, fmt);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	log.error("trap(%p) \"%s\": %s",
	          reinterpret_cast<const void *>(this),
	          name().c_str(),
	          buf);

	JS_ReportError(*cx, "BUG: trap(%p) \"%s\" %s",
	               reinterpret_cast<const void *>(this),
	               name().c_str(),
	               buf);

	va_end(ap);
}

bool
ircd::js::trap::on_ctor(const unsigned &argc,
                        JS::Value &argv)
{
	return false;
}

bool
ircd::js::trap::on_call(const unsigned &argc,
                        JS::Value &argv)
{
	return false;
}

bool
ircd::js::trap::on_enu(const JSObject &obj)
{
	return false;
}

bool
ircd::js::trap::on_has(const JSObject &obj,
                       const jsid &id)
{
	return false;
}

bool
ircd::js::trap::on_del(const JSObject &obj,
                       const jsid &id)
{
	return false;
}

JS::Value
ircd::js::trap::on_get(const JSObject &obj,
                       const jsid &id,
                       const JS::Value &val)
{
	return val;
}

JS::Value
ircd::js::trap::on_set(const JSObject &obj,
                       const jsid &id,
                       const JS::Value &val)
{
	return val;
}

JS::Value
ircd::js::trap::on_add(const JSObject &obj,
                       const jsid &id,
                       const JS::Value &val)
{
	return val;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/string.h
//

inline std::u16string
ircd::js::string_convert(const std::string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

    return converter.from_bytes(s);
}

inline std::u16string
ircd::js::string_convert(const char *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

    return s? converter.from_bytes(s) : std::u16string{};
}

inline std::string
ircd::js::string_convert(const std::u16string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

    return converter.to_bytes(s);
}

inline std::string
ircd::js::string_convert(const char16_t *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

    return s? converter.to_bytes(s) : std::string{};
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/error.h
//

ircd::js::error_handler::error_handler(closure &&handler)
:theirs{rt->error_handler}
,handler{std::move(handler)}
{
	rt->error_handler = this;
}

ircd::js::error_handler::error_handler(const closure &handler)
:theirs{rt->error_handler}
,handler{handler}
{
	rt->error_handler = this;
}

ircd::js::error_handler::~error_handler()
noexcept
{
	assert(rt->error_handler == this);
	rt->error_handler = theirs;
}

ircd::js::jserror::jserror(generate_skip_t)
:ircd::js::error(generate_skip)
{
}

ircd::js::jserror::jserror(const char *const fmt,
                           ...)
:ircd::js::error(generate_skip)
{
	va_list ap;
	va_start(ap, fmt);
	generate(JSEXN_ERR, fmt, ap);
	va_end(ap);
}

void
ircd::js::jserror::set_pending()
const
{
	JS::RootedValue ex(*cx, create_error());
	JS_SetPendingException(*cx, ex);
}

JS::Value
ircd::js::jserror::create_error(const JS::HandleObject &stack,
                                const JS::HandleString &file,
                                const std::pair<uint, uint> &linecol)
const
{
	JS::RootedValue ret(*cx);
	JS::RootedString msg(*cx);
	const auto type((JSExnType)report.exnType);
	const auto &line(linecol.first);
	const auto &col(linecol.second);
	if(!JS::CreateError(*cx, type, stack, file, line, col, const_cast<JSErrorReport *>(&report), msg, &ret))
		throw error("Failed to construct jserror exception!");

	return ret;
}

JS::Value
ircd::js::jserror::create_error()
const
{
	JS::RootedObject stack(*cx);
	JS::RootedString file(*cx);
	return create_error(stack, file, {0, 0});
}

void
ircd::js::jserror::generate(const JSExnType &type,
                            const char *const &fmt,
                            va_list ap)
{
	ircd::exception::generate(fmt, ap);
	msg = string_convert(what());
	report.ucmessage = msg.c_str();
	report.exnType = type;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/debug.h
//

std::string
ircd::js::debug(const JSErrorReport &r)
{
	std::stringstream ss;

	if(JSREPORT_IS_WARNING(r.flags))
		ss << "WARNING ";

	if(JSREPORT_IS_EXCEPTION(r.flags))
		ss << "EXCEPTION ";

	if(JSREPORT_IS_STRICT(r.flags))
		ss << "STRICT ";

	if(JSREPORT_IS_STRICT_MODE_ERROR(r.flags))
		ss << "STRICT_MODE_ERROR ";

	if(r.isMuted)
		ss << "MUTED ";

	if(r.filename)
		ss << "file[" << r.filename << "] ";

	if(r.lineno)
		ss << "line[" << r.lineno << "] ";

	if(r.column)
		ss << "col[" << r.column << "] ";

	if(r.linebuf())
		ss << "code[" << r.linebuf() << "] ";

	//if(r.tokenptr)
	//	ss << "toke[" << r.tokenptr << "] ";

	if(r.errorNumber)
		ss << "errnum[" << r.errorNumber << "] ";

	if(r.exnType)
		ss << reflect(JSExnType(r.exnType)) << " ";

	if(r.ucmessage)
		ss << "\"" << string_convert(r.ucmessage) << "\" ";

	for(auto it(r.messageArgs); it && *it; ++it)
		ss << "\"" << string_convert(*it) << "\" ";

	return ss.str();
}

std::string
ircd::js::debug(const JS::HandleObject &o)
{
	std::stringstream ss;

	if(JS_IsGlobalObject(o))    ss << "Global ";
	if(JS_IsNative(o))          ss << "Native ";
	if(JS::IsCallable(o))       ss << "Callable ";
	if(JS::IsConstructor(o))    ss << "Constructor ";

	bool ret;
	if(JS_IsExtensible(*cx, o, &ret) && ret)
		ss << "Extensible ";

	if(JS_IsArrayObject(*cx, o, &ret) && ret)
		ss << "Array ";

	return ss.str();
}

std::string
ircd::js::debug(const JS::Value &v)
{
	std::stringstream ss;

	if(v.isNull())        ss << "Null ";
	if(v.isUndefined())   ss << "Undefined ";
	if(v.isBoolean())     ss << "Boolean ";
	if(v.isTrue())        ss << "TrueValue ";
	if(v.isFalse())       ss << "FalseValue ";
	if(v.isNumber())      ss << "Number ";
	if(v.isDouble())      ss << "Double ";
	if(v.isInt32())       ss << "Int32 ";
	if(v.isString())      ss << "String ";
	if(v.isObject())      ss << "Object ";
	if(v.isSymbol())      ss << "Symbol ";

	return ss.str();
}

const char *
ircd::js::reflect(const JSFinalizeStatus &s)
{
	switch(s)
	{
		case JSFINALIZE_GROUP_START:        return "GROUP_START";
		case JSFINALIZE_GROUP_END:          return "GROUP_END";
		case JSFINALIZE_COLLECTION_END:     return "COLLECTION_END";
	}

	return "????";
}

const char *
ircd::js::reflect(const JSGCStatus &s)
{
	switch(s)
	{
		case JSGC_BEGIN:   return "BEGIN";
		case JSGC_END:     return "END";
	}

	return "????";
}

const char *
ircd::js::reflect(const JSExnType &e)
{
	switch(e)
	{
		case JSEXN_NONE:          return "?NONE?";
		case JSEXN_ERR:           return "Error";
		case JSEXN_INTERNALERR:   return "InternalError";
		case JSEXN_EVALERR:       return "EvalError";
		case JSEXN_RANGEERR:      return "RangeError";
		case JSEXN_REFERENCEERR:  return "ReferenceError";
		case JSEXN_SYNTAXERR:     return "SyntaxError";
		case JSEXN_TYPEERR:       return "TypeError";
		case JSEXN_URIERR:        return "URIError";
		case JSEXN_LIMIT:         return "?LIMIT?";
	}

	return "????";
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/compartment.h
//

ircd::js::compartment::compartment(JSObject *const &obj)
:compartment(obj, *cx)
{
}

ircd::js::compartment::compartment(JSObject *const &obj,
                                   context &c)
:c{&c}
,prev{JS_EnterCompartment(c, obj)}
,ours{JS_EnterCompartment(c, obj)}  // Enter same object compartment again to get its own ptr
,cprev{static_cast<compartment *>(JS_GetCompartmentPrivate(ours))}
{
	JS_SetCompartmentPrivate(ours, this);
}

ircd::js::compartment::compartment(compartment &&other)
noexcept
:c{std::move(other.c)}
,prev{std::move(other.prev)}
,ours{std::move(other.ours)}
,cprev{std::move(other.cprev)}
{
	JS_SetCompartmentPrivate(ours, this);
	other.ours = nullptr;
}

ircd::js::compartment::~compartment()
noexcept
{
	// branch not taken on std::move()
	if(ours)
	{
		JS_SetCompartmentPrivate(ours, cprev);
		JS_LeaveCompartment(*c, prev);
	}
}

void
ircd::js::for_each_compartment_our(const compartment::closure_our &closure)
{
	for_each_compartment([&closure]
	(JSCompartment *const &c)
	{
		if(our(c))
			closure(*our(c));
	});
}

void
ircd::js::for_each_compartment(const compartment::closure &closure)
{
	JS_IterateCompartments(*rt,
	                       const_cast<compartment::closure *>(&closure),
	                       compartment::handle_iterate);
}

void
ircd::js::compartment::handle_iterate(JSRuntime *const rt,
                                      void *const priv,
                                      JSCompartment *const c)
noexcept
{
	const auto &closure(*static_cast<compartment::closure *>(priv));
	closure(c);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/context.h
//

ircd::js::context::context(JSRuntime *const &runtime,
                           const struct opts &opts)
:custom_ptr<JSContext>
{
	JS_NewContext(runtime, opts.stack_chunk_size),
	[opts](JSContext *const ctx)                   //TODO: old gcc/clang can't copy from opts.dtor_gc
	noexcept
	{
		if(!ctx)
			return;

		// Free the user's privdata managed object
		delete static_cast<const privdata *>(JS_GetSecondContextPrivate(ctx));

		if(opts.dtor_gc)
			JS_DestroyContext(ctx);
		else
			JS_DestroyContextNoGC(ctx);
	}
}
,opts{opts}
{
	assert(&our(runtime) == rt);  // Trying to construct on thread without runtime thread_local

	// Use their privdata pointer to point to our instance. We can then use our(JSContext*)
	// to get back to `this` instance. Remember the pointer must change when this class is
	// std::move()'ed etc via the move constructor/assignment.
	JS_SetContextPrivate(get(), this);

	// Assign the thread_local here.
	cx = this;
}

ircd::js::context::context(context &&other)
noexcept
:custom_ptr<JSContext>{std::move(other)}
,opts{std::move(other.opts)}
{
	// Branch not taken for null/defaulted instance of JSContext smart ptr
	if(!!*this)
		JS_SetContextPrivate(get(), this);

	// Ensure the thread_local points here now.
	cx = this;
}

ircd::js::context &
ircd::js::context::operator=(context &&other)
noexcept
{
	static_cast<custom_ptr<JSContext> &>(*this) = std::move(other);
	opts = std::move(other.opts);

	// Branch not taken for null/defaulted instance of JSContext smart ptr
	if(!!*this)
		JS_SetContextPrivate(get(), this);

	// Ensure the thread_local points here now.
	cx = this;

	return *this;
}

ircd::js::context::~context()
noexcept
{
	// Branch not taken on std::move()
	if(!!*this)
		cx = nullptr;
}

ircd::js::context::lock::lock()
:lock{*cx}
{
}

ircd::js::context::lock::lock(context &c)
:c{&c}
{
	JS_BeginRequest(c);
}

ircd::js::context::lock::~lock()
noexcept
{
	JS_EndRequest(*c);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/runtime.h
//

ircd::js::runtime::runtime(const struct opts &opts)
:custom_ptr<JSRuntime>
{
	JS_NewRuntime(opts.maxbytes),
	JS_DestroyRuntime
}
,error_handler{nullptr}
,opts{opts}
{
	// We use their privdata to find `this` via our(JSRuntime*) function.
	// Any additional user privdata will have to ride a member in this class itself.
	JS_SetRuntimePrivate(get(), this);

	// Assign the thread_local runtime pointer here.
	rt = this;

	JS_SetErrorReporter(get(), handle_error);
	JS::SetOutOfMemoryCallback(get(), handle_out_of_memory, nullptr);
	JS::SetLargeAllocationFailureCallback(get(), handle_large_allocation_failure, nullptr);
	JS_SetGCCallback(get(), handle_gc, nullptr);
	JS_AddFinalizeCallback(get(), handle_finalize, nullptr);
	JS_SetCompartmentNameCallback(get(), handle_compartment_name);
	JS_SetDestroyCompartmentCallback(get(), handle_compartment_destroy);
	JS_SetContextCallback(get(), handle_context, nullptr);
	//JS_SetInterruptCallback(get(), nullptr);

	JS_SetNativeStackQuota(get(), opts.code_stack_max, opts.trusted_stack_max, opts.untrusted_stack_max);
}

ircd::js::runtime::runtime(runtime &&other)
noexcept
:custom_ptr<JSRuntime>{std::move(other)}
,error_handler{nullptr}
,opts{std::move(other.opts)}
{
	// Branch not taken for null/defaulted instance of JSRuntime smart ptr
	if(!!*this)
		JS_SetRuntimePrivate(get(), this);

	// Ensure the thread_local runtime always points here
	rt = this;
}

ircd::js::runtime &
ircd::js::runtime::operator=(runtime &&other)
noexcept
{
	static_cast<custom_ptr<JSRuntime> &>(*this) = std::move(other);
	error_handler = std::move(other.error_handler);
	opts = std::move(other.opts);

	// Branch not taken for null/defaulted instance of JSRuntime smart ptr
	if(!!*this)
		JS_SetRuntimePrivate(get(), this);

	// Ensure the thread_local runtime always points here
	rt = this;

	return *this;
}

ircd::js::runtime::~runtime()
noexcept
{
	// Branch not taken on std::move()
	if(!!*this)
		rt = nullptr;
}

bool
ircd::js::runtime::handle_interrupt(JSContext *const ctx)
noexcept
{
	auto &runtime(our(ctx).runtime());
	JS_SetInterruptCallback(runtime, nullptr);
	return false;
}

bool
ircd::js::runtime::handle_context(JSContext *const c,
                                  const uint op,
                                  void *const priv)
noexcept
{
	return true;
}

void
ircd::js::runtime::handle_compartment_destroy(JSFreeOp *const fop,
                                              JSCompartment *const compartment)
noexcept
{
}

void
ircd::js::runtime::handle_compartment_name(JSRuntime *const rt,
                                           JSCompartment *const compartment,
                                           char *const buf,
                                           const size_t max)
noexcept
{
}

void
ircd::js::runtime::handle_finalize(JSFreeOp *const fop,
                                   const JSFinalizeStatus status,
                                   const bool is_compartment,
                                   void *const priv)
noexcept
{
	log.debug("fop(%p): %s %s",
	          (const void *)fop,
	          reflect(status),
	          is_compartment? "COMPARTMENT" : "");
}

void
ircd::js::runtime::handle_gc(JSRuntime *const rt,
                             const JSGCStatus status,
                             void *const priv)
noexcept
{
	log.debug("runtime(%p): GC %s",
	          (const void *)rt,
	          reflect(status));
}

void
ircd::js::runtime::handle_large_allocation_failure(void *const priv)
noexcept
{
	log.error("Large allocation failure");
}

void
ircd::js::runtime::handle_out_of_memory(JSContext *const ctx,
                                        void *const priv)
noexcept
{
	log.error("JSContext(%p): out of memory", (const void *)ctx);
}

void
ircd::js::runtime::handle_error(JSContext *const ctx,
                                const char *const msg,
                                JSErrorReport *const report)
noexcept
{
	if(!rt->error_handler)
	{
		log.error("Unhandled: JSContext(%p): %s [%s]", (const void *)ctx, msg, debug(*report).c_str());
		return;
	}

	assert(report);
	rt->error_handler->handler(msg, *report);
}
