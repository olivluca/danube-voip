AC_INIT(svd, 0.1, luca@ventoso.org)
AC_CONFIG_SRCDIR([src/svd.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.

AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(SOFIA_SIP_UA, sofia-sip-ua >= 1.11.6)
AC_SUBST(SOFIA_SIP_UA_LIBS)
AC_SUBST(SOFIA_SIP_UA_CFLAGS)
AC_SUBST(SOFIA_SIP_UA_VERSION)

# Create files

AC_CONFIG_FILES([
Makefile
doc/Makefile
src/Makefile
])

AC_OUTPUT
