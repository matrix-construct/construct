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
 * Room
 *
 */
mc.room = class
{
	constructor(summary = {}, opts = {})
	{
		// Access/convenience to user's options structure
		this.opts = opts;
		Object.defaults(this.opts, mc.room.opts);
		if(maybe(() => summary.room_id.endsWith(":localhost")))
			this.opts.local = true;

		if(!Array.isArray(summary))
			this.id = summary.room_id;
		else
			this.id = summary[0].room_id;

		// Identity
		this.sid = mc.m.sid(this.id);
		this.uri = encodeURIComponent(this.id);

		// TODO: purge
		this.control = Object.copy(room.control);

		// Persistent storage
		if(!(this.id in mc.local.room)) mc.local.room[this.id] = {};
		if(!(this.id in mc.session.room)) mc.session.room[this.id] = {};
		if(!(this.id in mc.instance.room)) mc.instance.room[this.id] = {};

		this.local = mc.local.room[this.id];
		this.session = mc.session.room[this.id];
		this.instance = mc.instance.room[this.id];

		// Event Machine
		this.timeline = new room.timeline;
		this.timeline.id = this.id; //tmp

		this.oracle = new room.state(this.timeline);
		this.state = new Proxy(this.oracle.cache, this.oracle);
		this.content = new Proxy(this.state, this.oracle.content);

		// Accessories
		this.notifications = Function.bindtree(room.notifications, this);
		this.account = Function.bindtree(room.account, this);
		this.power = Function.bindtree(room.power, this);
		this.membership = Function.bindtree(room.membership, this, summary);
		this.input = Function.bindtree(room.input, this);
		this.dom = Function.bindtree(room.dom, this);
		this.receipt = Function.bindtree(room.receipt, this);
		this.typing = Function.bindtree(room.typing, this);
		this.focus = Function.bindtree(room.focus, this);
		this.scroll = Function.bindtree(room.scroll, this);
		this.sync = Function.bindtree(room.sync, this);

		// Generate some initial pseudo-state from the summary data.
		if(!Array.isArray(summary))
			this.timeline.insert(room.state.summary.parse(summary));
		else
			this.timeline.insert(summary);
	}

	get summary()
	{
		return room.state.summary(this.state);
	}
};

// Global scope alias
var room = mc.room;

/**
 * Default Room options
 *
 * Items in here only change when the user specifically changes settings;
 * unlike the control structure below which holds any misc model state.
 */
room.opts =
{
};

/**
 * Default Misc model state
 *
 * These things should be categorized better
 */
room.control =
{
	show_members: false,
	show_main_input: true,
	show_status_frieze: true,
	show_event_detail: false,
	show_event_info: {},

	left:
	{
		member: {},
	},

	input: "",            // ng-model of the main input textarea
	files: {},            // name => FileReader attached alongside the text
	evalmode: false,      // true = input will not be sent to channel

	content:
	{
		type: {},         // 'm.room.member': false,
		event_id: {},     // '$xyz:foo.com': false,
		zoom: undefined,  // event_id
	},

	messages: {},
};
