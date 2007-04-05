dnl aclocal.m4
dnl   macros autoconf uses when building configure from configure.in
dnl
dnl

dnl  EGG_CHECK_CC()
dnl
AC_DEFUN([EGG_CHECK_CC], 
[
if test "${cross_compiling-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system does not appear to have a working C compiler.
  A working C compiler is required to compile Eggdrop.

EOF
  exit 1
fi

if test -n "$GXX"; then
  CXXFLAGS="$CXXFLAGS -O3"
fi

])

dnl  EGG_IPV6_OPTIONS()
dnl
AC_DEFUN([EGG_IPV6_OPTIONS], 
[
AC_MSG_CHECKING(whether or not you disabled IPv6 support)
AC_ARG_ENABLE(ipv6, [  --disable-ipv6           disable IPv6 support],
[ ac_cv_dipv6="yes"
  AC_MSG_RESULT(yes)
],
[ ac_cv_dipv6="no"
  if test "$egg_cv_ipv6_supported" = "no"; then
    ac_cv_dipv6="no"
  fi
  AC_MSG_RESULT($ac_cv_dipv6)
])

if ! test "$EGG_CYGWIN" = "yes"; then
 if test "$ac_cv_dipv6" = "no"; then
   AC_DEFINE(USE_IPV6, 1, [Define if you want ipv6 support])
 fi
fi
])


dnl  EGG_CHECK_SOCKLEN_T()
dnl
AC_DEFUN([EGG_CHECK_SOCKLEN_T], 
[
AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(egg_cv_socklen_t,[
  AC_TRY_RUN([
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
  socklen_t test = sizeof(int);

  return 0;
}
  ],
egg_cv_socklen_t=yes, egg_cv_socklen_t=no, egg_cv_socklen_t=no)])
if test "$egg_cv_socklen_t" = "yes"; then
  AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define if you have support for socklen_t])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
])


dnl EGG_CHECK_CCPIPE()
dnl
dnl This function checks whether or not the compiler supports the `-pipe' flag,
dnl which speeds up the compilation.
dnl
AC_DEFUN([EGG_CHECK_CCPIPE],
[
  if test -n "$GXX" && test -z "$no_pipe"; then
    AC_CACHE_CHECK([whether the compiler understands -pipe], egg_cv_var_ccpipe, [
        ac_old_CXX="$CXX"
        CXX="$CXX -pipe"
        AC_COMPILE_IFELSE([[
          int main ()
          {
            return(0);
          }
        ]], [
          egg_cv_var_ccpipe="yes"
        ], [
          egg_cv_var_ccpipe="no"
        ])
        CXX="$ac_old_CXX"
    ])

    if test "$egg_cv_var_ccpipe" = "yes"; then
      CXX="$CXX -pipe"
    fi
  fi
])

dnl EGG_CHECK_CCWALL()
dnl
dnl See if the compiler supports -Wall.
dnl
AC_DEFUN([EGG_CHECK_CCWALL],
[
  if test -n "$GXX" && test -z "$no_wall"; then
    AC_CACHE_CHECK([whether the compiler understands -Wall], egg_cv_var_ccwall, [
      ac_old_CXXFLAGS="$CXXFLAGS"
      CXXFLAGS="$CXXFLAGS -Wall"
       AC_COMPILE_IFELSE([[
         int main ()
         {
           return(0);
         }
       ]], [
         egg_cv_var_ccwall="yes"
       ], [
         egg_cv_var_ccwall="no"
       ])
      CXXFLAGS="$ac_old_CXXFLAGS"
    ])

    if test "$egg_cv_var_ccwall" = "yes"; then
      CXXFLAGS="$CXXFLAGS -Wall"
    fi
  fi
])

dnl  EGG_CHECK_CCSTATIC()
dnl
dnl  Checks whether the compiler supports the `-static' flag.
AC_DEFUN([EGG_CHECK_CCSTATIC],
[
if test -z "$no_static"
then
  if test -n "$GXX"
  then
    AC_CACHE_CHECK(whether the compiler understands -static, egg_cv_var_ccstatic, [dnl
      ac_old_CXX="$CXX"
      CXX="$CXX -static"
      AC_TRY_COMPILE(,, egg_cv_var_ccstatic="yes", egg_cv_var_ccstatic="no")
      CXX="$ac_old_CXX"
      
    ])
    if test "$egg_cv_var_ccstatic" = "yes"
    then
      CCDEBUG="$CXX"
#      CXX="$CXX -static"
    else
      cat << 'EOF' >&2
configure: error:

  Your C compiler does not support -static.
  This compile flag is required for the botpack.

EOF
  exit 1
    fi
  fi
fi
])

