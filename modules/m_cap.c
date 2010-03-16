/* modules/m_cap.c
 * 
 *  Copyright (C) 2005 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2005 ircd-ratbox development team
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
 * $Id: m_cap.c 676 2006-02-03 20:05:09Z gxti $
 */

#include "stdinc.h"
#include "class.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "s_user.h"

typedef int (*bqcmp)(const void *, const void *);

static int m_cap(struct Client *, struct Client *, int, const char **);
static int modinit(void);

struct Message cap_msgtab = {
	"CAP", 0, 0, 0, MFLG_SLOW,
	{{m_cap, 2}, {m_cap, 2}, mg_ignore, mg_ignore, mg_ignore, {m_cap, 2}}
};

mapi_clist_av1 cap_clist[] = { &cap_msgtab, NULL };
DECLARE_MODULE_AV1(cap, modinit, NULL, cap_clist, NULL, NULL, "$Revision: 676 $");

#define _CLICAP(name, capserv, capclient, flags)	\
	{ (name), (capserv), (capclient), (flags), sizeof(name) - 1 }

#define CLICAP_FLAGS_STICKY	0x001

static struct clicap
{
	const char *name;
	int cap_serv;		/* for altering s->c */
	int cap_cli;		/* for altering c->s */
	int flags;
	int namelen;
} clicap_list[] = {
	_CLICAP("multi-prefix",	CLICAP_MULTI_PREFIX, 0, 0),
	_CLICAP("sasl", CLICAP_SASL, 0, 0)
};

#define CLICAP_LIST_LEN (sizeof(clicap_list) / sizeof(struct clicap))

static int clicap_sort(struct clicap *, struct clicap *);

static int
modinit(void)
{
	qsort(clicap_list, CLICAP_LIST_LEN, sizeof(struct clicap),
		(bqcmp) clicap_sort);
	return 0;
}

static int
clicap_sort(struct clicap *one, struct clicap *two)
{
	return irccmp(one->name, two->name);
}

static int
clicap_compare(const char *name, struct clicap *cap)
{
	return irccmp(name, cap->name);
}

/* clicap_find()
 *   Used iteratively over a buffer, extracts individual cap tokens.
 *
 * Inputs: buffer to start iterating over (NULL to iterate over existing buf)
 *         int pointer to whether the cap token is negated
 *         int pointer to whether we finish with success
 * Ouputs: Cap entry if found, NULL otherwise.
 */
static struct clicap *
clicap_find(const char *data, int *negate, int *finished)
{
	static char buf[BUFSIZE];
	static char *p;
	struct clicap *cap;
	char *s;

	*negate = 0;

	if(data)
	{
		rb_strlcpy(buf, data, sizeof(buf));
		p = buf;
	}

	if(*finished)
		return NULL;

	/* skip any whitespace */
	while(*p && IsSpace(*p))
		p++;

	if(EmptyString(p))
	{
		*finished = 1;
		return NULL;
	}

	if(*p == '-')
	{
		*negate = 1;
		p++;

		/* someone sent a '-' without a parameter.. */
		if(*p == '\0')
			return NULL;
	}

	if((s = strchr(p, ' ')))
		*s++ = '\0';

	if((cap = bsearch(p, clicap_list, CLICAP_LIST_LEN, 
				sizeof(struct clicap), (bqcmp) clicap_compare)))
	{
		if(s)
			p = s;
		else
			*finished = 1;
	}

	return cap;
}

/* clicap_generate()
 *   Generates a list of capabilities.
 *
 * Inputs: client to send to, subcmd to send,
 *         flags to match against: 0 to do none, -1 if client has no flags,
 *         int to whether we are doing CAP CLEAR
 * Outputs: None
 */
static void
clicap_generate(struct Client *source_p, const char *subcmd, int flags, int clear)
{
	char buf[BUFSIZE];
	char capbuf[BUFSIZE];
	char *p;
	int buflen = 0;
	int curlen, mlen;
	size_t i;

	mlen = rb_sprintf(buf, ":%s CAP %s %s",
			me.name, 
			EmptyString(source_p->name) ? "*" : source_p->name, 
			subcmd);

	p = capbuf;
	buflen = mlen;

	/* shortcut, nothing to do */
	if(flags == -1)
	{
		sendto_one(source_p, "%s :", buf);
		return;
	}

	for(i = 0; i < CLICAP_LIST_LEN; i++)
	{
		if(flags)
		{
			if(!IsCapable(source_p, clicap_list[i].cap_serv))
				continue;
			/* they are capable of this, check sticky */
			else if(clear && clicap_list[i].flags & CLICAP_FLAGS_STICKY)
				continue;
		}

		/* \r\n\0, possible "-~=", space, " *" */
		if(buflen + clicap_list[i].namelen >= BUFSIZE - 10)
		{
			/* remove our trailing space -- if buflen == mlen
			 * here, we didnt even succeed in adding one.
			 */
			if(buflen != mlen)
				*(p - 1) = '\0';
			else
				*p = '\0';

			sendto_one(source_p, "%s * :%s", buf, capbuf);
			p = capbuf;
			buflen = mlen;
		}

		if(clear)
		{
			*p++ = '-';
			buflen++;

			/* needs a client ack */
			if(clicap_list[i].cap_cli && 
			   IsCapable(source_p, clicap_list[i].cap_cli))
			{
				*p++ = '~';
				buflen++;
			}
		}
		else
		{
			if(clicap_list[i].flags & CLICAP_FLAGS_STICKY)
			{
				*p++ = '=';
				buflen++;
			}

			/* if we're doing an LS, then we only send this if
			 * they havent ack'd
			 */
			if(clicap_list[i].cap_cli &&
			   (!flags || !IsCapable(source_p, clicap_list[i].cap_cli)))
			{
				*p++ = '~';
				buflen++;
			}
		}

		curlen = rb_sprintf(p, "%s ", clicap_list[i].name);
		p += curlen;
		buflen += curlen;
	}

	/* remove trailing space */
	if(buflen != mlen)
		*(p - 1) = '\0';
	else
		*p = '\0';

	sendto_one(source_p, "%s :%s", buf, capbuf);
}

