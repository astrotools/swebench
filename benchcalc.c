#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void swe_set_ephe_path(char *path);
double swe_deltat_ex(double jd, int fl, char *serr);
int swe_calc(double jd, int pl, int fl, double *xx, char *serr);
void swe_close(void);

#define SE_SUN          0
#define SE_EARTH        14
#define SE_NPLANETS     23
#define SEFLG_JPLEPH    1
#define SEFLG_SWIEPH    2
#define SEFLG_SPEED	256
#define AS_MAXCH 256    /* used for string declarations, allowing 255 char+\0 */

int main(int argc, char const *argv[]) {
  int fl = SEFLG_SWIEPH;

  for (size_t i = 1; i < argc; i++) {
    if (strncmp(argv[i], "-jpl", 4) == 0) {
      fl = SEFLG_JPLEPH;
    }
  }

  fl |= SEFLG_SPEED;
  swe_set_ephe_path(NULL);

  double jd = 2451544.5; // 2000-01-01 00:00:00 UT
  #define end 2488069.5  // 2100-01-01 00:00:00 UT

  while (jd < end) {
    char err[AS_MAXCH] = {'\0'};

    double tt = jd + swe_deltat_ex(jd, fl, err);
    if (err[0] != '\0') {
      printf("swe_deltat_ex: %s\n", err);
      swe_close();
      return EXIT_FAILURE;
    }

    for (int pl = SE_SUN; pl < SE_NPLANETS; pl++) {
      if (pl == SE_EARTH) {
        continue;
      }

      double xx[6];
      int cfl = swe_calc(tt, pl, fl, xx, err);
      if (cfl < 0) {
        printf("swe_calc: %s\n", err);
        swe_close();
        return EXIT_FAILURE;
      }
    }

    jd += 1;
  }

  swe_close();
  return EXIT_SUCCESS;
}
