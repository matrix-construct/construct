// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CL_KERN_H

/// cl_kernel wrapping
struct ircd::cl::kern
{
	struct range;

	void *handle {nullptr};

  public:
	string_view name(const mutable_buffer &) const;
	uint argc() const;

	std::array<size_t, 3> compile_group_size(void *dev = nullptr) const;
	size_t preferred_group_size_multiple(void *dev = nullptr) const;
	size_t group_size(void *dev = nullptr) const;
	size_t local_mem_size(void *dev = nullptr) const;
	size_t stack_mem_size(void *dev = nullptr) const;

	void arg(const int, data &);
	void arg(const int, const const_buffer &);
	template<class T> void arg(const int, const T &);

	template<class... argv> kern(code &, const string_view &name, argv&&...);
	kern(code &, const string_view &name);
	kern() = default;
	kern(kern &&) noexcept;
	kern &operator=(const kern &) = delete;
	kern &operator=(kern &&) noexcept;
	~kern() noexcept;
};

/// NDRangeKernel dimension range selector
struct ircd::cl::kern::range
{
	std::array<size_t, 3>
	global { 0, 0, 0 },
	local  { 0, 0, 0 },
	offset { 0, 0, 0 };
};

template<class... argv>
inline
ircd::cl::kern::kern(code &c,
                     const string_view &name,
                     argv&&... a)
:kern{c, name}
{
	uint i(0);
	(this->arg(i++, a), ...);
}

template<class T>
inline void
ircd::cl::kern::arg(const int pos,
                    const T &val)
{
	static_assert(!std::is_same<T, cl::data>());
	arg(pos, const_buffer
	{
		reinterpret_cast<const char *>(&val), sizeof(T)
	});
}
