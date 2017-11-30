/*
 * Copyright (C) 2017 Matrix Construct Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
 *
 */

#pragma once
#define HAVE_IRCD_H

#if defined(PIC) && defined(PCH)
	#include "stdinc.pic.h"
#else
	#include "stdinc.h"
#endif

/// \brief Internet Relay Chat daemon. This is the principal namespace for IRCd.
///
///
namespace ircd
{
	struct init;

	extern runlevel_handler runlevel_changed;

	string_view reflect(const enum runlevel &);
	void init(boost::asio::io_service &ios, const std::string &conf, runlevel_handler = {});
	void init(boost::asio::io_service &ios, runlevel_handler = {});
	bool quit() noexcept;
}

/// The runlevel allows all observers to know the coarse state of IRCd and to
/// react accordingly. This can be used by the embedder of libircd to know
/// when it's safe to use or delete libircd resources. It is also used
/// similarly by the library and its modules.
///
/// Primary modes:
///
/// * HALT is the off mode. Nothing is/will be running in libircd until
/// an invocation of ircd::init();
///
/// * RUN is the service mode. Full client and application functionality
/// exists in this mode. Leaving the RUN mode is done with ircd::quit();
///
/// - Transitional modes: Modes which are working towards the next mode.
/// - Interventional modes:  Modes which are *not* working towards the next
/// mode and may require some user action to continue.
///
enum class ircd::runlevel
:int
{
	HALT     = 0,    ///< [inter] IRCd Powered off.
	READY    = 1,    ///< [inter] Ready for user to run ios event loop.
	START    = 2,    ///< [trans] Starting up subsystems for service.
	RUN      = 3,    ///< [inter] IRCd in service.
	QUIT     = 4,    ///< [trans] Clean shutdown in progress
	FAULT    = -1,   ///< [trans] QUIT with exception (dirty shutdown)
};

template<class T>
std::string
ircd::demangle()
{
	return demangle(typeid(T).name());
}
