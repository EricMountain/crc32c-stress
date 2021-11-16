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

// find_corruption -b 4 -c 4201578152 -f test.out -t 8 < ../data

uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#define BUF_LEN 100000000
#define THREADS 4

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


struct thread_info {             /* Used as argument to thread_start() */
    pthread_t thread_id;         /* ID returned by pthread_create() */
    long unsigned thread_num;    /* Application-defined thread # */
    ssize_t length;              /* Length of the block to checksum */
    unsigned int crc32_target;   /* CRC32C we are seeking to match */
    unsigned int crc32_found;    /* CRC32C computed to begin with */
    char *buffer;                /* Pointer to buffer to checksum. Threads need to make own copy. */
    int bytes_wrong;             /* Number of wrong bytes to assume */
    int offset;                  /* Offset into buffer at which to start permutations */
    char *output_file;
};

unsigned long long power(unsigned long long base, int exp)
{
    unsigned long long result = 1;
    while(exp)
    {
        result *= base;
        exp--;
    }
    return result;
}

static void * thread_start(void *arg)
{
    struct thread_info *tinfo = arg;

    ssize_t length = tinfo->length;
    uint32_t crc32_target = tinfo->crc32_target;
    uint32_t crc32_found = tinfo->crc32_found;
    char *buffer_ro = tinfo->buffer;
    int bytes_wrong = tinfo->bytes_wrong;
    int offset = tinfo->offset;
    char *output_file = tinfo->output_file;

//    printf("Thread %lu: length %ld, crc32_target %u\n", tinfo->thread_num, length, crc32_target);

    char *buffer = (char *) calloc(length, sizeof(char));
    if (buffer == NULL) {
        handle_error("thread calloc");
    }
    memcpy(buffer, buffer_ro, length * sizeof(char));

    uint32_t crc32_interim = crc32c(0, buffer, length);
    if (crc32_interim != crc32_found) {
        printf("%u %u\n", crc32_interim, crc32_found);
        handle_error("BUG!");
    }

    char save[bytes_wrong];
    memcpy(save, buffer+offset, bytes_wrong * sizeof(char));

    unsigned long long jx = power(0x100, bytes_wrong);
//    printf("bytes_wrong %d, jx=%llu 0x%llx, 0 < jx = %d\n", bytes_wrong, jx, jx, 0 < jx);
    for (unsigned long long j = 0; j < jx; j++) {
        uint64_t j2 = j;
        for (int m = 0; m < bytes_wrong; m++) {
            buffer[offset+m] = j2 & 0xff;
            j2 >>= 8;
        }
        uint32_t crc32_test = crc32c(0, buffer, length);
        if (crc32_test == crc32_target) {
            printf("Found solution %u, target %u, offset %d, bytes %d\n", crc32_test, crc32_target, offset, bytes_wrong);
            printf("Corrupt:\n");
            for (int n = 0; n < bytes_wrong; n++) {
                printf("%02hhx ", save[n]);
            }
            printf("\n");
            printf("Correct:\n");
            for (int n = 0; n < bytes_wrong; n++) {
                printf("%02hhx ", buffer[offset+n]);
            }
            printf("\n");
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            ssize_t written = write(fd, buffer, length);
            if (written == -1) {
                handle_error("write");
            } else if (written != length) {
                handle_error("short write");
            }
            close(fd);
//            exit(0);
        }
    }

    free(buffer);

    return NULL;
}

// Args: crc32-target out-file hint-bytes-wrong hint-offset, threads
int main(int argc, char *argv[]) {
    uint32_t crc32_target = 4201578152;
    int hint_bytes_wrong = 1;
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
    char *buffer = NULL;
    if ((buffer = malloc(BUF_LEN * sizeof(char))) == NULL) {
        handle_error("malloc");
    }
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

    pthread_attr_t attr;
    int s = pthread_attr_init(&attr);
    if (s != 0)
       handle_error_en(s, "pthread_attr_init");

    struct thread_info *tinfo = calloc(threads, sizeof(*tinfo));
    if (tinfo == NULL)
       handle_error("calloc");

    // Try sequential n-byte corruption
    // NB k is max 7, o/w (256 << ((k-1)*8)) will overflow I think. Need to do the math properly, but
    //    given length is a few thousand bytes the complexity is going through the roof anyway
    unsigned long thread_num = 1;
    for (int k = hint_bytes_wrong; k < 8; k++) {
        printf("k=%d, hint_offset=%d\n", k, hint_offset);
        for (int i = hint_offset; i < length-k; i++) {
            int thread_slot = i % threads;
            if (tinfo[thread_slot].thread_num != 0) {
                // In-use, we need to join
                pthread_join(tinfo[thread_slot].thread_id, NULL);
                tinfo[thread_slot].thread_num = 0;
            }

            tinfo[thread_slot].thread_num = thread_num++;
            tinfo[thread_slot].length = length;
            tinfo[thread_slot].crc32_target = crc32_target;
            tinfo[thread_slot].crc32_found = crc32_found;
            tinfo[thread_slot].buffer = buffer;
            tinfo[thread_slot].bytes_wrong = k;
            tinfo[thread_slot].offset = i;
            tinfo[thread_slot].output_file = output_file;

//            printf("create thread %lu\n", thread_num);
            int res = pthread_create(&tinfo[thread_slot].thread_id, &attr,
                                     &thread_start, &tinfo[thread_slot]);
            if (res != 0)
                handle_error("pthread_create");
        }
        for (int t = 0; t < threads; t++) {
            // Ensure all threads complete
            if (tinfo[t].thread_num != 0) {
                // In-use, we need to join
                pthread_join(tinfo[t].thread_id, NULL);
                tinfo[t].thread_num = 0;
            }
        }
        printf("Did not find %d-byte solution\n", k);
    }

    free(output_file);

	exit(0);
}

