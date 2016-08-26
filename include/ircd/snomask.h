/*
 * charybdis: An advanced ircd.
 * snomask.h: Management for user server-notice masks.
 *
 * Copyright (c) 2006 William Pitcock <nenolod@nenolod.net>
 * Copyright (c) 2016 Charybdis Development Team
 * Copyright (c) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#define HAVE_IRCD_SNOMASK_H

#ifdef __cplusplus
namespace ircd {
namespace sno  {

using mask = uint64_t;
extern mode_table<mask> table;

using mode = mode_lease<mask, table>;
extern const mode CCONNEXT;
extern const mode OPERSPY;
extern const mode BOTS;
extern const mode CCONN;
extern const mode DEBUG;
extern const mode FULL;
extern const mode SKILL;
extern const mode NCHANGE;
extern const mode REJ;
extern const mode GENERAL;
extern const mode UNAUTH;
extern const mode EXTERNAL;
extern const mode SPY;

extern const mask DEFAULT_OPER_SNOMASK;

}      // namespace snote
}      // namespace ircd
#endif // __cplusplus
