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

	enum class runlevel;
	using runlevel_handler = std::function<void (const enum runlevel &)>;

	extern bool debugmode;                      ///< Toggled by command line to indicate debug behavior
	extern const enum runlevel &runlevel;       ///< Reflects current running mode of library
	extern runlevel_handler runlevel_changed;   ///< User hook to get called on runlevel change.

	string_view reflect(const enum runlevel &);

	void init(boost::asio::io_service &ios, const std::string &conf, runlevel_handler = {});
	void init(boost::asio::io_service &ios, runlevel_handler = {});
	bool stop() noexcept;
}

enum class ircd::runlevel
{
	FAULT    = -1,
	STOPPED  = 0,
	READY    = 1,
	START    = 2,
	RUN      = 3,
	STOP     = 4,
};
