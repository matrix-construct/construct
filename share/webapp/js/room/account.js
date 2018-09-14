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

/**************************************
 *
 * Per-room account_data
 *
 */

room.account = {};

/** This object will be synchronized as the account_data
 */
room.account.data = {};

/** Update account-based storage object
 */
room.account.update = function(data)
{
	Object.update(this.account.data, data);
};

/** Write back account-based storage to server
 */
room.account.sync = async function()
{
	let user_id = mc.my.mxid;
	let data = this.account.data;
	let type = mc.opts.account_data.key;
	if(empty(data))
		return;

	let request = mc.m.user.rooms.account_data.put(user_id, this.id, type, data); try
	{
		let response = await request.response;
	}
	catch(error)
	{
		console.error("Error synchronizing account data for room " + this.id + ": " + error);
	}
};