static void
cap_ack(struct Client *source_p, const char *arg)
{
	struct clicap *cap;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;

	if(EmptyString(arg))
		return;

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		/* sent an ACK for something they havent REQd */
		if(!IsCapable(source_p, cap->cap_serv))
			continue;

		if(negate)
		{
			/* dont let them ack something sticky off */
			if(cap->flags & CLICAP_FLAGS_STICKY)
				continue;

			capdel |= cap->cap_cli;
		}
		else
			capadd |= cap->cap_cli;
	}

	source_p->localClient->caps |= capadd;
	source_p->localClient->caps &= ~capdel;
}

static void
cap_clear(struct Client *source_p, const char *arg)
{
	clicap_generate(source_p, "ACK", 
			source_p->localClient->caps ? source_p->localClient->caps : -1, 1);

	/* XXX - sticky capabs */
#ifdef CLICAP_STICKY
	source_p->localClient->caps = source_p->localClient->caps & CLICAP_STICKY;
#else
	source_p->localClient->caps = 0;
#endif
}

static void
cap_end(struct Client *source_p, const char *arg)
{
	if(IsRegistered(source_p))
		return;

	source_p->flags &= ~FLAGS_CLICAP;

	if(source_p->name[0] && source_p->flags & FLAGS_SENTUSER)
	{
		char buf[USERLEN+1];
		rb_strlcpy(buf, source_p->username, sizeof(buf));
		register_local_user(source_p, source_p, buf);
	}
}

static void
cap_list(struct Client *source_p, const char *arg)
{
	/* list of what theyre currently using */
	clicap_generate(source_p, "LIST",
			source_p->localClient->caps ? source_p->localClient->caps : -1, 0);
}

static void
cap_ls(struct Client *source_p, const char *arg)
{
	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	/* list of what we support */
	clicap_generate(source_p, "LS", 0, 0);
}

static void
cap_req(struct Client *source_p, const char *arg)
{
	char buf[BUFSIZE];
	char pbuf[2][BUFSIZE];
	struct clicap *cap;
	int buflen, plen;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;

	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	if(EmptyString(arg))
		return;

	buflen = rb_snprintf(buf, sizeof(buf), ":%s CAP %s ACK",
			me.name, EmptyString(source_p->name) ? "*" : source_p->name);

	pbuf[0][0] = '\0';
	plen = 0;

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		/* filled the first array, but cant send it in case the
		 * request fails.  one REQ should never fill more than two
		 * buffers --fl
		 */
		if(buflen + plen + cap->namelen + 6 >= BUFSIZE)
		{
			pbuf[1][0] = '\0';
			plen = 0;
			i = 1;
		}

		if(negate)
		{
			if(cap->flags & CLICAP_FLAGS_STICKY)
			{
				finished = 0;
				break;
			}

			strcat(pbuf[i], "-");
			plen++;

			capdel |= cap->cap_serv;
		}
		else
		{
			if(cap->flags & CLICAP_FLAGS_STICKY)
			{
				strcat(pbuf[i], "=");
				plen++;
			}

			capadd |= cap->cap_serv;
		}

		if(cap->cap_cli)
		{
			strcat(pbuf[i], "~");
			plen++;
		}

		strcat(pbuf[i], cap->name);
		strcat(pbuf[i], " ");
		plen += (cap->namelen + 1);
	}

	if(!finished)
	{
		sendto_one(source_p, ":%s CAP %s NAK :%s",
			me.name, EmptyString(source_p->name) ? "*" : source_p->name, arg);
		return;
	}

	if(i)
	{
		sendto_one(source_p, "%s * :%s", buf, pbuf[0]);
		sendto_one(source_p, "%s :%s", buf, pbuf[1]);
	}
	else
		sendto_one(source_p, "%s :%s", buf, pbuf[0]);

	source_p->localClient->caps |= capadd;
	source_p->localClient->caps &= ~capdel;
}

static struct clicap_cmd
{
	const char *cmd;
	void (*func)(struct Client *source_p, const char *arg);
} clicap_cmdlist[] = {
	/* This list *MUST* be in alphabetical order */
	{ "ACK",	cap_ack		},
	{ "CLEAR",	cap_clear	},
	{ "END",	cap_end		},
	{ "LIST",	cap_list	},
	{ "LS",		cap_ls		},
	{ "REQ",	cap_req		},
};

static int
clicap_cmd_search(const char *command, struct clicap_cmd *entry)
{
	return irccmp(command, entry->cmd);
}

static int
m_cap(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct clicap_cmd *cmd;

	if(!(cmd = bsearch(parv[1], clicap_cmdlist,
				sizeof(clicap_cmdlist) / sizeof(struct clicap_cmd),
				sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendto_one(source_p, form_str(ERR_INVALIDCAPCMD),
				me.name, EmptyString(source_p->name) ? "*" : source_p->name,
				parv[1]);
		return 0;
	}

	(cmd->func)(source_p, parv[2]);
	return 0;
}
