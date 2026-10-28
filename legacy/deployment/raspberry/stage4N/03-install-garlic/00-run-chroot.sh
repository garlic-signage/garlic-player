#!/bin/bash -e
#
# runs in chroot enviroment
# place teh requiered links in right directories
#

# link player start bash script to bin dir
ln -sf /usr/local/bin/start-garlic.sh /usr/bin/start-garlic.sh

# link player lib
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/local/lib/libgarlic.so
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/local/lib/libgarlic.so.1
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/local/lib/libgarlic.so.1.0
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/lib/libgarlic.so
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/lib/libgarlic.so.1
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/lib/libgarlic.so.1.0
ln -sf /usr/local/lib/libgarlic.so.1.0.0 /usr/lib/libgarlic.so.1.0.0
