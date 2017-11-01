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

mc.rooms.info = function(room_id, $event)
{
	if(mc.rooms.info.model == room_id)
	{
		mc.rooms.mode = "";
		return;
	}

	if(!mc.rooms.current(room_id))
		return;

	mc.rooms.info.model = room_id;
	mc.rooms.mode = "INFO";
};

mc.rooms.info.clear = function()
{
	delete mc.rooms.info.model;
};

mc.rooms.info.menu =
{
	"JOIN":
	{
		icon: "fa-thumbs-up",
		classes: { join: true, },
		hide: (room_id) => mc.rooms.joined[room_id] !== undefined,
	},

	"KNOCK":
	{
		icon: "fa-hand-rock-o",
		classes: { knock: true, disabled: true, },
		hide: (room_id) => mc.rooms.joined[room_id] !== undefined,
	},

	"HOLD TO LEAVE":
	{
		icon: "fa-thumbs-down",
		classes: { leave: true, },
		hide: (room_id) => mc.rooms.joined[room_id] === undefined,
		holds: 1000,
	},
};

mc.rooms.info.menu["JOIN"].click = async function($event, room_id)
{
	if(!room_id)
		room_id = mc.rooms.search.value.split(" ")[0];

	if(!(room_id in mc.rooms))
		mc.rooms[room_id] = new mc.room({room_id: room_id});

	let room = mc.rooms[room_id];

	// Try to join room by canon alias first
	let alias = room.state['m.room.canonical_alias'].content.alias;

	// Fallback to any alias
	if(empty(alias)) try
	{
		let aliases = room.state['m.room.aliases'];
		let state_key = Object.keys(aliases)[0];
		alias = aliases[state_key].content.aliases[0];
	}
	catch(e) {}

	// Fallback to room id
	if(empty(alias))
		alias = room_id;

	let request = mc.m.join.post(alias); try
	{
		let data = await request.response;
		mc.rooms.joined[room_id] = room;
		debug.object(data);
		return true;
	}
	catch(error)
	{
		error.element = $event.delegateTarget;
		throw error;
	}
};

mc.rooms.info.menu["HOLD TO LEAVE"].click = async function($event, room_id)
{
	let request = mc.m.rooms.leave.post(room_id,); try
	{
		let data = await request.response;
		debug.object(data);

		if(mc.rooms.current(room_id))
			mc.rooms.current.del(room_id);

		delete mc.rooms.joined[room_id];
		return true;
	}
	catch(error)
	{
		error.element = $event.delegateTarget;
		throw error;
	}
};

/*
	"FORGET":
	{
		icon: "fa-eraser",
		hide: (room_id) =>
			mc.rooms.joined[room_id] !== undefined,

		click: (event, room_id) =>
		{
			let opts =
			{
				element: event.delegateTarget,
			};

			mc.m.rooms.forget.post(room_id, opts, (error, data) =>
			{
				if(error)
					throw error;

				delete mc.rooms.list[room_id];
			});
		},
	},
};
*/
