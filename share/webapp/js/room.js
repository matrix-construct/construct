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

/**
 ******************************************************************************
 * Room
 *
 */
mc.room = class
{
	constructor(summary = {})
	{
		// Find the opts structure which may be attached to the summary
		// object; it was hidden in there to reduce the constructor arguments.
		this.opts = summary.opts? summary.opts : {};
		Object.defaults(this.opts, mc.room.opts);

		// Find the room identity in the summary object
		if(!Array.isArray(summary))
			this.id = summary.room_id;
		else
			this.id = summary[0].room_id;

		if(!this.id)
			throw mc.error("Room construction requires a room_id");

		// opts.local indicates whether this is a client-only room; it can be
		// set in the opts or if the room_id has a client-only domain.
		if(this.id.endsWith(":mc"))
			this.opts.local = true;

		// Identity
		this.sid = mc.m.sid(this.id);
		this.uri = encodeURIComponent(this.id);

		// Persistent storage facilities
		//
		// These bind a few levels of mc.storage into the room for
		// convenience. However, similar to the room.control object, relying
		// on these directly is deprecated, and everything should be kept
		// through events in the timeline which will then decide to how to
		// persist within these facilities on their own.
		//
		if(!(this.id in mc.local.room)) mc.local.room[this.id] = {};
		if(!(this.id in mc.session.room)) mc.session.room[this.id] = {};
		if(!(this.id in mc.instance.room)) mc.instance.room[this.id] = {};

		this.local = mc.local.room[this.id];
		this.session = mc.session.room[this.id];
		this.instance = mc.instance.room[this.id];

		// TODO: purge
		this.control = Object.copy(room.control);

		// Event Machine
		//
		// The timeline is the array of events backing the room. This object
		// is the point of authority for everything in the room. The view, the
		// state machine, and all functionality defined hereafter traces back
		// to the vector of events in this timeline.
		//
		this.timeline = new room.timeline;
		this.timeline.id = this.id; //tmp

		// State Machine
		//
		// `room.state` represents the collection of state events, but note
		// that these are full events. `room.content` is like room.state but
		// it jumps directly into the state event content, which is what most
		// accessors of the room state are really looking for. `room.oracle`
		// is the proxy backend object pulling the strings.
		//
		// patterns:
		//
		//   room.state['m.room.create'][''].content.creator
		//   room.content['m.room.create'][''].creator
		//
		//   room.state['m.room.member']['@jzk'].content.membership
		//   room.content['m.room.member']['@jzk'].membership
		//
		this.oracle = new room.state(this.timeline);
		this.state = new Proxy(this.oracle.cache, this.oracle);
		this.content = new Proxy(this.state, this.oracle.content);

		// Accessories
		//
		// These are subsystems providing feature functionality to the room.
		// The systems are organized and bound into their eponymous namespace
		// allowing for intuitive object access *relative to this room
		// instance*. In other words, we define mc.room.foo.bar as an
		// unbound object tree which is then executed as this.foo.bar() or
		// some.room.foo.bar() whereby default `bar` has the entire room
		// available as its `this` value. See Function.bindtree in util.js
		//
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

		// For local pseudo-rooms, we have some custom construction
		// functionality we want to run in here.
		if(this.opts.local)
			mc.room.constructor.local.call(this);

		// Generate some initial pseudo-state from the summary data.
		// This will stock the timeline with any initial information.
		if(!Array.isArray(summary))
			this.timeline.insert(room.state.summary.parse(summary));
		else
			this.timeline.insert(summary);
	}

	/** Generate a matrix /publicRooms style summary object from current state
	 */
	get summary()
	{
		return room.state.summary(this.state);
	}
};

/** Local pseudoroom constructor extension
 */
mc.room.constructor.local = function()
{
	if(empty(maybe(() => this.content["m.room.create"].creator)))
	{
		this.sync.one(
		{
			type: "m.room.create",
			state_key: "",
			content:
			{
				creator: mc.session.user_id,
			},
		});
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
 * Everything in here is being moved into actual state event objects in the
 * timeline itself, which will then be referenced as the sole model.
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
