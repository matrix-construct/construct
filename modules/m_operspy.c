/*  modules/m_operspy.c
 *  Copyright (C) 2003-2005 ircd-ratbox development team
 *  Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
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
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 *
 * $Id: m_operspy.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int ms_operspy(struct Client *client_p, struct Client *source_p,
		      int parc, const char *parv[]);

struct Message operspy_msgtab = {
	"OPERSPY", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {ms_operspy, 2}, mg_ignore}
};

mapi_clist_av1 operspy_clist[] = { &operspy_msgtab, NULL };
DECLARE_MODULE_AV1(operspy, NULL, NULL, operspy_clist, NULL, NULL, "$Revision: 254 $");

/* ms_operspy()
 *
 * parv[1] - operspy command
 * parv[2] - optional params
 */
static int
ms_operspy(struct Client *client_p, struct Client *source_p,
	   int parc, const char *parv[])
{
	static char buffer[BUFSIZE];
	char *ptr;
	int cur_len = 0;
	int len, i;

	if(parc < 4)
	{
		report_operspy(source_p, parv[1],
			    parc < 3 ? NULL : parv[2]);
	}
	/* buffer all remaining into one param */
	else
	{
		ptr = buffer;
		cur_len = 0;

		for(i = 2; i < parc; i++)
		{
			len = strlen(parv[i]) + 1;

			if((size_t)(cur_len + len) >= sizeof(buffer))
				return 0;

			rb_snprintf(ptr, sizeof(buffer) - cur_len, "%s ",
				 parv[i]);
			ptr += len;
			cur_len += len;
		}

		report_operspy(source_p, parv[1], buffer);
	}

	return 0;
}

