# Gud GUI Python script
# This script is used to display the security pic selection
# The output is part of the output of gudGUI.sh
# Apr 21, 2015
# Add Product/Manufacturer into the GUI
# May 12, 2015
# root@davejingtian.org
# http://davejingtian.org

import os
import sys
from Tkinter import *

config = "./pic/pic.conf"
guiInput = "./input/gudGUI.input"
guiProduct = ""
guiManufacturer = ""
totalNum = 0
picDir = ""
picFormat = ""
picUsed = ""
picIndexConf = ""
output = "Security_pic_index"
# Should be disable during the working mode as the output will be piped into gud!
debug = False
ut = False
if ut:
	guiInput = "./input/gudGUI.input.ut" 

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

# Read and parse the GUI input 
lines = [line.strip() for line in open(guiInput)]
for l in lines:
	if l.startswith('#'):
		continue
        if debug:
		print(l)
	# Split the line
	tmp = l.split('=')
	if debug:
		print(tmp)
	if tmp[0] == 'product':
		guiProduct = tmp[1]
	elif tmp[0] == 'manufacturer':
		guiManufacturer = tmp[1]
	else:
		if debug:
			print('Error: unknown config', tmp)
if debug:
	print('product=[%s], manufacturer=[%s]' %(guiProduct, guiManufacturer))


# Save the used pic indices
picUsedIndex = []
if os.path.isfile(picIndexConf):
	# Read-in the index config
	lines = [line.strip() for line in open(picIndexConf)]
	# Should be only one line in CSV format
	picUsedIndex = lines[0].split(',')
if debug:
	print('picUsedIndex:', picUsedIndex)

# Create the basic window
window = Tk()
window.wm_title("GoodUSB: Select a Security Picture")

# Update the index conf
def writeIndexConf():
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
	# Save this selected index
	picUsedIndex.append(str(v.get()))
	if debug:
		print('picUsedIndex:', picUsedIndex)
	# Write back to the config file
	writeIndexConf()
	# Do not use print to avoid extra newline/space
	sys.stdout.write(output + str(v.get()))
	window.destroy()

def cancelCallBack():
	# Do not care about the choice
	#print v.get()
	sys.stdout.write(output + '0')
	window.destroy()

# Add the product/manufacturer label
desc = 'Product: ' + guiProduct + '\n' + 'Manufacturer: ' + guiManufacturer
Label(window, text=desc).pack(anchor=CENTER)

# Construct the radio buttons
v = IntVar()
pics = []
for i in range(1, totalNum+1):
	# Check if the pic has been used already
	picSkip = False
	for s in picUsedIndex:
		if i == int(s):
			picSkip = True
			break
	if picSkip:
		continue
	# Display this picture	
	picFile = picDir+'/'+str(i)+'.'+picFormat
	if debug:
		print(picFile)
	pic = PhotoImage(file=picFile)
	# NOTE: have to save the reference before the image shows up!
	pics.append(pic)
	Radiobutton(window, image=pic, variable=v, value=i).pack(anchor=W)

# Construct the OK/Cancel buttons
Button(window, text="Complete Registration", command=okCallBack).pack(side='right')
Button(window, text="Suspend Registration", command=cancelCallBack).pack(side='left')

# Launch the GUI
mainloop()

