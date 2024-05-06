#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
INIT_DIR=$(pwd)
COMPILER_LIBC_DIR=${INIT_DIR}/libc


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    echo "Applying patch ${INIT_DIR}/yylloc.patch"
    git apply ${INIT_DIR}/yylloc.patch

    #Add your kernel build steps here
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    make -j8 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

#Create necessary base directories
mkdir ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs
if [ $? -eq 0 ];
then
   mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
   mkdir -p usr/bin usr/lib usr/sbin
   mkdir -p var/log
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

#Make and install busybox
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- 
make CONFIG_PREFIX=${OUTDIR}/rootfs  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- install


echo "Library dependencies"
cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

#Add library dependencies to rootfs
cd ${COMPILER_LIBC_DIR}
sudo cp ld-linux-aarch64.so.1 "$OUTDIR"/rootfs/lib
sudo cp libm.so.6 libresolv.so.2 libc.so.6 "$OUTDIR"/rootfs/lib64

#Make device nodes
cd "$OUTDIR"/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

#Clean and build the writer utility
cd ${INIT_DIR}
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-

#Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd ${INIT_DIR}
cp -r finder.sh finder-test.sh writer ./conf ./autorun-qemu.sh  ${OUTDIR}/rootfs/home

#Chown the root directory
cd "$OUTDIR"/rootfs
sudo chown -R root:root *

#Create initramfs.cpio.gz
cd "$OUTDIR"/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
