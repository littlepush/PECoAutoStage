#! /bin/bash

# Install PEUtils
curl -SL "https://github.com/littlepush/PEUtils/archive/master.zip" | tar -xz - -C .
[ $? -ne 0 ] && return $?
cd PEUtils-master
make && make install && make -f Makefile.debug && make -f Makefile.debug install
[ $? -ne 0 ] && return $?
cd -
rm -rf PEUtils-master
# Install PECoTask
curl -SL "https://github.com/littlepush/PECoTask/archive/master.zip" | tar -xz - -C .
[ $? -ne 0 ] && return $?
cd PECoTask-master
make && make install && make -f Makefile.debug && make -f Makefile.debug install
[ $? -ne 0 ] && return $?
cd -
rm -rf PECoTask-master
# Install PECoNet
curl -SL "https://github.com/littlepush/PECoNet/archive/master.zip" | tar -xz - -C .
[ $? -ne 0 ] && return $?
cd PECoNet-master
make && make install && make -f Makefile.debug && make -f Makefile.debug install
[ $? -ne 0 ] && return $?
cd -
rm -rf PECoNet-master
# Install PECoAutoStage
curl -SL "https://github.com/littlepush/PECoAutoStage/archive/master.zip" | tar -xz - -C .
[ $? -ne 0 ] && return $?
cd PECoAutoStage-master
make && make install && make -f Makefile.debug && make -f Makefile.debug install
[ $? -ne 0 ] && return $?
cd -
rm -rf PECoAutoStage-master
