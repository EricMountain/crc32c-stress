# Quick and dirty crc32c stress test and corruption finder

## Warning

This code is terrible. It's just a bunch of throw-away hacks for debugging an issue.

## Installation, use

``` shell
sudo apt-get update
sudo apt-get install build-essential
make
./crc32c < data
./stress < data
./find_corruption -c 4201578152 -f test.out -t 8 < data/2021-09-01-001-corrupt.data
./find_corruption -b 3 -o 310 -c 4201578152 -f test.out -t 8 < data/2021-09-01-001-corrupt.data
```
