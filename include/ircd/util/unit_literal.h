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
#define HAVE_IRCD_UTIL_UNIT_LITERAL_H

//
// C++14 user defined literals
//
// These are very useful for dealing with space. Simply write 8_MiB and it's
// as if a macro turned that into (8 * 1024 * 1024) at compile time (it did).
//

/// (Internal) Defines a unit literal with an unsigned long long basis.
///
#define IRCD_UNIT_LITERAL_UL(name, morphism)        \
constexpr auto                                      \
operator"" _ ## name(const unsigned long long val)  \
noexcept                                            \
{                                                   \
    return (morphism);                              \
}

/// (Internal) Defines a unit literal with a signed long long basis
///
#define IRCD_UNIT_LITERAL_LL(name, morphism)        \
constexpr auto                                      \
operator"" _ ## name(const long long val)           \
noexcept                                            \
{                                                   \
    return (morphism);                              \
}

/// (Internal) Defines a unit literal with a long double basis
///
#define IRCD_UNIT_LITERAL_LD(name, morphism)        \
constexpr auto                                      \
operator"" _ ## name(const long double val)         \
noexcept                                            \
{                                                   \
    return (morphism);                              \
}

namespace ircd {
inline namespace util {

// IEC unit literals
IRCD_UNIT_LITERAL_UL( B,    val                                                              )
IRCD_UNIT_LITERAL_UL( KiB,  val * 1024LL                                                     )
IRCD_UNIT_LITERAL_UL( MiB,  val * 1024LL * 1024LL                                            )
IRCD_UNIT_LITERAL_UL( GiB,  val * 1024LL * 1024LL * 1024LL                                   )
IRCD_UNIT_LITERAL_UL( TiB,  val * 1024LL * 1024LL * 1024LL * 1024LL                          )
IRCD_UNIT_LITERAL_UL( PiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL                 )
IRCD_UNIT_LITERAL_UL( EiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL        )

IRCD_UNIT_LITERAL_LD( B,    val                                                              )
IRCD_UNIT_LITERAL_LD( KiB,  val * 1024.0L                                                    )
IRCD_UNIT_LITERAL_LD( MiB,  val * 1024.0L * 1024.0L                                          )
IRCD_UNIT_LITERAL_LD( GiB,  val * 1024.0L * 1024.0L * 1024.0L                                )
IRCD_UNIT_LITERAL_LD( TiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L                      )
IRCD_UNIT_LITERAL_LD( PiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L            )
IRCD_UNIT_LITERAL_LD( EiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L  )

// SI unit literals
IRCD_UNIT_LITERAL_UL( KB,   val * 1000LL                                                     )
IRCD_UNIT_LITERAL_UL( MB,   val * 1000LL * 1000LL                                            )
IRCD_UNIT_LITERAL_UL( GB,   val * 1000LL * 1000LL * 1000LL                                   )
IRCD_UNIT_LITERAL_UL( TB,   val * 1000LL * 1000LL * 1000LL * 1000LL                          )
IRCD_UNIT_LITERAL_UL( PB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL                 )
IRCD_UNIT_LITERAL_UL( EB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL        )

IRCD_UNIT_LITERAL_LD( KB,   val * 1000.0L                                                    )
IRCD_UNIT_LITERAL_LD( MB,   val * 1000.0L * 1000.0L                                          )
IRCD_UNIT_LITERAL_LD( GB,   val * 1000.0L * 1000.0L * 1000.0L                                )
IRCD_UNIT_LITERAL_LD( TB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L                      )
IRCD_UNIT_LITERAL_LD( PB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L            )
IRCD_UNIT_LITERAL_LD( EB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L  )

} // namespace util
} // namespace ircd
