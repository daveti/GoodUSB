#!/bin/bash
# gudHoney.sh
# Redirect the untrusted USB device into the HoneyUSB
# May 8, 2015
# root@davejingtian.org
# http://davejingtian.org

Debug=false
idVendor=$1
idProduct=$2
VM="honeyUSB"
XML="./xml/honeyIloveU.xml"

# Construct the device XML file
printf "<hostdev mode='subsystem' type='usb'>\n" > $XML
printf "\t<source>\n" >> $XML
printf "\t\t<vendor id='%s'/>\n" "$idVendor" >> $XML
printf "\t\t<product id='%s'/>\n" "$idProduct" >> $XML
printf "\t</source>\n" >> $XML
printf "</hostdev>\n" >> $XML

# Debug
if $Debug
then
	echo "idVendor="$idVendor
	echo "idProduct="$idProduct
	echo "VM="$VM
	echo $XML
	cat $XML
fi

# Start the Honey USB in case not
virsh start $VM

# Pass through the device
virsh attach-device $VM $XML

# Rename the XML device
mv $XML $XML.$(date +%s) 2>&1
