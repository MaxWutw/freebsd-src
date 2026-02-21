
.PATH: ${SRCTOP}/sys/crypto/ccp

KMOD=	asp

SRCS=	asp.c sev.c	
SRCS+=	asp.h
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	pci_if.h

EXPORT_SYMS= YES

CFLAGS+=	-fms-extensions
CFLAGS.clang+=	-Wno-microsoft-anon-tag

.include <bsd.kmod.mk>
