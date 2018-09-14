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
 * Stochastic tools
 */
mc.random = {};

/** Generate a random string of characters from a dictionary string
 */
mc.random.string = (len = 1, dictionary = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") =>
{
	let ret = "";
	for(let i = 0; i < len; i++)
		ret += dictionary.charAt(Math.floor(Math.random() * dictionary.length));

	return ret;
};
