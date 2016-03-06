CFLAGS=		/INCLUDE=([-.INCLUDE])/ERROR_LIMIT=5/DEFINE=(_XOPEN_SOURCE_EXTENDED)/NOANSI_ALIAS
OBJECTS=	balloc.obj,	commio.obj,	crypt.obj,	event.obj, -
		helper.obj,	linebuf.obj,	nossl.obj,	patricia.obj, -
		poll.obj,	rb_libb.obj,	rawbuf.obj,	rb_memory.obj, -
		snprintf.obj,	tools.obj,	unix.obj

DEFAULT : RATBOX.OLB($(OBJECTS))

CLEAN :
	- DELETE *.OBJ;*, *.OLB;*