dnl EGG_PROG_HEAD_1()
dnl
AC_DEFUN([EGG_PROG_HEAD_1],
[
cat << 'EOF' > conftest.head
a
b
c
EOF

for ac_prog in 'head -1' 'head -n 1' 'sed 1q';
do
  AC_MSG_CHECKING([whether $ac_prog works])
  AC_CACHE_VAL(ac_cv_prog_HEAD_1,
[  if test -n "$HEAD_1"
  then
    ac_cv_prog_HEAD_1="$HEAD_1" # Let the user override the test.
  else
   if test "`cat conftest.head | $ac_prog`" = "a";
   then
     AC_MSG_RESULT([yes])
     ac_cv_prog_HEAD_1=$ac_prog
   else
     AC_MSG_RESULT([no])
   fi
  fi])dnl
  test -n "$ac_cv_prog_HEAD_1" && break
done

if test "${ac_cv_prog_HEAD_1-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'head -1' or 'head -n 1' command.
  A working 'head -1' (or equivalent) command is required to compile Eggdrop.

EOF
  exit 1
fi

rm -f conftest.head
HEAD_1=$ac_cv_prog_HEAD_1
AC_SUBST(HEAD_1)dnl
])

dnl  EGG_PROG_AWK()
dnl
AC_DEFUN([EGG_PROG_AWK], 
[
# awk is needed for Tcl library and header checks, and eggdrop version subst
AC_PROG_AWK
if test "${AWK-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'awk' command.
  A working 'awk' command is required to compile Eggdrop.

EOF
  exit 1
fi
])


dnl  EGG_PROG_BASENAME()
dnl
AC_DEFUN([EGG_PROG_BASENAME],
[
# basename is needed for Tcl library and header checks
AC_CHECK_PROG(BASENAME, basename, basename)
if test "${BASENAME-x}" = "x"
then
  cat << 'EOF' >&2
configure: error:

  This system seems to lack a working 'basename' command.
  A working 'basename' command is required to compile Eggdrop.

EOF
  exit 1
fi
])


dnl  EGG_CHECK_OS()
dnl
dnl
AC_DEFUN([EGG_CHECK_OS],
[
EGG_CYGWIN=no
AC_CACHE_CHECK(system type, egg_cv_var_system_type, egg_cv_var_system_type=`$UNAME -s`)
AC_CACHE_CHECK(system release, egg_cv_var_system_release, egg_cv_var_system_release=`$UNAME -r`)
AC_CACHE_CHECK(system machine, egg_cv_var_system_machine, egg_cv_var_system_machine=`$UNAME -m`)

BUILDOS="$egg_cv_var_system_type"
BUILDARCH="$egg_cv_var_system_machine"

case "$egg_cv_var_system_machine" in
  i*)
    CXX="$CXX -march=i486"
    BUILDARCH="i486"
  ;;
  *)
  ;;
esac

