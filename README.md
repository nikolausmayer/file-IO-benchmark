# iobench

**iobench** is a tiny disk-I/O benchmarking tool.

## Building

Just `make`.

If you need test data, this will create 100 files with 10 MB of randomness each:

```
$ cd example-data
$ bash make-random-example-files.sh
```

**The more data, the better!** The benchmark will be useless if all data is read from RAM or disk cache.

## Usage

**Do not run any other I/O or CPU load while running iobench!**

```
$ iobench --infiles test-files.txt --jobs 4
```

where `test-files.txt` is a one-path-per-line list of test files to be read, e.g.

```
$ cd example-data
$ bash make-random-example-files.sh
$ cat test-files
./example-data/0000.bin
./example-data/0001.bin
./example-data/0002.bin
./example-data/0003.bin
./example-data/0004.bin
./example-data/0005.bin
./example-data/0006.bin
./example-data/0007.bin
./example-data/0008.bin
./example-data/0009.bin
```

**iobench** allows for multithreaded testing and measures the speed at which the test files are read. It also measures the actual disk speed to detect caching, and the current CPU usage to detect if the application is constrained by CPU (instead of by I/O as desired).


## Notes

- **iobench** cannot detect caching on NFS or otherwise not-directy-attached filesystems.

- Start with a sinple thread `--jobs 1` to get a feeling for how fast your system is. Then ramp up the thread count until **iobench** stops complaining about being CPU-constrained and you see that the cumulative reading speed stops increasing.

- **iobench** is written in C++ which may not be the best choice for fast raw file I/O, so **iobench is not a benchmark for singlethreaded I/O**! Its only purpose is to max out your disk by throwing loads of reading threads at it.


## License

**iobench** and all dependencies are under MIT license.
