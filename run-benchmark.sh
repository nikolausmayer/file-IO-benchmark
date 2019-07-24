#!/bin/bash

bash "`dirname "$0"`"/run-benchmark-inner.sh | tee wrapper-log.txt;
tar --append --remove-files --file iobench-results.tar wrapper-log.txt;

