/*
 *  ircd-ratbox: A slightly useful ircd.
 *  ircd.h: A header for the ircd startup routines.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
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

	enum class runlevel :uint;
	using runlevel_handler = std::function<void (const enum runlevel &)>;

	extern bool debugmode;                      ///< Toggled by command line to indicate debug behavior
	extern const enum runlevel &runlevel;       ///< Reflects current running mode of library
	extern runlevel_handler runlevel_changed;   ///< User hook to get called on runlevel change.

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
:uint
{
	HALT     = 0x00,    ///< [inter] IRCd Powered off.
	READY    = 0x01,    ///< [inter] Ready for user to run ios event loop.
	START    = 0x02,    ///< [trans] Starting up subsystems for service.
	RUN      = 0x04,    ///< [inter] IRCd in service.
	QUIT     = 0x10,    ///< [trans] Clean shutdown in progress
	FAULT    = 0xFF,    ///< [trans] QUIT with exception (dirty shutdown)
};
