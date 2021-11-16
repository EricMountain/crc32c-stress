#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#define BUF_LEN 100000000

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[]) {
    char *buffer = NULL;
    if ((buffer = malloc(BUF_LEN * sizeof(char))) == NULL) {
        handle_error("malloc");
    }

	ssize_t len = 1;
	ssize_t acc = 0;
	while (len != 0) {
		len = read(STDIN_FILENO, buffer+acc, BUF_LEN);
		if (len == -1) {
			printf("error\n");
			return 1;
		}
		acc += len;
	}

	uint32_t crc32 = crc32c(0, buffer, acc);
	printf("crc32: %u 0x%x, acc %ld\n", crc32, crc32, acc);

	return 0;
}

