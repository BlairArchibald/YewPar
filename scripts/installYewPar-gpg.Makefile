.PHONY : all clean

INSTALL_LOC=${HOME}/sandbox/YewParInstall/
N_JOBS=16 # Number of jobs to use for parallel builds. possibly change if you want to use make -J n too.

# Hands off
CMAKE=$(INSTALL_LOC)/cmake-3.14.4-Linux-x86_64/bin/cmake

#all : YewPar/build/install YewPar/YewPar_env.sh
all : YewPar/YewPar_env.sh

boost_1_70_0 :
	wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz
	tar xzvf boost_1_70_0.tar.gz

boost_1_70_0/stage/lib : boost_1_70_0
	( cd boost_1_70_0 ; \
	bash bootstrap.sh ; \
	./b2 -j$(N_JOBS) )

cmake-3.14.4-Linux-x86_64 :
	wget https://github.com/Kitware/CMake/releases/download/v3.14.4/cmake-3.14.4-Linux-x86_64.tar.gz
	tar xzvf cmake-3.14.4-Linux-x86_64.tar.gz

gperftools-gperftools-2.7 :
	wget https://github.com/gperftools/gperftools/archive/gperftools-2.7.tar.gz
	tar xzvf gperftools-2.7.tar.gz

gperftools-gperftools-2.7/install/lib : gperftools-gperftools-2.7
	( cd gperftools-gperftools-2.7 ; \
	  bash autogen.sh ; \
      ./configure --prefix=$$(pwd)/install ; \
      make -j$(N_JOBS) ; \
      make install \
	)

hpx-1.2.1 :
	wget https://github.com/STEllAR-GROUP/hpx/archive/1.2.1.tar.gz
	mv 1.2.1.tar.gz hpx-1.2.1.tar.gz
	tar xzvf hpx-1.2.1.tar.gz

hpx-1.2.1/build/install : hpx-1.2.1 cmake-3.14.4-Linux-x86_64 boost_1_70_0/stage/lib gperftools-gperftools-2.7/install/lib
	(cd hpx-1.2.1 ; \
	 mkdir -p build	; \
	 cd build ; \
	 $(CMAKE) -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=$$(pwd)/install \
       -DHPX_WITH_PARCELPORT_MPI=ON \
       -DHPX_WITH_EXAMPLES=OFF \
       -DHPX_WITH_TESTS=OFF  \
       -DBOOST_ROOT=$(INSTALL_LOC)/boost_1_70_0/ \
       -DBOOST_LIBRARYDIR=$(INSTALL_LOC)/boost_1_70_0/stage/lib \
       -DTCMALLOC_LIBRARY=$(INSTALL_LOC)/gperftools-gperftools-2.7/install/lib/ \
       -DTCMALLOC_INCLUDE_DIR=$(INSTALL_LOC)/gperftools-gperftools-2.7/install/include \
       ../ ; \
	  make -j$(N_JOBS) ; \
	  make install )

YewPar :
	git clone https://github.com/BlairArchibald/YewPar.git

YewPar/build/install : hpx-1.2.1/build/install YewPar
	( cd YewPar ; \
      mkdir -p build ; \
      cd build ; \
      $(CMAKE) -DCMAKE_BUILD_TYPE=Release \
           -DCMAKE_INSTALL_PREFIX=$$(pwd)/install \
           -DHPX_DIR=$(INSTALL_LOC)/hpx-1.2.1/build/install/lib/cmake/HPX \
           -DYEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS=16 \
           -DYEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS=220 \
           -DYEWPAR_BUILD_APPS_SIP_NWORDS=128 \
           ../ ; \
    make -j$(N_JOBS) ; \
    make install )

YewPar/YewPar_env.sh : YewPar/build/install
	echo 'export PATH=$(INSTALL_LOC)/cmake-3.14.4-Linux-x86_64/bin/:$$PATH' >YewPar/YewPar_env.sh
	echo 'export LD_LIBRARY_PATH=$(INSTALL_LOC)/boost_1_70_0/stage/lib:$$LD_LIBRARY_PATH' >>YewPar/YewPar_env.sh
	echo 'export LD_LIBRARY_PATH=$(INSTALL_LOC)/hpx-1.2.1/build/install/lib:$$LD_LIBRARY_PATH' >>YewPar/YewPar_env.sh

clean :
	rm -rf boost_1_70_0
	rm -rf hpx-1.2.1
	rm -rf YewPar
	rm -rf cmake-3.14.4-Linux-x86_64
	rm -rf gperftools-gperftools-2.7
	rm -f boost_1_70_0.tar.gz
	rm -f hpx-1.2.1.tar.gz
	rm -f cmake-3.14.4-Linux-x86_64.tar.gz
	rm -f gperftools-2.7.tar.gz
