#include "ifx_boost_factory.h"
#include <stdio.h>
#include <math.h>
#include <time.h>
static IFX_BoostFactory bf;
static void run(float *x, int n){ for(int i=0;i<n;i+=64) IFX_BF_Process(&bf,x+i,(n-i)<64?(n-i):64); }
int main(){
  IFX_BF_Init(&bf,48000);
  static float b[48000];
  for(int m=0;m<BF_MODEL_COUNT;m++){
    IFX_BF_SetModel(&bf,m); IFX_BF_SetDrive(&bf,0.7); IFX_BF_SetTone(&bf,0.5);
    if(!IFX_BF_Service(&bf)) printf("service failed\n");
    for(int n=0;n<48000;n++) b[n]=0; run(b,48000);
    double dc=0,mx=0; for(int n=24000;n<48000;n++){dc+=b[n]; if(fabs(b[n])>mx)mx=fabs(b[n]);}
    for(int n=0;n<48000;n++) b[n]=0.1f*sinf(2*M_PI*220*n/48000.0);
    clock_t t0=clock(); run(b,48000); double us=(double)(clock()-t0)/CLOCKS_PER_SEC*1e6/48000;
    double acc=0; for(int n=24000;n<48000;n++) acc+=b[n]*b[n];
    printf("%-12s silence dc %.2e max %.2e | sine rms %.4f (%.1f dBFS) | host %.2f us/sample\n", IFX_BF_ModelName(m), dc/24000, mx, sqrt(acc/24000), 20*log10(sqrt(acc/24000)+1e-12), us);
  }
}
