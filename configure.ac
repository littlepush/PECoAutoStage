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

PECoInstallArgs=""

AC_ARG_ENABLE(
    [shared],
    AS_HELP_STRING([--disable-shared], [Disable shared library, link all static]),
    [
        PECoInstallArgs="--disable-shared"
        AC_SUBST([STATIC_FLAG], "-static -static-libstdc++ -static-libgcc")
    ],
    [
        AC_SUBST([STATIC_FLAG], "")
    ]
)

AC_ARG_ENABLE(
    [install-peco],
    AS_HELP_STRING([--enable-install-peco], [install PECo missing lib from source]),
    [INSTALL_PECo=yes],
    [INSTALL_PECo=no]
)

# Check if this is the source dir
AC_CONFIG_SRCDIR([costage.h])

AS_IF([test "x${prefix}" != "xNONE"], [
    CPPFLAGS="-I${prefix}/include ${CPPFLAGS}"
    LDFALGS="-L${prefix}/lib ${LDFALGS}"
    PECoInstallArgs="${PECoInstallArgs} --prefix=${prefix}"
])

AS_IF([test "x${libssl}" != "x"], [
    CPPFLAGS="-I${libssl}/include ${CPPFLAGS}"
    LDFLAGS="-L${libssl}/lib ${LDFALGS}"
    PECoInstallArgs="${PECoInstallArgs} libssl=${libssl}"
])

AS_IF([test "x${libpeco}" != "x"], [
    CPPFLAGS="-I${libpeco}/include/peutils \
    -I${libpeco}/include/pecotask \
    -I${libpeco}/include/peconet \
    ${CPPFLAGS}"
    LDFLAGS="-L${libpeco}/lib ${LDFLAGS}"
    PECoInstallArgs="${PECoInstallArgs} libpeco=${libpeco}"
])

AC_CHECK_LIB(crypto, CRYPTO_new_ex_data, [], [AC_MSG_ERROR([library 'crypto' is required for OpenSSL])])

FOUND_SSL_LIB_1_0="yes"
FOUND_SSL_LIB_1_1="yes"
AC_CHECK_LIB(ssl, SSL_library_init, [], [
    FOUND_SSL_LIB_1_0="no"
])
AC_CHECK_LIB(ssl, OPENSSL_init_ssl, [], [
    FOUND_SSL_LIB_1_1="no"
])
AS_IF([test "x${FOUND_SSL_LIB_1_1}" == "xyes"], [
    AC_SUBST([SSL_VERSION_1_0], 0)
    AC_SUBST([SSL_VERSION_1_1], 1)
], [
    AS_IF([test "x${FOUND_SSL_LIB_1_0}" == "xyes"], [
        AC_SUBST([SSL_VERSION_1_0], 1)
        AC_SUBST([SSL_VERSION_1_1], 0)
    ], [
        AC_MSG_FAILURE([library 'ssl' is required for OpenSSL])        
    ])
])
AC_CHECK_LIB([z], [gzread], [], [AC_MSG_FAILURE([could not find libz])])

function __install_PECo() {
    mkdir -p .peco-dep
    cd .peco-dep
    curl -o $1.zip -LS "https://github.com/littlepush/$1/archive/master.zip"
    unzip -u $1.zip
    cd $1-master
    chmod +x ./configure
    ./configure ${PECoInstallArgs} && make && make install
    RET=$?
    cd ../../
    rm -rf .peco-dep
    [[ $RET -ne 0 ]] && AC_MSG_FAILURE([cannot install $1 from source])
}

function __checkInstall_PECo() {
    AS_IF(
        [test "x${INSTALL_PECo}" == "xyes"],
        [__install_PECo $1],
        [AC_MSG_FAILURE([cannot find $1])]
    )
}

AC_CHECK_LIB([peutils], [PEUtils_Autoconf], [], [
    __checkInstall_PECo PEUtils
])

AC_CHECK_LIB([cotask], [PECoTask_Autoconf], [], [
    __checkInstall_PECo PECoTask
])

AC_CHECK_LIB([conet], [PECoNet_Autoconf], [], [
    __checkInstall_PECo PECoNet
])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h locale.h mach/mach.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h wchar.h wctype.h])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
