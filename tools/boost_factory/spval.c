/* spval MODEL STAGE DRIVE TONE VOICE OSRATE AMP FREQ U0DC
 * writes deck.cir (SPICE) and model.txt (C engine output at OSRATE) */
#include "ifx_boost_factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static bf_elem work[BF_MAX_STAGES][BF_MAX_ELEMS+1];
int main(int argc,char**argv){
  int m=atoi(argv[1]), st=atoi(argv[2]); float drive=atof(argv[3]), tone=atof(argv[4]); int voice=atoi(argv[5]);
  double fs=atof(argv[6]); double amp=atof(argv[7]), f=atof(argv[8]); double u0dc=atof(argv[9]);
  const bf_model_desc *md=&BF_MODELS[m]; bf_elem *w[BF_MAX_STAGES]={0};
  for(int s=0;s<md->nStages;s++){ memcpy(work[s],md->st[s].nl->elems,md->st[s].nl->nElems*sizeof(bf_elem)); memset(&work[s][md->st[s].nl->nElems],0,sizeof(bf_elem)); w[s]=work[s]; }
  float knobs[4]={.5,.5,.5,.5}, g=1; bf_ui ui={drive,tone,voice}; md->map(&ui,knobs,&g,w);
  if(u0dc<-900){ float u=0; for(int k=0;k<st;k++){ bf_netlist n2=*md->st[k].nl; n2.elems=work[k]; double Tk=1.0/(48000.0*(md->st[k].os?md->osFactor:1)); bf_dk_state ss; float y; bf_dk_dc_init(&ss,&n2,Tk,knobs,u,&y); if(md->st[k].railHi>md->st[k].railLo){ if(y>md->st[k].railHi)y=md->st[k].railHi; if(y<md->st[k].railLo)y=md->st[k].railLo;} u=y; } u0dc=u; printf("u0dc=%g\n",u0dc); }
  bf_netlist nl=*md->st[st].nl; nl.elems=work[st];
  FILE *o=fopen("deck.cir","w"); fprintf(o,"* model %d stage %d\n",m,st);
  int ui_=0, xn=100;
  for(int e=0;e<nl.nElems;e++){ const bf_elem*el=&nl.elems[e];
    switch(el->kind){
    case BF_E_R: fprintf(o,"R%d %d %d %.9g\n",e,el->n1,el->n2,el->value); break;
    case BF_E_C: fprintf(o,"C%d %d %d %.9g\n",e,el->n1,el->n2,el->value); break;
    case BF_E_L: fprintf(o,"L%d %d %d %.9g\n",e,el->n1,el->n2,el->value); break;
    case BF_E_V: if(el->aux==1) fprintf(o,"V%d %d %d DC %.9g\n",e,el->n1,el->n2,el->value);
                 else if(el->aux==0) fprintf(o,"V%d %d %d DC %.9g SIN(%.9g %.9g %.9g)\n",e,el->n1,el->n2,u0dc*el->value,u0dc*el->value,amp*el->value,f);
                 else fprintf(o,"V%d %d %d DC 0\n",e,el->n1,el->n2); break;
    case BF_E_POT: { double t=bf_taper(el->aux2,knobs[el->aux]); double r12=el->value*t, r23=el->value*(1-t); if(r12<0.5)r12=0.5; if(r23<0.5)r23=0.5;
        if(el->n1!=el->n2) fprintf(o,"RPa%d %d %d %.9g\n",e,el->n1,el->n2,r12);
        if(el->n2!=el->n3) fprintf(o,"RPb%d %d %d %.9g\n",e,el->n2,el->n3,r23); } break;
    case BF_E_OPAMP: if(el->value<=0) fprintf(o,"E%d %d 0 %d %d 1e5\n",e,el->n3,el->n1,el->n2);
        else { int x=xn++; fprintf(o,"G%d 0 %d %d %d 1\nRX%d %d 0 1e5\nCX%d %d 0 %.9g\nE%d %d 0 %d 0 1\n",e,x,el->n1,el->n2,e,x,e,x,1.0/(2*M_PI*el->value),e,el->n3,x); } break;
    case BF_E_BUF: fprintf(o,"E%d %d 0 %d 0 1\n",e,el->n3,el->n1); break;
    case BF_E_DPAIR: { const bf_diode_model*df=&BF_DIODES[el->aux]; fprintf(o,"D%da %d %d DM%da\n.model DM%da D(IS=%.6g N=%.6g)\n",e,el->n1,el->n2,e,e,df->Is,df->N*el->aux2);
        if(el->n3){ const bf_diode_model*dr=&BF_DIODES[el->n3]; fprintf(o,"D%db %d %d DM%db\n.model DM%db D(IS=%.6g N=%.6g)\n",e,el->n2,el->n1,e,e,dr->Is,dr->N*el->value);} } break;
    case BF_E_BJT: case BF_E_BJTFA: { const bf_bjt_model*q=&BF_BJTS[el->aux];
        fprintf(o,"Q%d %d %d %d QM%d\n.model QM%d %s(IS=%.6g BF=%.6g BR=%.6g NF=%.6g)\n",e,el->n1,el->n2,el->n3,e,e,q->pnp?"PNP":"NPN",q->Is,q->Bf,q->Br,q->Nf); } break;
    }}
  static char nbuf[32]; double T=1/fs; int N=(int)(0.2*fs);
  fprintf(o,".options reltol=1e-5 abstol=1e-12 vntol=1e-7 method=gear\n.control\ntran %.9g 0.2 0 %.9g\nlinearize\nlet outv = v(%d)%s\nwrdata spice.txt outv\n.endc\n.end\n",T,T,nl.outP, nl.outN ? (sprintf(nbuf,"-v(%d)",nl.outN), nbuf) : ""); fclose(o);
  bf_dk dk; bf_dk_state s; bf_dk_build(&dk,&nl,T,knobs); float ydc; bf_dk_dc_init(&s,&nl,T,knobs,(float)u0dc,&ydc);
  FILE *mo=fopen("model.txt","w");
  static float zero1=0; for(int n=0;n<=N;n++){ float x=(float)(u0dc+amp*sin(2*M_PI*f*n*T)); bf_dk_block(&dk,&s,&x,&zero1,1); fprintf(mo,"%.9g %.9g\n",n*T,x); }
  fclose(mo); printf("ydc=%g N=%d P=%d\n",ydc,dk.N,dk.P); return 0; }
