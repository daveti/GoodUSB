QEMU-KVM HCD controller pass-through

1. VM config under /etc/libvirt/qemu/
2. Command to determine the PCI address for the HCD controller - "lspci -nn | grep -i usb"
3. What we have done - passthrough the Intel USB 3.0 HCD controller to the VM

May 1, 2015

root@davejingtian.org

http://davejingtian.org