case "$egg_cv_var_system_type" in
  BSD/OS)
  ;;
  CYGWI*)
    AC_PROG_CC_WIN32
    CXX="$CXX $WIN32FLAGS"
    EGG_CYGWIN="yes"
    EGG_CYGWIN_BINMODE
    AC_DEFINE(CYGWIN_HACKS, 1, [Define if running under cygwin])
    AC_DEFINE(WIN32_LEAN_AND_MEAN, 1, [Define if windows])
  ;;
  IRIX)
  ;;
  HP-UX)
    AC_DEFINE(MD32_XARRAY, 1, [Define under HPUX])
  ;;
  Ultrix)
    SHELL=/bin/sh5
  ;;
  SINIX*)
  ;;
  BeOS)
  ;;
  Linux)
  ;;
  Lynx)
  ;;
  QNX)
  ;;
  OSF1)
    case "`echo $egg_cv_var_system_release | cut -d . -f 1`" in
      V*)
        if test "$AWK" = "gawk"
        then
          AWK=awk
        fi
      ;;
      1.0|1.1|1.2)
        AC_DEFINE(OSF1_HACKS, 1, [Define if running on OSF/1 platform])dnl
      ;;
      1.*)
        AC_DEFINE(OSF1_HACKS, 1, [Define if running on OSF/1 platform])dnl
      ;;
      *)
      ;;
    esac
    AC_DEFINE(STOP_UAC, 1, [Define if running on OSF/1 platform])dnl
    AC_DEFINE(BROKEN_SNPRINTF, 1, [Define to use Eggdrop's snprintf functions without regard to HAVE_SNPRINTF])dnl
  ;;
  SunOS)
    SUNOS="yes"
  ;;
  *BSD)
    # FreeBSD/OpenBSD/NetBSD
  ;;
  *)
    AC_MSG_CHECKING(if system is Mach based)
    if test -r /mach
    then
      AC_MSG_RESULT(yes)
      AC_DEFINE(BORGCUBES, 1, [Define if running on NeXT Step])dnl
    else
      AC_MSG_RESULT(no)
      AC_MSG_CHECKING(if system is QNX)
      if test -r /cmds
      then
        AC_MSG_RESULT(yes)
      else
        AC_MSG_RESULT(no)
        AC_MSG_RESULT(Something unknown!)
        AC_MSG_RESULT([If you get dynamic modules to work, be sure to let the devel team know HOW :)])
      fi
    fi
  ;;
esac
AC_SUBST(BUILDOS)dnl
AC_SUBST(BUILDARCH)dnl
])

dnl EGG_CYGWIN_BINMODE
dnl
dnl Check for binmode.o on Cygwin.
dnl
AC_DEFUN([EGG_CYGWIN_BINMODE],
[
  if test $EGG_CYGWIN = "yes"; then
    AC_MSG_CHECKING([for /usr/lib/binmode.o])
    if test -r /usr/lib/binmode.o; then
      AC_MSG_RESULT([yes])
      LIBS="$LIBS /usr/lib/binmode.o"
    else
      AC_MSG_RESULT([no])
      AC_MSG_WARN([Make sure the directory Eggdrop is installed into is mounted in binary mode.])
    fi
  fi
])

dnl  EGG_CHECK_LIBS()
dnl
AC_DEFUN([EGG_CHECK_LIBS], 
[
  AC_CHECK_LIB(socket, socket)
#  AC_CHECK_LIB(nsl, connect)
#  AC_CHECK_LIB(dl, dlopen)
#  AC_CHECK_LIB(nsl, gethostbyname)
#  AC_CHECK_LIB(dns, gethostbyname)

#  AC_CHECK_LIB(z, gzopen, ZLIB="-lz")
#  AC_CHECK_LIB(ssl, SSL_accept, SSL="-lssl -lcrypto", SSL="", -lcrypto) 
#  AC_CHECK_LIB(ssl, SSL_accept, SSL="-lcrypto", SSL="", -lcrypto) 
  # This is needed for Tcl libraries compiled with thread support
#  AC_CHECK_LIB(pthread, pthread_mutex_init, [dnl
#  ac_cv_lib_pthread_pthread_mutex_init=yes
#  ac_cv_lib_pthread="-lpthread"], [dnl
#    AC_CHECK_LIB(pthread, __pthread_mutex_init, [dnl
#    ac_cv_lib_pthread_pthread_mutex_init=yes
#    ac_cv_lib_pthread="-lpthread"], [dnl
#      AC_CHECK_LIB(pthreads, pthread_mutex_init, [dnl
#      ac_cv_lib_pthread_pthread_mutex_init=yes
#      ac_cv_lib_pthread="-lpthreads"], [dnl
#        AC_CHECK_FUNC(pthread_mutex_init, [dnl
#        ac_cv_lib_pthread_pthread_mutex_init=yes
#        ac_cv_lib_pthread=""],
#        ac_cv_lib_pthread_pthread_mutex_init=no)])])])
  if test "$SUNOS" = "yes"; then
    # For suns without yp
    AC_CHECK_LIB(dl, main)
    AC_CHECK_LIB(socket, main)
    AC_CHECK_LIB(nsl, main)
  fi

])

