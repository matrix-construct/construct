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

using mask = uint64_t;
extern mode_table<mask> table;
extern char available[64];

class mode
:public mode_lease<mask, table>
{
	void release() noexcept override;

  public:
	explicit mode(const char &c);
};

extern mode SERVNOTICE;   // server notices
extern mode WALLOP;       // send wallops to them
extern mode OPERWALL;     // operwalls
extern mode INVISIBLE;    // makes user invisible
extern mode CALLERID;     // block unless caller id's
extern mode LOCOPS;       // show locops
extern mode SERVICE;
extern mode DEAF;
extern mode NOFORWARD;    // don't forward
extern mode REGONLYMSG;   // only allow logged in users to msg
extern mode OPER;         // operator
extern mode ADMIN;        // admin on server
extern mode SSLCLIENT;    // using SSL

extern const mask DEFAULT_OPER_UMODES;

bool is(const mask &, const mask &);
void clear(mask &, const mask &);
void set(mask &, const mask &);

} // namespace mode
} // namespace client

// Import `umode` type into ircd:: namespace
namespace umode = client::mode;

} // namespace ircd


inline void
ircd::client::mode::set(mask &cur,
                        const mask &bit)
{
	cur |= bit;
}

inline void
ircd::client::mode::clear(mask &cur,
                          const mask &bit)
{
	cur &= ~bit;
}

inline bool
ircd::client::mode::is(const mask &cur,
                       const mask &bit)
{
	return (cur & bit) == bit;
}

#endif // __cplusplus
