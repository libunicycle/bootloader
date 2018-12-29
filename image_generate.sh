#/bin/sh

set -e

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
fi

OUTPUT=boot.img

truncate --size 100M $OUTPUT
sgdisk --clear \
  --new 1:: --typecode=1:ef00 --change-name=1:'EFI System' \
  $OUTPUT

  # Loop sparse file
LOOPDEV=$(losetup --find --show $OUTPUT)
partprobe ${LOOPDEV}

mkfs.fat -F32 ${LOOPDEV}p1

umount hda/ || true
rm -rf hda/
mkdir hda
mount ${LOOPDEV}p1 hda/
mkdir -p hda/EFI/BOOT/
cp bootloader.efi hda/EFI/BOOT/BOOTX64.EFI
cp bootloader.cfg hda/
umount hda/

losetup -d ${LOOPDEV}
rm -rf hda/

chown $SUDO_USER $OUTPUT
