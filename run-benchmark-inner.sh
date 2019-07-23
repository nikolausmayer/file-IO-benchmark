#!/bin/bash
#######################################################################
## Nikolaus Mayer, 2019 (mayern@cs.uni-freiburg.de)
#######################################################################

#######################################################################
## Run IOBENCH
#######################################################################


## Practice safe shellscripting, kids!
## Fail if any command fails (use "|| true" if a command is ok to fail)
set -e
## Fail if any pipeline command fails
set -o pipefail
## Fail if a glob does not expand
shopt -s failglob


#DEBUG="echo";


## Test if current folder is writable
if test ! -w .; then
  echo "Cannot write to this folder!";
  exit `false`;
fi
echo "";


## Test for needed programs
SHRED=`which shred || echo "A"`;
if test "$SHRED" = "A"; then
  echo "\"shred\" is not available, please install the \"coreutils\" package and try again!";
  exit `false`;
fi
WGET=`which wget || echo "A"`;
if test "$WGET" = "A"; then
  echo "\"wget\" is not available, please install the \"wget\" package and try again!";
  exit `false`;
fi


## CPU cores
NPROC=`which nproc || echo "A"`;
if test "$NPROC" = "A"; then
  read -r -p "\"nproc\" is not available. Please tell us how many CPU cores you have: " NPROC;
  printf "Ok, running for %d cores.\n" "$NPROC";
else
  NPROC=`nproc`;
  printf "%d cores detected.\n" "$NPROC";
fi
echo "";


## Available RAM
RAM=`grep MemTotal /proc/meminfo | tr -s ' ' | cut -s -d' ' -f2`;
RAM_GB=`expr $RAM / 1048576`;
printf "%d GB RAM detected.\n" "$RAM_GB";


## Check location of IOBENCH
FOLDER=`dirname "$0"`;
if test ! -d "$FOLDER"; then
  read -r -p "We can't seem to find the location of IOBENCH... please give us its folder: " FOLDER;
fi


## Compile benchmark if necessary
IOBENCH="$FOLDER/iobench";
if test ! -f "$IOBENCH"; then
  echo "Building iobench...";
  pushd "$FOLDER" && make && popd;
  echo "";
fi


## Get compiler info from binary
readelf -p .comment "$IOBENCH" > iobench-elfcomment.txt;


## Get "FlyingThings3D disparity" dataset
if test -d disparity; then
  echo "Found \"disparity\" dataset.";
else
  if test ! -f flyingthings3d__disparity.tar.bz2; then
    echo "Fetching \"disparity\" dataset... (ca. 86 Gigabytes)";
    $DEBUG "$WGET" --no-check-certificate "https://lmb.informatik.uni-freiburg.de/data/SceneFlowDatasets_CVPR16/Release_april16/data/FlyingThings3D/derived_data/flyingthings3d__disparity.tar.bz2";
  fi
  echo "decompressing \"disparity\" dataset... (slow; 86 GB --> ca. 104 GB)";
  $DEBUG tar xfj flyingthings3d__disparity.tar.bz2;
  echo "creating file index \"disparity_files.txt\"...";

  reps=`expr $RAM_GB / 104`;
  if test $reps -gt 0; then
    printf "Dataset is ca. 104 GB; creating %d copies of \"disparity\" to saturate available RAM..." $reps;
    rep_idx=0;
    while test $rep_idx -lt $reps; do
      cp -r disparity `printf "disparity-%d" $rep_idx`;
      rep_idx=`expr $rep_idx + 1`;
    done
  fi
fi;
find disparity* -type f -name "*pfm" | sort > disparity_files.txt;
if test `wc -l disparity_files.txt | cut -d' ' -f1` -ne 53520; then
  echo "Wrong number of files in \"disparity\" dataset! Please contact the benchmark provider.";
  exit `false`;
fi


## Generate "tinyrandom" dataset (1.000.000 files with 50KB each)
if test -d tinyrandom; then
  echo "Found \"tinyrandom\" dataset.";
else
  echo "Generating \"tinyrandom\" dataset... (ca. $RAM_GB Gigabytes)";
  mkdir tinyrandom;
  pushd tinyrandom;
  $DEBUG "$FOLDER"/example-data/make-random-example-files.sh --file-size 50K --number-of-files `expr $RAM / 50 + 1`;
  popd;
fi
find tinyrandom -type f -name "*bin" | sort > tinyrandom_files.txt;


## Generate "bigrandom" dataset (50 files with 2GB each)
if test -d bigrandom; then
  echo "Found \"bigrandom\" dataset.";
else
  echo "Generating \"bigrandom\" dataset... (ca. $RAM_GB Gigabytes)";
  mkdir bigrandom;
  pushd bigrandom;
  $DEBUG "$FOLDER"/example-data/make-random-example-files.sh --file-size 2G --number-of-files `expr $RAM / 2097152 + 1`;
  popd;
fi
find bigrandom -type f -name "*bin" | sort > bigrandom_files.txt;


## TODO do benchmarks (rotate datasets to counteract caching)
threadcount=1;
runindex=0;
target=`expr $NPROC \* 2`;
while test $threadcount -le $target; do
  "$IOBENCH" --infiles disparity_files.txt                                  \
             --jobs $threadcount                                            \
             --logfile "iobench-disparity-j${threadcount}-${runindex}.txt"  \
    | tee "iobench-disparity-j${threadcount}-${runindex}-stdout.txt";
  runindex=`expr $runindex + 1`;
  "$IOBENCH" --infiles tinyrandom_files.txt                                 \
             --jobs $threadcount                                            \
             --logfile "iobench-tinyrandom-j${threadcount}-${runindex}.txt" \
    | tee "iobench-tinyrandom-j${threadcount}-${runindex}-stdout.txt";
  runindex=`expr $runindex + 1`;
  "$IOBENCH" --infiles bigrandom_files.txt                                  \
             --jobs $threadcount                                            \
             --logfile "iobench-bigrandom-j${threadcount}-${runindex}.txt"  \
    | tee "iobench-bigrandom-j${threadcount}-${runindex}-stdout.txt";
  runindex=`expr $runindex + 1`;
  
  threadcount=`expr $threadcount + 1`;
done


## Wrap up results
tar --create --remove-files --file iobench-results.tar iobench-*.txt;

echo "All done!";


