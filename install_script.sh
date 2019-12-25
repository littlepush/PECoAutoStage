#! /bin/bash

curl -o- -LS "https://github.com/littlepush/PECoAutoStage/archive/master.zip" >/dev/null | tar -xz - -C .
cd PECoAutoStage-master
chmod +x configure
./configure --enable-install-peco $1 && make && make install