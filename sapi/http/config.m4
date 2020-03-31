PHP_ARG_ENABLE([http],,
  [AS_HELP_STRING([--enable-http],
    [Enable building of the http SAPI executable])],
  [no],
  [no])

dnl Configure checks.
AC_DEFUN([AC_HTTP_MICROHTTPD],
[
  AC_SEARCH_LIBS(microhttpd, microhttpd)
])

AC_MSG_CHECKING(for HTTP build)
if test "$PHP_HTTP" != "no"; then
  PHP_ADD_MAKEFILE_FRAGMENT($abs_srcdir/sapi/http/Makefile.frag)

  AC_MSG_RESULT($PHP_HTTP)

  LIBS="$LIBS -lmicrohttpd"

  SAPI_HTTP_PATH=sapi/http/php-http

  PHP_HTTP_CFLAGS="-I$abs_srcdir/sapi/http -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"

  PHP_HTTP_FILES="http.c \
  "

  PHP_SELECT_SAPI(http, program, $PHP_HTTP_FILES, $PHP_HTTP_CFLAGS, '$(SAPI_HTTP_PATH)')

  case $host_alias in
      *aix*)
        BUILD_HTTP="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_HTTP_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_FASTCGI_OBJS) \$(PHP_HTTP_OBJS) \$(EXTRA_LIBS) \$(HTTP_EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_HTTP_PATH)"
        ;;
      *darwin*)
        BUILD_HTTP="\$(CC) \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(NATIVE_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_FASTCGI_OBJS:.lo=.o) \$(PHP_HTTP_OBJS:.lo=.o) \$(PHP_FRAMEWORKS) \$(EXTRA_LIBS) \$(HTTP_EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_HTTP_PATH)"
      ;;
      *)
        BUILD_HTTP="\$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_FASTCGI_OBJS:.lo=.o) \$(PHP_HTTP_OBJS:.lo=.o) \$(EXTRA_LIBS) \$(HTTP_EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_HTTP_PATH)"
      ;;
  esac

  PHP_SUBST(SAPI_HTTP_PATH)
  PHP_SUBST(BUILD_HTTP)

else
  AC_MSG_RESULT(no)
fi
