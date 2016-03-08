#!/bin/bash

# gudGUI
# GoodUSB User Daemon GUI
# Feb 11, 2015
# Added support for security picture selection
# Apr 17, 2015
# Added support for description index for the user mode
# May 12, 2015
# root@davejingtian.org
# http://davejingtian.org

# Global vars
GUD_INPUT="./input/gudGUI.input"
GUD_OUTPUT=""
GUD_DEBUG=true
GUD_GUI_COLUMN1="Pick"
GUD_PIC_DIR="./pic"
GUD_GUI_SEC="./gudGUI.py"
GUD_OUTPUT_SEC=""
GUD_OK_LABEL="Reigister device"
GUD_CAN_LABEL="I already registered this device!"
# For user mode
# NOTE: this should be aligned with goodusb_dev[] in usb.c
USB_DEV_STORAGE="USB Storage (thumb drive, portable disk, SD reader)"
USB_DEV_KEYBOARD="USB Keyboard"
USB_DEV_MOUSE="USB Mouse"
USB_DEV_JOYSTICK="USB Joystick"
USB_DEV_WIRELESS="USB Wireless"
USB_DEV_CELLPHONE="USB Cellphone (iPhone, Nexus, Galaxy)"
USB_DEV_TABLET="USB Tablet (iPad, Nexus, Tab)"
USB_DEV_MICROPHONE="USB Microphone"
USB_DEV_SOUND="USB Sound (sound card, speaker, headphone)"
USB_DEV_HUB="USB Hub (USB port extension)"
USB_DEV_VIDEO="USB Video (WebCam)"
USB_DEV_HEADSET="USB Headset"
USB_DEV_CHARGER="USB Charger (E-cig, portable battery, toy)"
USB_DEV_COMM="USB Communication (USB-USB networking, ATM/Ethernet)"
USB_DEV_PRINTER="USB Printer"
USB_DEV_SCANNER="USB Scanner"
USB_DEV_UNKNOWN="USB UNKNOWN"
GUD_USER_MODE_TITLE="GoodUSB: Device Registration"
GUD_USER_MODE_TEXT="Please choose the desired device functionality:"
GUD_USER_MODE_COLUMN2="Device Description"
GUD_USER_MODE_WIDTH=500
GUD_USER_MODE_HEIGHT=600
# For pro mode
GUD_PRO_MODE_TITLE="GoodUSB: pro mode"
GUD_PRO_MODE_TEXT="Please choose the device interfaces:"
GUD_PRO_MODE_COLUMN2="Interface:SubClass:Protocol:EpNum"
GUD_PRO_MODE_HID="Use_limited_HID_driver"
GUD_PRO_MODE_COLUMNS=""
GUD_PRO_MODE_WIDTH=400
GUD_PRO_MODE_HEIGHT=300

# Parse the input file
PRO=$(sed -n "1p" "$GUD_INPUT" | cut -d"=" -f2 | tr -d "\n\t\r")
CONFIG_NUM=$(sed -n "2p" "$GUD_INPUT" | cut -d"=" -f2 | tr -d "\n\t\r")
INTERFACE_TOTAL_NUM=$(sed -n "3p" "$GUD_INPUT" | cut -d"=" -f2 | tr -d "\n\t\r")
PRODUCT=$(sed -n "4p" "$GUD_INPUT" | cut -d"=" -f2 | tr -d "\n\t\r")
MANUFACTURER=$(sed -n "5p" "$GUD_INPUT" | cut -d"=" -f2 | tr -d "\n\t\r")

if $GUD_DEBUG
then
	echo "pro=$PRO"
	echo "configNum=$CONFIG_NUM"
	echo "interfaceTotalNum=$INTERFACE_TOTAL_NUM"
	echo "product=$PRODUCT"
	echo "manufacturer=$MANUFACTURER"
fi

# Parse the interfaces only in pro mode
if [ "$PRO" = "true" ]
then
	for ((i = 0; i < $((INTERFACE_TOTAL_NUM)); i++))
	do
		INTERFACE=$(sed -n "$((6+i))p" "$GUD_INPUT" | tr -d "\n\t\r")
		if $GUD_DEBUG
		then
			echo "$INTERFACE"
		fi
		
		# Construct the columns
		GUD_PRO_MODE_COLUMNS="$GUD_PRO_MODE_COLUMNS TRUE "$INTERFACE""
	done

	# Append option "Limited HID driver"
	GUD_PRO_MODE_COLUMNS="$GUD_PRO_MODE_COLUMNS TRUE "$GUD_PRO_MODE_HID""
	if $GUD_DEBUG
	then
		echo $GUD_PRO_MODE_COLUMNS
	fi
fi

# Construct the GUI
GUD_GUI_TEXT="Product: $PRODUCT\n\
Manufacturer: $MANUFACTURER\n\
<u>Configuration Num: $CONFIG_NUM</u>\n\
<u>Interface Total Num: $INTERFACE_TOTAL_NUM</u>\n"

# Parse the results
if [ "$PRO" = "false" ]
then
	# User mode - radiolist
	GUD_OUTPUT=$(zenity --list \
			--title "$GUD_USER_MODE_TITLE" \
			--text "$GUD_GUI_TEXT$GUD_USER_MODE_TEXT" \
			--ok-label "$GUD_OK_LABEL" \
			--cancel-label "$GUD_CAN_LABEL" \
			--radiolist \
			--column "$GUD_GUI_COLUMN1" \
			--column "$GUD_USER_MODE_COLUMN2" \
			--width $GUD_USER_MODE_WIDTH \
			--height $GUD_USER_MODE_HEIGHT \
			FALSE "$USB_DEV_STORAGE" \
			FALSE "$USB_DEV_KEYBOARD" \
			FALSE "$USB_DEV_MOUSE" \
			FALSE "$USB_DEV_JOYSTICK" \
			FALSE "$USB_DEV_WIRELESS" \
			FALSE "$USB_DEV_CELLPHONE" \
			FALSE "$USB_DEV_TABLET" \
			FALSE "$USB_DEV_MICROPHONE" \
			FALSE "$USB_DEV_SOUND" \
			FALSE "$USB_DEV_HUB" \
			FALSE "$USB_DEV_VIDEO" \
			FALSE "$USB_DEV_HEADSET" \
			FALSE "$USB_DEV_CHARGER" \
			FALSE "$USB_DEV_COMM" \
			FALSE "$USB_DEV_PRINTER" \
			FALSE "$USB_DEV_SCANNER" \
			FALSE "$USB_DEV_UNKNOWN")
else
	# Pro mode - checklist
	GUD_OUTPUT=$(zenity --list \
			--title "$GUD_PRO_MODE_TITLE" \
			--text "$GUD_GUI_TEXT$GUD_PRO_MODE_TEXT" \
			--ok-label "$GUD_OK_LABEL" \
			--cancel-label "$GUD_CAN_LABEL" \
			--checklist \
			--column "$GUD_GUI_COLUMN1" \
			--column "$GUD_PRO_MODE_COLUMN2" \
			--width $GUD_PRO_MODE_WIDTH \
			--height $GUD_PRO_MODE_HEIGHT \
			--separator="|" \
			$GUD_PRO_MODE_COLUMNS)
fi

# Call the security picture selection
GUD_OUTPUT_SEC=$(python $GUD_GUI_SEC)

# Return the results
printf "$GUD_OUTPUT|$GUD_OUTPUT_SEC" 2>&1

# Clear
if $GUD_DEBUG
then
	mv $GUD_INPUT $GUD_INPUT.$(date +%s) 2>&1
else
	rm -rf $GUD_INPUT 2>&1
fi
