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

/**************************************
 * Scrolling & Viewport utils
 *
 */

mc.scroll = {};

mc.scroll.at = function(e)
{
	return $(e).prop("scrollTop");
};

mc.scroll.at.top = function(e)
{
	return mc.scroll.at(e) == 0;
};

mc.scroll.at.bottom = function(e)
{
	let st = $(e).prop("scrollTop");
	let sh = $(e).prop("scrollHeight");
	let ch = $(e).prop("clientHeight");
	return sh - ch <= st + 1;
};

mc.scroll.to = function(e, pos, speed = "slow")
{
	return $(e).animate({ scrollTop: pos }, speed);
};

mc.scroll.to.top = function(e, speed = "slow")
{
	return mc.scroll.to(e, 0, speed);
};

mc.scroll.to.bottom = function(e, speed = "slow")
{
	return mc.scroll.to(e, $(e).prop("scrollHeight"), speed);
};

/**
 * top: the percentage of area invisible above the viewport.
 * view: the percentage of the area in the viewport.
 * bottom: the percentage of the area invisible below the viewport.
 */
mc.scroll.pct = function(e)
{
	let st = $(e).prop("scrollTop");
	let sh = $(e).prop("scrollHeight");
	let ch = $(e).prop("clientHeight");
	return {
		top: st / sh,
		view: ch / sh,
		bottom: (sh - (st + ch)) / sh,
	}
};

/**
 * Given a DOM element, this function tests if the user can see the element,
 * (and for now it returns true only if the *whole* element is in the viewport).
 * Originally intended to figure out which events in the Matrix room timeline have
 * fallen into the scrollback and can be removed.
 */
function isElementInViewport(e)
{
	if(typeof jQuery === "function" && e instanceof jQuery)
		e = e[0];

	let rect = e.getBoundingClientRect();
	return (
		rect.top >= 0 &&
		rect.left >= 0 &&
		rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
		rect.right <= (window.innerWidth || document.documentElement.clientWidth)
	);
}

mc.get_rect = function(e)
{
	if(typeof jQuery === "function" && e instanceof jQuery)
		e = e[0];

	if(e === undefined)
		return undefined;

	let rect = e.getBoundingClientRect();
	return {
		top: rect.top,
		right: rect.right,
		bottom: rect.bottom,
		left: rect.left,
	};
};
