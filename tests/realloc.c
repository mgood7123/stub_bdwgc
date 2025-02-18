
#include <stdio.h>
#include <stdlib.h>

#include "gc.h"

#define COUNT 10000000

#define CHECK_OUT_OF_MEMORY(p) \
    do { \
        if (NULL == (p)) { \
            fprintf(stderr, "Out of memory\n"); \
            exit(69); \
        } \
    } while (0)

int main(void) {
  int i;
  unsigned long last_heap_size = 0;

  MANAGED_STACK_ADDRESS_BOEHM_GC_INIT();
  if (MANAGED_STACK_ADDRESS_BOEHM_GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");

  for (i = 0; i < COUNT; i++) {
    int **p = MANAGED_STACK_ADDRESS_BOEHM_GC_NEW(int *);
    int *q;

    CHECK_OUT_OF_MEMORY(p);
    q = (int *)MANAGED_STACK_ADDRESS_BOEHM_GC_MALLOC_ATOMIC(sizeof(int));
    CHECK_OUT_OF_MEMORY(q);
    if (*p != NULL) {
      fprintf(stderr, "MANAGED_STACK_ADDRESS_BOEHM_GC_malloc returned garbage (or NULL)\n");
      exit(1);
    }

    *p = (int *)MANAGED_STACK_ADDRESS_BOEHM_GC_REALLOC(q, 2 * sizeof(int));
    CHECK_OUT_OF_MEMORY(*p);

    if (i % 10 == 0) {
      unsigned long heap_size = (unsigned long)MANAGED_STACK_ADDRESS_BOEHM_GC_get_heap_size();

      if (heap_size != last_heap_size) {
        printf("Heap size: %lu\n", heap_size);
        last_heap_size = heap_size;
      }
    }
  }
  printf("SUCCEEDED\n");
  return 0;
}
