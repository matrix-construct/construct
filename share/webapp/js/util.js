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


/*
 ******************************************************************************
 ******************************************************************************
 *
 * Abstract Utils
 *
 */

/**
 */
let defined = (value) =>
	value !== undefined;

/** typeif - Functional if(typeof(foo) == "bar") { ... }
 */
let typeif = (value, type, closure) =>
	typeof(value) === type? closure() : undefined;

/** typeswitch - Functional switch(typeof(value)). Allows type
 * cases to be defined more liberally in the dictionary argument as well.
 */
let typeswitch = (value, cases) =>
{
	const type = typeof(value);
	const _case = cases[type];
	switch(typeof(_case))
	{
		case "undefined":  return cases["default"];
		case "function":   return _case();
		default:           return _case;
	}
};

/** empty - What we consider to be "empty" values. Intended as the test
 * functor for the Object.clean() function (defined later) which removes
 * useless values from objects usually before transmitting or storing etc.
 */
let empty = (value) =>
	typeswitch(value,
	{
		"undefined":  true,
		"object":     () => value === null || !Object.keys(value).length,
		"string":     () => !value.length,
		"function":   false,
		"boolean":    false,
		"number":     false,
		"default":    false,
	})
;

/** length -
 */
let length = (value) =>
	typeswitch(value,
	{
		"object":     () => value === null? 0 : Object.keys(value).length,
		"string":     () => value.length,
		"default":    undefined,
	})
;

/** maybe - V8-style nested traversal which handles the exceptions for bad access.
 * Access a nested value inside the maybe() closure to return the value if valid,
 * or return undefined when it doesn't exist or can't be traversed etc.
 *
 * Consider object:
 * { foo: { bar: 'baz' } }
 * 
 * let a = foo.bar.baz;                     // exception thrown (bad)
 * let b = maybe(() => foo.bar.baz);        // b = undefined (good)
 * let c = maybe(() => foo.bar.baz.bam);    // c = undefined (good)
 * let d = maybe(() => foo.bar);            // d = 'baz' (good)
 */
let maybe = (closure = () => undefined) =>
{
	try
	{
		return closure();
	}
	catch(e)
	{
		if(e instanceof ReferenceError)
			return undefined;

		if(e instanceof TypeError)
			return undefined;

		throw e;
	}
};

let mayif = (test = () => true, closure = () => undefined) =>
{
	return maybe(() => closure(test()));
};

/**
 */
let toggle = (obj, key, value = undefined) =>
{
	if(value === undefined)
		obj[key] =! obj[key];
	else
		obj[key] = value;
};

/**
 * Like "toggle" but prunes the object of the key when switching
 * to the false state.
 */
let togdel = (obj, key, value = true) =>
{
	if(!(key in obj) && value !== false)
		obj[key] = value;
	else
		delete obj[key];
};

/**
 * Like "togdel" but checks the current value against the argument
 */
let togswap = (obj, key, value = true) =>
{
	if((key in obj) && (obj[key] === value || value === false))
		delete obj[key];
	else if(value !== undefined)
		obj[key] = value;
};

/** Object.always() - Returns the argument unless undefined or null, then
 * an empty object is returned instead.
 */
Object.always = (obj) =>
	obj !== undefined && obj !== null? obj : {}
;

/** Object.each() - a forEach for an object where the closure argument
 * is presented with a (key, value) pair
 */
Object.each = (obj, closure) =>
	Object.keys(Object.always(obj)).forEach((key, i) =>
		closure(key, obj[key], i)
	)
;

/** Object.reach() - reverse iteration from Object.each()
 */
Object.reach = (obj, closure) =>
	Object.keys(Object.always(obj)).reverse().forEach((key, i) =>
		closure(key, obj[key], i)
	)
;

/** Object.oeach() - enumeration with own properties
 */
