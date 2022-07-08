// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_STDUSE_H

namespace ircd
{
	// 128-bit
	using u128 = uint128_t;
	using i128 = int128_t;
	using f128 = long double;

	// 64-bit
	using u64 = uint64_t;
	using i64 = int64_t;
	using f64 = double;

	// 32-bit
	using u32 = uint32_t;
	using i32 = int32_t;
	using c32 = char32_t;
	using f32 = float;

	// 16-bit
	using u16 = uint16_t;
	using i16 = int16_t;
	using c16 = char16_t;
	#if defined(HAVE__FLOAT16)
	#define HAVE_FP16
	using f16 = _Float16;
	#elif defined(HAVE___FP16)
	#define HAVE_FP16
	using f16 = __fp16;
	#elif defined(__clang__)
	#warning "Missing half-precision floating point support."
	#endif

	// 8-bit
	using u8 = uint8_t;
	using i8 = int8_t;
	#ifdef HAVE_CHAR8_T
	using c8 = char8_t;
	#endif
	#if defined(HAVE___FP8)
	#define HAVE_FP8
	using f8 = __fp8;
	#endif

	using std::get;
	using std::end;
	using std::begin;

	using std::nullptr_t;
	using std::nothrow_t;

	using std::const_pointer_cast;
	using std::static_pointer_cast;
	using std::dynamic_pointer_cast;

	using std::chrono::hours;
	using std::chrono::minutes;
	using std::chrono::seconds;
	using std::chrono::milliseconds;
	using std::chrono::microseconds;
	using std::chrono::nanoseconds;
	using std::chrono::duration;
	using std::chrono::duration_cast;
	using std::chrono::system_clock;
	using std::chrono::steady_clock;
	using std::chrono::high_resolution_clock;
	using std::chrono::time_point;

	using namespace std::string_literals;
	using namespace std::chrono_literals;
	using namespace std::literals::chrono_literals;
	using std::string_literals::operator""s;
	using std::chrono_literals::operator""s;

	namespace ph = std::placeholders;

	template<class... T>
	using ilist = std::initializer_list<T...>;

	using std::error_code;

	/// Simple gimmick to allow shorter declarations when both elements
	/// of a pair are the same.
	template<class A,
	         class B = A>
	using pair = std::pair<A, B>;
}
