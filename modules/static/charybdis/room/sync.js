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
 * Sync
 *
 * Called with a block of data specific to this room obtained from
 * the /sync resource. Called by the rooms.sync suite which is called
 * by the main sync loop.
 *
 * The action specifies which category the data was found under i.e "join"
 * "leave" or "invite". Rather than having separate function handlers for
 * each action, we simply consider it as advice through a single stack
 * of handlers.
 */
room.sync = function(data, action)
{
	Object.each(data, (name, data) =>
	{
		if(!(name in this.sync.handle))
		{
			console.warn("Unhandled room.sync type '" + name + "'");
			return;
		}

		this.sync.handle[name](data, action);
	});

	if(this.focused())
	{
		this.sync.activity();
		if(this.control.mode == "LIVE")
		{
			this.scroll.to.bottom("fast");
			this.receipt.send.current();
		}
	}
};

room.sync.activity = function()
{
	if(this.control.activity === true)
		return;

	let timeout = 100;
	this.control.activity = true;
	mc.timeout(timeout, () => this.control.activity = false);
};

/** This function is applied to events before they are handed off
 * to the room's timeline. The timeline may have other alterations.
 */
room.sync.fix_event = function(event, action)
{
	if(event.type == "m.room.member")
		if(event.state_key == mc.my.mxid)
			if(!event.membership && !maybe(() => event.content.membership) && action !== undefined)
				event.membership = action;
};

/** Handlers for members of the sync data for this room. Each member of this
 * object is a handler for a corresponding named key in the sync data parsed
 * by the room's sync function.
 */
room.sync.handle = {};

/** 
 * @param data - The member value in the sync data for the key this handler is handling.
 * @param action - The action from sync should be passed along with the data
 */
room.sync.handle["state"] = function(data, action)
{
	let events = data.events;
	if(events === undefined)
		return;

	events.forEach((event) =>
	{
		this.sync.fix_event(event, action);
		mc.users.learn(event);
	});

	this.timeline.insert(events);
};

room.sync.handle["timeline"] = function(data, action)
{
	let events = data.events;
	if(events === undefined)
		return;

	events.forEach((event) =>
	{
		this.sync.fix_event(event, action);
		mc.users.learn(event);
	});

	this.timeline.insert(events);
};

room.sync.handle["account_data"] = function(data, action)
{
	let events = data.events;
	if(events === undefined)
		return;

	let handlers = this.sync.handle["account_data"];
	events.forEach((event) =>
	{
		let type = event.type;
		if(!(type in handlers))
		{
			console.warn("Unhandled account_data [" + type + "] in " + this.id);
			return;
		}

		handlers[type](event);
	});
};

room.sync.handle["account_data"]["ircd_storage"] = function(event, action)
{
	this.account.update(event.content);
};

room.sync.handle["unread_notifications"] = function(data, action)
{
	Object.update(this.notifications, data);
};

room.sync.handle["ephemeral"] = function(data)
{
	let events = data.events;
	if(events === undefined)
		return;

	let handlers = this.sync.handle["ephemeral"];
	events.forEach((event) =>
	{
		let type = event.type;
		if(!(type in handlers))
		{
			console.warn("Unhandled ephemeral [" + type + "] in " + this.id);
			return;
		}

		this.sync.fix_event(event);
		mc.users.learn(event);
		handlers[type](event);
	});
};

room.sync.handle["ephemeral"]["m.typing"] = function(event)
{
	if(!maybe(() => event.content.user_ids))
		return;

	this.typing.update(event.content.user_ids);
};

room.sync.handle["ephemeral"]["m.receipt"] = function(event)
{
	this.receipt.update(event.content);
};

room.sync.handle["ephemeral"]["m.presence"] = function(event)
{
	debug.object({presence: event});
};
