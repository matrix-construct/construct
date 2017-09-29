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
 * Scrolling & Viewport interface
 *
 */

room.scroll = {};

/* Scrollback */
room.scroll.back = function(count = 128)
{
	return this.scroll.io(count, "b");
}

room.scroll.front = function(count = 128)
{
	return this.scroll.io(count, "f");
};

room.scroll.at = function(e = $(this.dom.items()))
{
	return mc.scroll.at(e);
};

room.scroll.at.top = function(e = $(this.dom.items()))
{
	return this.scroll.at(e) == 0;
};

room.scroll.at.bottom = function(e = $(this.dom.items()))
{
	return mc.scroll.at.bottom(e);
};

room.scroll.to = function(pos, speed = "slow", e = $(this.dom.items()))
{
	return mc.scroll.to(e, pos, speed);
};

room.scroll.to.top = function(speed = "slow", e = $(this.dom.items()))
{
	return mc.scroll.to.top(e, speed);
};

room.scroll.to.bottom = function(speed = "slow", e = $(this.dom.items()))
{
	return mc.scroll.to.bottom(e, speed);
};

room.scroll.to.event = function(event_id, speed = "slow", e = $(this.dom.items()))
{
	let st = $(e).prop("scrollTop");
	let item = $(this.dom.item(event_id));
	let rect = mc.get_rect(item);
	this.scroll.to(st + rect.top - 40, "fast", e);
};

/**
 * top: the percentage of area invisible above the viewport.
 * view: the percentage of the area in the viewport.
 * bottom: the percentage of the area invisible below the viewport.
 */
room.scroll.pct = function(e = $(this.dom.items()))
{
	return mc.scroll.pct(e);
};

/* Scroll event handler */
room.scroll.on = function(event)
{
	let e = event.delegateTarget;
	let pos = this.scroll.at(e);

	let at_top = this.scroll.at.top(e);
	let at_bottom = this.scroll.at.bottom(e);
	let pcts = this.scroll.pct(e);
	//debug.object({pos: pos, at_top: at_top, at_bottom: at_bottom, pcts: pcts}, 3);

	let going_up = pos < this.control.scroll_pos_last;
	let going_down = pos > this.control.scroll_pos_last;

	if(going_up && !at_bottom && this.control.mode == "LIVE")
	{
		this.control.mode = "PAST";
		mc.ng.apply();
	}
	else if(at_bottom && this.control.mode == "PAST")
	{
		this.control.mode = "LIVE";
		mc.ng.apply();
	}

	if(at_top || (going_up && pcts.top < 0.15)) try
	{
		this.scroll.back(48);
	}
	catch(e) {}

	if(!at_bottom && going_down && pcts.bottom < 0.15) try
	{
		this.scroll.front(48);
	}
	catch(e) {}

	this.control.scroll_pos_last = pos;
	this.control.scroll_pct_last = pcts;
};

room.scroll.io = async function(limit = 64, dir = "b")
{
	if(this.opts.local)
		return;

	if(this.control.scroll_requested)
		return;

	if(this.control.messages.stop == dir)
		return;
	else
		this.control.messages.stop = null;

	if(this.control.messages.dir == dir)
	{
		this.control.messages.start = this.control.messages.end;
		delete this.control.messages.end;
	} else {
		this.control.messages.dir = dir;
		delete this.control.messages.end;
	}

	this.control.messages.limit = limit;
	this.control.scroll_requested = true;
	room.sync.activity.call(this);
	let opts =
	{
		query:
		{
			from: this.control.messages.start,
			to: this.control.messages.end,
			dir: this.control.messages.dir,
			limit: this.control.messages.limit,
		},
	};

	let request = mc.m.rooms.messages.get(this.id, opts); try
	{
		let data = await request.response;
		let count = this.scroll.io.handle(data);
		this.control.messages.stop = count? null : dir;
		this.control.messages.start = data.start;
		this.control.messages.end = data.end;
	}
	finally
	{
		this.control.scroll_requested = false;
	}
};

room.scroll.io.handle = function(data)
{
	let events = data.chunk;
	this.timeline.insert(events);
	return events.length;
};
