AM_CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-fPIC \
	-DSSL_VERSION_1_0=${SSL_VERSION_1_0} \
	-DSSL_VERSION_1_1=${SSL_VERSION_1_1}
	
AM_LDFLAGS = -lssl -lcrypto -pthread -ldl

bin_PROGRAMS = coas
coas_SOURCES = \
	costage-runner.cpp \
	costage.cpp \
	modules.cpp \
	main.cpp 