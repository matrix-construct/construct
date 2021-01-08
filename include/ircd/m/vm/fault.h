// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_FAULT_H

namespace ircd::m::vm
{
	enum fault :uint;
	using fault_t = std::underlying_type<fault>::type;

	string_view reflect(const fault &);
	http::code http_code(const fault &);
}

/// Evaluation faults. These are reasons which evaluation has halted but may
/// continue after the user defaults the fault. They are basically types of
/// interrupts and traps, which are supposed to be recoverable. Only the
/// GENERAL protection fault (#gp) is an abort and is not supposed to be
/// recoverable. The fault codes have the form of bitflags so they can be
/// used in masks; outside of that case only one fault is dealt with at
/// a time so they can be switched as they appear in the enum.
///
enum ircd::m::vm::fault
:uint
{
	ACCEPT        = 0x00,  ///< No fault.
	EXISTS        = 0x01,  ///< Replaying existing event. (#ex)
	GENERAL       = 0x02,  ///< General protection fault. (#gp)
	INVALID       = 0x04,  ///< Non-conforming event format. (#ud)
	AUTH          = 0x08,  ///< Auth rules violation. (#av)
	STATE         = 0x10,  ///< Required state is missing (#st)
	EVENT         = 0x20,  ///< Eval requires addl events in the ef register (#ef)
	BOUNCE        = 0x40,  ///< The event is not needed at this time (#bo)
	DONOTWANT     = 0x80,  ///< The event will never be needed (cache this) (#dw)
};
