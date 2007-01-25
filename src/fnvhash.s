/*
 * charybdis: a slightly useful ircd.
 * fnvhash.s: x86-optimised FNV hashing implementation
 *
 * Copyright (c) 2006 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: fnvhash.s 2725 2006-11-09 23:43:35Z jilles $
 */

/* Safely moves hashv from %edx to %eax and returns back to the calling parent. */
fnv_out:
	movzbl	12(%ebp), %ecx
	movl	-4(%ebp), %eax
	movl	%eax, %edx
	shrl	%cl, %edx
	movl	12(%ebp), %eax
	xorl	$2, %eax
	decl	%eax
	andl	-4(%ebp), %eax
	xorl	%edx, %eax
	movl	%eax, -4(%ebp)
	movl	-4(%ebp), %eax
	leave
	ret

/*
 * Capitalizes the contents of %eax and adds it to the hashv in %edx.
 * Returns hashv in register %eax.
 *     - nenolod
 */
.globl fnv_hash_upper
	.type	fnv_hash_upper, @function
fnv_hash_upper:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$4, %esp
	movl	$-2128831035, -4(%ebp)		/* u_int32_t h = FNV1_32_INIT */
.eat_data_upper:				/* while loop construct */
	movl	8(%ebp), %eax			/* move value of *s to %eax */
	cmpb	$0, (%eax)			/* is eax == 0? */
	jne	.hash_capitalized		/* if no, then capitalize and hash */
	jmp	fnv_out				/* if yes, then exit out of the loop */
.hash_capitalized:
	movl	8(%ebp), %eax
	movzbl	(%eax), %eax			/* increment s (%eax) */
	movzbl	ToUpperTab(%eax), %edx		/* hashv ^= ToUpperTab(%eax) */
	leal	-4(%ebp), %eax
	xorl	%edx, (%eax)			/* hashv = 0 */
	incl	8(%ebp)
	movl	-4(%ebp), %eax
	imull	$16777619, %eax, %eax		/* FNV1_32_PRIME */
	movl	%eax, -4(%ebp)			/* add this byte to hashv, and */
	jmp	.eat_data_upper			/*   go back for more...       */

/*
 * Hashes (no case change) the contents of %eax and adds it to the hashv in %edx.
 * Returns hashv in register %eax.
 *     - nenolod
 */
.globl fnv_hash
	.type	fnv_hash, @function
fnv_hash:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$4, %esp
	movl	$-2128831035, -4(%ebp)		/* u_int32_t h = FNV1_32_INIT */
.eat_data:					/* again, the while loop construct */
	movl	8(%ebp), %eax			/* move value of *s to eax */
	cmpb	$0, (%eax)			/* is eax == 0? */
	jne	.hash_lowercase			/* if not, jump to .hash_lowercase */ 
	jmp	fnv_out				/* otherwise, jump to fnv_out */
.hash_lowercase:
	movl	8(%ebp), %eax
	movzbl	(%eax), %edx
	leal	-4(%ebp), %eax
	xorl	%edx, (%eax)
	incl	8(%ebp)				/* h << 1 */
	movl	-4(%ebp), %eax
	imull	$16777619, %eax, %eax		/* FNV1_32_PRIME */
	movl	%eax, -4(%ebp)			/* add this byte to hashv, then */
	jmp	.eat_data			/*   check for more...          */

/*
 * Hashes (no case change) the contents of %eax and adds it to the hashv in %edx.
 * Returns hashv in register %eax.
 *
 * Bounds checking is performed.
 *     - nenolod
 */
.globl fnv_hash_len
	.type	fnv_hash_len, @function
fnv_hash_len:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	movl	$-2128831035, -4(%ebp)
	movl	16(%ebp), %eax
	addl	8(%ebp), %eax
	movl	%eax, -8(%ebp)
.eat_data_len:
	movl	8(%ebp), %eax
	cmpb	$0, (%eax)
	je	fnv_out
	movl	8(%ebp), %eax
	cmpl	-8(%ebp), %eax
	jb	.hash_lowercase_len
	jmp	fnv_out
.hash_lowercase_len:
	movl	8(%ebp), %eax
	movzbl	(%eax), %edx
	leal	-4(%ebp), %eax
	xorl	%edx, (%eax)
	incl	8(%ebp)
	movl	-4(%ebp), %eax
	imull	$16777619, %eax, %eax		/* FNV1_32_PRIME */
	movl	%eax, -4(%ebp)
	jmp	.eat_data_len

/*
 * Hashes (no case change) the contents of %eax and adds it to the hashv in %edx.
 * Returns hashv in register %eax.
 *
 * Bounds checking is performed.
 *     - nenolod
 */
.globl fnv_hash_upper_len
	.type	fnv_hash_upper_len, @function
fnv_hash_upper_len:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	movl	$-2128831035, -4(%ebp)
	movl	16(%ebp), %eax
	addl	8(%ebp), %eax
	movl	%eax, -8(%ebp)
.eat_upper_len:
	movl	8(%ebp), %eax
	cmpb	$0, (%eax)
	je	fnv_out
	movl	8(%ebp), %eax
	cmpl	-8(%ebp), %eax
	jb	.hash_uppercase_len
	jmp	fnv_out
.hash_uppercase_len:
	movl	8(%ebp), %eax
	movzbl	(%eax), %eax
	movzbl	ToUpperTab(%eax), %edx
	leal	-4(%ebp), %eax
	xorl	%edx, (%eax)
	incl	8(%ebp)
	movl	-4(%ebp), %eax
	imull	$16777619, %eax, %eax		/* FNV1_32_PRIME */
	movl	%eax, -4(%ebp)
	jmp	.eat_upper_len
