# UEFI-based bootloader for Unicycle applications

UEFI bootloader that allows to load Unicycle application directly, without any support from operating system like
Linux. This bootloader implements [Uniboot](https://github.com/libunicycle/uniboot-spec) protocol on top of UEFI specification.

The bootloader supports booting from the same bootable device where bootloader.efi is located or loading from network using TFTP protocol.

## Booting Unicycle applications over the network
While booting from the disk is one of the possible options it requires generating and reflashing images to disk/USB stick every time
the Unicycle application is modified. Such workflow makes it harder to do quick iterative development.

To help to increase development speed this bootloader supports booting over the network.
The idea is derived from [Zircon](https://fuchsia.googlesource.com/zircon/+/refs/heads/master/bootloader/) project and shares many implementation details.
Once the device boots it sends a multicast request to check whether the network has a [bootserver](https://github.com/libunicycle/bootserver) running.
And if it has one it downloads the application binary file over TFTP and loads it into the device memory.

To enable network-based boot process one needs to make sure the test device UEFI configured with network enabled
then [bootserver](https://github.com/libunicycle/bootserver) need to be run at the host.

## Build project
To build UEFI bootloader binary please run
`make`

## Create bootable image
To create a bootable image please build bootloader using instructions above.
Then update bootloader config file `bootloader.cfg` to choose whether you prefer over-the-network or from-disk application boot.
Once you completed please run:

`sudo image_generate.sh`

At the end you'll get `boot.img` that is suitable for flashing to a disk or USB flash stick:

`sudo dd if=boot.img of=/dev/sdXXXX`

sdXXXX - device disk for your bootable device. You can find the name using `lsblk` tool.
