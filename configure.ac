#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([nemea-supervisor], [1.1.0], [svepemar@fit.cvut.cz])
AC_CONFIG_SRCDIR([supervisor.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall silent-rules])
RELEASE=1
AC_SUBST(RELEASE)
USERNAME=`git config --get user.name`
USERMAIL=`git config --get user.email`
AC_SUBST(USERNAME)
AC_SUBST(USERMAIL)
# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild, [""])
AM_CONDITIONAL(MAKE_RPMS, test x$RPMBUILD != x)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CHECK_PROG(HAVE_DOT, dot, yes)
AC_CHECK_PROG(HAVE_DISPLAY, display, yes)

if test "x$HAVE_DOT" = xyes; then
AC_DEFINE([HAVE_DOT], 1, [Use dot(1) for graph creation])
else
AC_DEFINE([HAVE_DOT], 0, [Use dot(1) for graph creation])
AC_MSG_WARN([We need graphviz...])
fi

if test "x$HAVE_DISPLAY" = xyes; then
AC_DEFINE([HAVE_DISPLAY], 1, [Use display(1) to show image.])
else
AC_DEFINE([HAVE_DISPLAY], 0, [Use display(1) to show image.])
AC_MSG_WARN([We need ImageMagick...])
fi

m4_include([m4/ax_libxml2_check.m4])
AX_LIBXML2_CHECK()

# Checks for libraries.

AC_ARG_ENABLE(jansson, AS_HELP_STRING([--enable-jansson],
		[enable dependency on jansson library, default: yes]),
[case "${enableval}" in
	yes) jansson=true ;;
	no)  jansson=false ;;
	*)   AC_MSG_ERROR([bad value ${enableval} for --enable-jannson]) ;;
esac], [jansson=true])

if test "${jansson}" = "true"; then
	PKG_CHECK_MODULES([JANSSON], [jansson], [JANSSONLIB="yes"], [AC_MSG_ERROR(Jansson was not found by pkg-config)])
	LDFLAGS="$JANSSON_LDFLAGS $LDFLAGS"
	LIBS="$JANSSON_LIBS $LIBS"
	CFLAGS="$JANSSON_CFLAGS $CFLAGS"
	CXXFLAGS="$JANSSON_CFLAGS $CXXFLAGS"
fi

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([dup2 memset mkdir])

AC_CONFIG_FILES([Makefile nemea-supervisor.spec nemea-supervisor munin/Makefile])
AC_OUTPUT
echo
echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE $VERSION"
echo "------------------------------------------------------------------------"
echo
echo
echo "Configuration Options Summary:"
echo
echo "  ASM.(32 bit only)..: $ASM"
echo "  Static binary......: $static"
echo
echo "Documentation..........: ${build_doc}"
echo
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  CXXFLAGS.............: $CXXFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo "  LIBS.................: $LIBS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
echo
