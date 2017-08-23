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
 *
 * Space & Time
 *
 */

/**
 * Date & time tools
 */
mc.date =
{
	now: () =>
		new Date().getTime(),

	unix: () =>
		mc.date.now() / 1000.0,

	ago: (epoch) =>
		mc.date.now() - epoch,
};

// Alias
mc.now = () =>
	mc.date.now();

// UTC suite
mc.date.utc =
{
	pretty: (epoch) =>
		new Date(epoch).toUTCString(),
}

// Locale suite
mc.date.local =
{
	pretty: (epoch) =>
		new Date(epoch).toLocaleString(),
};

// High resolution suite
mc.date.perf =
{
	now: () =>
		window.performance.now(),
};

// Convert numerical information into an english description
mc.date.describe = {};

/**
 * Describe a "time ago" for a past epoch in ms
 */
mc.date.describe.since = (ts, short = false) =>
{
	let elapsed = mc.date.now() - ts;
	return mc.date.describe.elapsed(elapsed, short);
}

/**
 * Describe a "time ago" from a milliseconds param
 */
mc.date.describe.elapsed = (ms, short = false) =>
{
	if(typeof(ms) != "number")
		return "";

	if(ms < 0)
		return "now";

	let order =
	{
		//Year:    (ms / (1000 * 60 * 60 * 24 * 7 * 4 * 12)),
		Month:   (ms / (1000 * 60 * 60 * 24 * 7 * 4)),
		Week:    (ms / (1000 * 60 * 60 * 24 * 7)),
		Day:     (ms / (1000 * 60 * 60 * 24)),
		hour:    (ms / (1000 * 60 * 60)),
		minute:  (ms / (1000 * 60)),
		second:  (ms / (1000)),
	};

	for(let key in order)
	{
		let val = parseInt(order[key]);
		if(val <= 0)
			continue;

		let unit = short? key[0] : " " + key;
		if(!short && val > 1)
			unit += 's';

		return "" + val + "" + unit;
	}

	return short? (ms + "ms") : "less than a second";
};

/**
 * Space / bytes tools
 */
mc.space = {};
mc.space.describe = {};

mc.space.describe.bytes = (bytes) =>
{
	if(typeof(bytes) != "number")
		return "";

	let order =
	{
		TiB: (bytes / (1024 * 1024 * 1024 * 1024)),
		GiB: (bytes / (1024 * 1024 * 1024)),
		MiB: (bytes / (1024 * 1024)),
		KiB: (bytes / (1024)),
		B: bytes,
	};

	for(let key in order)
	{
		let val = order[key];
		if(val >= 1.0)
			return "" + val.toFixed(2) + " " + key;
	}

	return "" + bytes + " Bytes";
};
