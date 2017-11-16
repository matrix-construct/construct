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
 ******************************************************************************
 * Rooms Sync
 *
 */

mc.rooms.sync = function(action, room_id, data)
{
	let handler = mc.rooms.sync[action];
	if(!handler)
		return;

	if(!(room_id in mc.rooms))
		mc.rooms[room_id] = new mc.room({room_id: room_id});

	let room = mc.rooms[room_id];
	handler(room, data);
};

mc.rooms.sync["join"] = function(room, data)
{
	if(!(room.id in mc.rooms.joined))
		mc.rooms.joined[room.id] = room;

	room.sync(data, "join");
	if((mc.rooms.current.empty() && empty(mc.session.rooms.history))
	|| (mc.session.rooms.history[0] == room.id))
	{
		mc.rooms.current.add(room.id);
		room.scroll.to.bottom();
	}

	delete mc.rooms.invited[room.id];
	delete mc.rooms.left[room.id];

	if(!mc.rooms.mode || mc.rooms.mode == "JOINED")
		mc.rooms.menu["JOINED"].click();
};

mc.rooms.sync["leave"] = function(room, data)
{
	if(!(room.id in mc.rooms.left))
		mc.rooms.left[room.id] = room;

	room.sync(data, "leave");

	delete mc.rooms.invited[room.id];
	delete mc.rooms.joined[room.id];

	if(!mc.rooms.mode)
		mc.rooms.menu["LEFT"].click();
};

mc.rooms.sync["invite"] = function(room, data)
{
	debug.object({invite: data}, 5);

};
