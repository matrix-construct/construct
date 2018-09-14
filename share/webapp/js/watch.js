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
 * Model watching
 *
 */

/**
 * Register a handler to be called when a variable at the path updates.
 * The first element of ["client"] in the path is implied and should not
 * be specified.
 *
 * Example: To watch for changes to mc.instance.ready:
 *
 * mc.watch(["instance", "ready"], (cur, old) => {});
 *
 * This function cannot be called until Angular inits the controllers
 * (it can't be called from the initial outer script).
 */
mc.watch = function(path, handler)
{
	let dir = mc.watch.tree;
	for(let i = 0; i < path.length - 1; i++)
	{
		if(!(path[i] in dir))
			dir[path[i]] = {};

		dir = dir[path[i]];
	}

	let wrapper =
	{
		path: path,
		handler: handler,
	};

	if((path.back() in dir))
	{
		dir[path.back()].push(wrapper);
		return wrapper;
	}

	dir[path.back()] = [wrapper];
	let watcher = () => maybe(() =>
	{
		let watched = client;
		path.forEach((part) => watched = watched[part]);
		return watched;
	});

	mc.watch.$scope.$watch(watcher, (cur, prev) =>
	{
		let handlers = dir[path.back()];
		handlers.forEach((wrapper) => wrapper.handler(cur, prev));
	});

	return wrapper;
};

/**
 * The watch tree describes handlers that fire when a variable that corresponds
 * to the path in the tree is updated. The tree is rooted under `client`
 * so anything in the client is reachable. The correspondence is
 * `mc.watch.tree.instance.authentic` to watch the var `mc.instance.authentic`
 */
mc.watch.tree = {};

/**
 * Deregister (unsubscribe) from a previous call to mc.watch()
 * by passing the return value of mc.watch() to this function.
 */
mc.watch.unwatch = function(wrapper)
{
	let path = wrapper.path;
	let dir = mc.watch.tree;
	for(let i = 0; i < path.length; i++)
	{
		if(!(path[i] in dir))
			dir[path[i]] = {};

		dir = dir[path[i]];
	}

	let idx = dir.indexOf(wrapper);
	if(idx < 0)
		return false;

	dir.splice(idx, 1);
	return true;
};

// Alias
mc.unwatch = mc.watch.unwatch;

/**
 * Convenience to mc.watch() which only calls when the value makes
 * a transition to your expectation.
 */
mc.watch.when = function(path, expect, handler)
{
	return mc.watch(path, (cur, prev) =>
	{
		if(cur === expect)
			handler(cur, prev);
	});
};

/**
 * Convenience to mc.watch() which only calls when the value makes
 * a transition away from your expectation.
 */
mc.watch.was = function(path, expect, handler)
{
	return mc.watch(path, (cur, prev) =>
	{
		if(prev === expect)
			handler(cur, prev);
	});
};

/**
 * Convenience to mc.watch() which only calls when the value makes
 * a transition to anything other than the expectation.
 */
mc.watch.unless = function(path, expect, handler)
{
	return mc.watch(path, (cur, prev) =>
	{
		if(cur !== expect)
			handler(cur, prev);
	});
};

/**
 * Convenience to mc.watch() which only calls when the specific
 * transition from before -> after has occurred.
 */
mc.watch.transit = function(path, before, after, handler)
{
	return mc.watch(path, (cur, prev) =>
	{
		if(prev === before && cur === after)
			handler(cur, prev);
	});
};

/**
 * Convenience to mc.watch() which sets the value to desired when
 * it is equal to expected.
 */
mc.watch.compare_exchange = function(path, expected, desired)
{
	return mc.watch.when(path, expected, (cur, prev) =>
	{
		if(cur !== expected)
			return;

		Object.terminal_edge(client, path, (key, obj) =>
		{
			obj[key] = desired;
		});
	});
};

/**
 * Convenience to mc.watch() which requires conditions to be
 * satisfied to call handler.
 *
 * The condition structure requires a path, and then an optional
 * suite of conditions where the key indicates the type of test for
 * the value analogous to the mc.watch function.
 *
 *  condition:
 *  {
 *     path: ["foo", "bar"]
 *     test: (cur, prev) => true,
 *     was: 123,
 *     when: 456,
 *     unless: 789,
 *  }
 */
mc.watch.conditions = function(handler, conditions, functor)
{
	conditions.forEach((condition) =>
	{
		mc.watch(condition.path, (cur, prev) =>
		{
			if(("test" in condition))
				condition.result = condition.test(cur, prev);

			if(("when" in condition))
				condition.result = cur === condition.when;

			if(("was" in condition))
				condition.result = prev === condition.was;

			if(("unless" in condition))
				condition.result = cur !== condition.unless;

			if(("defined" in condition))
				condition.result = condition["defined"]? cur !== undefined:
				                                         cur === undefined;

			if(functor.call(conditions, (condition) => condition.result))
				handler();
		});
	});
};

/**
 * Convenience to mc.watch() which requires every condition to
 * be satisfied to call handler
 */
mc.watch.every = function(handler, conditions)
{
	mc.watch.conditions(handler, conditions, Array.prototype.every);
};

/**
 * Convenience to mc.watch() which requires some condition to
 * be satisfied to call handler
 */
mc.watch.some = function(handler, conditions)
{
	mc.watch.conditions(handler, conditions, Array.prototype.some);
};
