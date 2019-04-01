// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifdef HAVE_LINUX_PERF_EVENT_H
#include <linux/perf_event.h>
#endif

//
// init
//

#ifdef HAVE_LINUX_PERF_EVENT_H
ircd::perf::init::init()
{

}
#else
ircd::perf::init::init()
{
}
#endif

#ifdef HAVE_LINUX_PERF_EVENT_H
ircd::perf::init::~init()
noexcept
{

}
#else
ircd::perf::init::~init()
noexcept
{
}
#endif
