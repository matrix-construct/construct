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

namespace ircd {

extern bool debugmode;            // Toggled by command line to indicate debug behavior
extern const bool &main_exited;   // Set when main context has finished.

// The signature of the callback indicating when IRCd's main context has completed.
using main_exit_cb = std::function<void ()>;

//
// Sets up the IRCd, handlers (main context), and then returns without blocking.
// Pass your io_service instance, it will share it with the rest of your program.
// An exception will be thrown on error.
//
// This function will setup the main program loop of libircd. The execution will
// occur when your io_service.run() or poll() is further invoked. For an explanation
// of the callback, see the documentation for ircd::stop().
//
void init(boost::asio::io_service &ios, const std::string &newconf_path, main_exit_cb = nullptr);

//
// Notifies IRCd to shutdown. A shutdown will occur asynchronously and this
// function will return immediately. main_exit_cb will be called when IRCd
// has no more work for the ios (main_exit_cb will be the last operation from
// IRCd posted to the ios).
//
// This function is the proper way to shutdown libircd after an init(), and while
// your io_service.run() is invoked without stopping your io_service shared by
// other activities unrelated to libircd. If your io_service has no other activities
// the run() will then return.
//
// This is useful when your other activities prevent run() from returning.
//
void stop();

//
// Replaces the callback passed to init() which will indicate libircd completion.
// This can be called anytime between init() and stop() to make that replacement.
//
void at_main_exit(main_exit_cb);

} // namespace ircd
