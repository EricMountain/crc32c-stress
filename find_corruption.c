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
#define THREADS 4

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


struct thread_info {              /* Used as argument to thread_start() */
    pthread_t thread_id;           /* ID returned by pthread_create() */
    int       thread_num;          /* Application-defined thread # */
    ssize_t length;              /* Length of the block to checksum */
    unsigned int crc32_target;   /* CRC32C we are seeking to match */
    char *buffer;                /* Pointer to buffer to checksum. Threads need to make own copy. */
    int bytes_wrong;             /* Number of wrong bytes to assume */
};


// Args: crc32-target out-file hint-bytes-wrong hint-offset, threads
int main(int argc, char *argv[]) {
    uint32_t crc32_target = 4201578152;
    int hint_bytes_wrong = 0;
    int hint_offset = 0;
    int opt;
    int threads = THREADS;
    char *output_file = strdup("output.data");

    while ((opt = getopt(argc, argv, "b:o:c:f:t:")) != -1) {
       switch (opt) {
       case 'b':
           hint_bytes_wrong = atoi(optarg);
           break;
       case 'o':
           hint_offset = atoi(optarg);
           break;
       case 'c':
           crc32_target = strtoul(optarg, NULL, 10);
           break;
       case 'f':
           free(output_file);
           output_file = strdup(optarg);
           break;
       case 't':
           threads = atoi(optarg);
           break;
       default: /* '?' */
           fprintf(stderr, "Usage: %s [-b bytes_wrong] [-o start_offset] [-c target_crc32] [-f output_file] [-t threads]\n",
                   argv[0]);
           exit(EXIT_FAILURE);
       }
    }

    // We'll break stupidly if file is > BUF_LEN bytes (infinite loop?)
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

    // Try sequential n-byte corruption
    // NB k is max 7, o/w (256 << ((k-1)*8)) will overflow I think. Need to do the math properly, but
    //    given length is a few thousand bytes the complexity is going through the roof anyway
    for (int k = hint_bytes_wrong; k < 7; k++) {
        char save[k];
        for (int i = hint_offset; i < length; i++) {
            // Spawn thread TODO
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
                    printf("Found solution %u, target %u, offset %d, bytes %d\n", crc32_test, crc32_target, i, k);
                    printf("Corrupt:\n");
                    for (int n = 0; n < k; n++) {
                        printf("%02hhx ", save[n]);
                    }
                    printf("\n");
                    printf("Correct:\n");
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

    free(output_file);

	exit(0);
}

