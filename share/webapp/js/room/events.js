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

room.prototype.current_event = function(minus = 0)
{
	return this.timeline[this.timeline.length - 1 - minus];
};

room.prototype.current_event_id = function(minus = 0)
{
	return this.current_event(minus).event_id;
};

room.events = class extends mc.ng.controller
{
	constructor($scope)
	{
		super("room.events", $scope);
		this.room = this.$scope.room;

		if(!this.room.scroll.initial_backfill)
			this.room.scroll.initial_backfill = this.initial_backfill();
	}
};

mc.ng.app.controller('room.events', room.events);

room.events.prototype.destructor = function()
{
	let room = this.room;

	// save scroll pos
	room.scroll.last = room.scroll.at();
};

room.events.prototype.constructed = async function()
{
	await this.room.scroll.initial_backfill;

	// restore saved scroll pos
	if(this.room.scroll.last !== undefined)
		this.room.scroll.to(this.room.scroll.last);
	else
		this.room.scroll.to.bottom();

	// register scroll event (TODO: directive??)
	this.room.receipt.send.current();
};

room.events.prototype.initial_backfill = async function()
{
	let limit = this.room.timeline.opts.limit;
	let count = Math.min(128, limit); try
	{
		await this.room.scroll.back(count);
	}
	catch(error)
	{
		console.error("Initial backfill error: " + error);
	}
};

/* Result of this determines the ng-if of the event in the view,
 * i.e if false, this event has no DOM element.
*/
room.events.prototype.can_render = function(event, $index)
{
	if(this.room.control.content.type[event.type] === false)
		return false;

	//if(this.room.timeline.length > this.room.timeline.opts.limit)
	//	if($index < this.room.timeline.horizon)
	//		return false;

	return true;
};

/* Result of this determines if this event is visible in the view.
 * The default behavior is to hide the accumulated state events below
 * the event horizon.
 */
room.events.prototype.can_show = function(event, $index)
{
	// Force showing a specific event by ID using this condition
	if(this.room.control.content.event_id[event.event_id] === true)
		return true;

	// Force hiding a specific event by ID using this condition
	if(this.room.control.content.event_id[event.event_id] === false)
		return false;

	if($index < this.room.timeline.length - this.room.timeline.opts.limit)
		if($index < this.room.timeline.horizon)
			return false;

	return true;
};

/* Right now before (if ever) the event timeline is split up into groupings
 * of events to provide margins and collapse for a grouping we use this to
 * determine if the event before it is
 */
room.events.prototype.is_grouping_start = function(event, $index)
{
	let prev = this.room.timeline[$index - 1];
	if(!prev)
		return true;

	if(event.type == "m.room.message" && prev.type != event.type)
		return true;

	if(event.type == "m.room.message" && event.sender != prev.sender)
		return true;

	if(event.type != "m.room.message" && prev.type == "m.room.message")
		return true;

	return false;
};

room.events.prototype.is_grouping_end = function(event, $index)
{
	let next = this.room.timeline[$index + 1];
	if(!next)
		return false;

	if(event.type == "m.room.message" && next.type != event.type)
		return true;

	if(event.type == "m.room.message" && next.sender != event.sender)
		return true;

	if(event.type != "m.room.message" && next.type == "m.room.message")
		return true;

	return false;
};
