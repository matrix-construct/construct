// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// This unit contains customization and experimentation for the core context
// switching routines. First note that everything here is platform-specific
// and not implemented on all platforms. Furthermore, boost::context functions
// overriden here are also overriden for the entire address space; if other
// libraries use boost::context our code will hook their context switches
// which we might not want. All of this must be considered to build or change
// any functionality from here.

#include <ircd/config.h>
#if defined(__x86_64__) && defined(__ELF__)

.file "ctx_x86_64.S"
.globl jump_fcontext

.text
.type jump_fcontext, @function

.align 16
// returns struct transfer_t { void *fctx, *data; };
// param DI = void *to
// param SI = void *vp
jump_fcontext:
	push      %rbp                     // store rbp 0x30
	push      %rbx                     // store rbx 0x28
	push      %r15                     // store r15 0x20
	push      %r14                     // store r14 0x18
	push      %r13                     // store r13 0x10
	push      %r12                     // store r12 0x08
	subq      $0x08, %rsp              // alloc fc_mxcsr | fc_x87_cw
//	fnstcw    0x04(%rsp)               // store x87 control-word
//	stmxcsr   0x00(%rsp)               // store MMX control- and status-word
	xchgq     %rsp, %rdi               // swap stack
	movq      %rsi, %rdx               // store transfer_t.data (retval)
	movq      %rdi, %rax               // store transfer_t.fctx (retval)
//	ldmxcsr   0x00(%rsp)               // load MMX control- and status-word
//	fldcw     0x04(%rsp)               // load x87 control-word
	addq      $0x08, %rsp              // free fc_mxcsr | fc_x87_cw
	pop       %r12                     // load r12                    <-- LLd miss
	pop       %r13                     // load r13
	pop       %r14                     // load r14
	pop       %r15                     // load r15
	pop       %rbx                     // load rbx
	pop       %rbp                     // load rbp                    <-- LLd miss
	retq                               // enter context

// mark the size of jump_fcontext
.size jump_fcontext, .-jump_fcontext

// Mark that we don't need executable stack.
.section .note.GNU-stack, "", %progbits
#endif