dnl  EGG_CHECK_FUNC_VSPRINTF()
dnl
AC_DEFUN([EGG_CHECK_FUNC_VSPRINTF], 
[
AC_CHECK_FUNCS(vsprintf)
if test "$ac_cv_func_vsprintf" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system does not have the sprintf/vsprintf libraries.
  These are required to compile almost anything.  Sorry.

EOF
  exit 1
fi
])

dnl  EGG_CHECK_FUNC_UNAME()
dnl
AC_DEFUN([EGG_CHECK_FUNC_UNAME], 
[
AC_CHECK_FUNCS(uname)
if test "$ac_cv_func_uname" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system does not have the uname() function.
  This is required for the botpack.

EOF
  exit 1
fi
])

dnl  EGG_CHECK_ZLIB()
dnl
AC_DEFUN([EGG_CHECK_ZLIB], 
[
if test "x${ZLIB}" = x; then
  cat >&2 <<EOF
configure: error:

  Your system does not provide a working zlib compression library. 
  It is required.

EOF
  exit 1
else
  if test "${ac_cv_header_zlib_h}" != yes; then
    cat >&2 <<EOF
configure: error:

  Your system does not provide the necessary zlib header file. 
  It is required.

EOF
    exit 1
  fi
fi
])


dnl  EGG_CHECK_SSL()
dnl
AC_DEFUN([EGG_CHECK_SSL], 
[
if test "x${SSL}" = x; then
  cat >&2 <<EOF
configure: error:

  Your system does not provide a working ssl library. 
  It is required. Download openssl at www.openssl.org

EOF
  exit 1
else
  if test "${ac_cv_header_openssl_ssl_h}" != yes; then
    cat >&2 <<EOF
configure: error:

  Your system does not provide the necessary ssl header file. 
  It is required. Download openssl at www.openssl.org

EOF
    exit 1
  fi
fi
])

dnl  EGG_HEADER_STDC()
dnl
AC_DEFUN([EGG_HEADER_STDC], 
[
if test "$ac_cv_header_stdc" = "no"
then
  cat << 'EOF' >&2
configure: error:

  Your system must support ANSI C Header files.
  These are required for the language support.  Sorry.

EOF
  exit 1
fi
])


dnl  EGG_CACHE_UNSET(CACHE-ID)
dnl
dnl  Unsets a certain cache item. Typically called before using
dnl  the AC_CACHE_*() macros.
AC_DEFUN([EGG_CACHE_UNSET], 
[
  unset $1
])


dnl  EGG_SUBST_VERSION()
dnl
AC_DEFUN([EGG_SUBST_VERSION], 
[
VERSION=`grep "char" $srcdir/src/main.c | $AWK '/egg_version/ {print [$]5}' | sed -e 's/\"//g' | sed -e 's/\;//g'`
version_num=`echo $VERSION | $AWK 'BEGIN {FS = "."} {printf("%d%02d%02d", [$]1, [$]2, [$]3)}'`
AC_DEFINE_UNQUOTED(EGG_VERSION, $version_num, [Defines the current pack version])dnl
AC_SUBST(VERSION)dnl
AC_SUBST(NUMVER)dnl
])


