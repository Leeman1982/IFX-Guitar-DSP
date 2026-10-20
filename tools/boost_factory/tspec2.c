#include <stdlib.h>
#include "ifx_boost_factory.h"
#include <stdio.h>
#include <math.h>
static IFX_BoostFactory bf; static float b[96000*2];
int main(int c,char**v){ int m=atoi(v[1]); double f0=atof(v[2]); IFX_BF_Init(&bf,48000); IFX_BF_SetModel(&bf,m); IFX_BF_SetDrive(&bf,atof(v[3])); IFX_BF_SetTone(&bf,m>=6?1:0.7f); IFX_BF_Service(&bf);
 int L=192000; for(int n=0;n<L;n++) b[n]=atof(v[4])*sinf(2*M_PI*f0*n/48000.0); for(int i=0;i<L;i+=64) IFX_BF_Process(&bf,b+i,64);
 FILE*o=fopen("spec.raw","wb"); fwrite(b+96000,4,96000,o); fclose(o); }
