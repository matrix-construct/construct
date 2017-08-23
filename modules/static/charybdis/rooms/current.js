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

/** Checks whether the room_id is a currently running room.
 *
 * @returns room - The room object or undefined
 */
mc.rooms.current = function(room_id)
{
	let index = mc.rooms.current.index(room_id);
	return mc.rooms.current.list[index];
};

mc.rooms.current.empty = function()
{
	return mc.rooms.current.list.length == 0;
};

/** 
 *
 */
mc.rooms.current.index = function(room_id)
{
	return mc.rooms.current.list.findIndex((room) => room.id == room_id);
};

// Multi-use function called whenever a room should be displayed
// which also makes it the currently focused room div.
mc.rooms.current.add = function(room_id)
{
	let room = mc.rooms.current(room_id);
	if(room !== undefined)
		return;

	room = mc.rooms[room_id];
	if(room === undefined)
		return;

	while(mc.rooms.current.list.length >= mc.rooms.current.max())
		mc.rooms.current.del.oldest();

	mc.rooms.current.list.push(room);
	mc.session.rooms.history.unshift(room.id);
	room.focus();
	return room;
};

// Multi-use function called whenever a room should be displayed
// which also makes it the currently focused room div.
mc.rooms.current.del = function(room_id)
{
	let idx = mc.rooms.current.index(room_id);
	if(idx >= 0)
	{
		let room = mc.rooms.current.list.splice(idx, 1)[0];
		room.focus.defocus();
	}

	if(Array.isArray(maybe(() => mc.session.rooms.history)))
	{
		let idx = mc.session.rooms.history.indexOf(room_id);
		if(idx >= 0)
			mc.session.rooms.history.splice(idx, 1);
	}
};

mc.rooms.current.del.oldest = function()
{
	let sorted = mc.rooms.current.list.sort((a, b) =>
	{
		return a.focus.last < b.focus.last? -1:
		       a.focus.last > b.focus.last? 1:
		                                    0;
	});

	let room = sorted[0];
	return room? mc.rooms.current.del(room.id) : undefined;
};

mc.rooms.current.max = function()
{
	let ret = maybe(() => mc.opts.rooms.current.max);
	return ret? ret : 1;
};

// Currently running rooms by ID
mc.rooms.current.list = [];