dnl  EGG_SUBST_MOD_UPDIR()
dnl
dnl  Since module's Makefiles aren't generated by configure, some
dnl  paths in src/mod/Makefile.in take care of them. For correct
dnl  path "calculation", we need to keep absolute paths in mind
dnl  (which don't need a "../" pre-pended).
AC_DEFUN([EGG_SUBST_MOD_UPDIR],
[
case "$srcdir" in
  [[\\/]]* | ?:[[\\/]]*)
    MOD_UPDIR=""
  ;;
  *)
    MOD_UPDIR="../"
  ;;
esac
AC_SUBST(MOD_UPDIR)dnl
])dnl


dnl EGG_REPLACE_IF_CHANGED(FILE-NAME, CONTENTS-CMDS, INIT-CMDS)
dnl
dnl Replace FILE-NAME if the newly created contents differs from the existing
dnl file contents. Otherwise, leave the file alone. This avoids needless
dnl recompiles.
m4_define(EGG_REPLACE_IF_CHANGED,
[
  AC_CONFIG_COMMANDS([replace-if-changed],
  [[
    egg_replace_file="$1"
    $2
    if test -f "$egg_replace_file" && cmp -s conftest.out $egg_replace_file; then
      echo "$1 is unchanged"
    else
      echo "creating $1"
      mv conftest.out $egg_replace_file
    fi
    rm -f conftest.out
  ]],
  [[$3]])
])

dnl  EGG_SAVE_PARAMETERS()
dnl
AC_DEFUN([EGG_SAVE_PARAMETERS],
[
  # Remove --cache-file and --srcdir arguments so they do not pile up.
  egg_ac_parameters=
  ac_prev=
  for ac_arg in $ac_configure_args; do
    if test -n "$ac_prev"; then
      ac_prev=
      continue
    fi
    case $ac_arg in
    -cache-file | --cache-file | --cache-fil | --cache-fi \
    | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
      ac_prev=cache_file ;;
    -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
    | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* \
    | --c=*)
      ;;
    --config-cache | -C)
      ;;
    -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
      ac_prev=srcdir ;;
    -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
      ;;
    *) egg_ac_parameters="$egg_ac_parameters $ac_arg" ;;
    esac
  done

  AC_SUBST(egg_ac_parameters)dnl
])


AC_DEFUN([AC_PROG_CC_WIN32], 
[
AC_MSG_CHECKING([how to access the Win32 API])
WIN32FLAGS=
AC_TRY_COMPILE(,[
#ifndef WIN32
# ifndef _WIN32
#  error WIN32 or _WIN32 not defined
# endif
#endif], [
dnl found windows.h with the current config.
AC_MSG_RESULT([present by default])
], [
dnl try -mwin32
ac_compile_save="$ac_compile"
dnl we change CC so config.log looks correct
save_CXX="$CXX"
ac_compile="$ac_compile -mwin32"
CXX="$CXX -mwin32"
AC_TRY_COMPILE(,[
#ifndef WIN32
# ifndef _WIN32
#  error WIN32 or _WIN32 not defined
# endif
#endif], [
dnl found windows.h using -mwin32
AC_MSG_RESULT([found via -mwin32])
ac_compile="$ac_compile_save"
CXX="$save_CXX"
WIN32FLAGS="-mwin32"
], [
ac_compile="$ac_compile_save"
CXX="$save_CXX"
AC_MSG_RESULT([not found])
])
])

])


AC_DEFUN([EGG_CHECK_RANDOM_MAX],
[
  AC_MSG_CHECKING([for random limit])

  case "$egg_cv_var_system_type" in
    SunOS)         RMAX=0x7FFFFFFF
       ;;
       *)                      RMAX=RAND_MAX
       ;;
  esac

  AC_MSG_RESULT([$RMAX])

  AC_DEFINE_UNQUOTED(RANDOM_MAX, $RMAX, [Define limit of random() function.])
])
