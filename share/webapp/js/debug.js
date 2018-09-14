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
 *
 * Debugging tools
 *
 */

/**
 * Debugging and testing related suite. This provides a more friendly version
 * of JSON.stringify() for debugging the contents of deep objects with formatting
 * and a recursion limit specifier; also a convenience tie-in to console.log();
 *
 * ex:
 *       debug.object({foo: {bar: 'baz'}}, 2);     // prints all content to console
 *       debug.object({foo: {bar: 'baz'}}, 1);     // does not print members of foo
 */
const debug =
{
	/** Recursive object stringification in pseudo-JSON
	 *
	 * @param {object} obj - Object to represent as string.
	 * @param {number} r_max - The maximum recursion depth.
	 * @param {number} r - The current frame's recursion depth.
	 * @returns {string} A string representation of the object.
	 */
	stringify(obj, r_max = 10, enumerator = Object.aeach, r = 1)
	{
		let tabular = (r, ret = "", i = 0) =>
		{
			for(; i < r; i++)
				ret += "\t";

			return ret;
		};

		let _object = (k, obj) =>
		{
			let n = empty(obj[k]);
			let s = n? "{}" : this.stringify(obj[k], r_max, enumerator, r + 1);
			return "\n" + tabular(r) + k + ": " + s + ',';
		};

		let _string = (k, obj) =>
			"\n" + tabular(r) + k + ": " + obj[k] + ",";

		let _function = (k) =>
			"\n" + tabular(r) + k + "()" + ",";

		switch(typeof(obj))
		{
			case "object":
				break;

			default:
				return tabular(r - 1) + JSON.stringify(obj);
		}

		// Iterate and append the members of the object to the return string
		// with recursion for objects/arrays if the max depth is not exceeded.
		let ret = "\n" + tabular(r - 1) + "{";
		let _property = (k) =>
		{
			switch(typeof(obj[k]))
			{
				case "object":
					if(r < r_max)
					{
						ret += _object(k, obj);
						break;
					} // [[fallthrough]]

				case "boolean":
				case "number":
				case "string":
					ret += _string(k, obj);
					break;

				case "function":
					ret += _function(k);
					break;
			}
		};

		enumerator(obj, (key) => _property(key));
		return ret + "\n" + tabular(r - 1) + "}";
	},

	log(func, obj, r_max = 1, enumerator = Object.aeach)
	{
		func(this.stringify(obj, r_max, enumerator));
	},

	/** Stringify to console.log() */
	object(obj, r_max = 2, enumerator = Object.aeach)
	{
		this.log(console.log, obj, r_max, enumerator);
	},

	/** Stringify to console.error() */
	error(obj, r_max = 2, enumerator = Object.aeach)
	{
		this.log(console.error, obj, r_max, enumerator);
	},

	/** Convenience Node.js style callback function to pass as the callback
	 * argument which prints the resulting object for viewing in the console.
	 */
	callback(error, data)
	{
		let obj = error !== undefined? error : data;
		let text = debug.stringify(obj, 2);

		if(error !== undefined)
			console.error(text);
		else
			console.log(text);
	},

	/** TODO: XXX
	 * To debug Angular model updating.
	 */
	digest(name, $scope)
	{
		let wc = $scope.$$watchersCount;
		let watchers = " $$watchersCount: " + wc;
		console.log("$digest() #" + $scope.$id + " '" + name + "'" + watchers);
	},
};

/** Convenience because I keep typing it
 */
Object.debug = function(...obj)
{
	return debug.object(...obj);
};

/**
 */
function stacktrace()
{
	function _stacktrace(caller)
	{
		if(!caller)
			return [];

		const name = caller.toString();
		const args = caller.arguments.join(',');
		const rep = name.split('(')[0].substring(9) + '(' + args + ')';
		return _stacktrace(caller.caller).concat([rep]);
	}

	return _stacktrace(arguments.callee.caller);
}

/** convenience stub
 */
function test(value = undefined, quiet = false)
{
	switch(typeof(value))
	{
		case "undefined":
			if(!quiet) console.log("test undefined");
			return true;

		case "string":
		case "boolean":
			if(!quiet) console.log("test " + value);
			return value;

		case "function":
			quiet? value() : console.log("test[F] " + value());

		default:
			if(!quiet) console.log("test: " + JSON.stringify(value));
			return true;
	}
}
