#! /bin/bash

PREFIX=${PREFIX}
[[ ! ${PREFIX} ]] && PREFIX=/usr

touch ${PREFIX}/bin/__coas >/dev/null 2>&1
if [ $? -eq 0 ]; then
    MAKE_INSTALL="make install"
    rm ${PREFIX}/bin/__coas
else
    MAKE_INSTALL="sudo make install"
fi

function __install_peco() {
    curl -o ./$1.zip -L "https://github.com/littlepush/$1/archive/master.zip"
    unzip -u ./$1.zip && rm ./$1.zip
    cd ./$1-master
    chmod +x configure
    ./configure --prefix=${PREFIX} && make && ${MAKE_INSTALL}
    RET=$?
    cd -
    rm -rf ./$1-master
    [[ $? -ne 0 ]] && exit $RET
}

# Check if already has peutils
[[ ! -f ${PREFIX}/include/peutils/peutils.h ]] && {
    __install_peco PEUtils
}

[[ ! -f ${PREFIX}/include/pecotask/cotask.h ]] && {
    __install_peco PECoTask
}

[[ ! -f ${PREFIX}/include/peconet/conet.h ]] && {
    __install_peco PECoNet
}

__install_peco PECoAutoStage
