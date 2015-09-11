#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([nemea-supervisor], [1.1.0], [svepemar@fit.cvut.cz])
AC_CONFIG_SRCDIR([supervisor.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall foreign silent-rules])
RELEASE=1
AC_SUBST(RELEASE)
USERNAME=`git config --get user.name`
USERMAIL=`git config --get user.email`
AC_SUBST(USERNAME)
AC_SUBST(USERMAIL)
# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild, [""])
AM_CONDITIONAL(MAKE_RPMS, test x$RPMBUILD != x)
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([debug],
        AC_HELP_STRING([--enable-debug],
        [Enable build with debug symbols and without optimizations.]),
        [if test "$enableval" = "yes"; then
                CFLAGS="-Wall -g -O0 $CFLAGS"
                CXXFLAGS="-Wall -g -O0 $CXXFLAGS"
        fi], [CFLAGS="-Wall -O3 $CFLAGS"
              CXXFLAGS="-Wall -O3 $CXXFLAGS"])
AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AC_DEFINE_DIR(initdir, [sysconfdir]/../init.d, [Path to init.d (init script)])

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

AC_ARG_ENABLE(repobuild, AS_HELP_STRING([--disable-repobuild],
                [disable local compilation without system installed Nemea libraries, default: yes]),
[case "${enableval}" in
        yes) repobuild=true ;;
        no)  repobuild=false ;;
        *)   AC_MSG_ERROR([bad value ${enableval} for --disable-repobuild]) ;;
esac], [repobuild=true])

# Checks for libraries.

AX_LIBTRAP_CHECK
AX_LIBXML2_CHECK

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([dup2 memset mkdir])

PKG_CHECK_MODULES([systemd], [systemd], [
SYSTEMD_UNIT_DIR=$(pkg-config --variable=systemdsystemunitdir systemd)
AC_SUBST(SYSTEMD_UNIT_DIR)
AM_CONDITIONAL([HAVE_SYSTEMD], [true])
], [AM_CONDITIONAL([HAVE_SYSTEMD], [false])])

AC_CONFIG_FILES([Makefile nemea-supervisor.spec nemea-supervisor nemea-supervisor.service munin/Makefile])
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
