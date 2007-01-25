/*
 *   Copyright (C) infinity-infinity God <God@Heaven>
 *
 *   Bob was here 
 *   $Id: m_42.c 3161 2007-01-25 07:23:01Z nenolod $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

static int mclient_42(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message hgtg_msgtab = {
  "42", 0, 0, 0, MFLG_SLOW,
  { mg_ignore, {mclient_42, 0}, mg_ignore, mg_ignore, mg_ignore, {mclient_42, 0}
  }
};

mapi_clist_av1 hgtg_clist[] = { &hgtg_msgtab, NULL };


DECLARE_MODULE_AV1(42, NULL, NULL, hgtg_clist, NULL, NULL, "Revision 0.42");


static int
mclient_42(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_one_notice(source_p, ":The Answer to Life, the Universe, and Everything.");
	return 0;
}


