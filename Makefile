all: crc32c stress find_corruption

crc32c.h: table_generator.c
	gcc -O3 -o table_generator table_generator.c
	./table_generator > crc32c.h

crc32c: crc32c.h crc32c.c main.c
	gcc -O3 -g -o crc32c crc32c.c main.c

stress: crc32c.h crc32c.c stress.c
	gcc -pthread -O3 -g -o stress crc32c.c stress.c

find_corruption: crc32c.h crc32c.c find_corruption.c
	gcc -pthread -O3 -g -o find_corruption crc32c.c find_corruption.c

.PHONY: clean
clean:
	rm -f stress crc32c
