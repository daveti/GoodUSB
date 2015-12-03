#!/bin/bash

if [ "$1" = "honey.txt" ]
then
	cat $1 | cut -d "[" -f2 | cut -d "]" -f1 | tr -d " " > tmp
else
	cat $1 | cut -d "[" -f3 | cut -d "]" -f1 | tr -d " " > tmp
fi
