/*
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.
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
