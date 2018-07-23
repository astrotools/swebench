#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// see swephexp.h
void swe_set_ephe_path(char *path);
void swe_set_jpl_file(char *fname);
double swe_deltat_ex(double jd, int fl, char *serr);
int swe_calc(double jd, int pl, int fl, double *xx, char *serr);
void swe_close(void);
#define SE_SUN 0
#define SE_EARTH 14
#define SE_NPLANETS 23
#define SEFLG_JPLEPH 1
#define SEFLG_SWIEPH 2
#define SEFLG_SPEED 256
#define AS_MAXCH 256 /* used for string declarations, allowing 255 char+\0 */
#define SE_FNAME_DE431 "de431.eph"

void failure() {
  swe_close();
  exit(EXIT_FAILURE);
}

struct calcargs {
  int begin;
  int end;
  int fl;
  bool output;
};

bool calc(const int begin, const int end, int fl, char *err, bool output) {
  swe_set_ephe_path(NULL);
  if ((fl & SEFLG_JPLEPH) != 0) {
    swe_set_jpl_file(SE_FNAME_DE431);
  }
  double jd = begin + .5;
  const double stop = end + .5;
  while (jd < stop) {
    double tt = jd + swe_deltat_ex(jd, fl, err);
    if (err[0] != '\0') {
      return false;
    }
    for (int pl = SE_SUN; pl < SE_NPLANETS; pl++) {
      if (pl == SE_EARTH) {
        continue;
      }
      double xx[6];
      int cfl = swe_calc(tt, pl, fl, xx, err);
      if (cfl < 0) {
        return false;
      }
      if (output) {
        const char *fmt = "%f %02d: %f %f %f %f %f %f\n";
        printf(fmt, tt, pl, xx[0], xx[1], xx[2], xx[3], xx[4], xx[5]);
      }
    }
    jd += 1;
  }
  return true;
}

void *calcthread(void *arg) {
  struct calcargs *args = arg;
  char err[AS_MAXCH];
  if (!calc(args->begin, args->end, args->fl, err, args->output)) {
    fprintf(stderr, "calculation failed (begin=%d, end=%d, err=%s)\n",
            args->begin, args->end, err);
  }
  return NULL;
}

void runcalc(int fl, int threads, bool output) {
  const int begin = 2415020; // + .5 = 1900-01-01 00:00:00 UT
  const int end = 2488069;   // + .5 = 2100-01-01 00:00:00 UT

  if (threads == 1) {
    char err[AS_MAXCH];
    if (!calc(begin, end, fl, err, output)) {
      fprintf(stderr, "calc error: %s", err);
      failure();
    }
    return;
  }

  // Divide work equally across all threads.
  // If work is not fully divisible by the
  // number of threads, keep the remainer
  // separate to distribute over the threads
  // manually.
  int remain = (end - begin) % threads;
  const int work = (end - begin - remain) / threads;

  struct calcargs *args = calloc(threads, sizeof(struct calcargs));
  if (args == NULL) {
    perror("failed to allocate calcargs memory");
    failure();
  }
  args[0].begin = begin;
  args[0].end = begin + work;
  args[0].fl = fl;
  args[0].output = output;
  if (remain > 0) {
    args[0].end += 1;
    remain--;
  }
  for (int i = 1; i < threads; i++) {
    args[i].begin = args[i - 1].end;
    args[i].end = args[i].begin + work;
    args[i].fl = fl;
    args[i].output = output;
    if (remain > 0) {
      args[i].end += 1;
      remain--;
    }
  }

  pthread_t thread[threads];
  for (int i = 0; i < threads; i++) {
    int result = pthread_create(&thread[i], NULL, &calcthread, &args[i]);
    if (result != 0) {
      char *msg;
      sprintf(msg, "failed to start thread %d", i);
      perror(msg);
      failure();
    }
  }
  for (int i = 0; i < threads; i++) {
    int result = pthread_join(thread[i], NULL);
    if (result != 0) {
      errno = result;
      char *msg;
      sprintf(msg, "failed to join thread %d", i);
      perror(msg);
      failure();
    }
  }
  free(args);
}

int main(int argc, char const *argv[]) {
  int fl = SEFLG_SWIEPH | SEFLG_SPEED;
  char randfile[AS_MAXCH];
  int threads = 1;
  bool output = false;
  for (size_t i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-jpl", 4) == 0) {
      fl |= SEFLG_JPLEPH;  // set JPLEPH
      fl &= ~SEFLG_SWIEPH; // remove SWIEPH
    }
    if (strncmp(argv[i], "-threads", 8) == 0) {
      i++;
      if (i == argc) {
        fprintf(stderr, "no number of threads given");
        failure();
      }
      int n = atoi(argv[i]);
      if (n <= 0) {
        fprintf(stderr, "need valid number of threads");
        failure();
      }
      threads = n;
    }
    if (strncmp(argv[i], "-output", 7) == 0) {
      output = true;
    }
  }

  runcalc(fl, threads, output);
  swe_close();
  return EXIT_SUCCESS;
}
