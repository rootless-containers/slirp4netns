AC_PREREQ([2.69])
AC_INIT([slirp4netns], [0.1], [https://github.com/rootless-containers/slirp4netns/issues])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_RANLIB

AM_INIT_AUTOMAKE([1.9 foreign subdir-objects])

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/timeb.h unistd.h getopt.h])

AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit clock_gettime inet_ntoa memmove memset socket strchr strdup strerror strstr strtol getopt_long])

AC_ARG_WITH(system-qemu,
	[AC_HELP_STRING([--with-system-qemu], [define if using qemu for managing the slirp network (not all functionalities will be supported)])],
	[with_system_qemu=$withval], [with_system_qemu=])
if test -n "$with_system_qemu"; then
	AC_DEFINE_UNQUOTED([SYSTEM_QEMU_PATH], "$with_system_qemu", [Define the path to the qemu executable to use])
fi
AM_CONDITIONAL([BUILD_LIBSLIRP], [test -z "$with_system_qemu"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
