/*
 * m_certfp.c: propagates client certificate fingerprint information
 *
 * Copyright (C) 2010 Jilles Tjoelker
 * Copyright (C) 2010 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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

#include "stdinc.h"
#include "client.h"
#include "common.h"
#include "match.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"

static int me_certfp(struct Client *, struct Client *, int, const char **);

struct Message certfp_msgtab = {
	"CERTFP", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, {me_certfp, 0}, mg_ignore}
};

mapi_clist_av1 certfp_clist[] = { &certfp_msgtab, NULL };

DECLARE_MODULE_AV1(certfp, NULL, NULL, certfp_clist, NULL, NULL, "$Revision$");

/*
** me_certfp
**      parv[1] = certfp string
*/
static int
me_certfp(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsPerson(source_p))
		return 0;
	if (parc != 2)
		return 0;

	rb_free(source_p->certfp);
	source_p->certfp = NULL;
	if (!EmptyString(parv[1]))
		source_p->certfp = rb_strdup(parv[1]);
	return 0;
}
