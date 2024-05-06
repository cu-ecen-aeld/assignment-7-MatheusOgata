#!/bin/sh 

FILEDIR=$1
SEARCHSTR=$2

if [ $# -le 1 ] || [ $# -gt 2 ];
then 
	echo "ERROR: Invalid number of argument"
	exit 1
fi

if [ -d $FILEDIR ]; 
then
        cd $FILEDIR
	echo "The number of files are $(ls -lR | grep "^-" -r | wc -l) and the number of matching lines are $(grep -r -n "$SEARCHSTR" . | wc -l)" 
	exit 0

else

	echo "ERROR: $FILEDIR does not represent a directory on the filesystem."
	exit 1

fi

