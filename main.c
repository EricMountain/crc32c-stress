#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#define BUF_LEN 5000

int main(int argc, char *argv[]) {
	char buffer[BUF_LEN];

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

