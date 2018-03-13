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
#define HAVE_IRCD_BUFFER_SHARED_BUFFER_H

template<class buffer>
struct ircd::buffer::shared_buffer
:std::shared_ptr<char>
,buffer
{
	shared_buffer(const size_t &size);
	shared_buffer() = default;
};

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(const size_t &size)
:std::shared_ptr<char>
{
	std::make_shared<char>(size)
}
,buffer
{
	std::shared_ptr<char>::get(), size
}
{}
