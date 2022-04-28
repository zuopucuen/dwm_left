# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

DWM_SRC = drw.c dwm.c util.c
DWM_OBJ = ${DWM_SRC:.c=.o}

barM_SRC = barM.c
barM_OBJ = ${barM_SRC:.c=.o}

all: options dwm barM

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwm: ${DWM_OBJ}
	${CC} -o $@ ${DWM_OBJ} ${LDFLAGS}

barM: ${barM_OBJ}
	$(CC) -g -o $@ ${barM_OBJ} -lX11 -lasound 

clean:
	rm -f dwm ${DWM_OBJ} barM ${barM_OBJ} dwm-${VERSION}.tar.gz

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README config.def.h config.mk\
		dwm.1 drw.h util.h ${SRC} dwm.png transient.c dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin
	cp -f barM ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm \
		${DESTDIR}${PREFIX}/bin/barM \
		${DESTDIR}${MANPREFIX}/man1/dwm.1
		

.PHONY: all options clean dist install uninstall
