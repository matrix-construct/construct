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

	if($index < this.room.timeline.horizon)
		return false;

	return true;
};
