/*
 *  ircd-ratbox: A slightly useful ircd.
 *  stdinc.h: Pull in all of the necessary system headers
 *
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
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
 *
 */

#if defined(PIC) && defined(PCH)
#include <rb/rb.pic.h>
#else
#include <rb/rb.h>
#endif

// TODO: move
// Allow a reference to an ios to be passed to ircd
#ifdef __cplusplus
namespace boost {
namespace asio  {

	struct io_service;

} // namespace asio
} // namespace boost
#endif // __cplusplus

namespace ircd
{
	using std::begin;
	using std::end;
	using std::get;
	using std::chrono::seconds;
	using std::chrono::milliseconds;
	using std::chrono::microseconds;
	using std::chrono::duration_cast;
	using ostream = std::ostream;
	namespace ph = std::placeholders;
	using namespace std::string_literals;
	using namespace std::literals::chrono_literals;
}

// Temp fwd decl scaffold
namespace ircd
{
	extern boost::asio::io_service *ios;

	struct client;

	namespace chan
	{
		struct chan;
		struct membership;
	}

	struct ConfItem;
	struct Blacklist;
	struct server_conf;
}

#include "util.h"
#include "util_timer.h"
#include "life_guard.h"
#include "defaults.h"
#include "exception.h"
#include "numeric.h"
#include "color.h"
#include "messages.h"
#include "protocol.h"
#include "rfc1459.h"
#include "fmt.h"
#include "err.h"
#include "path.h"
#include "s_assert.h"
#include "match.h"
#include "mode_table.h"
#include "mode_lease.h"
#include "snomask.h"
#include "ctx.h"
#include "ctx_prof.h"
#include "ctx_dock.h"
#include "ctx_mutex.h"
#include "ctx_shared_state.h"
#include "ctx_promise.h"
#include "ctx_future.h"
#include "ctx_async.h"
#include "ctx_pool.h"
#include "ctx_ole.h"
#include "hook.h"
#include "line.h"
#include "tape.h"
#include "cmds.h"
#include "vm.h"
#include "logger.h"
#include "db.h"
#include "js.h"

#include "u_id.h"

#include "client.h"

#include "newconf.h"
#include "conf.h"

#include "modules.h"

#include "info.h"
#include "stringops.h"

/*
#include "cache.h"
#include "whowas.h"
#include "tgchange.h"

#include "mask.h"
#include "chmode.h"
#include "channel.h"

#include "capability.h"
#include "certfp.h"
#include "class.h"
#include "hash.h"
#include "hook.h"
#include "monitor.h"
#include "ratelimit.h"
#include "reject.h"
#include "send.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_stats.h"
#include "substitution.h"
#include "supported.h"
#include "s_user.h"
*/
