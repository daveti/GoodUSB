Kernel Changes

Distro: Ubuntu

'uname -a': Linux daveti-ThinkCentre-M58e 3.13.11.11-daveti-goodusb #5 SMP Mon Feb 9 10:46:21 PST 2015 x86_64 x86_64 x86_64 GNU/Linux

Changes:

1. drivers/usb/core/goodusb.h
2. drivers/usb/core/goodusb.c
3. drivers/usb/core/hub.c
4. drivers/usb/core/message.c
5. drivers/usb/core/usb.c
6. drivers/usb/core/driver.c
7. drivers/usb/core/Makefile
8. drivers/base/core.c
9. include/linux/usb.h

Support for Capability-limited HID driver:

10. drivers/hid/hid-core.c
11. drivers/hid/usbhid/hid-core.c
12. drivers/hid/usbhid/goodusb_hid.h

Support for USB device fingerprint from the HCD and security picture for the following plugging and Fingerprint DB sync with gud

13. drivers/usb/core/config.c

Feb 9, 2015

root@davejingtian.org

http://davejingtian.org