Object.oeach = (obj, closure) =>
	Object.getOwnPropertyNames(Object.always(obj)).forEach((key, i) =>
		closure(key, obj[key], i)
	)
;

/** Object.aeach() - enumeration of all properties
 */
Object.aeach = (obj, closure) =>
{
	let i = 0;
	for(let key in obj)
		closure(key, obj[key], i++);

	return i;
};

/** Object.peach() - enumeration of prototype properties
 */
Object.peach = (obj, closure) =>
{
	let i = 0;
	for(let key in obj)
		if(!obj.hasOwnProperty(key))
			closure(key, obj[key], i++);

	return i;
};

/** Object.ofeach()
 */
Object.ofeach = (obj, closure) =>
{
	let i = 0;
	if(maybe(() => obj[Symbol.iterator] === 'function'))
		for(let [key, val] of obj)
			closure(key, val, i++);

	return i;
};

/** Object.map()
 */
Object.map = (obj, closure) =>
{
	let ret = {};
	Object.each(obj, (key, val) =>
		ret[key] = closure(key, val)
	);

	return ret;
};

/** Object.values() - Fills in for the experimental Object.values() standard js
 * function which is not always available.
 */
Object.values = Object.values? Object.values : (obj) =>
{
	const push = (val, key) =>
	{
		val.push(obj[key]);
		return val;
	};

	return Object.keys(Object.always(obj)).reduce(push, []);
};

/** Object.defaults() - Unlike Object.update(), an existing value in the target object
 * is not touched if it exists, otherwise the source value is used.
 * recursive.
 */
Object.defaults = (tgt,
                   src,
                   enumerator = Object.each) =>
	enumerator(src, (key, val) =>
	{
		if(typeof(tgt[key]) == "undefined")
		{
			if(typeof(val) == "object")
				tgt[key] = Object.copy(val, enumerator);
			else
				tgt[key] = val;
		}
		else if(typeof(tgt[key]) == "object")
		{
			if(typeof(val) == "object")
				Object.defaults(tgt[key], val, enumerator);
		}
	});

/** Object.update() - Like Object.assign() it writes over the target property
 * with the source property, but recursively in tgt.
 * recursive.
 */
Object.update = (tgt,
                 src,
                 enumerator = Object.each) =>
	enumerator(src, (key, val) =>
	{
		if(typeof(val) == "object")
		{
			if(typeof(tgt[key]) != "object")
				tgt[key] = Object.copy(val, enumerator);
			else
				Object.update(tgt[key], val, enumerator);
		}
		else tgt[key] = val;
	});

/** Object.clear()
 */
Object.clear = (src,
                enumerator = Object.each) =>
	enumerator(src, (key, val) =>
	{
		delete src[key];
	});

/** Object.clean() - Recursively strip keys in an object where the value passes the test.
 * The test is empty() by default. Objects like {foo: undefined} which still enumerate
 * the key 'foo' as a member can have 'foo' deleted using this function.
 * recursive.
 */
Object.clean = (obj,
                test = empty,
                enumerator = Object.each) =>
	enumerator(obj, (key, val) =>
	{
		switch(typeof(val))
		{
			case "object":
				Object.clean(val, test, enumerator);
				//[[fallthrough]]

			default:
				if(test(val))
					delete obj[key];

				break;
		};
	});

/** Object.copy() - Creates and returns fresh object updated from src
 * recursive.
 */
Object.copy = (src,
               enumerator = Object.each) =>
{
	const tgts =
	{
		"object":   Array.isArray(src)? [] : {},
		"default":  {},
	};

	const tgt = typeswitch(src, tgts);
	Object.update(tgt, src, enumerator);
	return tgt;
};

/** Object.copy() - Creates and returns fresh object updated from src
 * recursive.
 */
