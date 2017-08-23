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
 * Membership interface.
 *
 * This is a category of functions that query the room's state
 * apropos membership information and statistics et al
 */
room.membership = class
{
	constructor(room, summary)
	{
		this._num_joined_members = summary.num_joined_members;
	}

	static get [Class.construction]()
	{
		return true;
	}
};

/** Get member state event for an mxid
 */
room.membership.member = function(mxid)
{
	return this.membership.members()[mxid];
};

// Alias
room.prototype.member = function(mxid)
{
	return this.members()[mxid];
};

/** Get all the member state events, indexed by mxid.
 */
room.membership.members = function()
{
	return this.state['m.room.member'];
};

// Alias
room.prototype.members = function()
{
	return this.membership.members();
};

/** Get the member state event for this client's own member
 */
room.membership.me = function()
{
	return this.membership.member(client.session.user_id);
};

/**
 * Count the membership
 */
room.membership.count = function(type = "join")
{
	// Branch special case for when room membership has not yet been downloaded
	// but we may still find information from the room's public summary.
	if(type == "join" && this.membership._num_joined_members)
		return this.membership._num_joined_members;

	return this.membership.list(type).length;
};

/**
 * List mxids for a certain membership type, i.e "join", "left" etc.
 */
room.membership.list = function(type = "join")
{
	let members = this.membership.members();
	let filter = (mxid) => members[mxid].content.membership == type;
	return Object.keys(members).filter(filter);
};

/**
 * List mxids with the power to change state (i.e ops).
 * (vanity)
 */
room.membership.ops = function()
{
	let threshold = this.power.op();
	let test = (mxid) => this.power.user(mxid) >= threshold;
	return this.membership.list('join').filter(test);
};

/**
 * List mxids with the power to issue events (i.e voiced).
 * (vanity)
 */
room.membership.voiced = function()
{
	let threshold = this.power.voice();
	let test = (mxid) => this.power.user(mxid) >= threshold;
	return this.membership.list('join').filter(test);
};

/**
 * Returns the member state sorted chronologically
 */
room.membership.sorted = function(type = "join")
{
	let m = this.membership.members();
	let comparator = (a, b) => m[a].cmp(m[b]);
	let reducer = (acc, val) =>
	{
		acc.push(m[val]);
		return acc;
	};

	return this.membership.list(type).sort(comparator).reduce(reducer, []);
};

/** div.members element controller
 *
 */
room.membership.controller = class extends mc.ng.controller
{
	constructor($scope)
	{
		super("room.membership", $scope);
		this.room = this.$scope.room;

		this.filter = "join";
		this.frame = 64;
		this.step = 24;
		this.begin = 0;
		this.limit = () => this.begin + this.frame,
		this.list = [];

		if(!this.room.membership._init)
			this.room.membership._init = this.fetch();

		this.loading = true;
	}
};

mc.ng.app.controller("room.membership", room.membership.controller);

/**
 *
 */
room.membership.controller.prototype.destructor = function()
{
}

/**
 *
 */
room.membership.controller.prototype.constructed = async function()
{
	await this.room.membership._init;
	this.list = this.room.membership.sorted(this.filter);
	this.loading = false;
}

room.membership.controller.prototype.scroll = function($event)
{
	let e = $event.delegateTarget;

	if(mc.scroll.at.top(e))
		return this.scroll.up.call(this);

	if(mc.scroll.at.bottom(e))
		return this.scroll.down.call(this);
};

room.membership.controller.prototype.scroll.up = function()
{
	this.begin -= this.step;
	if(this.begin < 0)
		this.begin = 0;
};

room.membership.controller.prototype.scroll.down = function()
{
	this.begin += this.step;
	if(this.begin > this.list.length - this.step)
		this.begin = this.list.length - this.step;
};

/**
 * TODO: XXX: repurpose
 */
