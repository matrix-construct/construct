/*
 * umode_noctcp.c: user mode +C which blocks CTCPs to the user
 *
 * Copyright (c) 2016 M. Teufel
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
#include "s_user.h"
#include "numeric.h"
#include "inline/stringops.h"

static const char umode_noctcp_desc[] =
	"Adds user mode +C which blocks CTCPs to the user.";

static void umode_noctcp_process(hook_data_privmsg_user *);

mapi_hfn_list_av1 umode_noctcp_hfnlist[] = {
	{ "privmsg_user", (hookfn) umode_noctcp_process },
	{ NULL, NULL }
};

static void
umode_noctcp_process(hook_data_privmsg_user *data) {
	if (data->approved || data->msgtype == MESSAGE_TYPE_NOTICE) {
		return;
	}

	if (data->target_p->umodes & user_modes['C'] && *data->text == '\001' && rb_strncasecmp(data->text + 1, "ACTION", 6)) {
		sendto_one_numeric(data->source_p, ERR_CANNOTSENDTOUSER, form_str(ERR_CANNOTSENDTOUSER), data->target_p->name, "+C set");
		data->approved = ERR_CANNOTSENDTOUSER;
		return;
	}
}

static int
_modinit(void)
{
	user_modes['C'] = find_umode_slot();
	construct_umodebuf();

	return 0;
}

static void
_moddeinit(void)
{
	user_modes['C'] = 0;
	construct_umodebuf();
}

DECLARE_MODULE_AV2(umode_noctcp, _modinit, _moddeinit, NULL, NULL, umode_noctcp_hfnlist, NULL, NULL, umode_noctcp_desc);
