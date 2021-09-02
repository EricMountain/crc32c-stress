# Quick and dirty crc32c stress test

``` shell
sudo apt-get update
sudo apt-get install build-essential
make
./crc32c < data
./stress < data
```
