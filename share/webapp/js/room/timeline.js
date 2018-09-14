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


/******************************************************************************
 * Timeline
 *
 * The primary collection of events in a room. The view is watching it directly.
 * All known events for a room are stored in order here. This means state events
 * accumulate at the front of this array. The index at which this accumulation
 * ends is called the horizon. At and after the horizon index, state and non-state
 * events are mixed.
 * 
 * Adding events to the timeline must be done through the room.events.insert() iface.
 * Insertion is unary function there are no options. The intent of a user to scroll
 * backward or forward is deduced by which half of the timeline the insertions are
 * occurring so the other side can be trimmed.
 *
 * The user `class state` is kept in sync with the timeline automatically by making
 * queries into it and caching some results as needed for Angular.
 */
room.timeline = class extends Array
{
	constructor(...events)
	{
		super(...events);
		this.opts = Object.copy(room.timeline.opts);
		this.id = null;
	}

	static get [Symbol.species]()
	{
		return Array;
	}

	get horizon()
	{
		return room.timeline.horizon.call(this);
	}
};

/**
 * Default options for events system. The `room.opts` may contain an `events`
 * structure which will override these defaults.
 */
room.timeline.opts =
{
	// The number of non-state events in the timeline is trimmed to this value
	// on the antipode to the last insertion. Increasing this value can make
	// scrolling smoother. Decreasing this value can save memory.
	limit: mc.opts.timeline_limit,

	// Keeps the timeline filled to the limit
	autofill: true,
};

/** Last modification timestamp.
 *
 * Updated to reflect the last modification to the timeline. This can be
 * used for cache coherence purposes etc.
 */
room.timeline.prototype.modified = 0;

/**
 * @returns an array of indexes to the timeline ordered from youngest
 * to oldest which have passed the filter condition. Optionally limited
 * by lim.
 */
room.timeline.prototype.query = function(filter = () => true, lim = this.length)
{
	let ret = [];
	for(let i = this.length - 1; i >= 0 && lim > 0; i--)
	{
		let event = (this)[i];
		if(!filter(event, i))
			continue;

		ret.push(i);
		lim--;
	}

	return ret;
};

/**
 * @returns the most recent event index for the query
 */
room.timeline.prototype.current = function(filter = () => true)
{
	return this.query(filter, 1)[0];
};

/**
 * @returns the most recent event value for the query
 */
room.timeline.prototype.current_event = function(filter = () => true)
{
	return (this)[this.current(filter)];
};

/** Linear search for event in timeline
 */
room.timeline.prototype.get = function(event_id)
{
	return (this)[this.pos(event_id)];
};

/** Linear search for event in timeline
 */
room.timeline.prototype.pos = function(event_id)
{
	return this.findIndex((event) => event.event_id == event_id);
};

/** Linear search for event in timeline
 */
room.timeline.prototype.has = function(event_id)
{
	return this.some((event) => event.event_id == event_id);
};

/**
 * Timeline must be sorted so events with the same ID are next to
 * each other. This removes duplicate events which are expected
 * to have identical content. 
 */
room.timeline.prototype.unique = function()
{
	let i = 0;
	while(i < this.length - 1)
	{
		let self = (this)[i];
		let next = (this)[i+1];
		if(self.event_id == next.event_id)
			this.splice(i+1, 1);
		else
			i++;
	}
};

/** Alias for forEach()
 */
room.timeline.prototype.each = function(closure)
{
	return this.forEach(closure);
};

/** Reduces the iteration seen by the closure based on the condition.
 */
room.timeline.prototype.only = function(condition, closure)
{
	return this.filter(condition).forEach(closure);
};

/** Tally events that pass the condition test
 */
room.timeline.prototype.count = function(condition)
{
	let mapper = (event) =>
		condition(event)? true : false;

	let reducer = (acc, val) =>
		acc + val;

	return this.map(mapper).reduce(reducer, 0);
}

room.timeline.prototype.horizon_event = function()
{
	return (this)[this.horizon];
};

/** The length of the timeline after the horizon. This includes
 * both state and non-state events.
 */
room.timeline.prototype.window = function()
{
	return this.length - this.horizon;
};

/** Extracts a specific value from each event and returns
 * an array of only that data.
 */
room.timeline.prototype.child_values = function(key)
{
	let mapper = (event) =>
		event[key];

	let reducer = (acc, val) =>
	{
		acc.push(val);
		return acc;
	}

	return this.map(mapper).reduce(reducer, []);
}

/** A list of all event types known by the timeline
 */
room.timeline.prototype.types = function()
{
	let ret = [];
	this.child_values("type").forEach((type) =>
	{
		if(!ret.includes(type))
			ret.push(type);
	});

	return ret;
}

