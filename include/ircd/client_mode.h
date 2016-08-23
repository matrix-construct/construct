/*
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

#pragma once
#define HAVE_IRCD_CLIENT_MODE_H

#ifdef __cplusplus
namespace ircd   {
namespace client {
namespace mode   {

enum mode : uint
{
	SERVNOTICE   = 0x0001, // server notices
	WALLOP       = 0x0002, // send wallops to them
	OPERWALL     = 0x0004, // operwalls
	INVISIBLE    = 0x0008, // makes user invisible
	CALLERID     = 0x0010, // block unless caller id's
	LOCOPS       = 0x0020, // show locops
	SERVICE      = 0x0040,
	DEAF         = 0x0080,
	NOFORWARD    = 0x0100, // don't forward
	REGONLYMSG   = 0x0200, // only allow logged in users to msg
	OPER         = 0x1000, // operator
	ADMIN        = 0x2000, // admin on server
	SSLCLIENT    = 0x4000, // using SSL
};

const mode DEFAULT_OPER_UMODES
{
	SERVNOTICE   |
	OPERWALL     |
	WALLOP       |
	LOCOPS
};

bool is(const mode &mask, const mode &bits);
void clear(mode &mask, const mode &bits);
void set(mode &mask, const mode &bits);

} // namespace mode
} // namespace client

// Import `umode` type into ircd:: namespace
using umode = client::mode::mode;

} // namespace ircd


inline void
ircd::client::mode::set(mode &mask,
                        const mode &bits)
{
	mask |= bits;
}

inline void
ircd::client::mode::clear(mode &mask,
                          const mode &bits)
{
	mask &= ~bits;
}

inline bool
ircd::client::mode::is(const mode &mask,
                       const mode &bits)
{
	return (mask & bits) == bits;
}

#endif // __cplusplus
