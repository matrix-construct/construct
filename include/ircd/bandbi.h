/**
 * Copyright (C) 2016 Charybdis Development Team
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
#define HAVE_BANDBI_H

#ifdef __cplusplus
namespace ircd {

void init_bandb(void);

typedef enum
{
	BANDB_KLINE,
	BANDB_DLINE,
	BANDB_XLINE,
	BANDB_RESV,
	LAST_BANDB_TYPE
} bandb_type;

void bandb_add(bandb_type, struct Client *source_p, const char *mask1,
	       const char *mask2, const char *reason, const char *oper_reason, int perm);
void bandb_del(bandb_type, const char *mask1, const char *mask2);
void bandb_rehash_bans(void);

}       // namespace ircd
#endif  // __cplusplus
