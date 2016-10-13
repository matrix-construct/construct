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

	const size_t main_maxbytes(64_MiB); //TODO: Configuration
	const size_t mc_stackchunk(8_KiB);  //TODO: Configuration

	log.info("Initializing the main JS Runtime (main_maxbytes: %zu, mc_stackchunk: %zu)",
	         main_maxbytes,
	         mc_stackchunk);

	main = runtime(main_maxbytes);
	mc = context(main, mc_stackchunk);
}

ircd::js::init::~init()
noexcept
{
	log.info("Terminating the main JS Runtime");
	mc.reset();
	main.reset();

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
// ircd/js/context.h
//

///////////////////////////////////////////////////////////////////////////////
//
// ircd/js/runtime.h
//

void
ircd::js::runtime::handle_error(JSContext *const ctx,
                                const char *const msg,
                                JSErrorReport *const report)
{
	log.error("JSContext(%p): %s", ctx, msg);
}
