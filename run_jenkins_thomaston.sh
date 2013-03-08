#!/bin/sh -x
#
# External variables (to be set by Jenkins) are:
#  - NFS_VERS
#  - SERVER 
#

BASE=`pwd`

case $NFS_VERS in
  3)
    MOUNT_CMD="mount -o vers=3,lock $SERVER:/tmp /mnt" 
    ;;
  4)
    MOUNT_CMD="mount -o vers=4,lock $SERVER:/tmp /mnt" 
    ;;
  4.1)
    MOUNT_CMD="mount -o vers=4,minorversion=1,lock $SERVER:/tmp /mnt" 
    ;;
  9P)
    #MOUNT_CMD="mount -t 9P $SERVER:/tmp /mnt" 
    IPADDR=`resolveip -s $SERVER 2>&1` 
    MOUNT_CMD="mount -t 9p IPADDR /mnt -o uname=root,aname=/tmp,msize=65560,version=9p2000.L,debug-0x0,user=access,port=564"
    ;;
  *)
    echo "unsupported NFS_VERS $NFS_VERS"
    exit 1 
    ;;
esac

# Compile ganesha
mkdir -p build 
cd build
cmake CCMAKE_BUILD_TYPE=Debug $BASE/src && make 

# Copy ganesha to server
ssh $SERVER "pkill -9 ganesha.nfsd" || echo "Nothing to kill remotely"
scp ./MainNFSD/ganesha.nfsd        $SERVER:/tmp
scp ./FSAL/FSAL_VFS/libfsalvfs.so  $SERVER:/tmp
scp ../vfs.ganesha.nfsd.conf       $SERVER:/tmp

ssh $SERVER 'nohup /tmp/ganesha.nfsd -d -L /tmp/ganesha.log -f /tmp/vfs.ganesha.nfsd.conf -p /tmp/pid.ganesha &' &

# run the test
sleep 5

umount -f /mnt || echo "Nothing was mounted on /mnt"
$MOUNT_CMD || exit 1 
cd $BASE/non_regression_tests/thomasthon ; ./run_tests.sh -j /mnt || exit 1
umount -f /mnt

ssh $SERVER 'pkill -9 ganesha.nfsd'

cp /tmp/test_report.xml $BASE/build
echo
echo "===== thomasthon test is OK ===="
echo 
exit 0 
