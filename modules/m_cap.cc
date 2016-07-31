/* modules/m_cap.c
 *
 *  Copyright (C) 2005 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2005 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 */

#include <ircd/stdinc.h>
#include <ircd/class.h>
#include <ircd/client.h>
#include <ircd/match.h>
#include <ircd/ircd.h>
#include <ircd/numeric.h>
#include <ircd/msg.h>
#include <ircd/parse.h>
#include <ircd/modules.h>
#include <ircd/s_serv.h>
#include <ircd/s_user.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/hash.h>

static const char cap_desc[] = "Provides the commands used for client capability negotiation";

typedef int (*bqcmp)(const void *, const void *);

static void m_cap(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message cap_msgtab = {
	"CAP", 0, 0, 0, 0,
	{{m_cap, 2}, {m_cap, 2}, mg_ignore, mg_ignore, mg_ignore, {m_cap, 2}}
};

mapi_clist_av1 cap_clist[] = { &cap_msgtab, NULL };

DECLARE_MODULE_AV2(cap, NULL, NULL, cap_clist, NULL, NULL, NULL, NULL, cap_desc);

#define IsCapableEntry(c, e)		IsCapable(c, 1 << (e)->value)
#define HasCapabilityFlag(c, f)		(c->ownerdata != NULL && (((struct ClientCapability *)c->ownerdata)->flags & (f)) == f)

static inline int
clicap_visible(struct Client *client_p, const std::shared_ptr<CapabilityEntry> cap)
{
	struct ClientCapability *clicap;

	/* orphaned caps shouldn't be visible */
	if (cap->flags & CAP_ORPHANED)
		return 0;

	if (cap->ownerdata == NULL)
		return 1;

	clicap = (ClientCapability *)cap->ownerdata;
	if (clicap->visible == NULL)
		return 1;

	return clicap->visible(client_p);
}

/* clicap_find()
 *   Used iteratively over a buffer, extracts individual cap tokens.
 *
 * Inputs: buffer to start iterating over (NULL to iterate over existing buf)
 *         int pointer to whether the cap token is negated
 *         int pointer to whether we finish with success
 * Ouputs: Cap entry if found, NULL otherwise.
 */
static std::shared_ptr<CapabilityEntry>
clicap_find(const char *data, int *negate, int *finished)
{
	static char buf[BUFSIZE];
	static char *p;
	std::shared_ptr<CapabilityEntry> cap;
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

	if((cap = cli_capindex.find(p)) != NULL)
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
 *         flags to match against: 0 to do none, -1 if client has no flags
 * Outputs: None
 */
static void
clicap_generate(struct Client *source_p, const char *subcmd, int flags)
{
	char buf[BUFSIZE] = { 0 };
	char capbuf[BUFSIZE] = { 0 };
	int buflen = 0;
	int mlen;
	rb_dictionary_iter iter;

	mlen = snprintf(buf, sizeof buf, ":%s CAP %s %s",
			me.name,
			EmptyString(source_p->name) ? "*" : source_p->name,
			subcmd);

	/* shortcut, nothing to do */
	if(flags == -1)
	{
		sendto_one(source_p, "%s :", buf);
		return;
	}

	for (auto it = cli_capindex.cap_dict.begin(); it != cli_capindex.cap_dict.end(); it++)
	{
		size_t caplen = 0;
		const auto entry = it->second;
		const auto clicap(reinterpret_cast<ClientCapability *>(entry->ownerdata));
		const char *data = NULL;

		if(flags && !IsCapableEntry(source_p, entry))
			continue;

		if (!clicap_visible(source_p, entry))
			continue;

		caplen = strlen(entry->cap.c_str());
		if (!flags && (source_p->flags & FLAGS_CLICAP_DATA) && clicap != NULL && clicap->data != NULL)
			data = clicap->data(source_p);

		if (data != NULL)
			caplen += strlen(data) + 1;

		/* \r\n\0, possible "-~=", space, " *" */
		if(buflen + mlen >= BUFSIZE - 10)
		{
			/* remove our trailing space -- if buflen == mlen
			 * here, we didnt even succeed in adding one.
			 */
			capbuf[buflen] = '\0';

			sendto_one(source_p, "%s * :%s", buf, capbuf);

			buflen = mlen;
			memset(capbuf, 0, sizeof capbuf);
		}

		buflen = rb_snprintf_append(capbuf, sizeof capbuf, "%s%s%s ",
				entry->cap.c_str(), data != NULL ? "=" : "", data != NULL ? data : "");
	}

	/* remove trailing space */
	capbuf[buflen] = '\0';

	sendto_one(source_p, "%s :%s", buf, capbuf);
}

static void
cap_ack(struct Client *source_p, const char *arg)
{
	std::shared_ptr<CapabilityEntry> cap;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;

	if(EmptyString(arg))
		return;

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		/* sent an ACK for something they havent REQd */
		if(!IsCapableEntry(source_p, cap))
			continue;

		if(negate)
		{
			/* dont let them ack something sticky off */
			if(HasCapabilityFlag(cap, CLICAP_FLAGS_STICKY))
				continue;

			capdel |= (1 << cap->value);
		}
		else
			capadd |= (1 << cap->value);
	}

	source_p->localClient->caps |= capadd;
	source_p->localClient->caps &= ~capdel;
}

static void
cap_end(struct Client *source_p, const char *arg)
{
	if(IsRegistered(source_p))
		return;

	source_p->flags &= ~FLAGS_CLICAP;

	if(source_p->name[0] && source_p->flags & FLAGS_SENTUSER)
	{
		register_local_user(source_p, source_p);
	}
}

static void
cap_list(struct Client *source_p, const char *arg)
{
	/* list of what theyre currently using */
	clicap_generate(source_p, "LIST",
			source_p->localClient->caps ? source_p->localClient->caps : -1);
}

static void
cap_ls(struct Client *source_p, const char *arg)
{
	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	if (arg != NULL && !strcmp(arg, "302"))
	{
		source_p->flags |= FLAGS_CLICAP_DATA;
		source_p->localClient->caps |= CLICAP_CAP_NOTIFY;
	}

	/* list of what we support */
	clicap_generate(source_p, "LS", 0);
}

static void
cap_req(struct Client *source_p, const char *arg)
{
	char buf[BUFSIZE];
	char pbuf[2][BUFSIZE];
	std::shared_ptr<CapabilityEntry> cap;
	int buflen, plen;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;

	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	if(EmptyString(arg))
		return;

	buflen = snprintf(buf, sizeof(buf), ":%s CAP %s ACK",
			me.name, EmptyString(source_p->name) ? "*" : source_p->name);

	pbuf[0][0] = '\0';
	plen = 0;

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		size_t namelen = strlen(cap->cap.c_str());

		/* filled the first array, but cant send it in case the
		 * request fails.  one REQ should never fill more than two
		 * buffers --fl
		 */
		if(buflen + plen + namelen + 6 >= BUFSIZE)
		{
			pbuf[1][0] = '\0';
			plen = 0;
			i = 1;
		}

		if(negate)
		{
			if(HasCapabilityFlag(cap, CLICAP_FLAGS_STICKY))
			{
				finished = 0;
				break;
			}

			strcat(pbuf[i], "-");
			plen++;

			capdel |= (1 << cap->value);
		}
		else
		{
			if (!clicap_visible(source_p, cap))
			{
				finished = 0;
				break;
			}

			capadd |= (1 << cap->value);
		}

		/* XXX this probably should exclude REQACK'd caps from capadd/capdel, but keep old behaviour for now */
		if(HasCapabilityFlag(cap, CLICAP_FLAGS_REQACK))
		{
			strcat(pbuf[i], "~");
			plen++;
		}

		strcat(pbuf[i], cap->cap.c_str());
		if (!finished) {
			strcat(pbuf[i], " ");
			plen += (namelen + 1);
		}
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

static void
m_cap(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct clicap_cmd *cmd;

	if(!(cmd = (clicap_cmd *)bsearch(parv[1], clicap_cmdlist,
				sizeof(clicap_cmdlist) / sizeof(struct clicap_cmd),
				sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendto_one(source_p, form_str(ERR_INVALIDCAPCMD),
				me.name, EmptyString(source_p->name) ? "*" : source_p->name,
				parv[1]);
		return;
	}

	(cmd->func)(source_p, parv[2]);
}
