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


################
## Parameters ##
################
FILE_SIZE="10M";
NUMBER_OF_FILES=100;
while getopts "s:n:-:" OPTION; do
  case "${OPTION}" in
    -) 
      ## Long options using --optionname
      case "${OPTARG}" in 
        file-size) 
          VALUE="${!OPTIND}"; OPTIND=`expr $OPTIND + 1`;
          FILE_SIZE="${VALUE}";
          ;;
        file-size=*) 
          VALUE="${OPTARG#*=}";
          FILE_SIZE="${VALUE}";
          ;;
        number-of-files) 
          VALUE="${!OPTIND}"; OPTIND=`expr $OPTIND + 1`;
          NUMBER_OF_FILES="${VALUE}";
          ;;
        number-of-files=*) 
          VALUE="${OPTARG#*=}";
          NUMBER_OF_FILES="${VALUE}";
          ;;
        *)
          printf "%s\n" "Unknown option --${OPTARG}";
          exit `false`;
          ;;
      esac
      ;;
    s)
      FILE_SIZE="${OPTARG}";
      ;;
    n)   
      NUMBER_OF_FILES="${OPTARG}";
      ;;
    [?]) 
      exit `false`;
      ;;
  esac
done
#shift `expr $OPTIND - 1`;


#########################
## Create random files ##
#########################
echo "Creating $NUMBER_OF_FILES random files with size $FILE_SIZE each...";
file_index=0;
while test $file_index -lt $NUMBER_OF_FILES; do
  printf ".";
  filename=`printf "%20d.bin" $file_index`;
  touch $filename && $SHRED -n 1 -s ${FILE_SIZE} $filename;
  file_index=`expr $file_index + 1`;
done
printf "\n";


#############################################
## Create list of files for benchmark tool ##
#############################################
find . -name "*bin" -type f | sort > test-files.txt;


exit `:`;

