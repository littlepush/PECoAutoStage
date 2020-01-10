#! /bin/bash

[[ ! ${PREFIX} ]] && PREFIX=/usr/local

NOW_PATH=`pwd`
PLATFORM=`uname -m`
SSLCONFIG=""
TMP=`date +%s`

export LC_ALL="en_US.UTF-8"

function __OnExit() {
    cd ${NOW_PATH}
    rm -rf libpeco-tmp.${TMP}
    rm -rf coas.${TMP}
    rm -rf .search_path
}

trap __OnExit EXIT

if [ ! ${LIBSSL} ]; then
    SSLCONFIG="libssl=${LIBSSL}"
else
    # Search libssl folder in macOSX
    echo | ${_CC_} -E -Wp,-v - 2>&1 | grep -E "^ \/.*" | grep -v '(.*)' > .search_path

    FOUND_LIBSSL="no"
    while read p; do
        [[ -d ${p}/openssl ]] && {
            FOUND_LIBSSL="yes"
            break
        }
    done < .search_path

    [[ "x${FOUND_LIBSSL}" == "xno" ]] && {
        if [ "${PLATFORM}" == "Darwin" ]; then
            if [ -d /usr/local/opt/openssl ]; then
                SSLCONFIG="libssl=/usr/local/opt/openssl"
            else
                echo "Cannot find libssl in default path, please set LIBSSL in env"
                echo "or use 'brew install openssl'"
                exit 2
            fi
        else
            echo "Cannot find libssl in default path, please set LIBSSL in env"
            exit 2
        fi
    }
fi

touch ${PREFIX}/bin/__coas >/dev/null 2>&1
if [ $? -eq 0 ]; then
    MAKE_INSTALL="make install"
    rm ${PREFIX}/bin/__coas
else
    MAKE_INSTALL="sudo make install"
fi

which g++ >/dev/null 2>&1
has_gxx=$?
which clang++ >/dev/null 2>&1
has_clangxx=$?

[[ ${has_clangxx} -eq 0 ]] && {
    _CC_="clang++"
}
[[ ${has_gxx} -eq 0 ]] && {
    _CC_="g++"
}

which wget >/dev/null 2>&1
has_wget=$?
which curl >/dev/null 2>&1
has_curl=$?
[[ ${has_curl} -eq 0 ]] && {
    _FETCH="curl -L -o "
}
[[ ${has_wget} -eq 0 ]] && {
    _FETCH="wget -O "
}

# Check if we have installed libpeco
echo | ${_CC_} -E -Wp,-v - 2>&1 | grep -E "^ \/.*" | grep -v '(.*)' > .search_path
echo "${PREFIX}/include" >> .search_path
LIBPECO_has="no"

while read p; do
    [[ -f ${p}/peco/peco.h ]] && {
        LIBPECO_has="yes"
        break
    }
    [[ -f ${p}/libpeco/peco/peco.h ]] && {
        LIBPECO_has="yes"
        break
    }
done < .search_path

function compilePECo() {
    cd libpeco
    ./configure ${SSLCONFIG} --prefix=${PREFIX}
    [[ $? -ne 0 ]] && exit 3
    ${MAKE_INSTALL}
    cd ../
}

function installPECo_byFetch() {
    # Install libpeco
    ${_FETCH} ./libpeco.zip "https://github.com/littlepush/libpeco/archive/master.zip"
    [[ $? -ne 0 ]] && {
        echo "failed to download libpeco"
        exit 2
    }
    ${_FETCH} ./peutils.zip "https://github.com/littlepush/PEUtils/archive/master.zip"
    [[ $? -ne 0 ]] && {
        echo "failed to download PEUtils"
        exit 2
    }
    ${_FETCH} ./pecotask.zip "https://github.com/littlepush/PECoTask/archive/master.zip"
    [[ $? -ne 0 ]] && {
        echo "failed to download PECoTask"
        exit 2
    }
    ${_FETCH} ./peconet.zip "https://github.com/littlepush/PECoNet/archive/master.zip"
    [[ $? -ne 0 ]] && {
        echo "failed to download PECoNet"
        exit 2
    }
    ${_FETCH} ./pecorlog.zip "https://github.com/littlepush/PECoRLog/archive/master.zip"
    [[ $? -ne 0 ]] && {
        echo "failed to download PECoRLog"
        exit 2
    }
    unzip -u ./libpeco.zip && rm ./libpeco.zip
    unzip -u ./peutils.zip && rm ./peutils.zip
    unzip -u ./pecotask.zip && rm ./pecotask.zip
    unzip -u ./peconet.zip && rm ./peconet.zip
    unzip -u ./pecorlog.zip && rm ./pecorlog.zip

    mv -f libpeco-master libpeco
    rm -rf libpeco/PEUtils && mv -f PEUtils-master libpeco/PEUtils
    rm -rf libpeco/PECoTask && mv -f PECoTask-master libpeco/PECoTask
    rm -rf libpeco/PECoNet && mv -f PECoNet-master libpeco/PECoNet
    rm -rf libpeco/PECoRLog && mv -f PECoRLog-master libpeco/PECoRLog

    compilePECo
}
function installPECo_byGit() {
    GIT_SSH_COMMAND="ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
    git clone --recurse-submodules "https://github.com/littlepush/libpeco.git" ./libpeco
    compilePECo
}

[[ "x${LIBPECO_has}" == "xno" ]] && {
    mkdir libpeco-tmp.${TMP}
    cd libpeco-tmp.${TMP}

    which git >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        installPECo_byGit
    else
        installPECo_byFetch
    fi
    cd ../
}

mkdir coas.${TMP}
cd coas.${TMP}
which git >/dev/null 2>&1
if [ $? -eq 0 ]; then
    GIT_SSH_COMMAND=\"ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\"
    git clone "https://github.com/littlepush/PECoAutoStage.git"
else
    ${_FETCH} ./coas.zip "https://github.com/littlepush/PECoAutoStage/archive/master.zip"
    unzip -u ./coas.zip && rm ./coas.zip
    mv -f PECoAutoStage-master PECoAutoStage
fi
cd PECoAutoStage
./configure ${SSLCONFIG} --prefix=${PREFIX} 
${MAKE_INSTALL}