room.membership.controller.prototype.fetch = async function(opts = {})
{
	let req = mc.m.rooms.members.get(this.room.id, opts);
	let data = await req.response;
	mayif(() => data.chunk, (events) =>
	{
		this.room.timeline.insert(events);
	});

	// Remove the temporary member count now that we have valid data.
	delete this.room.membership._num_joined_members;

	return true;
};

Object.defineProperty(room.membership.controller, 'attributes', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"NAME":
	{
		value: (room, member) =>
			member.content.displayname,
	},

	"POWER":
	{
		show: (room, member) =>
			maybe(() => (member.user_id in room.state['m.room.power_levels'].content.users)),

		value: (room, member) =>
			maybe(() => room.state['m.room.power_levels'].content.users[member.user_id]),
	},

	"IDLE":
	{
		value: (room, member) =>
			mc.date.describe.elapsed(mc.users[member.user_id].last_active_ago),
	},

	"TS":
	{
		name: (room, member) =>
			maybe(() => mc.room.membership.menu[member.content.membership].name),

		show: (room, member) =>
			member.origin_server_ts,

		value: (room, member) =>
			mc.date.utc.pretty(member.origin_server_ts),
	},
}});

Object.defineProperty(room.membership, 'menu', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"knock":
	{
		icon: "fa-hand-rock-o",
	},

	"invite":
	{
		icon: "fa-handshake-o",
		name: "INVITED",
		show: true,
	},

	"join":
	{
		icon: "fa-thumbs-up",
		name: "JOINED",
		show: true,
	},

	"leave":
	{
		icon: "fa-thumbs-down",
		name: "LEFT",
		show: true,
	},

	"ban":
	{
		icon: "fa-gavel",
		name: "BANNED",
		show: true,
	},

	"admin":
	{
		icon: "fa-shield",
		name: "ADMIN",
		show: true,
	},

}});

/**
 * Drives the status indicator bar at the top of the room
 * apropos member information.
 */
Object.defineProperty(room.membership, 'indicator', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"KNCK":
	{
		count: (room) => room.membership.count('knock'),
		click: (room, $event) =>
		{
			room.control.member_filter = 'knock';
			room.control.show_members = true;
		},
	},

	"INVT":
	{
		count: (room) => room.membership.count('invite'),
		click: (room, $event) =>
		{
			room.control.member_filter = 'invite';
			room.control.show_members = true;
		},
	},

	"JOIN":
	{
		count: (room) => room.membership.count('join'),
		click: (room, $event) =>
		{
			room.control.member_filter = 'join';
			room.control.show_members = true;
		},
	},

	"LEFT":
	{
		count: (room) => room.membership.count('leave'),
		click: (room, $event) =>
		{
			room.control.member_filter = 'leave';
			room.control.show_members = true;
		},
	},

	"BAN":
	{
		count: (room) => room.membership.count('ban'),
		click: (room, $event) =>
		{
			room.control.member_filter = 'ban';
			room.control.show_members = true;
		},
	},

	"OP":
	{
		count: (room) => room.membership.ops().length,
		click: (room, $event) => {},
	},

	"V":
	{
		show: false,
		count: (room) => room.membership.voiced().length,
		click: (room, $event) => {},
	},
}});

Object.defineProperty(room.membership, 'member_info_menu', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"WHOIS":
	{
		icon: "fa-question",
	},

	"QUERY":
	{
		icon: "fa-edit",
	},

	"KICK":
	{
		icon: "fa-sign-out",
		click: (event, room, mxid) =>
		{
			room.request.kick.post(mxid, "and I ain't even mad", (error, data) =>
			{
				if(error)
					throw new client.error(error);

				debug.object({kick: data}, 4);
			});
		},
	},

	"BAN":
	{
		icon: "fa-gavel",
	},
}});

room.membership.member_info_menu["WHOIS"].click = async function($event, room, mxid)
{
	let request = mc.m.admin.whois.get(mxid);
	let data = await request.response;
	debug.object(data, 7);
};
