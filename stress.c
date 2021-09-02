#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#define BUF_LEN 5000
#define BIG_AREA 63000000000
#define THREADS 12

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct thread_info {              /* Used as argument to thread_start() */
    pthread_t thread_id;           /* ID returned by pthread_create() */
    int       thread_num;          /* Application-defined thread # */
    unsigned int subareas;         /* Number of subareas to pick from */
    ssize_t length;           /* Length of the block to checksum */
    unsigned int crc32_reference;  /* Reference value of the CRC32C to compare with */
    char *bigarea;                /* Big area in which subareas are to be checksummed */
};


static void * thread_start(void *arg)
{
    struct thread_info *tinfo = arg;

    unsigned int subareas = tinfo->subareas;
    ssize_t length = tinfo->length;
    unsigned int crc32_reference = tinfo->crc32_reference;
    char *bigarea = tinfo->bigarea;

    printf("Thread %d: subareas %u, length %ld, crc32_reference %u\n", tinfo->thread_num, subareas, length, crc32_reference);

    char statebuf[256];
    struct random_data rd;
    int res = initstate_r(time(NULL), statebuf, sizeof(statebuf), &rd);
    if (res != 0) {
        handle_error("initstate_r");
    }

    for(;;) {
        int32_t random_number;
        res = random_r(&rd, &random_number);
        int32_t normalised_random_number = random_number % subareas;

        char *subarea_start = bigarea+(normalised_random_number*length);
        uint32_t crc32_check = crc32c(0, subarea_start, length);
//        printf("crc32: %u 0x%x, length %ld, random_number %d, normalised_random_number %d, subareas %u\n",
//                crc32_check, crc32_check, length, random_number, normalised_random_number, subareas);
        if (crc32_check != crc32_reference) {
            printf("NOK CRC32C diff: %u (check) vs %u (reference)\n", crc32_check, crc32_reference);
            exit(3);
        }
    }

    return NULL;
}


int main(int argc, char *argv[]) {
	char buffer[BUF_LEN];

	ssize_t len = 1;
	ssize_t acc = 0;
	while (len != 0) {
		len = read(STDIN_FILENO, buffer+acc, BUF_LEN);
		if (len == -1) {
			handle_error("read");
		}
		acc += len;
	}

	uint32_t crc32_reference = crc32c(0, buffer, acc);
	printf("crc32: %u 0x%x, acc %ld\n", crc32_reference, crc32_reference, acc);

    // Allocate large area
    // Copy buffer into it and fill it
    // Check crc32c as we go
    // Spawn threads that repeatedly calculate and check crc32c for random parts of the large area

    char *bigarea = NULL;
    if ((bigarea = malloc(BIG_AREA * sizeof(char))) == NULL) {
        handle_error("malloc");
    }

    unsigned int subareas = BIG_AREA / acc;
    printf("Filling big area: %ld bytes, sub areas: %u\n", BIG_AREA, subareas);

    for (int i = 0; i < subareas; i++) {
        char *subarea_start = bigarea+(i*acc);
        memcpy(subarea_start, buffer, acc);
        uint32_t crc32_check = crc32c(0, subarea_start, acc);
//        printf("crc32: %u 0x%x, acc %ld\n", crc32_check, crc32_check, acc);
        if (crc32_check != crc32_reference) {
            printf("CRC32C diff: %u (check) vs %u (reference)\n", crc32_check, crc32_reference);
        }
    }

    int num_threads = THREADS;

    pthread_attr_t attr;
    int s = pthread_attr_init(&attr);
    if (s != 0)
       handle_error_en(s, "pthread_attr_init");

    struct thread_info *tinfo = calloc(num_threads, sizeof(*tinfo));
    if (tinfo == NULL)
       handle_error("calloc");

    for (int tnum = 0; tnum < num_threads; tnum++) {
       tinfo[tnum].thread_num = tnum + 1;
       tinfo[tnum].subareas = subareas;
       tinfo[tnum].length = acc;
       tinfo[tnum].crc32_reference = crc32_reference;
       tinfo[tnum].bigarea = bigarea;

       s = pthread_create(&tinfo[tnum].thread_id, &attr,
                          &thread_start, &tinfo[tnum]);
       if (s != 0)
           handle_error_en(s, "pthread_create");
    }

    s = pthread_attr_destroy(&attr);
    if (s != 0)
       handle_error_en(s, "pthread_attr_destroy");

    for (int tnum = 0; tnum < num_threads; tnum++) {
       s = pthread_join(tinfo[tnum].thread_id, NULL);
       if (s != 0)
           handle_error_en(s, "pthread_join");

       printf("Joined with thread %d\n", tinfo[tnum].thread_num);
    }

    free(tinfo);

	return 0;
}

