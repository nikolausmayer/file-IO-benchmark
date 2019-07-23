#!/bin/bash

bash "`dirname "$0"`"/run-benchmark-inner.sh | tee wrapper-log.txt;
tar rf --remove-files iobench-results.tar.gz wrapper-log.txt;

