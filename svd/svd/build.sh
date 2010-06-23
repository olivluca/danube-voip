#!/bin/sh

#
# ${1} where to put resulting binary (except .)
#

curr_path=`pwd`
libab_path=${curr_path}/../libab
tapi_name=drv_tapi-3.6.1
vinetic_name=drv_vinetic-1.3.1_tapi-3.6.1
path_to_bin=${curr_path}/../../../../../staging_dir_mipsel/bin/
patch_path=$curr_path/../../patches

PATH=$PATH:${path_to_bin}

aclocal
autoheader
automake -a --foreign
autoconf

./configure --host=mipsel-linux-uclibc --build=i686-linux-gnu

cd ${curr_path}/..
tar -xvpf ${tapi_name}.tar.gz 
tar -xvpf ${vinetic_name}.tar.gz 

ln -snf drv_sgatab 	sgatab
ln -snf ${tapi_name}	tapi
ln -snf ${vinetic_name}	vinetic

cd tapi
patch -p1 < $patch_path/tapi.patch
cd ../vinetic
patch -p1 < $patch_path/vinetic.patch

cd $libab_path
./build.sh

cd ${curr_path}
make

mv src/svd ${1}
mv src/svd_if ${1}
./clean_there

cd ${curr_path}/..
rm -rf sgatab tapi vinetic ${tapi_name} ${vinetic_name}

