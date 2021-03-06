#!/bin/sh -ex

TMP_FILES="/tmp/img1 /tmp/img1.new /tmp/img2 /tmp/img2.new /tmp/img3 /tmp/img3.new /tmp/img1.snap1"

rbd snap purge testimg1 || true
rbd rm testimg1 || true
rbd rm testimg2 || true
rbd rm testimg3 || true

rm -f $TMP_FILES

# create an image
dd if=/bin/sh of=/tmp/img1 bs=1k count=1 seek=10
dd if=/bin/dd of=/tmp/img1 bs=1k count=10 seek=100
dd if=/bin/rm of=/tmp/img1 bs=1k count=100 seek=1000
dd if=/bin/ls of=/tmp/img1 bs=1k seek=10000
dd if=/bin/ln of=/tmp/img1 bs=1k seek=100000

# import, snapshot
rbd import $RBD_CREATE_ARGS /tmp/img1 testimg1
rbd resize testimg1 --size=256
rbd export testimg1 /tmp/img2
rbd snap create testimg1 --snap=snap1
rbd resize testimg1 --size=128
rbd export testimg1 /tmp/img3

# info
rbd info testimg1 | grep 'size 128 MB'
rbd info --snap=snap1 testimg1 | grep 'size 256 MB'

# make copies
rbd copy testimg1 --snap=snap1 testimg2
rbd copy testimg1 testimg3

# verify the result
rbd info testimg2 | grep 'size 256 MB'
rbd info testimg3 | grep 'size 128 MB'

rbd export testimg1 /tmp/img1.new
rbd export testimg2 /tmp/img2.new
rbd export testimg3 /tmp/img3.new

cmp /tmp/img2 /tmp/img2.new
cmp /tmp/img3 /tmp/img3.new

# rollback
rbd snap rollback --snap=snap1 testimg1
rbd info testimg1 | grep 'size 256 MB'
rbd export testimg1 /tmp/img1.snap1
cmp /tmp/img2 /tmp/img1.snap1

# remove snapshots
rbd snap rm --snap=snap1 testimg1
rbd info --snap=snap1 testimg1 2>&1 | grep 'error setting snapshot context: (2) No such file or directory'

rm -f $TMP_FILES

echo OK
