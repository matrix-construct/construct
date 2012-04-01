/*
 * charybdis: an advanced ircd.
 * chm_nonotice: block NOTICEs (+T mode).
 *
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>
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

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "inline/stringops.h"

static unsigned int mode_nonotice;

static void chm_nonotice_process(hook_data_privmsg_channel *);

mapi_hfn_list_av1 chm_nonotice_hfnlist[] = {
	{ "privmsg_channel", (hookfn) chm_nonotice_process },
	{ NULL, NULL }
};

static void
chm_nonotice_process(hook_data_privmsg_channel *data)
{
	/* don't waste CPU if message is already blocked */
	if (data->approved || data->msgtype != MESSAGE_TYPE_NOTICE)
		return;

	if (data->chptr.mode.mode & mode_nonotice)
	{
		data->approved = ERR_CUSTOM;
		return;
	}
}

static int
_modinit(void)
{
	mode_nonotice = cflag_add('T', chm_simple);
	if (mode_nonotice == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	cflag_orphan('T');
}

DECLARE_MODULE_AV1(chm_nonotice, _modinit, _moddeinit, NULL, NULL, chm_nonotice_hfnlist, "$Revision$");
