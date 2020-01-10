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
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CXX
AC_PROG_CC
AC_CANONICAL_HOST
LT_INIT

# Check if this is the source dir
AC_CONFIG_SRCDIR([costage.h])

AS_IF([test "x${prefix}" != "xNONE"], [
    CPPFLAGS="-I${prefix}/include ${CPPFLAGS}"
    LDFALGS="-L${prefix}/lib ${LDFALGS}"
])

AS_IF([test "x${libssl}" != "x"], [
    CPPFLAGS="-I${libssl}/include ${CPPFLAGS}"
    LDFLAGS="-L${libssl}/lib ${LDFALGS}"
])

already_has_libpeco_path="no"
AS_IF([test "x${libpeco}" != "x"], [
    CPPFLAGS="-I${libpeco}/include/libpeco ${CPPFLAGS}"
    LDFLAGS="-L${libpeco}/lib ${LDFALGS}"
    already_has_libpeco_path="yes"
])

# Search all include path, if already has a sub folder named
# peco, means we have the right path
echo "${CPPFLAGS}" | tr ' ' '\n' > .cppflags
AS_IF([test "x${already_has_libpeco_path}" == "xno"], [
    while read line; do
        f=${line:0:2}
        AS_IF([test "${f}" != "-I"], [continue])
        p=${line:2}
        AC_CHECK_FILE(${p}/peco/peco.h, [
            already_has_libpeco_path="yes"
            break
        ])
        AC_CHECK_FILE(${p}/libpeco/peco/peco.h, [
            CPPFLAGS="-I${p}/libpeco ${CPPFLAGS}"
            already_has_libpeco_path="yes"
            break
        ])
    done < .cppflags
])
rm -rf .cppflags

echo | ${CC} -E -Wp,-v - 2>&1 | grep -E "^ \/.*" | grep -v '(.*)' > .search_path
AS_IF([test "x${already_has_libpeco_path}" == "xno"], [
    while read p; do
        AC_CHECK_FILE(${p}/peco/peco.h, [
            already_has_libpeco_path="yes"
            break
        ])
        AC_CHECK_FILE(${p}/libpeco/peco/peco.h, [
            CPPFLAGS="-I${p}/libpeco ${CPPFLAGS}"
            already_has_libpeco_path="yes"
            break
        ])
    done < .search_path
])
rm -rf .search_path

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
AC_CHECK_LIB([dl], [dlopen], [], [AC_MSG_FAILURE([could not find libld])])

AC_LANG_PUSH(C++)
AC_CHECK_LIB([peco], [PEUtils_Autoconf], [], [
    AC_MSG_FAILURE([could not find libpeco])
])
AC_LANG_POP(C++)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
