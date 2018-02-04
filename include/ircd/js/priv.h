// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JS_PRIV_H

///////////////////////////////////////////////////////////////////////////////
//
// To properly use the private slot: have your structure inherit from `priv_data` and construct
// it with std::make_shared(). When garbage collection of the js object occurs, the private data
// slot will be abstractly considered an `std::shared_ptr<priv_data>` and the reference count
// will be decremented. Note: The js object must have been created with flag JSCLASS_HAS_PRIVATE
//

namespace ircd {
namespace js   {

// ctor()/get()/set()/has()/del() have this overload to manipulate the private slot of an object.
IRCD_OVERLOAD(priv)

// Extend this class to store your data by the managed private slot of an object. Note: The js
// object must have been created with flag JSCLASS_HAS_PRIVATE.
struct priv_data
:std::enable_shared_from_this<priv_data>
{
	// The `delete` of the managed data will be on the type `priv_data`.
	// This vtable is anchored in ircd/js.cc.
	virtual ~priv_data() noexcept;
};

using priv_ptr = std::shared_ptr<priv_data>;

} // namespace js
} // namespace ircd
