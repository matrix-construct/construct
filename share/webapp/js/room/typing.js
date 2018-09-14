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
 * Typing notifications
 *
 */

room.typing = {};

/**
 * State of mxid => bool
 */
room.typing.active = {};

/**
 * Sends a typing event for my user in this room
 *
 * this = room
 */
room.typing.set = function(bool)
{
	if(this.opts.local)
		return;

	if(this.control.evalmode && bool)
		return;

	let mxid = mc.my.mxid;
	if(this.typing.active[mxid] === bool)
		return;

	let opts =
	{
		content:
		{
			typing: bool,
			timeout: 15 * 1000,
		},
	};

	this.typing.requesting = true;
	this.typing.active[mxid] = bool;
	client.m.rooms.typing.put(this.id, mxid, opts, (error, data) =>
	{
		this.typing.requesting = false;

		if(error)
		{
			delete this.typing.active[mxid];
			throw new client.error(error);
		}

		if(bool)
			this.typing.active[mxid] = true;
		else
			delete this.typing.active[mxid];
	});
};

/**
 * Function for room.typing.active, which is a dictionary of mxid => boolean.
 *
 * this = room
 */
room.typing.get = function(mxid = mc.my.mxid)
{
	return this.typing.active[mxid];
};

/**
 * Receives a list of users currently typing to update the view.
 *
 * this = room
 */
room.typing.update = function(user_ids = [])
{
	Object.each(this.typing.active, (key, val) =>
	{
		if(!user_ids.includes(key))
			delete this.typing.active[key];
	});

	user_ids.forEach((user_id) =>
	{
		this.typing.active[user_id] = true;
	});
};
