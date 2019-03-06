#include <unistd.h>
#include <stdlib.h>
#include <thread.h>

/* don't create more threads for less than this */
#define SLICE_THRESH   4096

/* how many threads per lwp */
#define THR_PER_LWP       4

/* cast the void to a one byte quanitity and compute the offset */
#define SUB(a, n)      ((void *) (((unsigned char *) (a)) + ((n) * width)))

typedef struct {
  void    *sa_base;
  int      sa_nel;
  size_t   sa_width;
  int    (*sa_compar)(const void *, const void *);
} sort_args_t;

/* for all instances of quicksort */
static int threads_avail;

#define SWAP(a, i, j, width) \
{ \
  int n; \
  unsigned char uc; \
  unsigned short us; \
  unsigned long ul; \
  unsigned long long ull; \
 \
  if (SUB(a, i) == pivot) \
    pivot = SUB(a, j); \
  else if (SUB(a, j) == pivot) \
    pivot = SUB(a, i); \
 \
  /* one of the more convoluted swaps I've done */ \
  switch(width) { \
  case 1: \
    uc = *((unsigned char *) SUB(a, i)); \
    *((unsigned char *) SUB(a, i)) = *((unsigned char *) SUB(a, j)); \
    *((unsigned char *) SUB(a, j)) = uc; \
    break; \
  case 2: \
    us = *((unsigned short *) SUB(a, i)); \
    *((unsigned short *) SUB(a, i)) = *((unsigned short *) SUB(a, j)); \
    *((unsigned short *) SUB(a, j)) = us; \
    break; \
  case 4: \
    ul = *((unsigned long *) SUB(a, i)); \
    *((unsigned long *) SUB(a, i)) = *((unsigned long *) SUB(a, j)); \
    *((unsigned long *) SUB(a, j)) = ul; \
    break; \
  case 8: \
    ull = *((unsigned long long *) SUB(a, i)); \
    *((unsigned long long *) SUB(a,i)) = *((unsigned long long *) SUB(a,j)); \
    *((unsigned long long *) SUB(a, j)) = ull; \
    break; \
  default: \
    for(n=0; n<width; n++) { \
      uc = ((unsigned char *) SUB(a, i))[n]; \
      ((unsigned char *) SUB(a, i))[n] = ((unsigned char *) SUB(a, j))[n]; \
      ((unsigned char *) SUB(a, j))[n] = uc; \
    } \
    break; \
  } \
}

