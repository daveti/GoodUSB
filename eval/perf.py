#!/usr/bin/python

# Perf.py
# Accept a file (time.perf) with a number at each line
# Compute the min/avg/max/mdev then
# Nov 10, 2013
# daveti@cs.uoregon.edu
# http://davejingtian.org

import sys

def readPerfFile(filePath):
        '''
        Read a perf file into a list
        '''
        data = []
        try:
                fnObj = open(filePath, 'r')
                for line in fnObj:
                        line = line.strip()
                        data.append(float(line))
        finally:
                fnObj.close()

        return(data)


def computeMdev(data, avg):
        '''
        Compute the mean deviation given the list and the average value
        '''
        num = len(data)
        sum = 0.0
        for d in data:
                sum += abs(d - avg)

        return(sum/num)
        

def main():
        '''
        Main method
        '''
        if len(sys.argv) != 2:
                print('Error: invalid number of parameters')
                return(1)

        # Read the file
        data = readPerfFile(sys.argv[1])

        # Find the min
        minV = min(data)

        # Compute the avg
        avgV = sum(data) / float(len(data))

        # Find the max
        maxV = max(data)

        # Compute the mdev
        mdevV = computeMdev(data, avgV)

        # Output
        print('time in ms min/avg/max/mdev')
        print('%f/%f/%f/%f' %(minV,avgV,maxV,mdevV))


if __name__ == '__main__':
        main()
