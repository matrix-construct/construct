/*
 * IRCd Charybdis 5/Matrix
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk (jason@zemos.net)
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
 *
 */

'use strict';

/**
 * Suite of functions 
 */
mc.my =
{
	get mxid()
	{
		return mc.session.user_id;
	},

	/** short id - Chops off the domain and other non-name characters from an mxid */
	get sid()
	{
		return mc.m.sid(mc.my.mxid);
	},

	/** domain of the current session's mxid) */
	get domain()
	{
		return mc.m.domid(mc.my.mxid);
	},

	/** server - the current host we are actually connected to, which may or may
	* not be this client user's homeserver. */
	get server()
	{
		return mc.server.host;
	},

	get displayname()
	{
		return mc.users.displayname(mc.my.mxid);
	},
};
