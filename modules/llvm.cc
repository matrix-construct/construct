// The Construct
//
// Copyright (C) The Construct Developers
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if !defined(IRCD_USE_LLVM) \
&& __has_include("<llvm/Config/llvm-config.h>") \
&& __has_include("<llvm/Object/Wasm.h>")
	#define IRCD_USE_LLVM
#endif

#if defined(IRCD_USE_LLVM) // -------------------------------------------------

#include <llvm/Config/llvm-config.h>
#include <llvm/Object/Wasm.h>

namespace ircd::llvm
{
	extern log::log log;
	extern info::versions version_api, version_abi;

	static void init(), fini();
}

ircd::mapi::header
IRCD_MODULE
{
	"LLVM Compiler Infrastructure",
	ircd::llvm::init,
	ircd::llvm::fini,
};

decltype(ircd::llvm::version_api)
ircd::llvm::version_api
{
    "llvm", info::versions::API, 0,
    {
		LLVM_VERSION_MAJOR,
		LLVM_VERSION_MINOR,
		LLVM_VERSION_PATCH,
    },
    LLVM_VERSION_STRING,
};

decltype(ircd::llvm::version_abi)
ircd::llvm::version_abi
{
    "llvm", info::versions::ABI, 0
};

decltype(ircd::llvm::log)
ircd::llvm::log
{
	"llvm"
};

void
ircd::llvm::init()
{
	log::info
	{
		log, "LLVM %s library; host:%s; %s%s%s%s%s%s",
		version_api.string,
		LLVM_HOST_TRIPLE,
		LLVM_ENABLE_THREADS?
			"multithreading ": "",
		LLVM_HAS_ATOMICS?
			"atomics ": "",
		LLVM_ON_UNIX?
			"unix ": "",
		LLVM_USE_INTEL_JITEVENTS?
			"intel-jit ": "",
		LLVM_USE_OPROFILE?
			"oprofile-jit ": "",
		LLVM_USE_PERF?
			"perf-jit ": "",
	};
}

void
ircd::llvm::fini()
{

}

#endif // IRCD_USE_LLVM  ------------------------------------------------------
