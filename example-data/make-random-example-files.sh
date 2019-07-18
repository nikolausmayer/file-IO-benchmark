#!/bin/bash

#######################
## Safety mechanisms ##
#######################
## Fail if any command fails (use "|| true" if a command is ok to fail)
set -e
## Fail if any pipeline command fails
set -o pipefail
## Fail if a glob does not expand
shopt -s failglob


##################################
## Test if "shred" is available ##
##################################
SHRED=`which shred || echo "A"`;
if test $SHRED = "A"; then
  echo "\"shred\" is not available, please install the \"coreutils\" package and try again!";
  exit `false`;
fi

if test ! -w .; then
  echo "cannot write to this folder!";
  exit `false`;
fi


#########################
## Create random files ##
#########################
FILE_SIZE_MB=10;
NUMBER_OF_FILES=100;
echo "Creating $NUMBER_OF_FILES random files with $FILE_SIZE_MB MB each...";
file_index=0;
while test $file_index -lt $NUMBER_OF_FILES; do
  filename=`printf "%20d.bin" $file_index`;
  touch $filename && $SHRED -n 1 -s ${FILE_SIZE_MB}M $filename;
  file_index=`expr $file_index + 1`;
done


#############################################
## Create list of files for benchmark tool ##
#############################################
find . -name "*bin" -type f | sort > test-files.txt;


exit `:`;

