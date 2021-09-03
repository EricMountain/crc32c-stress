#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>

uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#define BUF_LEN 5000

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[]) {
	char buffer[BUF_LEN];

	ssize_t len_tmp = 1;
	ssize_t length = 0;
	while (len_tmp != 0) {
		len_tmp = read(STDIN_FILENO, buffer+length, BUF_LEN);
		if (len_tmp == -1) {
			handle_error("read");
		}
		length += len_tmp;
	}

	uint32_t crc32_found = crc32c(0, buffer, length);
	printf("crc32: %u 0x%x, length %ld\n", crc32_found, crc32_found, length);

    uint32_t crc32_target = 4201578152;

    // Try single byte corruption
    for (int i = 0; i < length; i++) {
        char c = buffer[i];
        uint32_t crc32_interim = crc32c(0, buffer, length);
        if (crc32_interim != crc32_found) {
            printf("BUG!\n");
            exit(1);
        }
        for (int j = 0; j < 256; j++) {
            buffer[i] = j;
            uint32_t crc32_test = crc32c(0, buffer, length);
            if (crc32_test == crc32_target) {
                printf("OK!\n");
                exit(0);
            }
        }
        buffer[i] = c;
    }
    printf("Did not find single-byte solution\n");

    // Try sequential n-byte corruption
    // NB k is max 7, o/w (256 << ((k-1)*8)) will overflow I think. Need to do the math properly, but
    //    given length is a few thousand bytes the complexity is going through the roof anyway
    // NB2 Start at 3 b.c. we know there's a solution there
    for (int k = 3; k < 7; k++) {
        char save[k];
        // NB2 Start at 310 b.c. we know there's a solution there
        for (int i = 310; i < length; i++) {
            memcpy(save, buffer+i, k);
            uint32_t crc32_interim = crc32c(0, buffer, length);
            if (crc32_interim != crc32_found) {
                handle_error("BUG!");
            }
            for (uint64_t j = 0; j < 256 << ((k-1)*8); j++) {
                uint64_t j2 = j;
                for (int m = 0; m < k; m++) {
                    buffer[i+m] = j2 & 0xff;
                    j2 >>= 8;
                }
                uint32_t crc32_test = crc32c(0, buffer, length);
                if (crc32_test == crc32_target) {
                    printf("OK! found %u, target %u, offset %d, bytes %d\n", crc32_test, crc32_target, i, k);
                    printf("Replacements:\n");
                    for (int n = 0; n < k; n++) {
                        printf("%02hhx ", buffer[i+n]);
                    }
                    printf("\n");
                    int fd = open("data.fixed", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    ssize_t written = write(fd, buffer, length);
                    if (written == -1) {
                        handle_error("write");
                    } else if (written != length) {
                        handle_error("short write");
                    }
                    close(fd);
                    exit(0);
                }
            }
            memcpy(buffer+i, save, k);
        }
        printf("Did not find %d-byte solution\n", k);
    }

	exit(0);
}