Object.copy.include = (src,
                       whitelist = [],
                       enumerator = Object.each) =>
{
	whitelist = typeswitch(whitelist,
	{
		"object": whitelist,
		"string": [whitelist],
	});

	const tgt = typeswitch(src,
	{
		"object":   Array.isArray(src)? [] : {},
		"default":  {},
	});

	Object.update(tgt, src, (obj, closure) =>
		enumerator(obj, (key, val) =>
		{
			if((key in whitelist))
				closure(key, val);
		})
	);

	return tgt;
};

/** Object.copy() - Creates and returns fresh object updated from src
 * recursive.
 */
Object.copy.exclude = (src,
                       blacklist = [],
                       enumerator = Object.each) =>
{
	blacklist = typeswitch(blacklist,
	{
		"object": blacklist,
		"string": [blacklist],
	});

	const tgt = typeswitch(src,
	{
		"object":   Array.isArray(src)? [] : {},
		"default":  {},
	});

	Object.update(tgt, src, (obj, closure) =>
		enumerator(obj, (key, val) =>
		{
			if(!(key in blacklist))
				closure(key, val);
		})
	);

	return tgt;
};

/** Object.defaults() - Unlike Object.update(), an existing value in the target object
 * is not touched if it exists, otherwise the source value is used.
 * recursive.
 */
Object.defaulting = (tgt,
                     key,
                     defaulting_value = {}) =>
{
	if(!(key in tgt)) switch(typeof(defaulting_value))
	{
		case "object":
			tgt[key] = Object.copy(defaulting_value);
			break;

		case "function":
			tgt[key] = defaulting_value();
			break;

		default:
			tgt[key] = defaulting_value;
			break;
	}

	return tgt[key];
}

/* This is obviously no good if you don't leave the last element off your path
 */
Object.traverse = (obj, path) =>
{
	path.forEach((part) =>
		obj = obj[part]
	);

	return obj;
};

/* Traverse the path up to the second to last element
 */
Object.penultimate = (obj, path) =>
	Object.traverse(obj, path.slice(0, path.length - 1));

/* Traverses the path to present the parent and child at the destination
 */
Object.terminal_edge = (obj, path, destination) =>
	destination(Object.dotdot(obj, path)[path.back()], Object.dotdot(obj, path));

/** set theory suite
 */
Object.union = (a, b, closure) =>
{
	Object.keys(a).map((key, i) =>
	{
		closure(key);
	});

	Object.keys(b).map((key, i) =>
	{
		if(!(key in a))
			closure(key);
	});
};

/** set theory suite
 */
Object.intersection = (a, b, closure) =>
	Object.keys(a).map((key, i) =>
	{
		if((key in b))
			closure(key);
	});
;

/** set theory suite
 */
Object.difference = (a, b, closure) =>
	Object.keys(a).map((key, i) =>
	{
		if(!(key in b))
			closure(key);
	});
;

/** set theory suite
 */
Object.symmetric_difference = (a, b, closure) =>
{
	Object.difference(a, b, closure);
	Object.difference(b, a, closure);
};

/** Count returns an integer for the number of elements
 * that pass the condition
 */
Array.count = function(array, condition)
{
	const reducer = (acc, val) => acc += val;
	const mapper = (element, index, array) => condition(element, index, array)? true : false;
	return array.map(mapper).reduce(reducer, 0);
};

/** C++ style first-element access
 */
Object.defineProperty(Array.prototype, 'front',
{
	writable: false,
	enumerable: false,
	configurable: false,
	value: function()
	{
		return (this)[0];
	},
});

/** C++ style last-element access
 */
Object.defineProperty(Array.prototype, 'back',
{
	writable: false,
	enumerable: false,
	configurable: false,
	value: function()
	{
		return (this)[this.length - 1];
	},
});

/** filterIndex() returns an array of indexes rather than values
 */
Object.defineProperty(Array.prototype, 'filterIndex',
{
	writable: false,
	enumerable: false,
	configurable: false,
	value: function(condition)
	{
		let ret = [];
		this.forEach((element, index, array) =>
		{
			if(condition(element, index, array))
				ret.push(index);
		});

		return ret;
	},
});

