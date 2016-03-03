#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([nemea-supervisor], [1.3.0], [svepemar@fit.cvut.cz])
AC_CONFIG_SRCDIR([supervisor.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign silent-rules])
AM_SILENT_RULES([yes])
RELEASE=1
AC_SUBST(RELEASE)
USERNAME=`git config --get user.name`
USERMAIL=`git config --get user.email`
AC_SUBST(USERNAME)
AC_SUBST(USERMAIL)
# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild, [""])
AM_CONDITIONAL(MAKE_RPMS, test x$RPMBUILD != x)

pkgdatadir=${datadir}/nemea-supervisor
AC_SUBST(pkgdatadir)
rundir=${localstatedir}/run
defaultsocketdir=${rundir}/nemea-supervisor
AC_SUBST(defaultsocketdir)

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
AC_DEFINE_DIR(bindir, [bindir], [Path to binaries])
AC_DEFINE_DIR(defaultsocketdir, [defaultsocketdir], [Default path to socket directory])
AC_DEFINE_DIR(rundir, [rundir], [Default path to socket directory])

DEFAULT_PATH_TO_SOCKET=${defaultsocketdir}/nemea-supervisor.sock
AC_DEFINE_UNQUOTED([DEFAULT_PATH_TO_SOCKET], "${DEFAULT_PATH_TO_SOCKET}", [Default path to daemon socket])
AC_SUBST(DEFAULT_PATH_TO_SOCKET)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_CHECK_PROG(GIT, git, git, [""])

AC_ARG_ENABLE(repobuild, AS_HELP_STRING([--disable-repobuild],
                [disable local compilation without system installed Nemea libraries, default: yes]),
[case "${enableval}" in
        yes) repobuild=true ;;
        no)  repobuild=false ;;
        *)   AC_MSG_ERROR([bad value ${enableval} for --disable-repobuild]) ;;
esac], [repobuild=true])

if test -n "$GIT"; then
    GIT_VERSION=$($GIT log --format="%h" HEAD^..HEAD | head -1)
else
   GIT_VERSION=""
fi
AC_DEFINE_UNQUOTED(GIT_VERSION, ["$GIT_VERSION"], [Git revision])

# Checks for libraries.

AX_LIBTRAP_CHECK
AX_LIBXML2_CHECK

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h string.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 memset mkdir realpath select socket strchr strdup strstr getopt_long getopt])

# Define SUP_GETOPT()
if test "x$ac_cv_func_getopt_long" = xyes; then
  AC_DEFINE_UNQUOTED([SUP_GETOPT(argc, argv, optstr, longopts)],
    [getopt_long(argc, argv, optstr, longopts, NULL)],
    [Supervisor getopt macro. Argc and argv are number and values of arguments, optstr is a string containing legitimate option characters, longopts is the array of option structures (unused on system without getopt_long())])
elif test "x$ac_cv_func_getopt" = xyes; then
  AC_DEFINE_UNQUOTED([SUP_GETOPT(argc, argv, optstr, longopts)],
  [getopt(argc, argv, optstr)],
  [Supervisor getopt macro. Argc and argv are number and values of arguments, optstr is a string containing legitimate option characters, longopts is the array of option structures (unused for on system without getopt_long())])
else
  AC_MSG_ERROR([getopt() was not found, module depend on it...])
fi

# allow to skip systemd, enabled by default
AC_ARG_WITH([systemd],
	AC_HELP_STRING([--without-systemd], [Disable checking and usage of systemd]),
        [if test "$withval" = "no"; then
                withsystemd="no"
        else
                withsystemd="yes"
        fi], [withsystemd="yes"])

if test "x$withsystemd" = "xyes"; then
	PKG_CHECK_MODULES([systemd], [systemd], [
	SYSTEMD_UNIT_DIR=$(pkg-config --variable=systemdsystemunitdir systemd)
	SYSTEMD_SERVICE_FILE="$SYSTEMD_UNIT_DIR/nemea-supervisor.service"
	AC_SUBST(SYSTEMD_SERVICE_FILE)
	AC_SUBST(SYSTEMD_UNIT_DIR)
	AM_CONDITIONAL([HAVE_SYSTEMD], [true])
	], [AM_CONDITIONAL([HAVE_SYSTEMD], [false])])
else
	# disabled systemd
	AM_CONDITIONAL([HAVE_SYSTEMD], [false])
fi

AC_CONFIG_FILES([Makefile nemea-supervisor.spec nemea-supervisor nemea-supervisor.service munin/Makefile munin/nemea_supervisor configs/Makefile configs/prepare_default_config.sh])
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
