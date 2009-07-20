#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <ddraw.h>

#include "mw2shim.h"

const char NUMBERS[][10][5] = {
  "####", "   #", "####", "####", "#  #", "####", "####", "####", "####", "####",
  "#  #", "   #", "   #", "   #", "#  #", "#   ", "#   ", "   #", "#  #", "#  #",
  "#  #", "   #", "####", "####", "####", "####", "####", "   #", "####", "####",
  "#  #", "   #", "#   ", "   #", "   #", "   #", "#  #", "   #", "#  #", "   #",
  "####", "   #", "####", "####", "   #", "####", "####", "   #", "####", "   #",
};

#define plot8(surface, QX, QY, colour) do { ((unsigned char *)(surface->lpSurface))[(QX) + (QY) * surface->lPitch] = colour; } while(0);

void scaleplot8(LPDDSURFACEDESC surface, int xstart, int ystart, int x, int y, int xscale, int yscale, unsigned char colour) {
  int i, j;
  int sx = xstart + x * xscale, sy = ystart + y * yscale;

  for(i=0;i<xscale;i++)
    for(j=0;j<yscale;j++)
      plot8(surface, sx + i, sy + j, colour);
}

#define P(X, Y) do { scaleplot8(surface, x, y, X, Y, scale, scale, colour); } while(0);
void plotnumbers(int numbers, LPDDSURFACEDESC surface, int x, int y, int scale, unsigned char colour) {
  char buf[20];
  size_t len;
  unsigned int i;

  sprintf_s(buf, sizeof(buf), "%d", numbers);
  len = strlen(buf);

  for(i=0;i<len;i++) {
    char c = buf[i];
    int n = c - '0';
    int j;

    for(j=0;j<5;j++) { /* y loop */
      char const *cells = NUMBERS[j][n];
      int k;

      for(k=0;k<4;k++) /* x loop */
        if(cells[k] == '#')
          P(k + 6 * i, j);
    }
  }
}
#undef P
