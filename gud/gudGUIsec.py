#!/usr/bin/python

# Gud GUI SEC Python script
# This script is used to display the device with the security pic selected
# Apr 22, 2015
# Added support for device description for the user mode
# May 12, 2015
# root@davejingtian.org
# http://davejingtian.org

import os
import sys
import time
from Tkinter import *
import tkFont

config = "./pic/pic.conf"
totalNum = 0
picDir = ""
picFormat = ""
picUsed = ""
picIndexConf = ""
gudInput = "./input/gudGUI.input"
gudInputUT = "./input/gudGUIsec.input.ut"
output = "Enable"
proMode = False
configNum = ""
intfTotalNum = ""
manufacturer = ""
product = ""
limitedHidDriver = ""
securityPicIndex = ""
description = ""
interfaceList = []
saveIndex = False
# Should be disable during the working mode as the output will be piped into gud!
debug = False
ut = False

# Read and parse the config
lines = [line.strip() for line in open(config)]
for l in lines:
	if l.startswith('#'):
		continue
	if debug:
		print(l)
	# Split the line
	tmp = l.split('=')
	if debug:
		print(tmp)
	if tmp[0] == 'totalNum':
		totalNum = int(tmp[1])
	elif tmp[0] == 'picDir':
		picDir = tmp[1]
	elif tmp[0] == 'picFormat':
		picFormat = tmp[1]
	elif tmp[0] == 'picIndexConf':
		picIndexConf = tmp[1]
	else:
		if debug:
			print('Error: unknown config', tmp)
if debug:
	print('totalNum=[%d], picDir=[%s], picFormat=[%s], picIndexConf=[%s]' %(totalNum, picDir, picFormat, picIndexConf))

# Save the used pic indices
picUsedIndex = []
if os.path.isfile(picIndexConf):
	# Read-in the index config
	lines = [line.strip() for line in open(picIndexConf)]
	# Should be only one line in CSV format
	picUsedIndex = lines[0].split(',')
if debug:
	print('picUsedIndex:', picUsedIndex)

# Read and parse the gudInput
if ut:
	lines = [line.strip() for line in open(gudInputUT)]
else:
	lines = [line.strip() for line in open(gudInput)]
for l in lines:
	if l.startswith('#'):
		continue
	if debug:
		print(l)
	# Split the line
	tmp = l.split('=')
	if debug:
		print(tmp)
	if tmp[0] == 'pro':
		if tmp[1] == 'true':
			proMode = True
	elif tmp[0] == 'configNum':
		configNum = tmp[1]
	elif tmp[0] == 'interfaceTotalNum':
		intfTotalNum = tmp[1]
	elif tmp[0] == 'product':
		product = tmp[1]
	elif tmp[0] == 'manufacturer':
		manufacturer = tmp[1]
	elif tmp[0] == 'limitedHidDriver':
		limitedHidDriver = tmp[1]
	elif tmp[0] == 'securityPicIndex':
		securityPicIndex = tmp[1]
	elif tmp[0] == 'description':
		description = tmp[1]
	elif tmp[0].startswith('interface'):
		interfaceList.append(tmp[1])
	else:
		if debug:
			print('Error: unknown input', tmp)
if debug:
	print('proMode=[%d], configNum=[%s], intfTotalNum=[%s], product=[%s], manufacturer=[%s], limitedHidDriver=[%s], securityPicIndex=[%s], description=[%s]' %(proMode, configNum, intfTotalNum, product, manufacturer, limitedHidDriver, securityPicIndex, description))
	print('interfaces:', interfaceList)

# Defensive checking
if (securityPicIndex not in picUsedIndex) and (securityPicIndex != '0'):
	if debug:
		print('Warning: securityPicIndex [%s] is NOT saved as used' %(securityPicIndex))
	# We will save it
	saveIndex = True


# Create the basic window
window = Tk()
window.wm_title("GoodUSB: Do you recognize the device?")

# Create frames for pic/text and buttons
frame_desc = Frame(window)
frame_desc.pack()
frame_butt = Frame(window)
frame_butt.pack()

# Rename the input file for future debugging
def renameInputFile():
	if not ut:
		newName = gudInput + '.%Y%m%d%H%M%S'
		os.rename(gudInput, time.strftime(newName))

# Update the index conf
def writeIndexConf():
	# Add the security pic index
	picUsedIndex.append(securityPicIndex)
	indices = ""
	for i in picUsedIndex:
		indices += i + ','
	indices = indices[:-1]
	if debug:
		print('indices:', indices)
	# Open the file
	idxConf = open(picIndexConf, "w")
	idxConf.write(indices)
	idxConf.close()

# Set the callbacks for OK/Cancel buttons
def okCallBack():
	# Write back to the config file if needed
	if saveIndex:
		writeIndexConf()
	# Do not use print to avoid extra newline/space
	sys.stdout.write(output + '1')
	# Rename the input file
	renameInputFile()
	window.destroy()

def cancelCallBack():
	# Do not care about the choice
	#print v.get()
	sys.stdout.write(output + '0')
	# Rename the input file
	renameInputFile()
	window.destroy()

# Construct the security picture
pics = []
if securityPicIndex != '0':
	# Display this picture	
	picFile = picDir + '/' + securityPicIndex + '.' + picFormat
	if debug:
		print(picFile)
	pic = PhotoImage(file=picFile)
	# NOTE: have to save the reference before the image shows up!
	pics.append(pic)
	# Display the pic
	Label(frame_desc, image=pic).pack(side='left')

# Display the text description of the device
desc0 = '\nproduct: ' + product.capitalize() + '\n' + \
	'manufacturer: ' + manufacturer.capitalize()
# Support for underlining
desc1 =	'Configuration Num: ' + configNum + '\n' + \
	'Interface Total Num: ' + intfTotalNum
# Handle different mode
desc2 = ''
if proMode:
	desc2 = 'Limited HID Driver: ' + limitedHidDriver + \
		'\n' + 'Security Pic Index: ' + securityPicIndex + \
		'\n' + 'Interfaces:'
	for i in interfaceList:
		desc2 += '\n' + i
else:
	# Add device description for the user mode
	desc2 = 'Device Description: ' + description

# Display the text
l0 = Label(frame_desc, justify=LEFT, padx=10, text=desc0)
l0.pack(anchor=W)
l1 = Label(frame_desc, justify=LEFT, padx=10, text=desc1)
l1.pack(anchor=W)
l2 = Label(frame_desc, justify=LEFT, padx=10, text=desc2)
l2.pack(anchor=W)

# Underline the desc1
desc1Font = tkFont.Font(l1, l1.cget("font"))
desc1Font.configure(underline=True)
l1.configure(font=desc1Font)

# Construct the OK/Cancel buttons
Button(frame_butt, text="This is my device", command=okCallBack).pack(side='right')
Button(frame_butt, text="This is NOT my device!", command=cancelCallBack).pack(side='left')

# Launch the GUI
mainloop()

