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
