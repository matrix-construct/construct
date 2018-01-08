/*
 *  Copyright (C) 2017 Charybdis Development Team
 *  Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

#pragma once
#define HAVE_IRCD_IOS_H

// Boost headers are not exposed to our users unless explicitly included by a
// definition file. Other libircd headers may extend this namespace with more
// forward declarations.

/// Forward declarations for boost::system because it is not included here.
namespace boost::system
{
	struct error_code;
	namespace errc {}
}

/// Forward declarations for boost::asio because it is not included here.
namespace boost::asio
{
	struct io_context;
}

namespace ircd
{
	/// Alias so that asio:: can be used
	namespace asio = boost::asio;
	namespace errc = boost::system::errc;
	using boost::system::error_code;

	/// A record of the thread ID when static initialization took place (for ircd.cc)
	extern const std::thread::id static_thread_id;

	/// The thread ID of the main IRCd thread running the event loop.
	extern std::thread::id thread_id;

	/// The user's io_service
	extern asio::io_context *ios;

	/// IRCd's strand of the io_service
	struct strand extern *strand;

	bool is_main_thread();
	void assert_main_thread();

	void post(std::function<void ()>);
	void dispatch(std::function<void ()>);
}

inline void
ircd::assert_main_thread()
{
	assert(is_main_thread());
}

inline bool
ircd::is_main_thread()
{
	return std::this_thread::get_id() == ircd::thread_id;
}