/** Non-cryptographic Bernstein string hasher
 */
Object.defineProperty(String.prototype, 'hash',
{
	writable: false,
	enumerable: false,
	configurable: false,
	value: function(ret = 7681, i = 0)
	{
		for(; i < this.length; i++)
			ret = (ret * 33) ^ this.charCodeAt(i);

		return ret;
	},
});

/* Augmentation of Function
 */
const Class =
{
	construction: Symbol("Class.construction"),
};

/** Bind a tree of functions organized into namespaces (using
 * a nested object hierarchy) to a this value
 */
Function.bindtree = function(obj, that, ...args)
{
	const binding = function(src, that)
	{
		let ret = typeswitch(src,
		{
			"function": () =>
			{
				switch(src[Class.construction])
				{
					case true:
						return new src(that, ...args);

					default:
						return src.bind(that);
				}
			},

			"object": () =>
			{
				try
				{
					return Object.create(new src.__proto__(that));
				}
				catch(e) {}

				try
				{
					return Object.create(src.__proto__);
				}
				catch(e) {}

				return Object.create(Object.prototype);
			},
		});

		Object.each(src, (name, func) =>
		{
			switch(typeof(func))
			{
				case "function":
				case "object":
					ret[name] = binding(src[name], that);
					break;

				default:
					ret[name] = src[name];
					break;
			}
		});

		return ret;
	};

	return binding(obj, that);
};

/** Bind a tree of functions organized into namespaces (using
 * a nested object hierarchy) to a this value
 */
Function.bindtree.self = function(self, obj, that = self)
{
	Object.update(self, Function.bindtree(obj, that));
};

/** Extend this class to bind a tree of functions to your class.
 *
 * In your ctor: super(tree.root);
 */
Object.bindtree = class
{
	constructor(obj, opts = {}, that = this)
	{
		Object.each(obj, (name, branch) =>
		{
			opts.value = Function.bindtree(branch, that);
			Object.defineProperty(that, name, opts);
		});
	}
};

/**
 * Creates a proxy handler which always returns more proxies instead of plain
 * objects. The mission here is to have objects returned from anywhere under
 * the root always have their access completely handled.
 */
Proxy.recursive = class
{
	constructor()
	{
		//console.log("Proxy.recursive");
	}

	get(state, key, proxy)
	{
		console.log("Proxy.recursive.get(" + key + ") ((" + typeof(state[key]) + "))");

		switch(typeof(state[key]))
		{
			case "object":
			case "function":
				return new Proxy(state[key], new Proxy.recursive());

			default:
				return state[key];
		}
	}

	set(state, key, val, proxy)
	{
		console.log("Proxy.recursive.set(" + key + ", " + typeof(val) + ")");

		switch(typeof(val))
		{
			case "object":
			case "function":
				state[key] = new Proxy(val, new Proxy.recursive());
				return true;

			default:
				state[key] = val;
				return true;
		}

		return true;
	}
};

/**************************************
 *
 * Additional tools
 *
 */

/** When using a "last argument is always the callback" convention, this finds that
 * last argument of the function in which this is called and attempts to call it and
 * forward the supplied arguments. Silent errors.
 */
function callback(arguments_object, ...callback_arguments)
{
	if(!arguments_object.length)
		return;

	const user_function = arguments_object[arguments_object.length - 1];
	if(typeof(user_function) != "function")
		return;

	return user_function(...callback_arguments);
}

/**
 * Validate a string is a domain name (weak)
 */
function valid_domain(string)
{
	if(typeof(string) != "string")
		return false;

	if(!string.length)
		return false;

	return valid_domain.expression.test(string);
}

//TODO: improve me
valid_domain.expression = new RegExp("^[a-zA-Z0-9][a-zA-Z0-9-]{1,61}[a-zA-Z0-9](?:\.[a-zA-Z]{1,})*$");
