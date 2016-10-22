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

// Location of the main JSRuntime and JSContext instances.
runtime *main_runtime;
context *main_context;

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

	const scope exit([this]
	{
		// Ensure ~init() is always safe to call at any intermediate state
		if(std::current_exception())
			this->~init();
	});

	if(!JS_Init())
		throw error("JS_Init(): failure");

	struct runtime::opts runtime_opts;
	struct context::opts context_opts;
	log.info("Initializing the main JS Runtime (main_maxbytes: %zu)",
	         runtime_opts.maxbytes);

	main_runtime = new runtime(runtime_opts);
	main_context = new context(*main_runtime, context_opts);
	log.info("Initialized main JS Runtime and context (version: '%s')",
	         version(*main_context));

	{
		const std::lock_guard<context> lock{*main_context};
		ircd::mods::load("kernel");
	}
}

ircd::js::init::~init()
noexcept
{
	if(main_context && !!*main_context) try
	{
		const std::lock_guard<context> lock{*main_context};
		ircd::mods::unload("kernel");
	}
	catch(const std::exception &e)
	{
		log.warning("Failed to unload the kernel: %s", e.what());
	}

	log.info("Terminating the JS Main Runtime");
	delete main_context;
	delete main_runtime;

	log.info("Terminating the JS Engine");
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

void
__attribute__((noreturn))
js::ReportOutOfMemory(ExclusiveContext *const c)
{
	ircd::js::log.critical("jsalloc(): Reported out of memory (ExclusiveContext: %p)", (const void *)c);
	std::terminate();
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
	handle_add, // flags & JSCLASS_GLOBAL_FLAGS? nullptr : handle_add,
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
	flags & JSCLASS_GLOBAL_FLAGS? JS_GlobalObjectTraceHook : handle_trace,
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

ircd::js::object
ircd::js::trap::operator()()
{
	return { &_class };
}

ircd::js::object
ircd::js::trap::operator()(const object &proto)
{
	return object { &_class, proto };
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
	trap.debug("has: '%s'", string(id).c_str());
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
	trap.debug("del: '%s'", string(id).c_str());
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
	trap.debug("get: '%s'", string(id).c_str());
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
	trap.debug("set: '%s'", string(id).c_str());
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
	trap.host_exception("set: '%s': %s",
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
	trap.debug("add: '%s'", string(id).c_str());
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
// ircd/js/script.h
//

ircd::js::script::script(const JS::CompileOptions &opts,
                         const std::string &src)
:JS::Rooted<JSScript *>{*cx}
{
	if(!JS::Compile(*cx, opts, src.data(), src.size(), &(*this)))
		throw syntax_error("Failed to compile script");
}

ircd::js::value
ircd::js::script::operator()()
const
{
	value ret;
	if(!JS_ExecuteScript(*cx, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::script::operator()(JS::AutoObjectVector &stack)
const
{
	value ret;
	if(!JS_ExecuteScript(*cx, stack, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/function.h
//

ircd::js::function::function(JS::AutoObjectVector &stack,
                             const JS::CompileOptions &opts,
                             const char *const &name,
                             const std::vector<std::string> &args,
                             const std::string &src)
:JS::Rooted<JSFunction *>{*cx}
{
	std::vector<const char *> argp(args.size());
	std::transform(begin(args), end(args), begin(argp), []
	(const std::string &arg)
	{
		return arg.data();
	});

	if(!JS::CompileFunction(*cx,
	                        stack,
	                        opts,
	                        name,
	                        argp.size(),
	                        &argp.front(),
	                        src.data(),
	                        src.size(),
	                        &(*this)))
	{
		throw syntax_error("Failed to compile function");
	}
}

ircd::js::value
ircd::js::function::operator()(const object &that)
const
{
	return operator()(that, JS::HandleValueArray::empty());
}

ircd::js::value
ircd::js::function::operator()(const object &that,
                               const JS::HandleValueArray &args)
const
{
	value ret;
	if(!JS_CallFunction(*cx, that, *this, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/call.h
//

ircd::js::value
ircd::js::call(const object &obj,
               const JS::HandleFunction &func,
               const JS::HandleValueArray &args)
{
	value ret;
	if(!JS_CallFunction(*cx, obj, func, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::call(const object &obj,
               const JS::HandleValue &val,
               const JS::HandleValueArray &args)
{
	value ret;
	if(!JS_CallFunctionValue(*cx, obj, val, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::call(const object &obj,
               const char *const &name,
               const JS::HandleValueArray &args)
{
	value ret;
	if(!JS_CallFunctionName(*cx, obj, name, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::call(const object &obj,
               const std::string &name,
               const JS::HandleValueArray &args)
{
	return call(obj, name.c_str(), args);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/string.h
//

const size_t ircd::js::string::CBUFSZ
{
	1024
};

const char *
ircd::js::string::c_str()
const
{
	static thread_local char cbuf[CBUFS][CBUFSZ];
	static thread_local size_t ctr;

	char *const buf(cbuf[ctr]);
	native(get(), buf, CBUFSZ);
	ctr = (ctr + 1) % CBUFS;
	return buf;
}

std::u16string
ircd::js::string::convert(const std::string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	return converter.from_bytes(s);
}

std::u16string
ircd::js::string::convert(const char *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	return s? converter.from_bytes(s) : std::u16string{};
}

std::string
ircd::js::string::convert(const std::u16string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	return converter.to_bytes(s);
}

std::string
ircd::js::string::convert(const char16_t *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	return s? converter.to_bytes(s) : std::string{};
}

size_t
ircd::js::string::convert(const char16_t *const &str,
                          char *const &buf,
                          const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	//TODO: optimize
	const auto s(converter.to_bytes(str));
	return rb_strlcpy(buf, s.c_str(), max);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/value.h
//

std::string
ircd::js::native(const JSString *const &s)
{
	std::string ret(native_size(s) + 1, char());
	native(s, &ret.front(), ret.size());
	ret.resize(ret.size() - 1);
	return ret;
}

size_t
ircd::js::native(const JSString *const &s,
                 char *const &buf,
                 const size_t &max)
{
	if(unlikely(!max))
		return 0;

	ssize_t ret(JS_EncodeStringToBuffer(*cx, const_cast<JSString *>(s), buf, max));
	ret = std::max(ret, ssize_t(0));
	ret = std::min(ret, ssize_t(max - 1));
	buf[ret] = '\0';
	return ret;
}

size_t
ircd::js::native_size(const JSString *const &s)
{
	return JS_GetStringEncodingLength(*cx, const_cast<JSString *>(s));
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/error.h
//

ircd::js::jserror::jserror(const JS::Value &val)
:ircd::js::error{generate_skip}
,val{*cx, val}
{
}

ircd::js::jserror::jserror(generate_skip_t)
:ircd::js::error(generate_skip)
,val{*cx}
{
}

ircd::js::jserror::jserror(const char *const fmt,
                           ...)
:ircd::js::error{generate_skip}
,val{*cx}
{
	va_list ap;
	va_start(ap, fmt);
	generate(JSEXN_ERR, fmt, ap);
	va_end(ap);
}

ircd::js::jserror::jserror(const JSErrorReport &report)
:ircd::js::error{generate_skip}
,val{*cx}
{
	create(report);
}

ircd::js::jserror::jserror(pending_t)
:ircd::js::error{generate_skip}
,val{*cx}
{
	if(!restore_exception(*cx))
		return;

	auto &report(cx->report);
	if(report.flags & JSREPORT_EXCEPTION &&
	   report.errorNumber != 105)
	{
		JS::RootedObject obj(*cx, &val.toObject());
		if(likely(JS_ErrorFromException(*cx, obj)))
			report = *JS_ErrorFromException(*cx, obj);

		const auto msg(report.ucmessage? string::convert(report.ucmessage) : std::string{});
		snprintf(ircd::exception::buf, sizeof(ircd::exception::buf), "%s%s%s",
		         reflect((JSExnType)report.exnType),
		         msg.empty()? "." : ": ",
		         msg.c_str());

		return;
	}

	switch(report.errorNumber)
	{
		case 61: // JSAPI's code for interruption
			snprintf(ircd::exception::buf, sizeof(ircd::exception::buf),
			         "interrupted @ line: %u col: %u",
			         report.lineno,
			         report.column);
			break;

		case 105: // JSAPI's code for user reported error
			snprintf(ircd::exception::buf, sizeof(ircd::exception::buf),
			         "(BUG) Host exception");
			break;

		default:
			snprintf(ircd::exception::buf, sizeof(ircd::exception::buf),
			         "Unknown non-exception #%u flags[%02x]",
			         report.errorNumber,
			         report.flags);
			break;
	}
}

void
ircd::js::jserror::set_pending()
const
{
	JS_SetPendingException(*cx, val);
}

void
ircd::js::jserror::generate(const JSExnType &type,
                            const char *const &fmt,
                            va_list ap)
{
	ircd::exception::generate(fmt, ap);
	const auto msg(string::convert(what()));

	JSErrorReport report;
	report.ucmessage = msg.c_str();
	report.exnType = type;

	create(report);
}

void
ircd::js::jserror::create(const JSErrorReport &report)
{
	const auto type((JSExnType)report.exnType);
	const auto &col(report.column);
	const auto &line(report.lineno);

	JS::RootedString msg(*cx);
	JS::RootedString file(*cx);
	JS::RootedObject stack(*cx);
	if(!JS::CreateError(*cx,
	                    type,
	                    stack,
	                    file,
	                    line,
	                    col,
	                    const_cast<JSErrorReport *>(&report),
	                    msg,
	                    &val))
	{
		throw error("Failed to construct jserror exception!");
	}
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
		ss << "\"" << string::convert(r.ucmessage) << "\" ";

	for(auto it(r.messageArgs); it && *it; ++it)
		ss << "\"" << string::convert(*it) << "\" ";

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
	// Construct the context
	[this, &runtime, &opts]
	{
		const auto ret(JS_NewContext(runtime, opts.stack_chunk_size));

		// Use their privdata pointer to point to our instance. We can then use our(JSContext*)
		// to get back to `this` instance.
		JS_SetContextPrivate(ret, this);

		// Assign the thread_local here.
		cx = this;

		return ret;
	}(),

	// Plant the destruction
	[](JSContext *const ctx)
	noexcept
	{
		if(!ctx)
			return;

		// Free the user's privdata managed object
		delete static_cast<const privdata *>(JS_GetSecondContextPrivate(ctx));

		JS_DestroyContext(ctx);

		// Indicate to thread_local
		cx = nullptr;
	}
}
,opts{opts}
,except{nullptr}
,state
{{
	0,                // Semaphore value starting at 0.
	phase::ACCEPT,    // ACCEPT phase indicates nothing is running.
	irq::JS,          // irq::JS is otherwise here in case JS triggers an interrupt.
}}
,timer
{
	std::bind(&context::handle_timeout, this)
}
{
	assert(&our(runtime) == rt);  // Trying to construct on thread without runtime thread_local
	timer.set(opts.timer_limit);
}

ircd::js::context::~context()
noexcept
{
}

void
ircd::js::context::handle_timeout()
noexcept
{
	// The interruption is an atomic transaction so our condition
	// for a timer excess occurs in a synchronous closure.
	interrupt(*this, []
	{
		// At this time there is no yield logic so if the timer calls
		// the script is terminated.
		return irq::TERMINATE;
	});
}

bool
ircd::js::context::handle_interrupt()
{
	auto state(this->state.load(std::memory_order_acquire));

	// Spurious interrupt; ignore.
	if(unlikely(state.phase != phase::INTR && state.phase != phase::ENTER))
	{
		log.warning("context(%p): Spurious interrupt (irq: %02x)",
		            (const void *)this,
		            uint(state.irq));
		return true;
	}

	// After the interrupt is handled the phase indicates entry back to JS,
	// IRQ is left indicating JS in case we don't trigger the next interrupt.
	const scope interrupt_return([this, &state]
	{
		state.phase = phase::ENTER;
		state.irq = irq::JS;
		this->state.store(state, std::memory_order_release);
	});

	// Call the user hook if available
	if(on_intr)
	{
		// The user's handler returns -1 for non-overriding behavior
		const auto ret(on_intr(state.irq));
		if(ret != -1)
			return ret;
	}

	switch(state.irq)
	{
		default:
		case irq::NONE:
			assert(0);

		case irq::JS:
		case irq::USER:
			return true;

		case irq::YIELD:
			ctx::yield();
			return true;

		case irq::TERMINATE:
			return false;
	}
}

void
ircd::js::leave(context &c)
{
	c.timer.cancel();

	// Load the state to keep the current sem value up to date.
	// This thread is the only writer to that value.
	auto state(c.state.load(std::memory_order_relaxed));

	// The ACCEPT phase locks out the interruptor
	state.phase = phase::ACCEPT;

	// The ACCEPT is released and the current phase seen by interruptor is acquired.
	state = c.state.exchange(state, std::memory_order_acq_rel);

	// The executor (us) must check if the interruptor (them) has committed to an interrupt
	// targeting the JS run we are now leaving. JS may have exited after the commitment
	// and before the interrupt arrival.
	if(state.phase == phase::INTR)
		assert(interrupt_poll(c));
}

void
ircd::js::enter(context &c)
{
	// State was already acquired by the last leave();
	auto state(c.state.load(std::memory_order_relaxed));

	// Increment the semaphore for the next execution;
	++state.sem;

	// Set the IRQ to JS in case we don't trigger an interrupt, the handler
	// will see a correct value.
	state.irq = irq::JS;
	state.phase = phase::ENTER;

	// Commit to next execution.
	c.state.store(state, std::memory_order_release);
	c.timer.start();
}

bool
ircd::js::interrupt(context &c,
                    const interrupt_condition &closure)
{
	// Acquire the execution state.
	const auto state(c.state.load(std::memory_order_acquire));

	// Only proceed if something is even running.
	if(state.phase != phase::ENTER)
		return false;

	// See if user still wants an interruption.
	const auto req(closure());
	if(req == irq::NONE)
		return false;

	// The expected value of the state to transact.
	struct context::state in
	{
		state.sem,          // Expect the sem value to match, else the execution has changed.
		phase::ENTER,       // Expect the execution to be occurring.
		irq::JS,            // JS is always expected here instead of NONE.
	};

	// The restatement after the transaction.
	const struct context::state out
	{
		state.sem,          // Keep the same sem value.
		phase::INTR,        // Indicate our commitment to interrupting.
		req                 // Include the irq type.
	};

	// Attempt commitment to interrupt here.
	static const auto order_fail(std::memory_order_relaxed);
	static const auto order_success(std::memory_order_acq_rel);
	if(!c.state.compare_exchange_strong(in, out, order_success, order_fail))
	{
		// To reliably read from `in` here, order_fail should not be relaxed.
		return false;
	}

	// Commitment now puts the burden on the executor to not allow this interrupt to bleed into
	// the next execution, even if JS has already exited before its arrival.
	interrupt(c.runtime());
	return true;
}

bool
ircd::js::interrupt_poll(const context &c)
{
	return JS_CheckForInterrupt(c);
}

bool
ircd::js::restore_exception(context &c)
{
	if(unlikely(!c.except))
		return false;

	JS_RestoreExceptionState(c, c.except);
	c.except = nullptr;
	return true;
}

void
ircd::js::save_exception(context &c,
                         const JSErrorReport &report)
{
	if(c.except)
		throw error("(internal error): Won't overwrite saved exception");

	c.except = JS_SaveExceptionState(c),
	c.report = report;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/timer.h
//

ircd::js::timer::timer(const callback &timeout)
:finished{false}
,timeout{timeout}
,started{time_point::min()}
,limit{0us}
,state
{{
	0, false
}}
,thread{std::bind(&timer::worker, this)}
{
}

ircd::js::timer::~timer()
noexcept
{
	// This is all on the main IRCd thread, and the only point when it waits on the timer
	// thread. That's okay as long as this is on IRCd shutdown etc.
	std::unique_lock<decltype(mutex)> lock(mutex);
	finished = true;

	// Wait for timer thread to exit. Notify before the unlock() could double tap,
	// but should always make for a reliable way to do this shutdown.
	cond.notify_all();
	lock.unlock();
	thread.join();
}

ircd::js::timer::time_point
ircd::js::timer::start()
{
	// The counter is incremented indicating a new timing request, invalidating
	// anything the timer was previously doing.
	auto state(this->state.load(std::memory_order_relaxed));
	++state.sem;
	state.running = true;

	// Commit to starting a new timer operation, unconditionally overwrite previous.
	started = steady_clock::now();
	this->state.store(state, std::memory_order_release);

	// The timing thread is notified here. It may have already started on our new
	// request. However, this notifcation must not be delayed and the timing thread
	// must wake up soon/now.
	cond.notify_one();

	return started;
}

bool
ircd::js::timer::cancel()
{
	// Relaxed acquire of the state to get the sem value.
	auto state(this->state.load(std::memory_order_relaxed));

	// Expect the timer to be running and order the timer to idle with an invalidation.
	struct state in        { state.sem,      true   };
	const struct state out { state.sem + 1,  false  };

	// Commit to cancellation. On a successful state transition, wake up the thread.
	static const auto order_fail(std::memory_order_relaxed);
	static const auto order_success(std::memory_order_acq_rel);
	if(this->state.compare_exchange_strong(in, out, order_success, order_fail))
	{
		cond.notify_one();
		return true;
	}
	else return false;
}

void
ircd::js::timer::set(const callback &timeout)
{
	this->timeout = timeout;
	std::atomic_thread_fence(std::memory_order_release);
}

void
ircd::js::timer::set(const microseconds &limit)
{
	this->limit.store(limit, std::memory_order_release);
}

void
ircd::js::timer::worker()
{
	// This lock is only ever held by this thread except during a finish condition.
	// Notifications to the condition are only broadcast by the main thread.
	std::unique_lock<decltype(mutex)> lock(mutex);
	while(likely(!finished))
		handle(lock);
}

void
ircd::js::timer::handle(std::unique_lock<std::mutex> &lock)
{
	struct state ours, theirs;
	const auto idle_condition([this, &ours]
	{
		if(unlikely(finished))
			return true;

		// Acquire the request operation
		ours = this->state.load(std::memory_order_acquire);

		// Require a start time
		if(started == time_point::min())
			return false;

		return ours.running;
	});

	const auto cancel_condition([this, &ours, &theirs]
	{
		if(unlikely(finished))
			return true;

		// Acquire the request state and compare it to our saved state to see
		// if the counter has changed or if there is cancellation.
		theirs = this->state.load(std::memory_order_consume);
		return ours.sem != theirs.sem || !theirs.running;
	});

	// Wait for a running condition into `in`
	cond.wait(lock, idle_condition);
	if(unlikely(finished))
		return;

	// Wait for timeout or cancellation
	const auto limit(this->limit.load(std::memory_order_consume));
	if(cond.wait_until(lock, started + limit, cancel_condition))
	{
		// A cancel or increment of the counter into the next request has occurred.
		// This thread will go back to sleep in the idle_condition unless or until
		// the next start() is triggered.
		return;
	}

	// A timeout has occurred. This is the last chance for a belated cancellation to be
	// observed. If a transition from running to !running can take place on this counter
	// value, the user has not invalidated this request and desires the timeout.
	assert(ours.running);
	const struct state out { ours.sem, false };
	static const auto order_fail(std::memory_order_relaxed);
	static const auto order_success(std::memory_order_acq_rel);
	if(state.compare_exchange_strong(ours, out, order_success, order_fail))
		if(likely(timeout))
			timeout();
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
	JS_SetInterruptCallback(get(), handle_interrupt);

	JS_SetNativeStackQuota(get(), opts.code_stack_max, opts.trusted_stack_max, opts.untrusted_stack_max);
}

ircd::js::runtime::runtime(runtime &&other)
noexcept
:custom_ptr<JSRuntime>{std::move(other)}
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
	log.debug("JSContext(%p): Interrupt", (const void *)ctx);

	auto &c(our(ctx));
	return c.handle_interrupt();
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
	assert(report);
/*
	log.debug("JSContext(%p) Error report: %s | %s",
	          (const void *)ctx,
	          msg,
	          debug(*report).c_str());

	log.critical("Unhandled: JSContext(%p): %s [%s]",
	             (const void *)ctx,
	             msg,
	             debug(*report).c_str());
*/
	save_exception(our(ctx), *report);
}
