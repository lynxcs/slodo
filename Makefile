PREFIX?=/usr
BINDIR=${PREFIX}/bin

slodo: slodo.c text.h
	clang slodo.c -lxcb -lxcb-keysyms -lX11 -o slodo -O3

install: slodo
	install -D -m 755 slodo ${DESTDIR}${BINDIR}/slodo

uninstall:
	rm -f ${DESTDIR}${BINDIR}/slodo
