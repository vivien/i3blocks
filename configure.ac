AC_INIT([i3blocks], 1.5)
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE(foreign)
AC_PROG_CC
AC_CONFIG_HEADERS([i3blocks-config.h])
PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
  [BASH_COMPLETION_DIR="$(pkg-config --variable=completionsdir bash-completion)"],
  [BASH_COMPLETION_DIR="$datadir/bash-completion/completions"]
)
AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "xBASH_COMPLETION_DIR" != "x"])
AC_CONFIG_FILES([
  Makefile
])
AC_OUTPUT
