// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/js/js.h>
#include <js/Initialization.h>                   // JS_Init() / JS_ShutDown()
#include <mozilla/ThreadLocal.h>                 // For GetThreadType() linkage hack (see: down)

// Must be defined until we address issues with SpiderMonkey fixed inside any
// IRCD_JS_FIX blocks.
#define IRCD_JS_FIX

// This was only ever defined for the SpiderMonkey headers and some of our hacks,
// but we need to undef it to not step on the log facility log::DEBUG.
// Use JS_DEBUG as an analog instead.
#undef DEBUG

// Logging facility for this submodule with SNOMASK.
decltype(ircd::js::log)
ircd::js::log
{
	"js", 'J'
};

namespace ircd {
namespace js   {

// Location of the thread_local runtext (js/context.h)
// If this is null, js is not available on your thread.
__thread context *cx;
__thread trap *tree;

// Whenever a JSClass is seen by the runtime it has to remain reachable for the lifetime
// of the runtimet. They don't give any further access or callbacks to free the object,
// expecting it to be static or something (yea right). What we have here is a place for
// traps to dump their JSClass on destruction and then this can be reaped later.
std::forward_list<std::unique_ptr<JSClass>> class_drain;

// Internal prototypes
const char *reflect(const ::js::CTypesActivityType &);

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

	const unwind exit([this]
	{
		// Ensure ~init() is always safe to call at any intermediate state
		if(std::current_exception())
			this->~init();
	});

	{
		const char *errmsg;
		if((errmsg = JS_InitWithFailureDiagnostic()))
			throw error("JS_Init(): %s", errmsg);
	}

	struct context::opts context_opts;
	log.info("Initializing the main JS context (main_maxbytes: %zu)",
	         context_opts.max_bytes);

	assert(!cx);
	cx = new context(context_opts);

	// Additional options
	//set(*cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);

