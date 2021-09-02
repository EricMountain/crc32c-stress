all: crc32c stress


crc32c.h: table_generator.c
	gcc -O3 -o table_generator table_generator.c
	./table_generator > crc32c.h

crc32c: crc32c.h crc32c.c main.c
	gcc -O3 -o crc32c crc32c.c main.c

stress: crc32c.h crc32c.c stress.c
	gcc -lpthread -O3 -o stress crc32c.c stress.c

.PHONY: clean
clean:
	rm -f stress crc32c
