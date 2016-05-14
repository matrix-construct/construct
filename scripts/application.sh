#!/bin/sh
# application.sh: Special package building rules for charybdis.
#
# Copyright (c) 2007-2008 atheme.org
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

if [ "x$TIP" = "x" ]; then
	echo "Please don't run me directly."
	exit
fi

# Charybdis wants the git head to be in include/serno.h, in its own format.
SERNO=`git log -1 --date=format:%Y%m%d --pretty=format:%cd-%h`
DATECODE=`git log -1 --pretty=format:%ct`

echo "[charybdis] Generating include/serno.h for tip $MYTIP."
cat << _EOF_ > include/serno.h
/* Generated automatically by makepackage. Any changes made here will be lost. */
#define SERNO "$SERNO"
#define DATECODE ${DATECODE}UL
_EOF_