	log.info("Initialized main JS context (version: '%s')",
	         version(*cx));
}

ircd::js::init::~init()
noexcept
{
	log.info("Terminating the main JS context");

 	delete cx;
	cx = nullptr;

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

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/js.h - With 3rd party (JSAPI) symbols
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/task.h
//

ircd::js::task::task(const std::string &source)
:task{locale::char16::conv(source)}
{
}

ircd::js::task::task(const std::u16string &source)
try
:pid
{
	0
}
,global{[this]
{
	JS::CompartmentCreationOptions opts;

	// Global object is constructed using the root trap (JSClass) at *tree;
	// This is a thread_local registered by the kernel.so module.
	struct global global(*tree, nullptr, opts);

	// The root trap is configured with HAS_PRIVATE and that slot is set so we can find this
	// `struct task` from the global object using task::get(object). The global object
	// can first be found from a context or active compartment. As a convenience `struct task`
	// can be found contextually with task::get(void).
	JS_SetPrivate(global, this);

	return global;
}()}
,main{[this, &source]
() -> struct module
{
	// A compartment for the global must be entered to compile in this scope
	const compartment c(this->global);

	// TODO: options
	JS::CompileOptions opts(*cx);
	//opts.setCanLazilyParse(true);
	//opts.setSourceIsLazy(true);
	//opts.setIntroductionType("GeneratorFunction");
	//opts.forceAsync = true;

	// The compilation is also conducted asynchronously: it will yield the current
	// ircd::ctx until it is complete.
	const auto instantiate{true};
	return
	{
		 module::yielding, opts, source, nullptr, instantiate
	};
}()}
{
	//const compartment c(this->global);
}
catch(const std::exception &e)
{
	throw;
}

ircd::js::task::~task()
noexcept
{
	run_gc(*cx);
}

bool
ircd::js::task::enter(const std::weak_ptr<task> &ptr,
                      const std::function<void (task &)> &closure)
try
{
	const life_guard<struct task> task(ptr);
	return enter(*task, closure);
}
catch(const std::bad_weak_ptr &e)
{
	ircd::js::log.warning("task::enter(%p, closure: %p): expired task",
	                      (const void *)&ptr,
	                      (const void *)&closure);
	return false;
}

bool
ircd::js::task::enter(task &t,
                      const std::function<void (task &)> &closure)
{
	const std::lock_guard<context> lock{*cx};
	const compartment compartment(t.global);
	run([&closure, &t]
	{
		closure(t);
	});

	return true;
}

ircd::js::object
ircd::js::reflect(const task &task)
{
	const object global(task.global);
	const object reflect(get(global, "Reflect"));
	const function parse(get(reflect, "parse"));
	return parse(global, decompile(task));
}

ircd::js::string
ircd::js::decompile(const task &task,
                    const bool &pretty)
{
	return decompile(task.main, "main", pretty);
}


///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/global.h
//

ircd::js::global::global(trap &trap,
                         JSPrincipals *const &principals,
                         JS::CompartmentCreationOptions copts,
                         JS::CompartmentBehaviors bopts)
:object{[&trap, &principals, &copts, &bopts]
{
	//copts.setMergeable(true);
	//copts.setInvisibleToDebugger(true);
	copts.setTrace(handle_trace);
	const JS::CompartmentOptions opts
	{
		copts, bopts
	};

	return JS_NewGlobalObject(*cx,
	                          &trap.jsclass(),
	                          principals,
	                          JS::DontFireOnNewGlobalHook,
	                          opts);
}()}
,module_resolve_hook{[this]
{
	const compartment c(*this);
	return std::make_unique<function::native>("ModuleResolveHook", 0, 0, [this]
	(object::handle func, value::handle that, const args &args)
	{
		assert(args.size() == 2);
		const object requestor(args.at(0));
		const string requesting(args.at(1));
		module &importer(module::our(requestor));
		return import(importer, requesting, object(that));
	});
}()}
{
	const compartment c(*this);
	if(!JS_InitStandardClasses(*cx, *this))
		throw error("Failed to init standard classes for global object");

	JS_InitReflectParse(*cx, *this);
	JS::SetModuleResolveHook(*cx, *module_resolve_hook);
	JS_FireOnNewGlobalObject(*cx, *this);
}

ircd::js::global::~global()
noexcept
{
}

void
ircd::js::global::handle_trace(JSTracer *const tracer,
                               JSObject *const obj)
noexcept
{
	//assert(tracer->runtime() == cx->get());

	log.debug("context(%p): global tracer(%p) object(%p)",
	          nullptr,//(const void *)&our(tracer->context()),
	          (const void *)tracer,
	          (const void *)obj);
}

ircd::js::object
ircd::js::global::import(module &importer,
                         const string &requesting,
                         const object &that)
{
	log.debug("context(%p): module '%s' import '%s' (that: %p existing: %d)",
	          (const void *)cx,
	          std::string("<fixme>"),
	          std::string(requesting),
	          (const void *)that.address(),
	          imports.find(requesting) != end(imports));

	const auto it(imports.find(requesting));
	if(it != end(imports))
	{
		const auto &exporter(*it->second);
		const string name(exporter.trap->name);
		js::set(that, name, exporter.trap->construct());
		return static_cast<const object &>(exporter);
	}

	static const std::string prefix
	{
		std::string(fs::MODPATH) + "/"
	};

	string fname(prefix);
	const string delim("_");
	tokens(requesting, '.', [&fname, &delim]
	(const string &token)
	{
		fname += token;
		fname += delim;
	});

	fname = substr(fname, 0, fname.size() - 1);
	fname += ".so";

	ircd::module *const m(new ircd::module(std::string(fname)));
	const auto exporter(m->get<module *>("IRCD_JS_MODULE"));
	const auto iit(imports.emplace(requesting, exporter));
	const string name(exporter->trap->name);
	js::set(that, name, exporter->trap->construct());
	return static_cast<const object &>(*exporter);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/trap_property.h
//

ircd::js::trap::property::property(struct trap &trap,
                                   const std::string &name,
                                   const uint &flags)
try
:trap{&trap}
,name{name}
,spec{0}
{
	spec.name = this->name.data(),
	spec.flags = flags,
	spec.accessors.setter.native = { handle_set };
	spec.accessors.getter.native = { handle_get };

	const auto iit(trap.memprop.emplace(this->name, this));
	if(!iit.second)
		throw error("trap property '%s' already exists on '%s'",
		            this->name,
		            trap.name);

	log.debug("Registered property '%s' on trap '%s'",
	          this->name,
	          trap.name);
}
catch(const error &e)
{
	throw error("Failed to register property '%s': out slots on trap '%s': %s",
	            this->name.c_str(),
	            trap.name.c_str(),
	            e.what());
}

ircd::js::trap::property::~property()
noexcept
{
	const auto erased(trap->memprop.erase(name));
	assert(erased);
}

bool
ircd::js::trap::property::handle_get(JSContext *const c,
                                     const unsigned argc,
                                     JS::Value *const argv)
noexcept try
{
	using js::function;

	const struct args args(argc, argv);
	object that(args.computeThis(c));
	function func(args.callee());
	const string name(js::name(func));

	auto &trap(from(that));
	trap.debug(that.get(), "get '%s' (property)",
	           name.c_str());

	property &prop(*trap.memprop.at(name));
	args.rval().set(prop.on_get(func, that));
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto ca(JS::CallArgsFromVp(argc, argv));
	object that(ca.computeThis(c));
	auto &trap(from(that));
	trap.host_exception(that.get(), "property get: %s", e.what());
	return false;
}

bool
ircd::js::trap::property::handle_set(JSContext *const c,
                                     const unsigned argc,
                                     JS::Value *const argv)
noexcept try
{
	using js::function;

	const struct args args(argc, argv);
	object that(args.computeThis(c));
	function func(args.callee());
	const string name(js::name(func));

	auto &trap(from(that));
	trap.debug(that.get(), "set '%s' (property)",
	           name.c_str());

	property &prop(*trap.memprop.at(name));
	args.rval().set(prop.on_get(func, that));
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto ca(JS::CallArgsFromVp(argc, argv));
	object that(ca.computeThis(c));
	auto &trap(from(that));
	trap.host_exception(that.get(), "property set: %s", e.what());
	return false;
}

ircd::js::value
ircd::js::trap::property::on_get(function::handle,
                                 object::handle that)
{
	return {};
}

ircd::js::value
ircd::js::trap::property::on_set(function::handle,
                                 object::handle that,
                                 value::handle val)
{
	return val;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/trap_function.h
//

ircd::js::trap::function::function(struct trap &trap,
                                   const std::string &name,
                                   const uint16_t &flags,
                                   const uint16_t &arity,
                                   const closure &lambda)
:trap{&trap}
,name{name}
,lambda{lambda}
,spec
{
	this->name.data(),
	{ handle_call },
	arity,
	flags
}
{
	const auto iit(trap.memfunc.emplace(this->name, this));
	if(!iit.second)
		throw error("trap function '%s' already exists on '%s'",
		            this->name,
		            trap.name);

	log.debug("Registered function '%s' on trap '%s'",
	          this->name,
	          trap.name);
}

ircd::js::trap::function::~function()
noexcept
{
	const auto erased(trap->memfunc.erase(this->name));
	assert(erased);
}

ircd::js::function
ircd::js::trap::function::operator()(const object::handle &obj)
const
{
	const auto jsf(::js::DefineFunctionWithReserved(*cx,
	                                                obj,
	                                                spec.name,
	                                                spec.call.op,
	                                                spec.nargs,
	                                                spec.flags));
	if(unlikely(!jsf))
		throw internal_error("Failed to create trap::function");

	js::function ret(jsf);
	::js::SetFunctionNativeReserved(ret, 0, pointer_value(this));
	return ret;
}

bool
ircd::js::trap::function::handle_call(JSContext *const c,
                                      const unsigned argc,
                                      JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);

	const struct args args(argc, argv);
	const object func(args.callee());
	auto &trap(from(func));
	if(args.isConstructing())
	{
		log.debug("trap(%p) %s() ctor argv[%u]",
		          (const void *)&trap,
		          trap.name,
		          argc);

		const value that(trap.on_new(func, args));
		args.rval().set(that);
		log.debug("trap(%p) this(%p) %s() leave",
		          (const void *)&trap,
		          (const void *)that.address(),
		          trap.name);
	} else {
		const value that(args.computeThis(c));
		log.debug("trap(%p) this(%p) %s() call argv[%u]",
		          (const void *)&trap,
		          (const void *)that.address(),
		          trap.name,
		          argc);

		args.rval().set(trap.on_call(func, that, args));
		log.debug("trap(%p) this(%p) %s() leave",
		          (const void *)&trap,
		          (const void *)that.address(),
		          trap.name);
	}

	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	const struct args args(argc, argv);
	auto &func(args.callee());
	auto &trap(from(&func));

	log.error("trap(%p) \"%s()\": %s",
	          reinterpret_cast<const void *>(&trap),
	          trap.name,
	          e.what());

	JS_ReportErrorUTF8(*cx, "BUG: trap(%p) \"%s()\": %s",
	                   reinterpret_cast<const void *>(&trap),
	                   trap.name.c_str(),
	                   e.what());
	return false;
}

ircd::js::trap::function &
ircd::js::trap::function::from(JSObject *const &func)
{
	const auto tval(::js::GetFunctionNativeReserved(func, 0));
	return *pointer_value<function>(tval);
}

ircd::js::value
ircd::js::trap::function::on_call(object::handle obj,
                                  value::handle val,
                                  const args &args)
{
	return lambda(obj, val, args);
}

ircd::js::value
ircd::js::trap::function::on_new(object::handle obj,
                                 const args &args)
{
	value ud;
	return on_call(obj, ud, args);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/trap.h
//

namespace ircd {
namespace js   {

} // namespace js
} // namespace ircd

ircd::js::trap::trap(const std::string &name,
                     const uint &flags,
                     const uint &prop_flags)
:name{name}
,prototrap{nullptr}
,classp
{
	this->name.data(),
	flags | JSCLASS_FOREGROUND_FINALIZE,
	(flags & JSCLASS_IS_GLOBAL)? &gcops : &cops,
	{ this } // reserved[0] TODO: ?????????
}
{
	if(this->name == "[global]")
		ircd::js::tree = this;

	log.debug("Registered trap '%s' @ %p",
	          jsclass().name,
	          (const void *)this);
}

ircd::js::trap::~trap()
noexcept
{
	log.debug("Unregistered trap '%s' @ %p",
	          jsclass().name,
	          (const void *)this);
}

const JSClassOps
ircd::js::trap::cops
{
	handle_add,
	handle_del,
	handle_get,
	handle_set,
	handle_enu,
	handle_has,
	nullptr,     // JSConvertOp - Obsolete since SpiderMonkey 44  // 45 = mayResolve?
	handle_dtor,
	handle_call,
	handle_inst,
	handle_ctor,
	handle_trace,
};

const JSClassOps
ircd::js::trap::gcops
{
	handle_add,  // flags & JSCLASS_GLOBAL_FLAGS? nullptr : handle_add,
	handle_del,
	handle_get,
	handle_set,
	handle_enu,
	handle_has,
	nullptr,     // JSConvertOp - Obsolete since SpiderMonkey 44  // 45 = mayResolve?
	handle_dtor,
	handle_call,
	handle_inst,
	handle_ctor,
	JS_GlobalObjectTraceHook,
};

ircd::js::object
ircd::js::trap::construct(const vector<value>::handle &argv)
{
	const object globals;
	return construct(globals, argv);
}

ircd::js::object
ircd::js::trap::construct(const object::handle &globals,
                          const vector<value>::handle &argv)
{
	const object prototype(this->prototype(globals));
	return JS_New(*cx, prototype, argv);
}

ircd::js::object
ircd::js::trap::prototype(const object::handle &globals)
{
	// Create parent object of this object in the prototype chain
	const object super
	{
		prototrap? prototrap->construct() : object{object::uninitialized}
	};

	// Build static property descriptor array
	std::vector<JSPropertySpec> sprop
	{
		this->sprop.size() + 1,  // JS requires classical null terminator
		{ 0 }
	};

	// Build static function descriptor array
	std::vector<JSFunctionSpec> sfunc
	{
		this->sfunc.size() + 1,  // JS requires classical null terminator
		{ 0 }
	};

	// Build member property descriptor array
	std::vector<JSPropertySpec> memprop
	{
		this->memprop.size() + 1,  // JS requires classical null terminator
		{ 0 }
	};

	std::transform(begin(this->memprop), end(this->memprop), begin(memprop), []
	(const auto &it)
	{
		const auto &prop(*it.second);
		return prop.spec;
	});

	// Mozilla really screwed the pooch here. There is no way to specify a function
	// with a reserved slot in order to find our trap::function instance from the
	// JSNative callback. Their API surface is too broad and there are too many different
	// and mutually exclusive ways to define functions. So we have to be inconsistent
	// here and not use InitClass()'s facility for defining member functions.

	// Build member function descriptor array
	std::vector<JSFunctionSpec> memfunc
	{
		this->memfunc.size() + 1,  // JS requires classical vector terminator
		{ 0 }
	};

//	std::transform(begin(this->memfunc), end(this->memfunc), begin(memfunc), []
//	(const auto &it)
//	{
//		const auto &function(*it.second);
//		return function.spec;
//	});

	// Create object
	const object proto
	{
		JS_InitClass(*cx,
		             globals,
		             super,
		             &jsclass(),
		             nullptr,
		             0,
		             memprop.data(),
		             memfunc.data(),
		             sprop.data(),
		             sfunc.data())
	};

	// Member functions defined after the fact here.
	std::for_each(begin(this->memfunc), end(this->memfunc), [&proto]
	(const auto &it)
	{
		const auto &deffun(*it.second);
		const auto func(deffun(proto));
	});

	//JS_DefineConstIntegers(*cx, proto, cis.data());
	//JS_DefineConstDoubles(*cx, proto, cds.data());

	return proto;
}

void
ircd::js::trap::handle_dtor(JSFreeOp *const op,
                            JSObject *const obj)
noexcept try
{
	assert(op);
	assert(obj);
	//assert(&our_runtime(*op) == rt);

	auto &trap(from(*obj));
	trap.debug(obj, "dtor");
	trap.on_gc(obj);
}
catch(const std::exception &e)
{
	auto &trap(from(*obj));
	log.critical("Unhandled on GC (fop: %p obj: %p): %s",
	             (const void *)op,
	             (const void *)obj,
	             e.what());
	assert(0);
}

bool
ircd::js::trap::handle_ctor(JSContext *const c,
                            unsigned argc,
                            JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);
	assert(!pending_exception(*cx));

	const struct args args(argc, argv);
	object that(args.callee());

	auto &trap(from(that));
	trap.debug(that.get(), "ctor '%s' argv[%zu]",
	           trap.name.c_str(),
	           args.size());

	object ret(JS_NewObjectWithGivenProto(*cx, &trap.jsclass(), that));
	trap.on_new(that, ret, args);
	args.rval().set(JS::Value(ret));
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto ca(JS::CallArgsFromVp(argc, argv));
	object that(ca.callee());
	auto &trap(from(that));
	trap.host_exception(that.get(), "ctor: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_call(JSContext *const c,
                            unsigned argc,
                            JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);
	assert(!pending_exception(*cx));

	const struct args args(argc, argv);
	value that(args.computeThis(c));
	object func(args.callee());

	//auto &trap_that(from(object(that)));
	auto &trap_func(from(func));

	//trap_that.debug(that.get(), "call: '%s'", trap_func.name().c_str());
	trap_func.debug(func.get(), "call argv[%zu]", args.size());
	args.rval().set(trap_func.on_call(func, that, args));
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	auto ca(JS::CallArgsFromVp(argc, argv));
	object func(ca.callee());
	auto &trap(from(func));
	trap.host_exception(func.get(), "call: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_enu(JSContext *const c,
                           JS::HandleObject obj)
noexcept try
{
	assert(&our(c) == cx);

	auto &trap(from(obj));
	trap.debug(obj.get(), "enumerate");
	trap.on_enu(obj);
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
	trap.host_exception(obj.get(), "enu: %s", e.what());
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
	assert(!pending_exception(*cx));

	auto &trap(trap::from(obj));
	trap.debug(obj.get(), "has '%s'", string(id).c_str());
	*resolved = trap.on_has(obj, id);
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
	trap.host_exception(obj.get(), "has '%s': %s",
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
	assert(!pending_exception(*cx));

	auto &trap(from(obj));
	trap.debug(obj.get(), "del '%s'", string(id).c_str());
	if(trap.on_del(obj, id))
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
	trap.host_exception(obj.get(), "del '%s': %s",
	                    string(id).c_str(),
	                    e.what());
	return false;
}

bool
ircd::js::trap::handle_getter(JSContext *const c,
                              unsigned argc,
                              JS::Value *const argv)
noexcept try
{
	using js::function;

	const struct args args(argc, argv);
	const object that(args.computeThis(c));
	const function func(args.callee());
	const string name(js::name(func));

	auto &trap(from(that));
	trap.debug(that.get(), "get '%s' (getter)",
	           name.c_str());

	//value &val(it->second);
	//args.rval().set(val);
	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	const auto ca(JS::CallArgsFromVp(argc, argv));
	const object that(ca.computeThis(c));
	auto &trap(from(that));
	trap.host_exception(that.get(), "getter: %s", e.what());
	return false;
}

bool
ircd::js::trap::handle_setter(JSContext *const c,
                              unsigned argc,
                              JS::Value *const argv)
noexcept try
{
	using js::function;

	const struct args args(argc, argv);
	const object that(args.computeThis(c));
	const function func(args.callee());

	const value val(args[0]);
	const auto type(js::type(val));
	const string name(js::name(func));

	auto &trap(from(that));
	trap.debug(that.get(), "set '%s' (%s) (setter)",
	           name.c_str(),
	           reflect(type));

	switch(js::type(type))
	{
		case jstype::OBJECT:
		{
			//const auto flags(JSPROP_SHARED);
			//object ret(JS_DefineObject(*cx, object(val), "", &trap.jsclass(), flags));
			//args.rval().set(ret);
			//return true;
		}

		default:
			args.rval().set(val);
			return true;
	}
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	const auto ca(JS::CallArgsFromVp(argc, argv));
	const object that(ca.computeThis(c));
	auto &trap(from(that));
	trap.host_exception(that.get(), "setter: %s", e.what());
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
	assert(!pending_exception(*cx));

	auto &trap(from(obj));
	trap.debug(obj.get(), "get '%s'", string(id).c_str());
	const value ret(trap.on_get(obj, id, val));
	val.set(ret.get());
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
	trap.host_exception(obj.get(), "get: '%s': %s",
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
	assert(!pending_exception(*cx));

	auto &trap(from(obj));
	trap.debug(obj.get(), "set '%s'", string(id).c_str());
	const value ret(trap.on_set(obj, id, val));
	val.set(ret.get());
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
	trap.host_exception(obj.get(), "set '%s': %s",
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
	assert(!pending_exception(*cx));

	auto &trap(from(obj));
	const string name(id);
	trap.debug(obj.get(), "add '%s' %s @%p",
	           name.c_str(),
	           reflect(type(val)),
	           (const void *)val.address());

	trap.on_add(obj, id, val);
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
	trap.host_exception(obj.get(), "add '%s': %s",
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
	trap.debug(obj.get(), "inst");
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
	trap.host_exception(obj.get(), "inst: %s", e.what());
	return false;
}

void
ircd::js::trap::handle_trace(JSTracer *const tracer,
                             JSObject *const obj)
noexcept try
{
	assert(cx);
	assert(tracer);
	assert(obj);

	auto &trap(from(*obj));
	trap.debug(obj, "trace");
	trap.on_trace(obj);
}
catch(const jserror &e)
{
	e.set_pending();
	return;
}
catch(const std::exception &e)
{
	auto &trap(from(*obj));
	trap.host_exception(obj, "trace: %s", e.what());
	return;
}

ircd::js::trap &
ircd::js::trap::from(const JSObject &o)
{
	return from(&o);
}

ircd::js::trap &
ircd::js::trap::from(const JSObject *const &o)
{
	auto *const c(JS_GetClass(const_cast<JSObject *>(o)));
	if(!c)
	{
		log.critical("trap::from(): Trapped on an object without a JSClass!");
		ircd::terminate(); //TODO: exception
	}

	if(!c->reserved[0])
	{
		log.critical("trap::from(): Trap called on a trap instance that has gone out of scope!");
		ircd::terminate(); //TODO: exception
	}

	return *static_cast<trap *>(c->reserved[0]);  //TODO: ???
}

void
ircd::js::trap::debug(const void *const &that,
                      const char *const fmt,
                      ...)
const
{
	va_list ap;
	va_start(ap, fmt);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	log.debug("trap(%p) this(%p) '%s' %s",
	          reinterpret_cast<const void *>(this),
	          that,
	          jsclass().name,
	          buf);

	va_end(ap);
}

void
ircd::js::trap::host_exception(const void *const &that,
                               const char *const fmt,
                               ...)
const
{
	va_list ap;
	va_start(ap, fmt);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	log.error("trap(%p) this(%p) '%s' %s",
	          reinterpret_cast<const void *>(this),
	          that,
	          jsclass().name,
	          buf);

	JS_ReportErrorUTF8(*cx, "BUG: trap(%p) this(%p) '%s' %s",
	                   reinterpret_cast<const void *>(this),
	                   that,
	                   jsclass().name,
	                   buf);

	va_end(ap);
}

void
ircd::js::trap::on_gc(JSObject *const &that)
{
	if(jsclass().flags & JSCLASS_IS_GLOBAL) //TODO: leak??
		return;

	if(jsclass().flags & JSCLASS_HAS_PRIVATE)
		del(that, priv);
}

void
ircd::js::trap::on_trace(const JSObject *const &)
{
}

void
ircd::js::trap::on_new(object::handle,
                       object &,
                       const args &)
{
}

void
ircd::js::trap::on_enu(object::handle)
{
}

bool
ircd::js::trap::on_has(object::handle,
                       id::handle id)
{
//	const string sid(id);
//	if(children.count(sid))
//		return false;
//
//	if(std::any_of(begin(memfun), end(memfun), [&sid]
//	(const auto &it)
//	{
//		const auto &memfun(*it.second);
//		return sid == memfun.name;
//	}))
//		return false;
//
//	value val;
//	const auto flags(JSPROP_SHARED | JSPROP_ENUMERATE);
//	if(!JS_DefinePropertyById(*cx, obj, id, val, flags, handle_getter, handle_setter))
//		throw jserror("Failed to define property '%s'", sid.c_str());
//
//	const auto flags(0);
//	if(!JS_DefineObject(*cx, obj, sid.c_str(), &jsclass(), flags))
//		throw jserror("Failed to define property '%s'", sid.c_str());

	return false;
}

bool
ircd::js::trap::on_del(object::handle,
                       id::handle)
{
	return true;
}

void
ircd::js::trap::on_add(object::handle,
                       id::handle,
                       value::handle)
{
}

ircd::js::value
ircd::js::trap::on_get(object::handle obj,
                       id::handle id,
                       value::handle val)
{
	return val;
}

ircd::js::value
ircd::js::trap::on_set(object::handle,
                       id::handle,
                       value::handle val)
{
	return val;
}

ircd::js::value
ircd::js::trap::on_call(object::handle,
                        value::handle,
                        const args &)
{
	return {};
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/module.h
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/script.h
//

namespace ircd {
namespace js   {

void handle_compile_async(void *, void *) noexcept;

} // namespace js
} // namespace ircd

bool
ircd::js::compilable(const std::string &str,
                     const object &stack)
{
	return compilable(str.c_str(), str.size(), stack);
}

bool
ircd::js::compilable(const char *const &str,
                     const size_t &len,
                     const object &stack)
{
	return JS_BufferIsCompilableUnit(*cx, stack, str, len);
}

size_t
ircd::js::bytecodes(const JS::Handle<JSScript *> &s,
                    uint8_t *const &buf,
                    const size_t &max)
{
	JS::TranscodeBuffer tcbuf;
	const auto code
	{
		JS::EncodeScript(*cx, tcbuf, s)
	};

	switch(code)
	{
		case JS::TranscodeResult_Ok:
		{
			if(unlikely(tcbuf.length() > max))
				throw internal_error
				{
					"Insufficient buffer size %zu where %zu is required.",
					max,
					tcbuf.length()
				};

			memcpy(buf, tcbuf.begin(), tcbuf.length());
			return tcbuf.length();
		}

		case JS::TranscodeResult_Throw:
			throw jserror(jserror::pending);

		default:
			throw internal_error
			{
				"Failed to encode script to XDR (error %d)", code
			};
	};
}

ircd::js::string
ircd::js::decompile(const JS::Handle<JSScript *> &s,
                    const char *const &name,
                    const bool &pretty)
{
	uint flags(0);
	flags |= pretty;
	return JS_DecompileScript(*cx, s, name, flags);
}

ircd::ctx::future<void *>
ircd::js::compile_async(const JS::ReadOnlyCompileOptions &opts,
                        const std::u16string &src,
                        const bool &module)
{
	auto promise(std::make_unique<ctx::promise<void *>>());
	if(!JS::CanCompileOffThread(*cx, opts, src.size()))
	{
		log.debug("context(%p): Rejected asynchronous script compile (script size: %zu)",
		          (const void *)cx,
		          src.size());

		ctx::future<void *> ret(*promise);
		promise->set_value(nullptr);
		return ret;
	}

	const auto &compile
	{
		module? JS::CompileOffThreadModule : JS::CompileOffThread
	};

	if(!compile(*cx, opts, src.data(), src.size(), handle_compile_async, promise.get()))
		throw internal_error("Failed to compile %s concurrently",
		                     module? "module" : "script");

	return *promise.release();
}

void
ircd::js::handle_compile_async(void *const token,
                               void *const priv)
noexcept
{
	// This frame is entered on a thread owned by SpiderMonkey, not IRCd. Do not call
	// ircd::log from here, it is not thread-safe.
//	printf("[thread %s]: context(%p): compile(%p) READY (priv: %p)\n",
//	       ircd::string(std::this_thread::get_id()).c_str(),
//	       (const void *)cx,
//	       token,
//	       priv);

	// Setting the value of the promise and then deleting the promise is thread-safe.
	// Note that JS::FinishOffThreadScript(); will need to be called on the main thread.
	auto *const _promise(reinterpret_cast<ctx::promise<void *> *>(priv));
	const std::unique_ptr<ctx::promise<void *>> promise(_promise);
	promise->set_value(token);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/function.h
//

ircd::js::value
ircd::js::function::operator()(const object::handle &that,
                               const vector<value>::handle &argv)
const
{
	return call(*this, that, argv);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/function_literal.h
//

ircd::js::function::literal::literal(const char *const &name,
                                     const std::initializer_list<const char *> &prototype,
                                     const char *const &text)
:root<JSFunction *>{}
,name{name}
,text{text}
,prototype{prototype}
{
	JS::CompileOptions opts{*cx};
	JS::AutoObjectVector stack{*cx};
	if(!JS::CompileFunction(*cx,
	                        stack,
	                        opts,
	                        name,
	                        this->prototype.size(),
	                        &this->prototype.front(),
	                        text,
	                        strlen(text),
	                        &(*this)))
	{
		throw syntax_error("Failed to compile function literal");
	}
}

ircd::js::function::literal::literal(literal &&other)
noexcept
:root<JSFunction *>
{
	std::move(other)
}
,name{std::move(other.name)}
,text{std::move(other.text)}
,prototype{std::move(other.prototype)}
{
}

ircd::js::function::literal
operator ""_function(const char *const text, const size_t len)
{
	return { "<literal>", {}, text };
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/function_native.h
//

ircd::js::function::native::native(const char *const &name,
                                   const uint &flags,
                                   const uint &arity,
                                   const closure &lambda)
:native::root::type{[this, &name, &flags, &arity]
{
	const auto jsf(::js::DefineFunctionWithReserved(*cx,
	                                                object(),
	                                                name,
	                                                handle_call,
	                                                arity,
	                                                flags));
	if(unlikely(!jsf))
		throw internal_error("Failed to create function::native '%s'", name);

	js::function ret(jsf);
	::js::SetFunctionNativeReserved(ret, 0, pointer_value(this));
	return ret;
}()}
,_name{name}
,lambda{lambda}
{
}

ircd::js::function::native::~native()
noexcept
{
}

ircd::js::value
ircd::js::function::native::operator()(const object::handle &that,
                                       const vector<value>::handle &argv)
const
{
	return call(*this, that, argv);
}

ircd::js::string
ircd::js::function::native::name()
const
{
	return JS_GetFunctionId(*this);
}


ircd::js::string
ircd::js::function::native::display_name()
const
{
	return JS_GetFunctionDisplayId(*this);
}

uint16_t
ircd::js::function::native::arity()
const
{
	return JS_GetFunctionArity(*this);
}

bool
ircd::js::function::native::handle_call(JSContext *const c,
                                        const unsigned argc,
                                        JS::Value *const argv)
noexcept try
{
	assert(&our(c) == cx);

	const struct args args(argc, argv);
	const object func(args.callee());
	auto &instance(from(func));
	if(args.isConstructing())
	{
		log.debug("native(%p) %s() ctor argv[%u]",
		          (const void *)&instance,
		          instance._name,
		          argc);

		const value that(instance.on_new(func, args));
		args.rval().set(that);
		log.debug("native(%p) this(%p) %s() leave",
		          (const void *)&instance,
		          (const void *)that.address(),
		          instance._name);
	} else {
		const value that(args.computeThis(c));
		log.debug("native(%p) this(%p) %s() call argv[%u]",
		          (const void *)&instance,
		          (const void *)that.address(),
		          instance._name,
		          argc);

		args.rval().set(instance.on_call(func, that, args));
		log.debug("native(%p) this(%p) %s() leave",
		          (const void *)&instance,
		          (const void *)that.address(),
		          instance._name);
	}

	return true;
}
catch(const jserror &e)
{
	e.set_pending();
	return false;
}
catch(const std::exception &e)
{
	const struct args args(argc, argv);
	const object func(args.callee());
	const function::native &instance(from(func));
	log.error("native(%p) \"%s()\": %s",
	          std::addressof(instance),
	          instance._name,
	          e.what());

	JS_ReportErrorUTF8(*cx, "BUG: native(%p) \"%s()\": %s",
	                   std::addressof(instance),
	                   instance._name,
	                   e.what());
	return false;
}

ircd::js::function::native &
ircd::js::function::native::from(JSObject *const &func)
{
	const auto tval(::js::GetFunctionNativeReserved(func, 0));
	return *pointer_value<function::native>(tval);
}

ircd::js::value
ircd::js::function::native::on_call(object::handle obj,
                                    value::handle val,
                                    const args &args)
{
	return lambda(obj, val, args);
}

ircd::js::value
ircd::js::function::native::on_new(object::handle obj,
                                   const args &args)
{
	value ud;
	return on_call(obj, ud, args);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/for_each.h
//

void
ircd::js::for_each(object::handle obj,
                   const each_key_val &closure)
{
	for_each(obj, iter::none, closure);
}

void
ircd::js::for_each(object::handle obj,
                   const iter &flags,
                   const each_key_val &closure)
{
	for_each(obj, flags, each_id([&obj, &closure]
	(const id &key)
	{
		const value val(get(obj, key));
		closure(key, val);
	}));
}

void
ircd::js::for_each(object::handle obj,
                   const each_key &closure)
{
	for_each(obj, iter::none, closure);
}

void
ircd::js::for_each(object::handle obj,
                   const iter &flags,
                   const each_key &closure)
{
	for_each(obj, flags, each_id([&obj, &closure]
	(const id &id)
	{
		closure(id);
	}));
}

void
ircd::js::for_each(object::handle obj,
                   const each_id &closure)
{
	for_each(obj, iter::none, closure);
}

void
ircd::js::for_each(object::handle obj,
                   const iter &flags,
                   const each_id &closure)
{
	JS::AutoIdVector props(*cx);
	if(::js::GetPropertyKeys(*cx, obj, uint(flags), &props))
		for(size_t i(0); i < props.length(); ++i)
			closure(props[i]);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/call.h
//

ircd::js::value
ircd::js::call(const std::string &name,
               const object::handle &that,
               const vector<value>::handle &args)
{
	return call(name.c_str(), that, args);
}

ircd::js::value
ircd::js::call(const char *const &name,
               const object::handle &that,
               const vector<value>::handle &args)
{
	value ret;
	if(!JS_CallFunctionName(*cx, that, name, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::call(const value::handle &val,
               const object::handle &that,
               const vector<value>::handle &args)
{
	value ret;
	if(!JS_CallFunctionValue(*cx, that, val, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::call(const function::handle &func,
               const object::handle &that,
               const vector<value>::handle &args)
{
	value ret;
	if(!JS_CallFunction(*cx, that, func, args, &ret))
		throw jserror(jserror::pending);

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/del.h
//

void
ircd::js::del(const object::handle &src,
              const char *const path)
{
	value val;
	object obj(src);
	const char *fail(nullptr);
	char buffer[strlen(path) + 1];
	ircd::tokens(path, ".", mutable_buffer{buffer, sizeof(buffer)}, [&path, &val, &obj, &fail]
	(const string_view &part)
	{
		if(fail)
			throw type_error("cannot recurse through non-object '%s' in `%s'", fail, path);

		if(!JS_GetProperty(*cx, obj, part.data(), &val) || undefined(val))
			throw reference_error("%s", part.data());

		object tmp(obj.get());
		if(!JS_ValueToObject(*cx, val, &obj) || !obj.get())
		{
			fail = part.data();
			obj = std::move(tmp);
		}
	});

	del(obj, id(val));
}

void
ircd::js::del(const object::handle &obj,
              const uint32_t &idx)
{
	JS::ObjectOpResult res;
	if(!JS_DeleteElement(*cx, obj, idx, res))
		throw jserror(jserror::pending);

	if(!res.checkStrict(*cx, obj))
		throw jserror(jserror::pending);
}

void
ircd::js::del(const object::handle &obj,
              const id &id)
{
	return del(obj, id::handle(id));
}

void
ircd::js::del(const object::handle &obj,
              const id::handle &id)
{
	JS::ObjectOpResult res;
	if(!JS_DeletePropertyById(*cx, obj, id, res))
		throw jserror(jserror::pending);

	if(!res.checkStrict(*cx, obj, id))
		throw jserror(jserror::pending);
}

void
ircd::js::del(JSObject *const &obj,
              priv_t)
{
	if(unlikely(~flags(obj) & JSCLASS_HAS_PRIVATE))
		throw error("del(priv): Object has no private slot");

	void *const existing(JS_GetPrivate(obj));
	delete reinterpret_cast<priv_ptr *>(existing);
	JS_SetPrivate(obj, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/set.h
//

void
ircd::js::set(const object::handle &src,
              const char *const path,
              const value &val)
{
	object obj(src);
	char buffer[strlen(path) + 1];
	const char *fail(nullptr), *key(nullptr);
	ircd::tokens(path, ".", mutable_buffer{buffer, sizeof(buffer)}, [&path, &obj, &fail, &key]
	(const string_view &part)
	{
		if(fail)
			throw type_error("cannot recurse through non-object '%s' in `%s'", fail, path);

		if(key)
			throw reference_error("%s", part.data());

		key = part.data();
		if(strcmp(key, path) == 0)
			return;

		value tmp;
		if(!JS_GetProperty(*cx, obj, part.data(), &tmp) || undefined(tmp))
			return;

		if(!JS_ValueToObject(*cx, tmp, &obj) || !obj.get())
			fail = part.data();
	});

	if(!key)
		return;

	if(!JS_SetProperty(*cx, obj, key, val))
		throw jserror(jserror::pending);
}

void
ircd::js::set(const object::handle &obj,
              const id &id,
              const value &val)
{
	set(obj, id::handle(id), val);
}

void
ircd::js::set(const object::handle &obj,
              const id::handle &id,
              const value &val)
{
	set(obj, id, value::handle(val));
}

void
ircd::js::set(const object::handle &obj,
              const id::handle &id,
              const value::handle &val)
{
	if(!JS_SetPropertyById(*cx, obj, id, val))
		throw jserror(jserror::pending);
}

void
ircd::js::set(JSObject *const &obj,
              priv_data &data)
{
	set(obj, shared_from(data));
}

void
ircd::js::set(JSObject *const &obj,
              const std::shared_ptr<priv_data> &data)
{
	if(unlikely(~flags(obj) & JSCLASS_HAS_PRIVATE))
		throw error("set(priv): Object has no private slot");

	void *const existing(JS_GetPrivate(obj));
	delete reinterpret_cast<priv_ptr *>(existing);
	JS_SetPrivate(obj, new priv_ptr(data));
}

void
ircd::js::set(JSObject *const &obj,
              const reserved &slot,
              const JS::Value &val)
{
	JS_SetReservedSlot(obj, uint(slot), val);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/get.h
//

ircd::js::value
ircd::js::get(const object::handle &src,
              const char *const path)
{
	value ret;
	object obj(src);
	const char *fail(nullptr);
	char buffer[strlen(path) + 1];
	ircd::tokens(path, ".", mutable_buffer{buffer, sizeof(buffer)}, [&obj, &path, &ret, &fail]
	(const string_view &part)
	{
		if(fail)
			throw type_error("cannot recurse through non-object '%s' in `%s'", fail, path);

		if(!JS_GetProperty(*cx, obj, part.data(), &ret) || undefined(ret))
			throw reference_error("%s", part.data());

		if(!JS_ValueToObject(*cx, ret, &obj) || !obj.get())
			fail = part.data();
	});

	return ret;
}

ircd::js::value
ircd::js::get(const object::handle &obj,
              const uint32_t &idx)
{
	value ret;
	if(!JS_GetElement(*cx, obj, idx, &ret) || undefined(ret))
		throw reference_error("[%u]", idx);

	return ret;
}

ircd::js::value
ircd::js::get(const object::handle &obj,
              const id &id)
{
	return get(obj, id::handle(id));
}

ircd::js::value
ircd::js::get(const object::handle &obj,
              const id::handle &id)
{
	value ret;
	if(!JS_GetPropertyById(*cx, obj, id, &ret) || undefined(ret))
		throw reference_error("%s", string(id).c_str());

	return ret;
}

JS::Value
ircd::js::get(JSObject *const &obj,
              const reserved &slot)
{
	return JS_GetReservedSlot(obj, uint(slot));
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/has.h
//

bool
ircd::js::has(const object::handle &src,
              const char *const path)
{
	bool ret(true);
	object obj(src);
	const char *fail(nullptr);
	char buffer[strlen(path) + 1];
	ircd::tokens(path, ".", mutable_buffer{buffer, sizeof(buffer)}, [&obj, &path, &ret, &fail]
	(const string_view &part)
	{
		if(fail)
			throw type_error("cannot recurse through non-object '%s' in `%s'", fail, path);

		if(!JS_HasProperty(*cx, obj, part.data(), &ret))
			throw jserror(jserror::pending);

		if(!ret)
			return;

		value tmp;
		if(!JS_GetProperty(*cx, obj, part.data(), &tmp) || undefined(tmp))
		{
			ret = false;
			return;
		}

		if(!JS_ValueToObject(*cx, tmp, &obj) || !obj.get())
			fail = part.data();
	});

	return ret;
}

bool
ircd::js::has(const object::handle &obj,
              const uint32_t &idx)
{
	bool ret;
	if(!JS_HasElement(*cx, obj, idx, &ret))
		throw jserror(jserror::pending);

	return ret;
}

bool
ircd::js::has(const object::handle &obj,
              const id &id)
{
	return has(obj, id::handle(id));
}

bool
ircd::js::has(const object::handle &obj,
              const id::handle &id)
{
	bool ret;
	if(!JS_HasPropertyById(*cx, obj, id, &ret))
		throw jserror(jserror::pending);

	return ret;
}

bool
ircd::js::has(const JSObject *const &obj,
              priv_t)
{
	if(~flags(obj) & JSCLASS_HAS_PRIVATE)
		return false;

	const auto vp(JS_GetPrivate(const_cast<JSObject *>(obj)));
	const auto sp(reinterpret_cast<const priv_ptr *>(vp));
	return sp && !!*sp;
}

bool
ircd::js::has(const JSObject *const &obj,
              const reserved &slot)
{
	return flags(obj) & JSCLASS_HAS_RESERVED_SLOTS(uint(slot));
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/object.h
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/priv.h
//

// Anchor the struct priv_data vtable here.
ircd::js::priv_data::~priv_data()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/string.h
//

char *
ircd::js::c_str(const JSString *const &str)
{
	static thread_local char cbuf[CSTR_BUFS][CSTR_BUFSIZE];
	static thread_local size_t ctr;

	char *const buf(cbuf[ctr]);
	native(str, buf, CSTR_BUFSIZE);
	ctr = (ctr + 1) % CSTR_BUFS;
	return buf;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/value.h
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/json.h
//

namespace ircd {
namespace js   {
namespace json {

bool write_callback(const char16_t *, uint32_t, void *) noexcept;

} // namespace json
} // namespace js
} // namespace ircd

ircd::js::value
ircd::js::json::parse(const string &string)
{
	value ret;
	if(!JS_ParseJSON(*cx, string, &ret))
		throw jserror(jserror::pending);

	return ret;
}

ircd::js::value
ircd::js::json::parse(const char *const &str)
{
	return parse(locale::char16::conv(str));
}

ircd::js::value
ircd::js::json::parse(const std::string &str)
{
	return parse(locale::char16::conv(str));
}

ircd::js::value
ircd::js::json::parse(const std::u16string &str)
{
	return parse(str.c_str(), str.size());
}

ircd::js::value
ircd::js::json::parse(const char16_t *const &str,
                      const size_t &size)
{
	value ret;
	if(!JS_ParseJSON(*cx, str, size, &ret))
		throw jserror(jserror::pending);

	return ret;
}

std::u16string
ircd::js::json::stringify(const value &val,
                          const bool &pretty)
{
	value v(val);
	return stringify(value::handle_mutable(&v), pretty);
}

std::u16string
ircd::js::json::stringify(value &v,
                          const bool &pretty)
{
	return stringify(value::handle_mutable(&v), pretty);
}

std::u16string
ircd::js::json::stringify(const value::handle_mutable &val,
                          const bool &pretty)
{
	const object fmtr;
	const string sp(string::literal, pretty? u"\t" : u"");
	return stringify(val, fmtr, sp);
}

std::u16string
ircd::js::json::stringify(const value::handle_mutable &val,
                          const JS::HandleObject &fmtr,
                          const value &sp)
{
	std::u16string ret;
	stringify(val, fmtr, sp, [&ret]
	(const char16_t *const &ptr, const uint &len)
	{
		ret.assign(ptr, len);
		return true;
	});

	return ret;
}

void
ircd::js::json::stringify(const value::handle_mutable &val,
                          const closure &cont)
{
	value sp;
	object fmtr;
	return stringify(val, fmtr, sp, cont);
}

void
ircd::js::json::stringify(const value::handle_mutable &val,
                          const JS::HandleObject &fmtr,
                          const value &sp,
                          const closure &cont)
{
	if(!JS_Stringify(*cx, val, fmtr, sp, write_callback, const_cast<closure *>(&cont)))
		throw jserror(jserror::pending);
}

bool
ircd::js::json::write_callback(const char16_t *const str,
                               const uint32_t len,
                               void *const priv)
noexcept
{
	const auto &func(*reinterpret_cast<const closure *>(priv));
	return func(str, len);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/native.h
//

namespace ircd {
namespace js   {

void native_external_noop(JS::Zone *const zone, const JSStringFinalizer *const fin, char16_t *const buf);
void native_external_deleter(JS::Zone *const zone, const JSStringFinalizer *const fin, char16_t *const buf);

JSStringFinalizer native_external_delete
{
	native_external_deleter
};

JSStringFinalizer native_external_static
{
	native_external_noop
};

} // namespace js
} // namespace ircd

std::string
ircd::js::native(const JSString *const &s)
{
	std::string ret(native_size(s) + 1, char());
	native(s, &ret.front(), ret.size());
	ret.resize(ret.size() - 1);
	return ret;
}

size_t
ircd::js::native_size(const JSString *const &s)
{
	return JS_GetStringEncodingLength(*cx, const_cast<JSString *>(s));
}

size_t
ircd::js::native(const JSString *const &s,
                 char *const &buf,
                 const size_t &max)
{
	if(unlikely(!max))
		return 0;

	ssize_t ret(s? JS_EncodeStringToBuffer(*cx, const_cast<JSString *>(s), buf, max) : 0);
	ret = std::max(ret, ssize_t(0));
	ret = std::min(ret, ssize_t(max - 1));
	buf[ret] = '\0';
	return ret;
}

void
ircd::js::native_external_deleter(JS::Zone *const zone,
                                  const JSStringFinalizer *const fin,
                                  char16_t *const buf)
{
	log.debug("context(%p) zone(%p): string(%p) delete (dtor @%p) \"%s\"",
	          (const void *)cx,
	          (const void *)zone,
	          (const void *)buf,
	          (const void *)fin,
	          locale::char16::conv(buf).c_str());

	delete[] buf;
}

void
ircd::js::native_external_noop(JS::Zone *const zone,
                               const JSStringFinalizer *const fin,
                               char16_t *const buf)
{
	log.debug("string literal release (zone: %p fin: %p buf: %p)",
	          (const void *)zone,
	          (const void *)fin,
	          (const void *)buf);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/error.h
//

ircd::js::jserror::jserror(const JS::Value &val)
:ircd::js::error{generate_skip}
,val{val}
{
}

ircd::js::jserror::jserror(JSObject *const &obj)
:ircd::js::error{generate_skip}
,val{obj? JS::ObjectValue(*obj) : JS::UndefinedValue() }
{
}

ircd::js::jserror::jserror(JSObject &obj)
:ircd::js::error{generate_skip}
,val{JS::ObjectValue(obj)}
{
}

ircd::js::jserror::jserror(generate_skip_t)
:ircd::js::error(generate_skip)
,val{}
{
}

ircd::js::jserror::jserror(const JSErrorReport &report)
:ircd::js::error{generate_skip}
,val{}
{
	create(report);
}

ircd::js::jserror::jserror(pending_t)
:ircd::js::error{generate_skip}
,val{}
{
	//if(unlikely(!restore_exception(*cx)))
	//	throw error("No exception pending");

	value val;
	if(unlikely(!JS_GetPendingException(*cx, &val)))
		throw error("Failed to recover pending exception");

	object obj(val);
	const auto rp(JS_ErrorFromException(*cx, obj));
	if(unlikely(!rp))
		throw error("Error report from exception failed");

	auto &report(*rp);
	generate_what_our(report);
	log.debug("jserror(%p): from pending [%s]: %s",
	          (const void *)this,
	          debug(report),
	          what());

	this->val = val;
	clear_exception(*cx);
}

void
ircd::js::jserror::set_uncatchable()
const
{
	set_pending();
	clear_exception(*cx);
}

void
ircd::js::jserror::set_pending()
const
{
	JS_SetPendingException(*cx, &val);
	save_exception(*cx);
}

void
ircd::js::jserror::generate(const JSExnType &type,
                            const char *const &fmt,
                            const va_rtti &ap)
{
	ircd::exception::generate(fmt, ap);
	const auto msg(locale::char16::conv(what()));

	JSErrorReport report;
	//report.message = msg.c_str();
	report.exnType = type;

	create(report);
}

void
ircd::js::jserror::create(const JSErrorReport &report)
{
	JSErrorReport cpy(report);
	create(cpy);
}

void
ircd::js::jserror::create(JSErrorReport &report)
{
	JS::AutoFilename fn;
	const auto col(report.column? nullptr : &report.column);
	const auto line(report.lineno? nullptr : &report.lineno);
	DescribeScriptedCaller(*cx, &fn, line, col);

	JS::RootedString msg
	{
		*cx,
		//JS_NewStringCopyUTF8N(*cx, report.message())
	};

	JS::RootedString file
	{
		*cx,
		JS_NewStringCopyZ(*cx, fn.get()?: "<unknown>")
	};

	JS::RootedObject stack(*cx, nullptr);
	const auto type((JSExnType)report.exnType);
	if(!JS::CreateError(*cx,
	                    type,
	                    stack,
	                    file,
	                    report.lineno,
	                    report.column,
	                    &report,
	                    msg,
	                    &val))
	{
		throw error("Failed to construct jserror exception!");
	}

	JS::Rooted<JSObject *> obj(*cx);
	if(unlikely(!JS_ValueToObject(*cx, &val, &obj)))
		throw error("Failed to convert value to object on exception!");

	JS::Rooted<JS::Value> msgv(*cx, JS::StringValue(msg));
	if(unlikely(!JS_SetProperty(*cx, obj, "message", msgv)))
		throw error("Failed to set jserror.message property on exception!");

	generate_what_our(report);
}

void
ircd::js::jserror::generate_what_our(const JSErrorReport &report)
{
	char linebuf[64];
	snprintf(linebuf, sizeof(linebuf), "@%u+%u: ",
	         report.lineno,
	         report.column);

	const auto msg(report.message());
	const auto empty(false);
	snprintf(ircd::exception::buf, sizeof(ircd::exception::buf), "%s%s%s%s",
	         reflect((JSExnType)report.exnType),
	         empty? "." : ": ",
	         empty? "" : !report.lineno && !report.column? "" : linebuf,
	         msg.c_str());
}

void
ircd::js::jserror::generate_what_js(const JSErrorReport &report)
{
	//const auto str(native(::js::ErrorReportToString(*cx, const_cast<JSErrorReport *>(&report))));
	//snprintf(ircd::exception::buf, sizeof(ircd::exception::buf), "%s", str.c_str());
}

void
ircd::js::replace_message(JSErrorReport &report,
                          const char *fmt,
                          ...)
{
	va_list ap;
	va_start(ap, fmt);

	char buf[BUFSIZE];
	vsnprintf(buf, sizeof(buf), fmt, ap);

	const size_t ucsz(sizeof(buf) * 2);
	custom_ptr<char16_t> ucbuf(reinterpret_cast<char16_t *>(js_malloc(ucsz)), js_free);
	locale::char16::conv(buf, ucbuf.get(), ucsz);

	//custom_ptr<void> existing(const_cast<char16_t *>(report.message()), js_free);
	//report.ucmessage = ucbuf.release();

	va_end(ap);
}

#if defined(IRCD_JS_FIX)
void
::JSErrorReport::freeLinebuf()
{
	js_free(const_cast<char16_t *>(linebuf()));
}
#endif // IRCD_JS_FIX

#if defined(IRCD_JS_FIX)
void
::JSErrorReport::freeMessage()
{
}
#endif // IRCD_JS_FIX

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/compartment.h
//

ircd::js::compartment::compartment(const JSVersion &ver)
:compartment{*cx, ver}
{
}

ircd::js::compartment::compartment(context &c,
                                   const JSVersion &ver)
:compartment
{
	current_global(c)?: throw error("Cannot enter compartment without global"),
	c,
	ver
}
{
}

ircd::js::compartment::compartment(JSObject *const &obj,
                                   const JSVersion &ver)
:compartment{obj, *cx, ver}
{
}

ircd::js::compartment::compartment(JSObject *const &obj,
                                   context &c,
                                   const JSVersion &ver)
:c{&c}
,prev{JS_EnterCompartment(c, obj)}
,ours{current_compartment(c)}
,cprev{static_cast<compartment *>(JS_GetCompartmentPrivate(ours))}
{
	JS_SetCompartmentPrivate(ours, this);
	JS_SetVersionForCompartment(ours, ver);
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
	JS_IterateCompartments(*cx,
	                       const_cast<compartment::closure *>(&closure),
	                       compartment::handle_iterate);
}

void
ircd::js::compartment::handle_iterate(JSContext *const cx,
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

namespace ircd  {
namespace js    {

void handle_activity_ctypes(JSContext *, enum ::js::CTypesActivityType) noexcept;

} // namespace js
} // namespace ircd

ircd::js::context::context(const struct opts &opts,
                           JSContext *const &parent)
:context
{
	std::make_unique<struct opts>(opts),
	parent
}
{
}

ircd::js::context::context(std::unique_ptr<struct opts> opts,
                           JSContext *const &parent)
:custom_ptr<JSContext>
{
	// Construct the context
	[this, &opts, &parent]
	{
		const auto ret(JS_NewContext(opts->max_bytes, opts->max_nursery_bytes, parent));

		// Use their privdata pointer to point to our instance. We can then use our(JSContext*)
		// to get back to `this` instance.
		JS_SetContextPrivate(ret, this);

		return ret;
	}(),

	// Plant the destruction
	[](JSContext *const ctx)
	noexcept
	{
		if(!ctx)
			return;

		// Free the user's privdata managed object
		//delete static_cast<const priv_data *>(JS_GetSecondContextPrivate(ctx));

		JS_DestroyContext(ctx);
	}
}
,opts{std::move(opts)}
,tid{std::this_thread::get_id()}
,except{nullptr}
,state
{{
	0,                // Semaphore value starting at 0.
	phase::LEAVE,     // LEAVE phase indicates nothing is running.
	irq::JS,          // irq::JS is otherwise here in case JS triggers an interrupt.
}}
,timer
{
	std::bind(&context::handle_timeout, get())
}
,star{nullptr}
{
	JS::SetWarningReporter(get(), handle_error);
	JS_AddInterruptCallback(get(), handle_interrupt);
	//JS_SetAccumulateTelemetryCallback(get(), handle_telemetry);
	JS::SetOutOfMemoryCallback(get(), handle_out_of_memory, nullptr);
	JS::SetLargeAllocationFailureCallback(get(), handle_large_allocation_failure, nullptr);
	JS_SetGCCallback(get(), handle_gc, nullptr);
	JS::SetGCSliceCallback(get(), handle_slice);
	JS_SetObjectsTenuredCallback(get(), handle_objects_tenured, nullptr);
	JS_AddFinalizeCallback(get(), handle_finalize, nullptr);
	JS_SetCompartmentNameCallback(get(), handle_compartment_name);
	JS_SetDestroyCompartmentCallback(get(), handle_compartment_destroy);
	JS_SetSweepZoneCallback(get(), handle_zone_sweep);
	JS_SetDestroyZoneCallback(get(), handle_zone_destroy);
	::js::SetPreserveWrapperCallback(get(), handle_preserve_wrapper);
	JS_SetGrayGCRootsTracer(get(), handle_trace_gray, nullptr);
	JS_AddExtraGCRootsTracer(get(), handle_trace_extra, nullptr);
	::js::SetActivityCallback(get(), handle_activity, this);
	::js::SetCTypesActivityCallback(get(), handle_activity_ctypes);
	JS::SetEnqueuePromiseJobCallback(get(), handle_promise_enqueue_job, this);
	JS::SetPromiseRejectionTrackerCallback(get(), handle_promise_rejection_tracker, this);
	JS::SetAsyncTaskCallbacks(get(), handle_start_async_task, handle_finish_async_task);
	JS::SetGetIncumbentGlobalCallback(get(), handle_get_incumbent_global);
	::js::SetStopwatchStartCallback(get(), handle_stopwatch_start, this);
	::js::SetStopwatchCommitCallback(get(), handle_stopwatch_commit, this);
	::js::SetGetPerformanceGroupsCallback(get(), handle_get_performance_groups, nullptr);

	timer.set(this->opts->timer_limit);
	JS_SetNativeStackQuota(get(), this->opts->code_stack_max, this->opts->trusted_stack_max, this->opts->untrusted_stack_max);
	JS_SetParallelParsingEnabled(get(), this->opts->concurrent_parsing);
	JS_SetOffthreadIonCompilationEnabled(get(), this->opts->concurrent_jit);
	JS_SetGCZeal(get(), this->opts->gc_zeal_mode, this->opts->gc_zeal_freq);

	if(!JS::InitSelfHostedCode(get()))
		throw error("JS::InitSelfHostedCode: failure.");
}

ircd::js::context::~context()
noexcept
{
	// If items still exist on the tracing lists at this point (runtime shutdown):
	// that is bad. The objects are still reachable but should have removed themselves.
	if(unlikely(!tracing.heap.empty()))
	{
		// When the context terminates with an active exception the exception itself
		// can be ignored and leak. This case should eventually lead to IRCd termination...
		if(std::uncaught_exception() && tracing.heap.size() == 1)
			return;

		log.critical("context(%p): !!! LEAK !!! %zu traceable items still reachable on the heap",
		             (const void *)this,
		             tracing.heap.size());
		assert(0);
	}
}

void
ircd::js::leave(context &c)
{
	c.timer.cancel();

	// Load the state to keep the current sem value up to date.
	// This thread is the only writer to that value.
	auto state(c.state.load(std::memory_order_relaxed));

	// The LEAVE phase locks out the interruptor
	state.phase = phase::LEAVE;

	// The LEAVE is released and the current phase seen by interruptor is acquired.
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
                    const irq &req)
{
	if(req == irq::NONE)
		return false;

	// Acquire the execution state. Proceed if something was running.
	const auto state(c.state.load(std::memory_order_acquire));
	if(state.phase != phase::ENTER)
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
	JS_RequestInterruptCallback(c);
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
ircd::js::save_exception(context &c)
{
	if(c.except)
	{
		//log.warning("save_exception(): Dropping unrestored exception @ %p", (const void *)c.except);
		//JS_DropExceptionState(*cx, c.except);
		throw error("(internal error): Won't overwrite saved exception @ %p", (const void *)c.except);
	}

	c.except = JS_SaveExceptionState(c);
}

bool
ircd::js::maybe_gc(context &c)
noexcept
{
	// JS_MaybeGC dereferences the context's current zone without checking if the context
	// is even in a compartment/has a current zone; we must check here.
	if(!current_zone(c))
		return false;

	JS_MaybeGC(c);
	return true;
}

void
ircd::js::out_of_memory(context &c)
{
	JS_ReportOutOfMemory(c);
}

void
ircd::js::allocation_overflow(context &c)
{
	JS_ReportAllocationOverflow(c);
}

uint32_t
ircd::js::get(context &c,
              const JSGCParamKey &key)
{
	//return JS_GetGCParameterForThread(c, key);       // broken
	return JS_GetGCParameter(c, key);
}

void
ircd::js::set(context &c,
              const JSGCParamKey &key,
              const uint32_t &val)
{
	//JS_SetGCParameterForThread(c, key, val);         // broken
	JS_SetGCParameter(c, key, val);
}

bool
ircd::js::run_gc(context &c)
noexcept
{
	JS_GC(c);
	return true;
}

//
// Callback surface
//

JSObject *
ircd::js::context::handle_get_incumbent_global(JSContext *const cx)
noexcept
{
	auto &c(our(cx));
	log.debug("context(%p): get incumbent global (current_global: %p)",
	          (const void *)cx,
	          current_global(c));

	return current_global(c);
}

bool
ircd::js::context::handle_start_async_task(JSContext *const cx,
                                           JS::AsyncTask *const task)
noexcept
{
	log.debug("context(%p): async task(%p) START",
	          (const void *)cx,
	          task);

	return true;
}

bool
ircd::js::context::handle_finish_async_task(JS::AsyncTask *const task)
noexcept
{
	log.debug("context(%p): async task(%p) FINISH",
	          nullptr,
	          task);

	return true;
}

bool
ircd::js::context::handle_promise_enqueue_job(JSContext *const c,
                                              JS::HandleObject job,
                                              JS::HandleObject allocation_site,
                                              JS::HandleObject incumbent_global,
                                              void *const priv)
noexcept
{
	log.debug("context(%p): promise enqueue job (priv: %p)",
	          (const void *)c,
	          priv);

//	dump(job);
//	printf("--\n");
//	dump(allocation_site);

	return true;
}

void
ircd::js::context::handle_promise_rejection_tracker(JSContext *const c,
                                                    JS::HandleObject promise,
                                                    PromiseRejectionHandlingState state,
                                                    void *const priv)
noexcept
{
	log.debug("context(%p): promise rejection track (state: %s priv: %p)",
	          (const void *)c,
	          reflect(state),
	          priv);
}

void
ircd::js::context::handle_activity(void *const priv,
                                   const bool active)
noexcept
{
	assert(priv);
	auto &c(*static_cast<struct context *>(priv));
	//const auto tid(std::this_thread::get_id());
	const auto &msg(active? "ENTER" : "LEAVE");
	log.debug("context(%p): %s",
	          //ircd::string(tid).c_str(),
	          (const void *)c.ptr(),
	          msg);
}

void
ircd::js::handle_activity_ctypes(JSContext *const cx,
                                 const ::js::CTypesActivityType t)
noexcept
{
	log.debug("context(%p): %s",
	          (const void *)cx,
	          reflect(t));
}

bool
ircd::js::context::handle_preserve_wrapper(JSContext *const cx,
                                           JSObject *const obj)
noexcept
{
	log.debug("context(%p): (object: %p) preserve wrapper",
	          (const void *)cx,
	          (const void *)obj);

	return true;
}

void
ircd::js::context::handle_gc(JSContext *const cx,
                             const JSGCStatus status,
                             void *const priv)
noexcept
{
	log.debug("context(%p): GC %s (priv: %p)",
	          (const void *)cx,
	          reflect(status),
	          (const void *)priv);
}

void
ircd::js::context::handle_slice(JSContext *const cx,
                                JS::GCProgress progress,
                                const JS::GCDescription &d)
noexcept
{
	log.debug("context(%p): SLICE: %s (description: %p)",
	          (const void *)cx,
	          reflect(progress),
	          (const void *)&d);
}

void
ircd::js::context::handle_objects_tenured(JSContext *const cx,
                                          void *const priv)
noexcept
{
	log.debug("context(%p): objects tenured (priv: %p)",
	          (const void *)cx,
	          (const void *)priv);
}

void
ircd::js::context::handle_finalize(JSFreeOp *const fop,
                                   const JSFinalizeStatus status,
                                   const bool is_compartment,
                                   void *const priv)
noexcept
{
	log.debug("context(%p): fop(%p): %s %s (priv: %p)",
	          (const void *)cx,
	          (const void *)fop,
	          reflect(status),
	          is_compartment? "COMPARTMENT" : "",
	          priv);
}

void
ircd::js::context::handle_compartment_destroy(JSFreeOp *const fop,
                                              JSCompartment *const compartment)
noexcept
{
	auto *const c(our(compartment));
	auto *const cx(c? &static_cast<context &>(*c) : (context *)nullptr);
	log.debug("context(%p): compartment: %p %s%sdestroy: fop(%p)",
	          cx? (const void *)cx->ptr() : (const void *)nullptr,
	          (const void *)compartment,
	          ::js::IsSystemCompartment(compartment)? "[system] " : "",
	          ::js::IsAtomsCompartment(compartment)? "[atoms] " : "",
	          (const void *)fop);
}

void
ircd::js::context::handle_compartment_name(JSContext *const cx,
                                           JSCompartment *const compartment,
                                           char *const buf,
                                           const size_t max)
noexcept
{
	log.debug("context(%p): comaprtment: %p (buf@%p: max: %zu)",
	          (const void *)cx,
	          (const void *)compartment,
	          (const void *)buf,
	          max);
}

void
ircd::js::context::handle_zone_destroy(JS::Zone *const zone)
noexcept
{
	log.debug("context(%p): zone: %p %s%sdestroy",
	          (const void *)cx,
	          (const void *)zone,
	          ::js::IsSystemZone(zone)? "[system] " : "",
	          ::js::IsAtomsZone(zone)? "[atoms] " : "");
}

void
ircd::js::context::handle_zone_sweep(JS::Zone *const zone)
noexcept
{
	log.debug("context(%p): zone: %p %s%ssweep",
	          (const void *)cx,
	          (const void *)zone,
	          ::js::IsSystemZone(zone)? "[system] " : "",
	          ::js::IsAtomsZone(zone)? "[atoms] " : "");
}

void
ircd::js::context::handle_weak_pointer_compartment(JSContext *const cx,
                                                   JSCompartment *const comp,
                                                   void *const data)
noexcept
{
	log.debug("context(%p): weak pointer compartment(%p) %p",
	          (const void *)cx,
	          (const void *)comp,
	          data);
}

void
ircd::js::context::handle_weak_pointer_zone(JSContext *const cx,
                                            void *const data)
noexcept
{
	log.debug("context(%p): weak pointer zone %p",
	          (const void *)cx,
	          data);
}

void
ircd::js::context::handle_trace_extra(JSTracer *const tracer,
                                      void *const priv)
noexcept
{
	log.debug("context(%p): tracer(%p) %s: extra (priv: %p) count: %zu",
	          (const void *)cx,
	          (const void *)tracer,
	          debug(*tracer).c_str(),
	          priv,
	          cx->tracing.heap.size());

	if(unlikely(std::uncaught_exception() && cx->tracing.heap.size() == 1))
	{
		log.warning("context(%p): tracer(%p) %s: extra skipped due to uncaught exception",
		            (const void *)cx,
		            (const void *)tracer,
		            debug(*tracer).c_str());
		return;
	}

	cx->tracing(tracer);
}

void
ircd::js::context::handle_trace_gray(JSTracer *const tracer,
                                     void *const priv)
noexcept
{
	log.debug("context(%p): tracer(%p): gray (priv: %p)",
	          (const void *)cx,
	          (const void *)tracer,
	          priv);
}

void
ircd::js::context::handle_large_allocation_failure(void *const priv)
noexcept
{
	log.error("context(%p): Large allocation failure (priv: %p)",
	          (const void *)cx,
	          priv);

	assert(0); //TODO: XXX
}

void
ircd::js::context::handle_out_of_memory(JSContext *const cx,
                                        void *const priv)
noexcept
{
	log.error("context(%p): out of memory",
	          (const void *)cx);

	assert(0); //TODO: XXX
}

bool
ircd::js::context::handle_stopwatch_start(const uint64_t us,
                                          void *const priv)
noexcept
{
	assert(priv);
	auto &c(*static_cast<context *>(priv));
	log.debug("context(%p): stopwatch start (microseconds: %lu priv: %p)",
	          (const void *)c.get(),
	          us,
	          priv);

	return true;
}

bool
ircd::js::context::handle_stopwatch_commit(const uint64_t us,
                                           ::js::PerformanceGroupVector &vec,
                                           void *const priv)
noexcept
{
	assert(priv);
	auto &c(*static_cast<context *>(priv));
	log.debug("context(%p): stopwatch commit (microseconds: %lu priv: %p)",
	          (const void *)c.get(),
	          us,
	          priv);

	return true;
}

bool
ircd::js::context::handle_get_performance_groups(JSContext *const cx,
                                                 ::js::PerformanceGroupVector &vec,
                                                 void *const priv)
noexcept
{
	log.debug("context(%p): get performance groups (priv: %p)",
	          (const void *)cx,
	          priv);

	return true;
}

void
ircd::js::context::handle_telemetry(const int id,
                                    const uint32_t sample,
                                    const char *const key)
noexcept
{
	//const auto tid(std::this_thread::get_id());
	log.debug("context(%p): telemetry(%02d) %s: %u %s",
	          //ircd::string(tid).c_str(),
	          (const void *)cx,
	          id,
	          reflect_telemetry(id),
	          sample,
	          key?: "");
}

void
ircd::js::context::handle_timeout(JSContext *const cx)
noexcept
{
	// At this time there is no yield logic so if the timer calls the script is terminated.
	auto &c(our(cx));
	interrupt(c, irq::TERMINATE);
}

bool
ircd::js::context::handle_interrupt(JSContext *const cx)
noexcept
{
	auto &c(our(cx));
	auto state(c.state.load(std::memory_order_acquire));
	log.debug("context(%p): Interrupt: IRQ[%u] phase[%u]",
	          (const void *)cx,
	          uint(state.irq),
	          uint(state.phase));

	// Spurious interrupt; ignore.
	if(unlikely(state.phase != phase::INTR && state.phase != phase::ENTER))
	{
		log.warning("context(%p): Spurious interrupt (irq: %02x)",
		            (const void *)cx,
		            uint(state.irq));
		return true;
	}

	// After the interrupt is handled the phase indicates entry back to JS,
	// IRQ is left indicating JS in case we don't trigger the next interrupt.
	const unwind interrupt_return([&c, &state]
	{
		state.phase = phase::ENTER;
		state.irq = irq::JS;
		c.state.store(state, std::memory_order_release);
	});

	// Call the user hook if available
	if(c.on_intr)
	{
		// The user's handler returns -1 for non-overriding behavior
		const auto ret(c.on_intr(state.irq));
		if(ret != -1)
			return ret;
	}

	switch(state.irq)
	{
		case irq::JS:
		case irq::USER:
			return true;

		case irq::YIELD:
			ctx::yield();
			return true;

		case irq::TERMINATE:
			return false;

		default:
		case irq::NONE:
			assert(0);
			return false;
	}
}

void
ircd::js::context::handle_error(JSContext *const cx,
                                JSErrorReport *const report)
noexcept try
{
	assert(report);
	const log::facility facility
	{
		JSREPORT_IS_WARNING(report->flags)? log::WARNING:
		                                    log::DEBUG
	};

	log(facility, "context(%p): %s",
		(const void *)cx,
		debug(*report).c_str());

	auto &c(our(cx));
	if(JSREPORT_IS_EXCEPTION(report->flags))
	{
		// If except state is saved this is a redundant report from an exception as our
		// intertwined c++ -> js -> c++ stack blows up.
		if(c.except)
			return;

		// This is likely an uncaught exception from a throw in JS. We create a new exception
		// object because we lost the one from the user and make that pending now in case some
		// other opportunity for the user to catch this is presented.
		jserror e(*report);
		e.set_pending();
		return;
	}

	if(report->exnType == JSEXN_INTERNALERR)
	{
		static const std::string msg("god save jsapi");
		internal_error ie("%s", msg.c_str());
		ie.set_pending();
		return;
	}

	switch(report->errorNumber)
	{
		case 61: // JSAPI's code for interruption
		{
			report->exnType = JSEXN_ERR;
			report->flags |= JSREPORT_EXCEPTION;
			replace_message(*report, "interrupted @ line[%u] col[%u]",
			                report->lineno,
			                report->column);

			jserror e(*report);
			e.set_uncatchable();
			return;
		}

		case 105: // JSAPI's code for user reported error
		{
			report->exnType = JSEXN_INTERNALERR;
			report->flags |= JSREPORT_EXCEPTION;
			replace_message(*report, "(BUG) Host exception");
			jserror e(*report);
			e.set_uncatchable();
			return;
		}
	}
}
catch(const std::exception &e)
{
	log.critical("triple fault: %s\n", e.what());
	ircd::terminate();
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
// ircd/js/tracing.h
//

namespace ircd {
namespace js   {

void trace_heap(JSTracer *const &tracer, tracing::thing &thing);

} // namespace js
} // namespace ircd

ircd::js::tracing::tracing()
{
}

ircd::js::tracing::~tracing()
noexcept
{
	if(std::uncaught_exception() && heap.size() == 1)
		return;

	assert(heap.empty());
}

void
ircd::js::tracing::operator()(JSTracer *const &tracer)
{
	for(auto &thing : cx->tracing.heap)
		trace_heap(tracer, thing);
}

void
ircd::js::trace_heap(JSTracer *const &tracer,
                     tracing::thing &thing)
{
	if(thing.type != jstype::OBJECT)
		log.debug("context(%p): tracer(%p): heap<%s> @ %p",
		          nullptr, //(const void *)tracer->runtime(),
		          (const void *)tracer,
		          reflect(thing.type),
		          (const void *)thing.ptr);

	if(likely(thing.ptr)) switch(thing.type)
	{
		case jstype::VALUE:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JS::Value> *>(thing.ptr));
			if(!ptr->address())
				break;

			//JS_CallValueTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::OBJECT:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JSObject *> *>(thing.ptr));
			//if(!ptr->get())
				break;

			log.debug("context(%p): tracer(%p): heap<%s> @ %p object(%p trap: %p '%s')",
			          nullptr, //(const void *)tracer->runtime(),
			          (const void *)tracer,
			          reflect(thing.type),
			          (const void *)thing.ptr,
			          (const void *)ptr->get(),
			          (const void *)(has_jsclass(*ptr)? &jsclass(*ptr) : nullptr),
			          has_jsclass(*ptr)? jsclass(*ptr).name : "<no trap>");

			//JS_CallObjectTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::FUNCTION:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JSFunction *> *>(thing.ptr));
			//if(!ptr->get())
				break;

			//JS_CallFunctionTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::SCRIPT:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JSScript *> *>(thing.ptr));
			//if(!ptr->get())
				break;

			//JS_CallScriptTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::STRING:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JSString *> *>(thing.ptr));
			//if(!ptr->get())
				break;

			//JS_CallStringTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::ID:
		{
			const auto ptr(reinterpret_cast<JS::Heap<jsid> *>(thing.ptr));
			//if(!ptr->address())
				break;

			//JS_CallIdTracer(tracer, ptr, "heap");
			break;
		}

		case jstype::SYMBOL:
		{
			const auto ptr(reinterpret_cast<JS::Heap<JS::Symbol *> *>(thing.ptr));
			//if(!ptr->get())
				break;

			break;
		}

		default:
		{
			log.warning("context(%p): tracer(%p): heap<%s> @ %p",
			            nullptr, //(const void *)tracer->runtime(),
			            (const void *)tracer,
			            reflect(thing.type),
			            (const void *)thing.ptr);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/debug.h
//

void
ircd::js::log_gcparams()
{
	for(int i(0); i < 50; ++i)
	{
		const auto key(static_cast<JSGCParamKey>(i));
		const char *const name(reflect(key));
		if(!strlen(name))
			continue;

		// These trigger assertion failures
		switch(key)
		{
			case JSGC_NUMBER:
			case JSGC_MAX_MALLOC_BYTES:
			case JSGC_ALLOCATION_THRESHOLD:
				continue;

			default:
				break;
		}

		log.debug("context(%p) %s => %u",
		          (const void *)cx,
		          name,
		          get(*cx, key));
	}
}

void
ircd::js::backtrace()
{
	#ifdef JS_DEBUG
	::js::DumpBacktrace(*cx);
	#endif
}

void
ircd::js::dump_promise(const JS::HandleObject &promise)
{
	#ifdef JS_DEBUG
	JS::DumpPromiseAllocationSite(*cx, promise);
	JS::DumpPromiseResolutionSite(*cx, promise);
	#endif
}

void
ircd::js::dump(const JSString *const &v)
{
	#ifdef JS_DEBUG
	::js::DumpString(const_cast<JSString *>(v));
	#endif
}

void
ircd::js::dump(const JSAtom *const &v)
{
	#ifdef JS_DEBUG
	::js::DumpAtom(const_cast<JSAtom *>(v));
	#endif
}

void
ircd::js::dump(const JSObject *const &v)
{
	#ifdef JS_DEBUG
	::js::DumpObject(const_cast<JSObject *>(v));
	#endif
}

void
ircd::js::dump(const JS::Value &v)
{
	#ifdef JS_DEBUG
	::js::DumpValue(v);
	#endif
}

void
ircd::js::dump(const jsid &v)
{
	#ifdef JS_DEBUG
	::js::DumpId(v);
	#endif
}

void
ircd::js::dump(const JSContext *v)
{
	#ifdef JS_DEBUG
	::js::DumpPC(const_cast<JSContext *>(v));
	#endif
}

void
ircd::js::dump(const JSScript *const &v)
{
	#ifdef JS_DEBUG
	::js::DumpScript(*cx, const_cast<JSScript *>(v));
	#endif
}

void
ircd::js::dump(const char16_t *const &v, const size_t &len)
{
	#ifdef JS_DEBUG
	::js::DumpChars(v, len);
	#endif
}

void
ircd::js::dump(const ::js::InterpreterFrame *v)
{
	#ifdef JS_DEBUG
	::js::DumpInterpreterFrame(*cx, const_cast<::js::InterpreterFrame *>(v));
	#endif
}

std::string
ircd::js::debug(const JSTracer &t)
{
	return t.isMarkingTracer()?     "MARKING":
	       t.isWeakMarkingTracer()? "WEAKMARKING":
	       t.isTenuringTracer()?    "TENURING":
	       t.isCallbackTracer()?    "CALLBACK":
	                                "UNKNOWN";
}

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

	if(r.isMuted)
		ss << "MUTED ";

	if(r.filename)
		ss << "file[" << r.filename << "] ";

	if(r.lineno)
		ss << "line[" << r.lineno << "] ";

	if(r.column)
		ss << "col[" << r.column << "] ";

	if(r.linebuf())
		ss << "code[" << std::u16string(r.linebuf()) << "] ";

	//if(r.tokenptr)
	//	ss << "toke[" << r.tokenptr << "] ";

	if(r.errorNumber)
		ss << "errnum[" << r.errorNumber << "] ";

	if(r.exnType)
		ss << reflect(JSExnType(r.exnType)) << " ";

//	if(r.message())
//		ss << "\"" << locale::char16::conv(r.message()) << "\" ";

//	for(auto it(r.messageArgs); it && *it; ++it)
//		ss << "\"" << locale::char16::conv(*it) << "\" ";

	return ss.str();
}

std::string
ircd::js::debug(const JS::HandleObject &o)
{
	std::stringstream ss;

	if(JS_IsGlobalObject(o))            ss << "Global ";
	if(JS_IsNative(o))                  ss << "Native ";
	if(JS::IsCallable(o))               ss << "Callable ";
	if(JS::IsConstructor(o))            ss << "Constructor ";
	if(JS_ObjectIsFunction(*cx, o))     ss << "Function ";

	bool ret;
	if(JS_IsExtensible(*cx, o, &ret))
		if(ret)
			ss << "Extensible ";

	if(JS_IsArrayObject(*cx, o, &ret))
		if(ret)
			ss << "Array ";

	if(JS::IsArray(*cx, o, &ret))
		if(ret)
			ss << "Array.isArray ";

	if(JS_ObjectIsRegExp(*cx, o, &ret))
		if(ret)
			ss << "RegExp ";

	if(JS_ObjectIsDate(*cx, o, &ret))
		if(ret)
			ss << "Date ";

	if(JS::IsPromiseObject(o))
	{
		ss << "Promise[#" << JS::GetPromiseID(o);
		ss << " " << reflect(JS::GetPromiseState(o));
		ss << "] ";
	}

	if(JS_IsArrayBufferObject(o))
	{
		ss << "ArrayBuffer[" << JS_GetArrayBufferByteLength(o);
		ss << " " << JS_IsDetachedArrayBufferObject(o)? "DETACHED" : "ATTACHED";
		ss << " " << JS_IsMappedArrayBufferObject(o)? "MAPPED" : "UNMAPPED";
		ss << "] ";
	}

	if(JS_IsSharedArrayBufferObject(o))
	{
		ss << "SharedArrayBuffer[" << JS_GetSharedArrayBufferByteLength(o);
		ss << " " << JS_IsDetachedArrayBufferObject(o)? "DETACHED" : "ATTACHED";
		ss << " " << JS_IsMappedArrayBufferObject(o)? "MAPPED" : "UNMAPPED";
		ss << "] ";
	}

	if(JS_IsArrayBufferViewObject(o))
	{
		ss << "ArrayBufferView[" << JS_GetArrayBufferViewByteLength(o);
		ss << "] ";
	}

	if(JS_IsDataViewObject(o))
	{
		ss << "DataView[" << JS_GetDataViewByteLength(o);
		ss << " @" << JS_GetDataViewByteOffset(o);
		ss << "] ";
	}

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
	if(v.isSymbol())      ss << "Symbol ";
	if(v.isObject())      ss << "Object ";

	if(v.isObject())
	{
		JS::RootedObject obj(*cx);
		JS::RootedValue rv(*cx, v);
		if(JS_ValueToObject(*cx, rv, &obj))
			ss << "(" << debug(obj) << ") ";
	}

	return ss.str();
}

const char *
ircd::js::reflect_telemetry(const int &id)
{
	switch(id)
	{
		case JS_TELEMETRY_GC_REASON:                                  return "GC_REASON";
		case JS_TELEMETRY_GC_MS:                                      return "GC_MS";
		case JS_TELEMETRY_GC_BUDGET_MS:                               return "GC_BUDGET_MS";
		case JS_TELEMETRY_GC_ANIMATION_MS:                            return "GC_ANIMATION_MS";
		case JS_TELEMETRY_GC_MAX_PAUSE_MS:                            return "GC_MAX_PAUSE_MS";
		case JS_TELEMETRY_GC_MARK_MS:                                 return "GC_MARK_MS";
		case JS_TELEMETRY_GC_SWEEP_MS:                                return "GC_SWEEP_MS";
		case JS_TELEMETRY_GC_MARK_ROOTS_MS:                           return "GC_MARK_ROOTS_MS";
		case JS_TELEMETRY_GC_MARK_GRAY_MS:                            return "GC_MARK_GRAY_MS";
		case JS_TELEMETRY_GC_SLICE_MS:                                return "GC_SLICE_MS";
		case JS_TELEMETRY_GC_SLOW_PHASE:                              return "GC_SLOW_PHASE";
		case JS_TELEMETRY_GC_MMU_50:                                  return "GC_MMU_50";
		case JS_TELEMETRY_GC_RESET:                                   return "GC_RESET";
		case JS_TELEMETRY_GC_INCREMENTAL_DISABLED:                    return "GC_INCREMENTAL_DISABLED";
		case JS_TELEMETRY_GC_NON_INCREMENTAL:                         return "GC_NON_INCREMENTAL";
		case JS_TELEMETRY_GC_SCC_SWEEP_TOTAL_MS:                      return "GC_SCC_SWEEP_TOTAL_MS";
		case JS_TELEMETRY_GC_SCC_SWEEP_MAX_PAUSE_MS:                  return "GC_SCC_SWEEP_MAX_PAUSE_MS";
		case JS_TELEMETRY_GC_MINOR_REASON:                            return "GC_MINOR_REASON";
		case JS_TELEMETRY_GC_MINOR_REASON_LONG:                       return "GC_MINOR_REASON_LONG";
		case JS_TELEMETRY_GC_MINOR_US:                                return "GC_MINOR_US";
		case JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_CONTENT:  return "DEPRECATED_LANGUAGE_EXTENSIONS_IN_CONTENT";
		case JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_ADDONS:   return "DEPRECATED_LANGUAGE_EXTENSIONS_IN_ADDONS";
		case JS_TELEMETRY_ADDON_EXCEPTIONS:                           return "ADDON_EXCEPTIONS";
	}

	return "";
}

const char *
ircd::js::reflect_prop(const uint &flag)
{
	switch(flag)
	{
		case JSPROP_ENUMERATE:                  return "JSPROP_ENUMERATE";
		case JSPROP_READONLY:                   return "JSPROP_READONLY";
		case JSPROP_PERMANENT:                  return "JSPROP_PERMANENT";
		case JSPROP_PROPOP_ACCESSORS:           return "JSPROP_PROPOP_ACCESSORS";
		case JSPROP_GETTER:                     return "JSPROP_GETTER";
		case JSPROP_SETTER:                     return "JSPROP_SETTER";
		case JSPROP_SHARED:                     return "JSPROP_SHARED";
		case JSPROP_INTERNAL_USE_BIT:           return "JSPROP_INTERNAL_USE_BIT";
		case JSFUN_STUB_GSOPS:                  return "JSFUN_STUB_GSOPS";
		case JSFUN_CONSTRUCTOR:                 return "JSFUN_CONSTRUCTOR";
		case JSPROP_REDEFINE_NONCONFIGURABLE:   return "JSPROP_REDEFINE_NONCONFIGURABLE";
		case JSPROP_RESOLVING:                  return "JSPROP_RESOLVING";
		case JSPROP_IGNORE_ENUMERATE:           return "JSPROP_IGNORE_ENUMERATE";
		case JSPROP_IGNORE_READONLY:            return "JSPROP_IGNORE_READONLY";
		case JSPROP_IGNORE_PERMANENT:           return "JSPROP_IGNORE_PERMANENT";
		case JSPROP_IGNORE_VALUE:               return "JSPROP_IGNORE_VALUE";
	}

	return "";
}

const char *
ircd::js::reflect(const JS::PromiseState &state)
{
	switch(state)
	{
		case JS::PromiseState::Pending:     return "Pending";
		case JS::PromiseState::Fulfilled:   return "Fulfilled";
		case JS::PromiseState::Rejected:    return "Rejected";
	}

	return "";
}

const char *
ircd::js::reflect(const PromiseRejectionHandlingState &state)
{
	switch(state)
	{
		case PromiseRejectionHandlingState::Unhandled:   return "Unhandled";
		case PromiseRejectionHandlingState::Handled:     return "Handled";
	}

	return "";
}

const char *
ircd::js::reflect(const ::js::CTypesActivityType &t)
{
	using namespace ::js;

	switch(t)
	{
		case CTYPES_CALL_BEGIN:       return "CTYPES_CALL_BEGIN";
		case CTYPES_CALL_END:         return "CTYPES_CALL_END";
		case CTYPES_CALLBACK_BEGIN:   return "CTYPES_CALLBACK_BEGIN";
		case CTYPES_CALLBACK_END:     return "CTYPES_CALLBACK_END";
	}

	return "";
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

	return "";
}

const char *
ircd::js::reflect(const JSGCParamKey &s)
{
	switch(s)
	{
		case JSGC_MAX_BYTES:                       return "JSGC_MAX_BYTES";
		case JSGC_MAX_MALLOC_BYTES:                return "JSGC_MAX_MALLOC_BYTES";
		case JSGC_BYTES:                           return "JSGC_BYTES";
		case JSGC_NUMBER:                          return "JSGC_NUMBER";
		case JSGC_MODE:                            return "JSGC_MODE";
		case JSGC_UNUSED_CHUNKS:                   return "JSGC_UNUSED_CHUNKS";
		case JSGC_TOTAL_CHUNKS:                    return "JSGC_TOTAL_CHUNKS";
		case JSGC_SLICE_TIME_BUDGET:               return "JSGC_SLICE_TIME_BUDGET";
		case JSGC_MARK_STACK_LIMIT:                return "JSGC_MARK_STACK_LIMIT";
		case JSGC_HIGH_FREQUENCY_TIME_LIMIT:       return "JSGC_HIGH_FREQUENCY_TIME_LIMIT";
		case JSGC_HIGH_FREQUENCY_LOW_LIMIT:        return "JSGC_HIGH_FREQUENCY_LOW_LIMIT";
		case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:       return "JSGC_HIGH_FREQUENCY_HIGH_LIMIT";
		case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:  return "JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX";
		case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:  return "JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN";
		case JSGC_LOW_FREQUENCY_HEAP_GROWTH:       return "JSGC_LOW_FREQUENCY_HEAP_GROWTH";
		case JSGC_DYNAMIC_HEAP_GROWTH:             return "JSGC_DYNAMIC_HEAP_GROWTH";
		case JSGC_DYNAMIC_MARK_SLICE:              return "JSGC_DYNAMIC_MARK_SLICE";
		case JSGC_ALLOCATION_THRESHOLD:            return "JSGC_ALLOCATION_THRESHOLD";
		case JSGC_MIN_EMPTY_CHUNK_COUNT:           return "JSGC_MIN_EMPTY_CHUNK_COUNT";
		case JSGC_MAX_EMPTY_CHUNK_COUNT:           return "JSGC_MAX_EMPTY_CHUNK_COUNT";
		case JSGC_COMPACTING_ENABLED:              return "JSGC_COMPACTING_ENABLED";
		case JSGC_REFRESH_FRAME_SLICES_ENABLED:    return "JSGC_REFRESH_FRAME_SLICES_ENABLED";
	}

	return "";
}

const char *
ircd::js::reflect(const JSGCStatus &s)
{
	switch(s)
	{
		case JSGC_BEGIN:   return "BEGIN";
		case JSGC_END:     return "END";
	}

	return "";
}

const char *
ircd::js::reflect(const JSGCMode &s)
{
	switch(s)
	{
		case JSGC_MODE_GLOBAL:       return "GLOBAL";
		case JSGC_MODE_INCREMENTAL:  return "INCREMENTAL";
		case JSGC_MODE_ZONE:         return "ZONE";
	}

	return "";
}

const char *
ircd::js::reflect(const JS::GCProgress &s)
{
	switch(s)
	{
		case JS::GC_CYCLE_BEGIN:   return "CYCLE_BEGIN";
		case JS::GC_SLICE_BEGIN:   return "SLICE_BEGIN";
		case JS::GC_SLICE_END:     return "SLICE_END";
		case JS::GC_CYCLE_END:     return "CYCLE_END";
	}

	return "";
}

const char *
ircd::js::reflect(const JSExnType &e)
{
	switch(e)
	{
		case JSEXN_WARN:              return "Warning";
		case JSEXN_ERR:               return "Error";
		case JSEXN_INTERNALERR:       return "InternalError";
		case JSEXN_EVALERR:           return "EvalError";
		case JSEXN_RANGEERR:          return "RangeError";
		case JSEXN_REFERENCEERR:      return "ReferenceError";
		case JSEXN_SYNTAXERR:         return "SyntaxError";
		case JSEXN_TYPEERR:           return "TypeError";
		case JSEXN_URIERR:            return "URIError";
		case JSEXN_LIMIT:             return "?LIMIT?";
		case JSEXN_DEBUGGEEWOULDRUN:  return "DebugeeWouldRun";
		case JSEXN_WASMCOMPILEERROR:  return "WASMCompileError";
		case JSEXN_WASMRUNTIMEERROR:  return "WASMRuntimeError";
	}

	return "";
}

const char *
ircd::js::reflect(const JSType &t)
{
	switch(t)
	{
		case JSTYPE_VOID:         return "VOID";
		case JSTYPE_OBJECT:       return "OBJECT";
		case JSTYPE_FUNCTION:     return "FUNCTION";
		case JSTYPE_STRING:       return "STRING";
		case JSTYPE_NUMBER:       return "NUMBER";
		case JSTYPE_BOOLEAN:      return "BOOLEAN";
		case JSTYPE_NULL:         return "NULL";
		case JSTYPE_SYMBOL:       return "SYMBOL";
		case JSTYPE_LIMIT:        return "LIMIT";
	}

	return "";
}

const char *
ircd::js::reflect(const jstype &t)
{
	switch(t)
	{
		case jstype::VALUE:        return "VALUE";
		case jstype::OBJECT:       return "OBJECT";
		case jstype::FUNCTION:     return "FUNCTION";
		case jstype::SCRIPT:       return "SCRIPT";
		case jstype::STRING:       return "STRING";
		case jstype::SYMBOL:       return "SYMBOL";
		case jstype::ID:           return "ID";
	}

	return "";
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/type.h
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/version.h
//

///////////////////////////////////////////////////////////////////////////////
//
// Misc / Fixes / Temp
//

#if defined(IRCD_JS_FIX)
void
__attribute__((noreturn))
js::ReportOutOfMemory(ExclusiveContext *const c)
{
	ircd::js::log.critical("jsalloc(): Reported out of memory (ExclusiveContext: %p)", (const void *)c);
	ircd::terminate();
}
#endif //IRCD_JS_FIX

//
// This DEBUG section is a fix for linkage errors when SpiderMonkey is compiled
// in debug mode.
//
#if defined(JS_DEBUG) && defined(IRCD_JS_FIX)
namespace js  {
namespace oom {

extern mozilla::detail::ThreadLocal<uint32_t> threadType;

uint32_t
GetThreadType()
{
	assert(0);
	return threadType.get();
}

} // namespace oom
} // namespace js
#endif // JS_DEBUG && IRCD_JS_FIX