room.timeline.prototype.servers = function()
{
	let ret = [];
	this.child_values("sender").forEach((sender) =>
	{
		let domain = mc.m.domid(sender);
		if(!ret.includes(domain))
			ret.push(domain);
	});

	return ret;
}

room.timeline.prototype.filter_by_sender = function(mxid)
{
	return this.filtered((event) => event.sender == mxid);
};

room.timeline.prototype.filter_by_server = function(hostname)
{
	return this.filtered((event) => mc.m.domid(event.sender) == hostname);
};

room.timeline.prototype.stats = function()
{
	return {
		total: this.length,
		types: this.types().length,
		state: this.count(mc.event.is_state),
		servers: this.servers().length,
		horizon: this.horizon,
	};
};

room.timeline.prototype.pending = {};

/**
 * The legitimate way to create a new event
 */
room.timeline.prototype.issue = function(event, opts = {})
{
	this.pending[event.type] = event;
	mc.m.rooms.state.put(this.id, event.type, event.state_key, event.content, opts, (error, data) =>
	{
		if(error)
		{
			delete this.pending[event.type];
			throw new mc.error(error);
		}

		this.pending[event.type].event_id = data.event_id;
		this.pending[event.type].sender = mc.session.user_id;
		this.insert(this.pending[event.type]);
		delete this.pending[event.type];
	});
};

/**
 * The legitimate way for the room to process an event
 */
room.timeline.prototype.insert = function(event)
{
	if(typeof(event) != "object")
		return false;

	if(Array.isArray(event))
		return this.insert.array.call(this, event);
	else
		return this.insert.array.call(this, [event]);
};

room.timeline.prototype.insert.array = function(events)
{
	if(empty(events))
		return;

	events = events.map((event) =>
	{
		if(!(event instanceof mc.event))
			return new mc.event(event);
		else
			return event;
	});

	events.forEach(this.fix.bind(this));
	events.sort(mc.event.cmp);
	this.push(...events);
	this.sort(mc.event.cmp);
	this.unique();

	let last_a = events.back().event_id;
	let last_b = this.back().event_id;
	let dir = last_a != last_b? 'b' : 'f';
	this.trim(dir);

	this.modified = mc.now();
};

/**
 * Maintains the size of the timeline relative to the closest side
 * of the last insertion index.
 */
room.timeline.prototype.trim = function(dir = 'f')
{
	let window = this.window();
	if(window < this.opts.limit)
		return;

	if(dir == 'b')
		this.trim.newest.call(this);
	else
		this.trim.oldest.call(this);
};

/**
 * Maintains the size of the timeline by removing the latest events.
 * This is intended for going back in time, i.e scrolling up.
 *
 * We treat with both state and non-state events the same for this end
 * by simply slicing them all off (the future is opaque).
 */
room.timeline.prototype.trim.newest = function()
{
	if(!this.opts.limit)
		return false;

	let length = this.count(mc.event.is_not_state);
	let start = this.opts.limit - length;
	if(start >= 0)
		return false;

	this.splice(start);
	return true;
};

/**
 * Maintains the size of the timeline by removing the oldest events
 * This is intended for going forward in time, i.e scrolling down.
 *
 * We have to deal with state and non-state events differently here.
 * Non-state events can fall off the front below the horizon, but state
 * events have to accumulate at the horizon. When a state event hits the
 * horizon, some older state event it replaces is also dropped.
 */
room.timeline.prototype.trim.oldest = function()
{
	let window = () => this.count(mc.event.is_not_state);
	while(window() > this.opts.limit)
	{
		let horizon = this.horizon;
		let event = (this)[horizon];
		if(mc.event.is_not_state(event))
		{
			// Trivial removal of oldest non-state event
			this.splice(horizon, 1);
			continue;
		}

		// Accumulate state events at the front of the timeline. Attempt
		// to remove the event which is outdated by this event.
		for(let i = 0; i < horizon; i++)
		{
			let other = (this)[i];
			if(other.type != event.type)
				continue;

			if(other.state_key != event.state_key)
				continue;

			this.splice(i, 1);
			break;
		}
	}
};

room.timeline.prototype.fix = function(event)
{
	if(event.room_id === undefined)
		event.room_id = this.id;

	if(!event.origin_server_ts)
		event.origin_server_ts = mc.now();
};

/** The index of the horizon. Before this index, all events are state
 * events. For this index and after, state and non-state events are mixed.
 *
 * this = timeline
 */
room.timeline.horizon = function()
{
	if(this._horizon_cache_ts < this.modified)
	{
		this._horizon_cache = this.findIndex(mc.event.is_not_state);
		this._horizon_cache_ts = this.modified;
	}

	return this._horizon_cache;
};

/** Instance variable for horizon caching. When this doesn't match
 * the timeline.modified then the horizon has to be recalculated
 */
room.timeline.prototype._horizon_cache_ts = -1;
room.timeline.prototype._horizon_cache = 0;
