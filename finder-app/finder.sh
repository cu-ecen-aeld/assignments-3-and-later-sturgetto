#!/bin/sh

if [ $# -ne 2 ]; then
    echo "ERROR: Invalid number of arguments."
    echo "Total number of arguments should be 2."
    echo "The order of the arguments should be:"
    echo "   1)File Directory Path"
    echo "   2)String to be searched in the specified directory path."
    exit 1
fi

filesdir=$1
if [ -d ${filesdir} ]; then
    searchstr=$2
    numfiles=$(find ${filesdir} | wc -l)
    if [ $? -eq 0 ]; then
        numlines=$(grep -R ${searchstr} ${filesdir} | wc -l)
        if [ $? -eq 0 ]; then
            numfiles=$(expr ${numfiles} - 1)
            echo "The number of files are ${numfiles} and the number of matching lines are ${numlines}"
            exit 0
        else
            echo "ERROR: Unable to find lines in files in File Directory Path."
            exit 1
        fi
    else
        echo "ERROR: Unable to find files in File Directory Path."
        exit 1
    fi
else
    echo "ERROR: Specified File Directory Path does not exist."
    exit 1
fi
