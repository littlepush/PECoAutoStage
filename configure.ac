#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(
    [PECoAutoStage], 
    [0.1.1], 
    [littlepush@gmail.com], 
    [], 
    [https://github.com/littlepush/PECoAutoStage])
AC_CONFIG_AUX_DIR([build])
AM_MAINTAINER_MODE([disable])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
AC_PROG_CC
AC_CANONICAL_HOST

# Check if this is the source dir
AC_CONFIG_SRCDIR([costage.h])

# Checks for libraries.
case "${host_os}" in
    darwin*)
        AC_CHECK_FILE([/usr/local/opt/openssl/include/openssl/ssl.h], [
                CPPFLAGS="-I/usr/local/opt/openssl/include ${CPPFLAGS}"
                LDFLAGS="-L/usr/local/opt/openssl/lib ${LDFLAGS}"
            ], [
                AC_MSG_FAILURE([cannot find openssl/ssl.h])
            ])
        ;;
esac

AC_CHECK_LIB(crypto, CRYPTO_new_ex_data, [], [AC_MSG_ERROR([library 'crypto' is required for OpenSSL])])
FOUND_SSL_LIB="no"
AC_CHECK_LIB(ssl, OPENSSL_init_ssl, [
    FOUND_SSL_LIB="yes"
    AC_SUBST([SSL_VERSION_1_0], 0)
    AC_SUBST([SSL_VERSION_1_1], 1)
])
AC_CHECK_LIB(ssl, SSL_library_init, [
    FOUND_SSL_LIB="yes"
    AC_SUBST([SSL_VERSION_1_0], 1)
    AC_SUBST([SSL_VERSION_1_1], 0)
])
AS_IF(
    [test "x$FOUND_SSL_LIB" == "xno"], 
    [AC_MSG_FAILURE([library 'ssl' is required for OpenSSL])]
)
AC_CHECK_LIB([z], [gzread], [], [AC_MSG_FAILURE([could not find libz])])

AC_ARG_ENABLE(
    [install-peco],
    AS_HELP_STRING([--enable-install-peco], [install PECo Framework Packages from source]),
    [INSTALL_PECo=yes],
    [INSTALL_PECo=no]
)

function __install_PECo() {
    mkdir -p .peco-dep
    cd .peco-dep
    curl -o- -LS "https://github.com/littlepush/$1/archive/master.zip" | tar -xz - -C .
    cd $1-master
    chmod +x ./configure
    ./configure $2 && make && make install
    [[ $? -ne 0 ]] && AC_MSG_FAILURE([cannot install PECoTask from source])
    cd ../../
    rm -rf .peco-dep
}

function __checkInstall_PECo() {
    AS_IF(
        [test "x${INSTALL_PECo}" == "xyes"],
        [__install_PECo $1 $2],
        [AC_MSG_FAILURE([cannot find $1])]
    )
}

CPPFLAGS_old=${CPPFLAGS}
CPPFLAGS="-I/usr/local/include/peutils ${CPPFLAGS}"
LDFLAGS_old=${LDFLAGS}
LDFLAGS="-L/usr/local/lib ${LDFLAGS}"
AC_CHECK_LIB([peutils], [PEUtils_Autoconf], [], [
    AS_IF([test "x${prefix}" == "xNONE"], [
        __checkInstall_PECo PEUtils
    ], [
        CPPFLAGS="-I${prefix}/include/peutils ${CPPFLAGS_old}"
        LDFLAGS="-L${prefix}/lib ${LDFLAGS_old}"
        AC_CHECK_LIB([peutils], [PEUtils_AutoConf], [], [
            __checkInstall_PECo PEUtils --prefix=${prefix}
        ])
    ])
])

CPPFLAGS_old=${CPPFLAGS}
CPPFLAGS="-I/usr/local/include/pecotask ${CPPFLAGS}"
LDFLAGS_old=${LDFLAGS}
LDFLAGS="-L/usr/local/lib ${LDFLAGS}"
AC_CHECK_LIB([cotask], [PECoTask_Autoconf], [], [
    AS_IF([test "x${prefix}" == "xNONE"], [
        __checkInstall_PECo PECoTask
    ], [
        CPPFLAGS="-I${prefix}/include/pecotask ${CPPFLAGS_old}"
        LDFLAGS="-L${prefix}/lib ${LDFLAGS_old}"
        AC_CHECK_LIB([cotask], [PECoTask_Autoconf], [], [
            __checkInstall_PECo PECoTask --prefix=${prefix}
        ])
    ])
])

CPPFLAGS_old=${CPPFLAGS}
CPPFLAGS="-I/usr/local/include/peconet ${CPPFLAGS}"
LDFLAGS_old=${LDFLAGS}
LDFLAGS="-L/usr/local/lib ${LDFLAGS}"
AC_CHECK_LIB([conet], [PECoNet_Autoconf], [], [
    AS_IF([test "x${prefix}" == "xNONE"], [
        __checkInstall_PECo PECoNet
    ], [
        CPPFLAGS="-I${prefix}/include/peconet ${CPPFLAGS_old}"
        LDFLAGS="-L${prefix}/lib ${LDFLAGS_old}"
        AC_CHECK_LIB([conet], [PECoNet_Autoconf], [], [
            __checkInstall_PECo PECoNet --prefix=${prefix}
        ])
    ])
])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h locale.h mach/mach.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h wchar.h wctype.h])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
