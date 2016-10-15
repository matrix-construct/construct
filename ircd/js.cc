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

//	ss << "\"" << std::u16string(r.ucmessage) << "\" ";

//	for(auto it(r.messageArgs); it && *it; ++it)
//		ss << "\"" << std::u16string(*it) << "\" ";

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
	JS_SetContextPrivate(get(), this);
}

ircd::js::context::context(context &&other)
noexcept
:custom_ptr<JSContext>{std::move(other)}
,opts{std::move(other.opts)}
{
	// Branch not taken for null/defaulted instance of JSContext smart ptr
	if(!!*this)
		JS_SetContextPrivate(get(), this);
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

	return *this;
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
,opts(opts)
{
	JS_SetRuntimePrivate(get(), this);

	JS_SetErrorReporter(get(), handle_error);
	JS::SetOutOfMemoryCallback(get(), handle_out_of_memory, nullptr);
	JS::SetLargeAllocationFailureCallback(get(), handle_large_allocation_failure, nullptr);
	JS_SetGCCallback(get(), handle_gc, nullptr);
	JS_AddFinalizeCallback(get(), handle_finalize, nullptr);
	JS_SetDestroyCompartmentCallback(get(), handle_destroy_compartment);
	JS_SetContextCallback(get(), handle_context, nullptr);
	//JS_SetInterruptCallback(get(), nullptr);

	JS_SetNativeStackQuota(get(), opts.code_stack_max, opts.trusted_stack_max, opts.untrusted_stack_max);
}

ircd::js::runtime::runtime(runtime &&other)
noexcept
:custom_ptr<JSRuntime>{std::move(other)}
,opts(std::move(other.opts))
{
	// Branch not taken for null/defaulted instance of JSRuntime smart ptr
	if(!!*this)
		JS_SetRuntimePrivate(get(), this);
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

	return *this;
}

bool
ircd::js::runtime::handle_interrupt(JSContext *const ctx)
{
	auto &runtime(our(ctx).runtime());
	JS_SetInterruptCallback(runtime, nullptr);
	return false;
}

bool
ircd::js::runtime::handle_context(JSContext *const c,
                                  const uint op,
                                  void *const priv)
{
	return true;
}

void
ircd::js::runtime::handle_iterate_compartments(JSRuntime *const rt,
                                               void *const priv,
                                               JSCompartment *const compartment)
{
}

void
ircd::js::runtime::handle_destroy_compartment(JSFreeOp *const fop,
                                              JSCompartment *const compartment)
{
}

void
ircd::js::runtime::handle_finalize(JSFreeOp *const fop,
                                   const JSFinalizeStatus status,
                                   const bool is_compartment,
                                   void *const priv)
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
{
	log.debug("runtime(%p): GC %s",
	          (const void *)rt,
	          reflect(status));
}

void
ircd::js::runtime::handle_large_allocation_failure(void *const priv)
{
	log.error("Large allocation failure");
}

void
ircd::js::runtime::handle_out_of_memory(JSContext *const ctx,
                                        void *const priv)
{
	log.error("JSContext(%p): out of memory", (const void *)ctx);
}

void
ircd::js::runtime::handle_error(JSContext *const ctx,
                                const char *const msg,
                                JSErrorReport *const report)
{
	log.error("JSContext(%p): %s [%s]", (const void *)ctx, msg, debug(*report).c_str());
}
