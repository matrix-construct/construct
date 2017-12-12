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

/** Client Sync
 *
 * This is called in the main sync loop.
 */
mc.sync = async function(opts = {})
{
	let filter = maybe(() => opts.query.filter);
	if(filter === undefined)
		filter = mc.filter("initial_sync");
	else
		filter = mc.filter("sync");

	Object.update(opts,
	{
		query:
		{
			set_presence: mc.instance.presence,
			filter: filter,
		}
	});

	let request = mc.m.sync.get(opts);
	{
		let data = await request.response;
		opts.query.since = maybe(() => data.next_batch);

		if(mc.opts.sync_debug)
			debug.object(data, mc.opts.sync_debug);

		for(let key in data)
		{
			let handler = mc.sync[key];
			if(handler !== undefined)
				handler(data[key]);
		}
	}

	return true;
};

mc.sync["rooms"] = function(rooms)
{
	Object.each(rooms, (action, rooms) =>
		Object.each(rooms, (room_id, room) =>
			mc.sync["rooms"].room(action, room_id, room)));
};

mc.sync["presence"] = function(presence)
{
	if(presence.events)
		mc.users.sync(presence.events);
};

mc.sync["account_data"] = function(account_data)
{
	let events = account_data.events;
	if(empty(events))
		return;

	let handlers = mc.sync["account_data"];
	events.forEach((event) =>
	{
		let type = event.type;
		if(!(type in handlers))
		{
			console.warn("Unhandled account_data event type[" + type + "]");
			return;
		}

		handlers[type](event);
	});
};

mc.sync["account_data"]["ircd_storage"] = function(event)
{
	mc.storage.account.update(event.content);
};

mc.sync["to_device"] = function(to_device)
{
	//debug.object({to_device: to_device}, 3);
};

mc.sync["device_lists"] = function(device_lists)
{
	//debug.object({device_lists: device_lists}, 3);
};

mc.sync["rooms"].room = function(action, room_id, data)
{
	let handler = mc.sync["rooms"].room[action];
	if(!handler)
		return;

	let room = mc.rooms.get({room_id: room_id});
	mc.rooms.set(room.id, action, room);
	handler(room, data);
};

mc.sync["rooms"].room["join"] = function(room, data)
{
	room.sync(data, "join");

	let no_current = (mc.rooms.current.empty() && empty(mc.session.rooms.history));
	let is_current = (mc.session.rooms.history[0] == room.id);
	if(no_current || is_current)
	{
		mc.rooms.current.add(room.id);
		room.scroll.to.bottom();
	}

	delete mc.rooms.invite[room.id];
	delete mc.rooms.leave[room.id];

	if(!mc.rooms.mode || mc.rooms.mode == "JOINED")
		mc.rooms.menu["JOINED"].click();
};

mc.sync["rooms"].room["leave"] = function(room, data)
{
	room.sync(data, "leave");

	delete mc.rooms.invite[room.id];
	delete mc.rooms.join[room.id];

	if(!mc.rooms.mode)
		mc.rooms.menu["LEFT"].click();
};

mc.sync["rooms"].room["invite"] = function(room, data)
{
	debug.object({invite: data}, 5);
};
