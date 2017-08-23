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

/******************************************************************************
 * Event
 *
 */
mc.event = class
{
	constructor(event = {})
	{
		Object.update(this, event);
		Object.defaults(this,
		{
			event_id: mc.m.random.mxid("$"),
			origin_server_ts: mc.now(),
		});
	}

	// functors
	static is_state(event)             { return event.state_key !== undefined;                     }
	static is_not_state(event)         { return !mc.event.is_state(event);                         }
	static is_type(type, event)        { return event.type == type;                                }
	static is_pending(event)           { return !event.event_id.startsWith("$");                   }
	static is_local(event)             { return mc.m.domid(event.event_id) == "localhost";         }
	static is_not_local(event)         { return !mc.event.is_local(event);                         }
	static cmp(a, b)                   { return a.cmp(b);                                          }
};

/**
 * Get events surrounding this event
 */
mc.event.prototype.context = function(opts = {})
{
	if(!this.room_id || !this.event_id)
		return false;

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.context.get(this.room_id, this.event_id, opts, handler);
};

/**
 * Ordering comparator for events.
 */
mc.event.prototype.cmp = function(other)
{
	let cmp = (a, b) =>
	    a == undefined?  undefined:
	    b == undefined?  undefined:
		a < b?          -1:
		a > b?           1:
		a == b?          0:
		                 undefined;

	let age_a = maybe(() => this.unsigned.age);
	let age_b = maybe(() => other.unsigned.age);
	let ret;// = cmp(age_b, age_a);
	//if(ret !== undefined)
	//	return ret;

	// The origin server ts is used next to calculate the age
	let ts_a = maybe(() => this.origin_server_ts);
	let ts_b = maybe(() => other.origin_server_ts);
	ret = cmp(ts_a, ts_b);
	if(ret !== undefined)
		return ret;

	// Make an attempt to salvage any time data we actually have to compare
	if((age_a || ts_a) && (age_b || ts_b))
	{
		let now = mc.now();
		if(age_a && !ts_a)
			ts_a = now - age_a;

		if(age_b && !ts_b)
			ts_b = now - age_b;

		ret = cmp(ts_a, ts_b);
		if(ret !== undefined)
			return ret;
	}

	// Sort by room ID
	ret = cmp(this.room_id, other.room_id);
	if(ret !== undefined)
		return ret;

	// Sort by event ID
	ret = cmp(this.event_id, other.event_id);
	if(ret !== undefined)
		return ret;

	// Sort by sender
	ret = cmp(this.sender, other.sender);
	if(ret !== undefined)
		return ret;

	// Sort by content
	ret = cmp(this.content, other.content);
	if(ret !== undefined)
		return ret;

	// Final attempt
	return cmp(this, other);
};
