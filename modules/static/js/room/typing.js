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