static void *
_quicksort(void *arg)
{
  sort_args_t *sargs = (sort_args_t *) arg;
  register void *a = sargs->sa_base;
  int n = sargs->sa_nel;
  int width = sargs->sa_width;
  int (*compar)(const void *, const void *) = sargs->sa_compar;
  register int i;
  register int j;
  int z;
  int thread_count = 0;
  void *t;
  void *b[3];
  void *pivot = 0;
  sort_args_t sort_args[2];
  thread_t tid;

  /* find the pivot point */
  switch(n) {
  case 0:
  case 1:
    return 0;
  case 2:
    if ((*compar)(SUB(a, 0), SUB(a, 1)) > 0) {
      SWAP(a, 0, 1, width);
    }
    return 0;
  case 3:
    /* three sort */
    if ((*compar)(SUB(a, 0), SUB(a, 1)) > 0) {
      SWAP(a, 0, 1, width);
    }
    /* the first two are now ordered, now order the second two */
    if ((*compar)(SUB(a, 2), SUB(a, 1)) < 0) {
      SWAP(a, 2, 1, width);
    }
    /* should the second be moved to the first? */
    if ((*compar)(SUB(a, 1), SUB(a, 0)) < 0) {
      SWAP(a, 1, 0, width);
    }
    return 0;
  default:
    if (n > 3) {
      b[0] = SUB(a, 0);
      b[1] = SUB(a, n / 2);
      b[2] = SUB(a, n - 1);
      /* three sort */
      if ((*compar)(b[0], b[1]) > 0) {
        t = b[0];
        b[0] = b[1];
        b[1] = t;
      }
      /* the first two are now ordered, now order the second two */
      if ((*compar)(b[2], b[1]) < 0) {
        t = b[1];
        b[1] = b[2];
        b[2] = t;
      }
      /* should the second be moved to the first? */
      if ((*compar)(b[1], b[0]) < 0) {
        t = b[0];
        b[0] = b[1];
        b[1] = t;
      }
      if ((*compar)(b[0], b[2]) != 0)
        if ((*compar)(b[0], b[1]) < 0)
          pivot = b[1];
        else
          pivot = b[2];
    }
    break;
  }
  if (pivot == 0)
    for(i=1; i<n; i++) {
      if (z = (*compar)(SUB(a, 0), SUB(a, i))) {
        pivot = (z > 0) ? SUB(a, 0) : SUB(a, i);
        break;
      }
    }
  if (pivot == 0)
    return;

  /* sort */
  i = 0;
  j = n - 1;
  while(i <= j) {
    while((*compar)(SUB(a, i), pivot) < 0)
      ++i;
    while((*compar)(SUB(a, j), pivot) >= 0)
      --j;
    if (i < j) {
      SWAP(a, i, j, width);
      ++i;
      --j;
    }
  }

  /* sort the sides judiciously */
  switch(i) {
  case 0:
  case 1:
    break;
  case 2:
    if ((*compar)(SUB(a, 0), SUB(a, 1)) > 0) {
      SWAP(a, 0, 1, width);
    }
    break;
  case 3:
    /* three sort */
    if ((*compar)(SUB(a, 0), SUB(a, 1)) > 0) {
      SWAP(a, 0, 1, width);
    }
    /* the first two are now ordered, now order the second two */
    if ((*compar)(SUB(a, 2), SUB(a, 1)) < 0) {
      SWAP(a, 2, 1, width);
    }
    /* should the second be moved to the first? */
    if ((*compar)(SUB(a, 1), SUB(a, 0)) < 0) {
      SWAP(a, 1, 0, width);
    }
    break;
  default:
    sort_args[0].sa_base          = a;
    sort_args[0].sa_nel           = i;
    sort_args[0].sa_width         = width;
    sort_args[0].sa_compar        = compar;
    if ((threads_avail > 0) && (i > SLICE_THRESH)) {
      threads_avail--;
      thr_create(0, 0, _quicksort, &sort_args[0], 0, &tid);
      thread_count = 1;
    } else
      _quicksort(&sort_args[0]);
    break;
  }
  j = n - i;
  switch(j) {
  case 1:
    break;
  case 2:
    if ((*compar)(SUB(a, i), SUB(a, i + 1)) > 0) {
      SWAP(a, i, i + 1, width);
    }
    break;
  case 3:
    /* three sort */
    if ((*compar)(SUB(a, i), SUB(a, i + 1)) > 0) {
      SWAP(a, i, i + 1, width);
    }
    /* the first two are now ordered, now order the second two */
    if ((*compar)(SUB(a, i + 2), SUB(a, i + 1)) < 0) {
      SWAP(a, i + 2, i + 1, width);
    }
    /* should the second be moved to the first? */
    if ((*compar)(SUB(a, i + 1), SUB(a, i)) < 0) {
      SWAP(a, i + 1, i, width);
    }
    break;
  default:
    sort_args[1].sa_base          = SUB(a, i);
    sort_args[1].sa_nel           = j;
    sort_args[1].sa_width         = width;
    sort_args[1].sa_compar        = compar;
    if ((thread_count == 0) && (threads_avail > 0) && (i > SLICE_THRESH)) {
      threads_avail--;
      thr_create(0, 0, _quicksort, &sort_args[1], 0, &tid);
      thread_count = 1;
    } else
      _quicksort(&sort_args[1]);
    break;
  }
  if (thread_count) {
    thr_join(tid, 0, 0);
    threads_avail++;
  }
  return 0;
}

void
quicksort(void *a, size_t n, size_t width,
          int (*compar)(const void *, const void *))
{
  static int ncpus = -1;
  sort_args_t sort_args;

  if (ncpus == -1) {
    ncpus = sysconf( _SC_NPROCESSORS_ONLN);

    /* lwp for each cpu */
    if ((ncpus > 1) && (thr_getconcurrency() < ncpus))
      thr_setconcurrency(ncpus);

    /* thread count not to exceed THR_PER_LWP per lwp */
    threads_avail = (ncpus == 1) ? 0 : (ncpus * THR_PER_LWP);
  }
  sort_args.sa_base = a;
  sort_args.sa_nel = n;
  sort_args.sa_width = width;
  sort_args.sa_compar = compar;
  (void) _quicksort(&sort_args);
}