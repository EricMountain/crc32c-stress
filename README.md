# Quick and dirty crc32c stress test

## Warning

This code is terrible. It's just a bunch of throw-away hacks for debugging an issue.

## Installation, use

``` shell
sudo apt-get update
sudo apt-get install build-essential
make
./crc32c < data
./stress < data
```
