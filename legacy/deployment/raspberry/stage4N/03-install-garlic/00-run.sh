#!/bin/bash -e
#
# creating of required directories
# place all the files to the right directories
#

# for debugging
#ROOTFS_DIR=/home/niko/build_pi_image

# install player binary
mkdir -p "${ROOTFS_DIR}/usr/local/bin"
install -m 755 files/garlic-player "${ROOTFS_DIR}/usr/local/bin"
install -m 755 files/start-garlic.sh "${ROOTFS_DIR}/usr/local/bin"

# install player lib
mkdir -p "${ROOTFS_DIR}/usr/local/lib"
install -m 644 files/libgarlic.so.1.0.0 "${ROOTFS_DIR}/usr/local/lib"

#install desktop file
mkdir -p "${ROOTFS_DIR}/usr/local/share/applications"
install -m 755 files/garlic-player.desktop "${ROOTFS_DIR}/usr/local/share/applications"
install -m 755 files/garlic-player.desktop "${ROOTFS_DIR}/etc/xdg/autostart"

#install icon
mkdir -p "${ROOTFS_DIR}/usr/local/share/applications/pixmaps"
install -m 644 files/garlic-player.png "${ROOTFS_DIR}/usr/local/share/applications/pixmaps"

#install splash screen
rm -f "${ROOTFS_DIR}/usr/share/plymouth/themes/pix/splash.png"
install -m 644 files/splash.png "${ROOTFS_DIR}/usr/share/plymouth/themes/pix"
