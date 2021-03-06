
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "ViennaRNA/utils.h"
#include "ViennaRNA/fold_vars.h"
#include "ViennaRNA/energy_par.h"
#include "ViennaRNA/constraints.h"
#include "ViennaRNA/exterior_loops.h"
#include "ViennaRNA/gquad.h"
#include "ViennaRNA/multibranch_loops.h"

#ifdef ON_SAME_STRAND
#undef ON_SAME_STRAND
#endif

#define ON_SAME_STRAND(I,J,C)  (((I)>=(C))||((J)<(C)))


PRIVATE int
E_mb_loop_fast( vrna_fold_compound_t *vc,
                int i,
                int j,
                int *dmli1,
                int *dmli2);

PRIVATE int
E_mb_loop_fast_comparative( vrna_fold_compound_t *vc,
                            int i,
                            int j,
                            int *dmli1,
                            int *dmli2);

PRIVATE FLT_OR_DBL
exp_E_mb_loop_fast( vrna_fold_compound_t *vc,
                    int i,
                    int j,
                    FLT_OR_DBL *qqm1);

PRIVATE FLT_OR_DBL
exp_E_mb_loop_fast_comparative( vrna_fold_compound_t *vc,
                                int i,
                                int j,
                                FLT_OR_DBL *qqm1);

PRIVATE int
E_ml_stems_fast(vrna_fold_compound_t *vc,
                int i,
                int j,
                int *fmi,
                int *dmli);

PRIVATE int
E_ml_stems_fast_comparative(vrna_fold_compound_t *vc,
                            int i,
                            int j,
                            int *fmi,
                            int *dmli);

/*
#################################
# BEGIN OF FUNCTION DEFINITIONS #
#################################
*/

PUBLIC int
vrna_E_mb_loop_fast(vrna_fold_compound_t *vc,
                    int i,
                    int j,
                    int *dmli1,
                    int *dmli2){

  int e = INF;

  if(vc){
    switch(vc->type){
      case VRNA_VC_TYPE_SINGLE:
        e = E_mb_loop_fast(vc, i, j, dmli1, dmli2);
        break;

      case VRNA_VC_TYPE_ALIGNMENT:
        e = E_mb_loop_fast_comparative(vc, i, j, dmli1, dmli2);
        break;
    }
  }

  return e;
}


PUBLIC int
vrna_E_ml_stems_fast( vrna_fold_compound_t *vc,
                      int i,
                      int j,
                      int *fmi,
                      int *dmli){

  int e = INF;

  if(vc){
    switch(vc->type){
      case VRNA_VC_TYPE_SINGLE:
        e = E_ml_stems_fast(vc, i, j, fmi, dmli);
        break;

      case VRNA_VC_TYPE_ALIGNMENT:
        e = E_ml_stems_fast_comparative(vc, i, j, fmi, dmli);
        break;
    }
  }

  return e;
}


PUBLIC FLT_OR_DBL
vrna_exp_E_mb_loop_fast( vrna_fold_compound_t *vc,
                    int i,
                    int j,
                    FLT_OR_DBL *qqm1){

  FLT_OR_DBL q = 0.;

  if(vc){
    switch(vc->type){
      case VRNA_VC_TYPE_SINGLE:
        q = exp_E_mb_loop_fast(vc, i, j, qqm1);
        break;

      case VRNA_VC_TYPE_ALIGNMENT:
        q = exp_E_mb_loop_fast_comparative(vc, i, j, qqm1);
        break;
    }
  }

  return q;
}


PRIVATE int
E_mb_loop_fast_comparative( vrna_fold_compound_t *vc,
                            int i,
                            int j,
                            int *dmli1,
                            int *dmli2){

  char          *hard_constraints;
  short         **S, **S5, **S3;
  int           ij, *indx, e, decomp, s, n_seq, dangle_model, *type;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_sc_t     **scs;

  n_seq             = vc->n_seq;
  indx              = vc->jindx;
  hard_constraints  = vc->hc->matrix;
  P                 = vc->params;
  md                = &(P->model_details);
  scs               = vc->scs;
  dangle_model      = md->dangles;
  ij                = indx[j] + i;
  e                 = INF;

  /* multi-loop decomposition ------------------------*/
  if(hard_constraints[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){
    decomp = dmli1[j-1];

    type  = (int *)vrna_alloc(n_seq * sizeof(int));
    S     = vc->S;
    S5    = vc->S5;     /*S5[s][i] holds next base 5' of i in sequence s*/
    S3    = vc->S3;     /*Sl[s][i] holds next base 3' of i in sequence s*/

    for(s = 0; s < n_seq; s++){
      type[s] = md->pair[S[s][j]][S[s][i]];
      if(type[s] == 0)
        type[s] = 7;
    }

    if(dangle_model){
      for(s = 0; s < n_seq; s++){
        decomp += E_MLstem(type[s], S5[s][j], S3[s][i], P);
      }
    }
    else{
      for(s = 0; s < n_seq; s++){
        decomp += E_MLstem(type[s], -1, -1, P);
      }
    }
    if(scs)
      for(s = 0; s < n_seq; s++){
        if(scs[s]){
          if(scs[s]->energy_bp)
            decomp += scs[s]->energy_bp[indx[j] + i];
        }
      }

    free(type);

    e = decomp + n_seq * P->MLclosing;
  }

  return e;
}


PRIVATE int
E_mb_loop_fast( vrna_fold_compound_t *vc,
                int i,
                int j,
                int *dmli1,
                int *dmli2){

  unsigned char type, tt;
  char          *ptype, *hc, eval_loop, el;
  short         S_i1, S_j1, *S;
  int           decomp, en, e, cp, *indx, *hc_up, *fc, ij, hc_decompose,
                dangle_model, *rtype;
  vrna_sc_t     *sc;
  vrna_param_t  *P;
#ifdef WITH_GEN_HC
  vrna_callback_hc_evaluate *f;
#endif

  cp            = vc->cutpoint;
  ptype         = vc->ptype;
  S             = vc->sequence_encoding;
  indx          = vc->jindx;
  hc            = vc->hc->matrix;
  hc_up         = vc->hc->up_ml;
  sc            = vc->sc;
  fc            = vc->matrices->fc;
  P             = vc->params;
  ij            = indx[j] + i;
  hc_decompose  = hc[ij];
  dangle_model  = P->model_details.dangles;
  rtype         = &(P->model_details.rtype[0]);
  type          = (unsigned char)ptype[ij];
  /* init values */
  e             = INF;
  decomp        = INF;

#ifdef WITH_GEN_HC
  f  = vc->hc->f;
#endif

  if(cp < 0){
    S_i1    = S[i+1];
    S_j1    = S[j-1];
  } else {
    S_i1  = ON_SAME_STRAND(i, i + 1, cp) ? S[i+1] : -1;
    S_j1  = ON_SAME_STRAND(j - 1, j, cp) ? S[j-1] : -1;
  }

  if((S_i1 >= 0) && (S_j1 >= 0)){ /* regular multi branch loop */

    eval_loop = hc_decompose & VRNA_CONSTRAINT_CONTEXT_MB_LOOP;
    el        = eval_loop;

#ifdef WITH_GEN_HC
    if(f)
      el = (f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

    if(el){
      decomp = dmli1[j-1];
      tt = rtype[type];

      if(tt == 0)
        tt = 7;

      if(decomp != INF){
        switch(dangle_model){
          /* no dangles */
          case 0:   decomp += E_MLstem(tt, -1, -1, P);
                    if(sc){
                      if(sc->energy_bp)
                        decomp += sc->energy_bp[ij];

                      if(sc->f)
                        decomp += sc->f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, sc->data);
                    }
                    break;

          /* double dangles */
          case 2:   decomp += E_MLstem(tt, S_j1, S_i1, P);
                    if(sc){
                      if(sc->energy_bp)
                        decomp += sc->energy_bp[ij];

                      if(sc->f)
                        decomp += sc->f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, sc->data);
                    }
                    break;

          /* normal dangles, aka dangles = 1 || 3 */
          default:  decomp += E_MLstem(tt, -1, -1, P);
                    if(sc){
                      if(sc->energy_bp)
                        decomp += sc->energy_bp[ij];

                      if(sc->f)
                        decomp += sc->f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, sc->data);
                    }
                    break;
        }
      }
    }

    if(dangle_model % 2){  /* dangles == 1 || dangles == 3 */

      el = eval_loop;
      el = (hc_up[i + 1] > 0) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+2, j-1, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if(dmli2[j-1] != INF){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en = dmli2[j-1] + E_MLstem(tt, -1, S_i1, P) + P->MLbase;
          if(sc){
            if(sc->energy_up)
              en += sc->energy_up[i+1][1];

            if(sc->energy_bp)
              en += sc->energy_bp[ij];

            if(sc->f)
              en += sc->f(i, j, i+2, j-1, VRNA_DECOMP_PAIR_ML, sc->data);
          }
          decomp = MIN2(decomp, en);
        }
      }

      el = eval_loop;
      el = ((hc_up[i + 1] > 0) && (hc_up[j-1] > 0)) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+2, j-2, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if(dmli2[j-2] != INF){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en = dmli2[j-2] + E_MLstem(tt, S_j1, S_i1, P) + 2*P->MLbase;
          if(sc){
            if(sc->energy_up)
              en += sc->energy_up[i+1][1]
                    + sc->energy_up[j-1][1];

            if(sc->energy_bp)
              en += sc->energy_bp[ij];

            if(sc->f)
              en += sc->f(i, j, i+2, j-2, VRNA_DECOMP_PAIR_ML, sc->data);
          }
          decomp = MIN2(decomp, en);
        }
      }

      el = eval_loop;
      el = (hc_up[j-1] > 0) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+1, j-2, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if(dmli1[j-2] != INF){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en = dmli1[j-2] + E_MLstem(tt, S_j1, -1, P) + P->MLbase;
          if(sc){
            if(sc->energy_up)
              en += sc->energy_up[j-1][1];

            if(sc->energy_bp)
              en += sc->energy_bp[ij];

            if(sc->f)
              en += sc->f(i, j, i+1, j-2, VRNA_DECOMP_PAIR_ML, sc->data);
          }
          decomp = MIN2(decomp, en);
        }
      }

    } /* end if dangles % 2 */

    if(decomp != INF)
      e = decomp + P->MLclosing;

  } /* end regular multibranch loop */

  if(!ON_SAME_STRAND(i, j, cp)){ /* multibrach like cofold structure with cut somewhere between i and j */

    eval_loop = hc_decompose & VRNA_CONSTRAINT_CONTEXT_MB_LOOP;
    el        = eval_loop;

#ifdef WITH_GEN_HC
    if(f)
      el = (f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

    if(el){
      if((fc[i+1] != INF) && (fc[j-1] != INF)){
        decomp = fc[i+1] + fc[j-1];
        tt = rtype[type];

        if(tt == 0)
          tt = 7;

        switch(dangle_model){
          case 0:   decomp += E_ExtLoop(tt, -1, -1, P);
                    break;
          case 2:   decomp += E_ExtLoop(tt, S_j1, S_i1, P);
                    break;
          default:  decomp += E_ExtLoop(tt, -1, -1, P);
                    break;
        }
      }
    }

    if(dangle_model % 2){ /* dangles == 1 || dangles == 3 */
      el  = eval_loop;
      el  = (hc_up[i+1] > 0) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+2, j-1, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if((fc[i+2] != INF) && (fc[j-1] != INF)){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en     = fc[i+2] + fc[j-1] + E_ExtLoop(tt, -1, S_i1, P);
          decomp = MIN2(decomp, en);
        }
      }

      el  = eval_loop;
      el  = (hc_up[j-1] > 0) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+1, j-2, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if((fc[i+1] != INF) && (fc[j-2] != INF)){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en     = fc[i+1] + fc[j-2] + E_ExtLoop(tt, S_j1, -1, P);
          decomp = MIN2(decomp, en);
        }
      }

      el  = eval_loop;
      el  = ((hc_up[i+1] > 0) && (hc_up[j-1] > 0)) ? el : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+2, j-2, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        if((fc[i+2] != INF) && (fc[j-2] != INF)){
          tt = rtype[type];

          if(tt == 0)
            tt = 7;

          en     = fc[i+2] + fc[j-2] + E_ExtLoop(tt, S_j1, S_i1, P);
          decomp = MIN2(decomp, en);
        }
      }
    }

    e = MIN2(e, decomp);
  }
  return e;
}

PUBLIC int
E_mb_loop_stack(int i,
                int j,
                vrna_fold_compound_t *vc){

  unsigned char type, type_2;
  char          eval_loop, el, *hc, *ptype;
  int           e, decomp, en, i1k, k1j1, ij, k, *indx, *c, *fML, turn, *rtype;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_sc_t     *sc;
#ifdef WITH_GEN_HC
  vrna_callback_hc_evaluate *f;
#endif

  indx  = vc->jindx;
  hc    = vc->hc->matrix;
  c     = vc->matrices->c;
  fML   = vc->matrices->fML;
  P     = vc->params;
  md    = &(P->model_details);
  turn  = md->min_loop_size;
  ptype = vc->ptype;
  rtype = &(md->rtype[0]);
  sc    = vc->sc;
  e     = INF;
  ij    = indx[j] + i;
  type  = ptype[ij];

#ifdef WITH_GEN_HC
  f = vc->hc->f;
#endif

  eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP;

#ifdef WITH_GEN_HC
  if(f)
    eval_loop = (f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, vc->hc->data)) ? eval_loop : (char)0;
#endif

  if(eval_loop){
    if(type == 0)
      type = 7;

    decomp = INF;
    k1j1  = indx[j-1] + i + 2 + turn + 1;
    for (k = i+2+turn; k < j-2-turn; k++, k1j1++){
      i1k   = indx[k] + i + 1;

      /* honour generalized hard constraint */
      el    = hc[i1k] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, i+1, k, VRNA_DECOMP_ML_COAXIAL, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        type_2  = rtype[(unsigned char)ptype[i1k]];

        if(type_2 == 0)
          type_2 = 7;

        en      = c[i1k]+P->stack[type][type_2]+fML[k1j1];
        if(sc){
          if(sc->f)
            en += sc->f(i, j, i+1, k, VRNA_DECOMP_ML_COAXIAL, sc->data);
        }
        decomp  = MIN2(decomp, en);
      }

      el    = hc[k1j1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;

#ifdef WITH_GEN_HC
      if(f)
        el = (f(i, j, k+1, j-1, VRNA_DECOMP_ML_COAXIAL, vc->hc->data)) ? el : (char)0;
#endif

      if(el){
        type_2  = rtype[(unsigned char)ptype[k1j1]];

        if(type_2 == 0)
          type_2 = 7;

        en      = c[k1j1]+P->stack[type][type_2]+fML[i1k];
        if(sc){
          if(sc->f)
            en += sc->f(i, j, k+1, j-1, VRNA_DECOMP_ML_COAXIAL, sc->data);
        }
        decomp  = MIN2(decomp, en);
      }
    }
    /* no TermAU penalty if coax stack */
    decomp += 2*P->MLintern[1] + P->MLclosing;
    if(sc){
      if(sc->energy_bp)
        decomp += sc->energy_bp[ij];
      if(sc->f)
        decomp += sc->f(i, j, i+1, j-1, VRNA_DECOMP_PAIR_ML, sc->data);
    }
    e = decomp;

  }
  return e;
}

PUBLIC int
E_ml_rightmost_stem(int i,
                    int j,
                    vrna_fold_compound_t *vc){

  char              eval_loop, *hc;
  short             *S;
  int               en, length, *indx, *hc_up, *c, *fm, *ggg, ij, type, hc_decompose,
                    dangle_model, with_gquad, cp, e;
  vrna_param_t      *P;
  vrna_sc_t         *sc;
#ifdef WITH_GEN_HC
  vrna_callback_hc_evaluate *f;
#endif

  P             = vc->params;
  length        = vc->length;
  S             = vc->sequence_encoding;
  indx          = vc->jindx;
  hc            = vc->hc->matrix;
  hc_up         = vc->hc->up_ml;
  sc            = vc->sc;
  c             = vc->matrices->c;
  fm            = (P->model_details.uniq_ML) ? vc->matrices->fM1 : vc->matrices->fML;
  ggg           = vc->matrices->ggg;
  ij            = indx[j] + i;
  type          = vc->ptype[ij];
  hc_decompose  = hc[ij];
  dangle_model  = P->model_details.dangles;
  with_gquad    = P->model_details.gquad;
  cp            = vc->cutpoint;
  e             = INF;

#ifdef WITH_GEN_HC
  f = vc->hc->f;
#endif

  if(ON_SAME_STRAND(i - 1, i, cp)){
    if(ON_SAME_STRAND(j, j + 1, cp)){
      eval_loop = hc_decompose & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;

#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(i, j, i, j, VRNA_DECOMP_ML_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif

      if(eval_loop){

        if(type == 0)
          type = 7;

        e = c[ij];
        if(e != INF){
          switch(dangle_model){
            case 2:   e += E_MLstem(type, (i==1) ? S[length] : S[i-1], S[j+1], P);
                      break;
            default:  e += E_MLstem(type, -1, -1, P);
                      break;
          }
          if(sc){
            if(sc->f)
              e += sc->f(i, j, i, j, VRNA_DECOMP_ML_STEM, sc->data);
          }
        }
      }

      if(with_gquad)
        if(ON_SAME_STRAND(i, j, cp)){
          en  = ggg[ij] + E_MLstem(0, -1, -1, P);
          e   = MIN2(e, en);
        }

    }

    if(ON_SAME_STRAND(j - 1, j, cp)){
      eval_loop = (hc_up[j] > 0) ? (char)1 : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(i, j, i, j-1, VRNA_DECOMP_ML_ML, vc->hc->data)) ? eval_loop : (char)0;
#endif

      if(eval_loop){
        if(fm[indx[j - 1] + i] != INF){
          en = fm[indx[j - 1] + i] + P->MLbase;
          if(sc){
            if(sc->energy_up)
              en += sc->energy_up[j][1];
            if(sc->f)
              en += sc->f(i, j, i, j-1, VRNA_DECOMP_ML_ML, sc->data);
          }
          e = MIN2(e, en);
        }
      }
    }
  }
  return e;
}

PRIVATE int
E_ml_stems_fast(vrna_fold_compound_t *vc,
                int i,
                int j,
                int *fmi,
                int *dmli){

  char          *ptype, *hc, eval_loop;
  short         *S;
  int           k, en, decomp, mm5, mm3, type_2, k1j, stop, length, *indx, *hc_up,
                *c, *fm, ij, dangle_model, turn, type, *rtype, circular, cp, e;
  vrna_sc_t     *sc;
  vrna_param_t  *P;
#ifdef WITH_GEN_HC
  vrna_callback_hc_evaluate *f;
#endif

  length        = (int)vc->length;
  ptype         = vc->ptype;
  S             = vc->sequence_encoding;
  indx          = vc->jindx;
  hc            = vc->hc->matrix;
  hc_up         = vc->hc->up_ml;
  sc            = vc->sc;
  c             = vc->matrices->c;
  fm            = vc->matrices->fML;
  P             = vc->params;
  ij            = indx[j] + i;
  dangle_model  = P->model_details.dangles;
  turn          = P->model_details.min_loop_size;
  type          = ptype[ij];
  rtype         = &(P->model_details.rtype[0]);
  circular      = P->model_details.circ;
  cp            = vc->cutpoint;
  e             = INF;

#ifdef WITH_GEN_HC
  f = vc->hc->f;
#endif

  /*  extension with one unpaired nucleotide at the right (3' site)
      or full branch of (i,j)
  */
  e = E_ml_rightmost_stem(i,j,vc);

  /*  extension with one unpaired nucleotide at 5' site
      and all other variants which are needed for odd
      dangle models
  */
  if(ON_SAME_STRAND(i - 1, i, cp)){

    if(ON_SAME_STRAND(i, i + 1, cp)){
      eval_loop = (hc_up[i] > 0) ? (char)1 : (char)0;

#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(i, j, i+1, j, VRNA_DECOMP_ML_ML, vc->hc->data)) ? eval_loop : (char)0;
#endif

      if(eval_loop){
        if(fm[ij + 1] != INF){
          en = fm[ij + 1] + P->MLbase;
          if(sc){
            if(sc->energy_up)
              en += sc->energy_up[i][1];
            if(sc->f)
              en += sc->f(i, j, i+1, j, VRNA_DECOMP_ML_ML, sc->data);
          }
          e = MIN2(e, en);
        }

      }
    }

    if(dangle_model % 2){ /* dangle_model = 1 || 3 */

      mm5 = ((i>1) || circular) ? S[i] : -1;
      mm3 = ((j<length) || circular) ? S[j] : -1;

      if(ON_SAME_STRAND(i, i + 1, cp)){
        eval_loop = hc[ij+1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;
        eval_loop = (hc_up[i] > 0) ? eval_loop : (char)0;

#ifdef WITH_GEN_HC
        if(f)
          eval_loop = (f(i, j, i+1, j, VRNA_DECOMP_ML_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif

        if(eval_loop){
          if(c[ij+1] != INF){
            type = ptype[ij+1];

            if(type == 0)
              type = 7;

            en = c[ij+1] + E_MLstem(type, mm5, -1, P) + P->MLbase;
            if(sc){
              if(sc->energy_up)
                en += sc->energy_up[i][1];
              if(sc->f)
                en += sc->f(i, j, i+1, j, VRNA_DECOMP_ML_STEM, sc->data);
            }
            e = MIN2(e, en);
          }
        }
      }

      if(ON_SAME_STRAND(j - 1, j, cp)){
        eval_loop = hc[indx[j-1]+i] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;
        eval_loop = (hc_up[j] > 0) ? eval_loop : (char)0;

#ifdef WITH_GEN_HC
        if(f)
          eval_loop = (f(i, j, i, j-1, VRNA_DECOMP_ML_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif

        if(eval_loop){
          if(c[indx[j-1]+i] != INF){
            type = ptype[indx[j-1]+i];

            if(type == 0)
              type = 7;

            en = c[indx[j-1]+i] + E_MLstem(type, -1, mm3, P) + P->MLbase;
            if(sc){
              if(sc->energy_up)
                en += sc->energy_up[j][1];
              if(sc->f)
                en += sc->f(i, j, i, j-1, VRNA_DECOMP_ML_STEM, sc->data);
            }
            e = MIN2(e, en);
          }
        }
      }

      if(ON_SAME_STRAND(j - 1, j, cp) && ON_SAME_STRAND(i, i + 1, cp)){
        eval_loop = hc[indx[j-1]+i+1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC;
        eval_loop = (hc_up[i] && hc_up[j]) ? eval_loop : (char)0;

#ifdef WITH_GEN_HC
        if(f)
          eval_loop = (f(i, j, i+1, j-1, VRNA_DECOMP_ML_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif

        if(eval_loop){
          if(c[indx[j-1]+i+1] != INF){
            type = ptype[indx[j-1]+i+1];

            if(type == 0)
              type = 7;

            en = c[indx[j-1]+i+1] + E_MLstem(type, mm5, mm3, P) + 2*P->MLbase;
            if(sc){
              if(sc->energy_up)
                en += sc->energy_up[j][1] + sc->energy_up[i][1];
              if(sc->f)
                en += sc->f(i, j, i+1, j-1, VRNA_DECOMP_ML_STEM, sc->data);
            }
            e = MIN2(e, en);
          }
        }
      }
    } /* end special cases for dangles == 1 || dangles == 3 */
  }

  /* modular decomposition -------------------------------*/
  k1j   = indx[j] + i + turn + 2;
  stop  = (cp > 0) ? (cp - 1) : (j - 2 - turn);

  /* duplicated code is faster than conditions in loop */
  if(sc && sc->f){
    for (decomp = INF, k = i + 1 + turn; k <= stop; k++, k1j++){
      if((fmi[k] != INF ) && (fm[k1j] != INF)){
        en = fmi[k] + fm[k1j];
        en += sc->f(i, j, k, k+1, VRNA_DECOMP_ML_ML_ML, sc->data);
        decomp = MIN2(decomp, en);
      }
    }
    k++; k1j++;
    for (;k <= j - 2 - turn; k++, k1j++){
      if((fmi[k] != INF) && (fm[k1j] != INF)){
        en = fmi[k] + fm[k1j];
        en += sc->f(i, j, k, k+1, VRNA_DECOMP_ML_ML_ML, sc->data);
        decomp = MIN2(decomp, en);
      }
    }
  } else {
    for (decomp = INF, k = i + 1 + turn; k <= stop; k++, k1j++){
      if((fmi[k] != INF ) && (fm[k1j] != INF)){
        en = fmi[k] + fm[k1j];
        decomp = MIN2(decomp, en);
      }
    }
    k++; k1j++;
    for (;k <= j - 2 - turn; k++, k1j++){
      if((fmi[k] != INF) && (fm[k1j] != INF)){
        en = fmi[k] + fm[k1j];
        decomp = MIN2(decomp, en);
      }
    }
  }


  dmli[j] = decomp;               /* store for use in fast ML decompositon */
  e = MIN2(e, decomp);

  /* coaxial stacking */
  if (dangle_model==3) {
    /* additional ML decomposition as two coaxially stacked helices */
    int ik;
    k1j = indx[j]+i+turn+2;
    for (decomp = INF, k = i + 1 + turn; k <= stop; k++, k1j++){
      ik = indx[k]+i;
      if((hc[ik] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC) && (hc[k1j] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC)){
        type    = rtype[(unsigned char)ptype[ik]];
        type_2  = rtype[(unsigned char)ptype[k1j]];

        if(type == 0)
          type = 7;
        if(type_2 == 0)
          type_2 = 7;

        en      = c[ik] + c[k1j] + P->stack[type][type_2];
        if(sc){
          if(sc->f)
            en += sc->f(i, k, k+1, j, VRNA_DECOMP_ML_COAXIAL, sc->data);
        }
        decomp  = MIN2(decomp, en);
      }
    }
    k++; k1j++;
    for (; k <= j-2-turn; k++, k1j++){
      ik = indx[k]+i;
      if((hc[ik] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC) && (hc[k1j] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC)){
        type    = rtype[(unsigned char)ptype[ik]];
        type_2  = rtype[(unsigned char)ptype[k1j]];

        if(type == 0)
          type = 7;
        if(type_2 == 0)
          type_2 = 7;

        en      = c[ik] + c[k1j] + P->stack[type][type_2];
        if(sc){
          if(sc->f)
            en += sc->f(i, k, k+1, j, VRNA_DECOMP_ML_COAXIAL, sc->data);
        }
        decomp  = MIN2(decomp, en);
      }
    }

    decomp += 2*P->MLintern[1];        /* no TermAU penalty if coax stack */
#if 0
        /* This is needed for Y shaped ML loops with coax stacking of
           interior pairts, but backtracking will fail if activated */
        DMLi[j] = MIN2(DMLi[j], decomp);
        DMLi[j] = MIN2(DMLi[j], DMLi[j-1]+P->MLbase);
        DMLi[j] = MIN2(DMLi[j], DMLi1[j]+P->MLbase);
        new_fML = MIN2(new_fML, DMLi[j]);
#endif
    e = MIN2(e, decomp);
  }

  fmi[j] = e;

  return e;
}


PRIVATE int
E_ml_stems_fast_comparative(vrna_fold_compound_t *vc,
                            int i,
                            int j,
                            int *fmi,
                            int *dmli){

  char            *hard_constraints;
  short           **S, **S5, **S3;
  unsigned short  **a2s;
  int             e, energy, *c, *fML, *ggg, ij, *indx, s, n_seq, k,
                  dangle_model, decomp, turn, *type;
  vrna_param_t    *P;
  vrna_md_t       *md;
  vrna_mx_mfe_t   *matrices;
  vrna_hc_t       *hc;
  vrna_sc_t       **scs;

  n_seq             = vc->n_seq;
  matrices          = vc->matrices;
  P                 = vc->params;
  md                = &(P->model_details);
  c                 = matrices->c;
  fML               = matrices->fML;
  ggg               = matrices->ggg;
  indx              = vc->jindx;
  hc                = vc->hc;
  scs               = vc->scs;
  hard_constraints  = hc->matrix;
  dangle_model      = md->dangles;
  turn              = md->min_loop_size;
  a2s               = vc->a2s;
  ij                = indx[j] + i;
  e                 = INF;

  if(hc->up_ml[i]){
    energy = fML[ij+1] + n_seq * P->MLbase;
    if(scs)
      for(s = 0; s < n_seq; s++){
        if(scs[s]){
          if(scs[s]->energy_up)
            energy += scs[s]->energy_up[a2s[s][i]][1];
        }
      }
    e = MIN2(e, energy);
  }

  if(hc->up_ml[j]){
    energy = fML[indx[j-1]+i] + n_seq * P->MLbase;
    if(scs)
      for(s = 0;s < n_seq; s++){
        if(scs[s]){
          if(scs[s]->energy_up)
            energy += scs[s]->energy_up[a2s[s][j]][1];
        }
      }
    e = MIN2(e, energy);
  }

  if(hard_constraints[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
    energy = c[ij];

    type  = (int *)vrna_alloc(n_seq * sizeof(int));
    S     = vc->S;
    S5    = vc->S5;     /*S5[s][i] holds next base 5' of i in sequence s*/
    S3    = vc->S3;     /*Sl[s][i] holds next base 3' of i in sequence s*/

    for(s = 0; s < n_seq; s++){
      type[s] = md->pair[S[s][i]][S[s][j]];
      if(type[s] == 0)
        type[s] = 7;
    }

    if(dangle_model){
      for(s = 0; s < n_seq; s++){
        energy += E_MLstem(type[s], S5[s][i], S3[s][j], P);
      }
    }
    else{
      for(s = 0; s < n_seq; s++){
        energy += E_MLstem(type[s], -1, -1, P);
      }
    }
    e = MIN2(e, energy);

    if(md->gquad){
      decomp = ggg[indx[j] + i] + n_seq * E_MLstem(0, -1, -1, P);
      e = MIN2(e, decomp);
    }

    free(type);
  }


  /* modular decomposition -------------------------------*/
  for (decomp = INF, k = i+1+turn; k <= j-2-turn; k++)
    decomp = MIN2(decomp, fmi[k] + fML[indx[j] + k + 1]);

  dmli[j] = decomp; /* store for later use in ML decompositon */

  e = MIN2(e, decomp);

  fmi[j] = e; /* store for later use in ML decompositon */

  return e;
}

PRIVATE FLT_OR_DBL
exp_E_mb_loop_fast( vrna_fold_compound_t *vc,
                    int i,
                    int j,
                    FLT_OR_DBL *qqm1){

  unsigned char     type, tt;
  char              hc, *ptype;
  short             *S1;
  int               ij, k, kl, *my_iindx, *jindx, *rtype, cp;
  FLT_OR_DBL        qbt1, temp, qqqmmm, *qm, *scale, expMLclosing;
  vrna_sc_t         *sc;
  vrna_exp_param_t  *pf_params;
  vrna_md_t         *md;

  cp            = vc->cutpoint;
  my_iindx      = vc->iindx;
  jindx         = vc->jindx;
  sc            = vc->sc;
  ptype         = vc->ptype;
  S1            = vc->sequence_encoding;
  qm            = vc->exp_matrices->qm;
  scale         = vc->exp_matrices->scale;
  pf_params     = vc->exp_params;
  md            = &(pf_params->model_details);
  ij            = jindx[j] + i;
  hc            = vc->hc->matrix[ij];
  expMLclosing  = pf_params->expMLclosing;
  qbt1          = 0.;

  /*multiple stem loop contribution*/
  if((hc & VRNA_CONSTRAINT_CONTEXT_MB_LOOP) && ON_SAME_STRAND(i,i+1,cp) && ON_SAME_STRAND(j-1,j,cp)) {
    type    = (unsigned char)ptype[ij];
    rtype   = &(md->rtype[0]);
    tt      = rtype[type];

    if(tt == 0)
      tt = 7;

    qqqmmm  =   expMLclosing
              * exp_E_MLstem(tt, S1[j-1], S1[i+1], pf_params)
              * scale[2];

    temp = 0.0;
    kl = my_iindx[i+1]-(i+1);

    if(sc){
      if(sc->exp_energy_bp)
        qqqmmm *= sc->exp_energy_bp[my_iindx[i] - j];

      if(sc->exp_f){
        qqqmmm *= sc->exp_f(i, j, i, j, VRNA_DECOMP_PAIR_ML, sc->data);

        for (k=i+2; k<=j-1; k++,kl--){
          if (ON_SAME_STRAND(k-1,k,cp)){
            temp +=   qm[kl]
                    * qqm1[k]
                    * sc->exp_f(i+1, j-1, k-1, k, VRNA_DECOMP_ML_ML_ML, sc->data);
          }
        }
      } else {
        for (k=i+2; k<=j-1; k++,kl--){
          if (ON_SAME_STRAND(k-1,k,cp)){
            temp +=   qm[kl]
                    * qqm1[k];
          }
        }
      }
    } else {
      for (k=i+2; k<=j-1; k++,kl--){
        if (ON_SAME_STRAND(k-1,k,cp)){
          temp +=   qm[kl]
                  * qqm1[k];
        }
      }
    }

    qbt1 += temp * qqqmmm;
  }

  return qbt1;
}

PRIVATE FLT_OR_DBL
exp_E_mb_loop_fast_comparative( vrna_fold_compound_t *vc,
                                int i,
                                int j,
                                FLT_OR_DBL *qqm1){

  char              hc;
  short             **S, **S5, **S3;
  int               jij, k, kl, *my_iindx, *jindx, *types, n_seq, s;
  FLT_OR_DBL        qbt1, temp, qqqmmm, *qm, *scale, expMLclosing;
  vrna_sc_t         **scs;
  vrna_exp_param_t  *pf_params;
  vrna_md_t         *md;

  my_iindx      = vc->iindx;
  jindx         = vc->jindx;
  qm            = vc->exp_matrices->qm;
  scale         = vc->exp_matrices->scale;
  pf_params     = vc->exp_params;
  md            = &(pf_params->model_details);
  jij           = jindx[j] + i;
  hc            = vc->hc->matrix[jij];
  expMLclosing  = pf_params->expMLclosing;
  qbt1          = 0.;
  types         = NULL;

  /*multiple stem loop contribution*/
  if(hc & VRNA_CONSTRAINT_CONTEXT_MB_LOOP) {

    S       = vc->S;
    S5      = vc->S5;     /*S5[s][i] holds next base 5' of i in sequence s*/
    S3      = vc->S3;     /*Sl[s][i] holds next base 3' of i in sequence s*/
    scs     = vc->scs;
    n_seq   = vc->n_seq;
    types   = (int *)vrna_alloc(sizeof(int) * n_seq);

    qqqmmm  = 1.;

    for(s = 0; s < n_seq; s++){
      types[s] = md->pair[S[s][j]][S[s][i]];
      if(types[s] == 0)
        types[s] = 7;
    }

    for(s = 0; s < n_seq; s++){
      qqqmmm *=   exp_E_MLstem(types[s], S5[s][j], S3[s][i], pf_params)
                * expMLclosing;
    }

    if(scs){
      for(s = 0; s < n_seq; s++){
        if(scs[s]){
          if(scs[s]->exp_energy_bp)
            qqqmmm *= scs[s]->exp_energy_bp[my_iindx[i] - j];
        }
      }
    }

    /* multi-loop loop contribution */
    temp  = 0.;
    kl    = my_iindx[i+1]-(i+1);

    for(k = i + 2; k <= j - 1; k++, kl--){
      temp += qm[kl] * qqm1[k];
    }

    temp *= scale[2];

    qbt1 = temp * qqqmmm;
  }

  /* cleanup */
  free(types);

  return qbt1;
}

/*
#################################
# Backtracking functions below  #
#################################
*/

PUBLIC int
vrna_BT_mb_loop_fake( vrna_fold_compound_t *vc,
                      int *u,
                      int *i,
                      int *j,
                      vrna_bp_stack_t *bp_stack,
                      int *stack_count){

  unsigned char type;
  char          *ptype;
  short         mm5, mm3, *S1;
  int           length, ii, jj, k, en, cp, fij, fi, *my_c, *my_fc, *my_ggg,
                *idx, with_gquad, dangle_model, turn;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_hc_t     *hc;
  vrna_sc_t     *sc;

  cp            = vc->cutpoint;
  length        = vc->length;
  P             = vc->params;
  md            = &(P->model_details);
  hc            = vc->hc;
  sc            = vc->sc;
  S1            = vc->sequence_encoding;
  ptype         = vc->ptype;
  idx           = vc->jindx;
  my_c          = vc->matrices->c;
  my_fc         = vc->matrices->fc;
  my_ggg        = vc->matrices->ggg;
  turn          = md->min_loop_size;
  with_gquad    = md->gquad;
  dangle_model  = md->dangles;

  ii = *i;
  jj = *j;

  if(ii < cp){ /* 'lower' part (fc[i<cut,j=cut-1]) */

    /* nibble off unpaired 5' bases */
    do{
      fij = my_fc[ii];
      fi  = (hc->up_ext[ii]) ? my_fc[ii + 1] : INF;

      if(sc){
        if(sc->energy_up)
          fi += sc->energy_up[ii][1];
      }

      if(++ii == jj)
        break;

    } while (fij == fi);
    ii--;

    if (jj < ii + turn + 2){ /* no more pairs */
      *u = *i = *j = -1;
      return 1;
    }

    mm5 = (ii > 1 && ON_SAME_STRAND(ii - 1, ii, cp)) ? S1[ii - 1] : -1;

    /* i or i+1 is paired. Find pairing partner */
    switch(dangle_model){
      case 0:   for(k = ii + turn + 1; k <= jj; k++){
                  if(hc->matrix[idx[k] + ii] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    type = (unsigned char)ptype[idx[k] + ii];

                    if(type == 0)
                      type = 7;

                    if(fij == my_fc[k + 1] + my_c[idx[k] + ii] + E_ExtLoop(type, -1, -1, P)){
                      bp_stack[++(*stack_count)].i = ii;
                      bp_stack[(*stack_count)].j   = k;
                      *u = k + 1;
                      *i = ii;
                      *j = k;
                      return 1;
                    }
                  }

                  if (with_gquad){
                    if(fij == my_fc[k + 1] + my_ggg[idx[k] + ii]){
                      *u = k + 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, ii, k, bp_stack, stack_count);
                      return 1;
                    }
                  }
                }
                break;

      case 2:   for(k = ii + turn + 1; k <= jj; k++){
                  if(hc->matrix[idx[k] + ii] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    mm3 = ON_SAME_STRAND(k, k + 1, cp) ? S1[k + 1] : -1;
                    type = (unsigned char)ptype[idx[k] + ii];

                    if(type == 0)
                      type = 7;

                    if(fij == my_fc[k + 1] + my_c[idx[k] + ii] + E_ExtLoop(type, mm5,  mm3, P)){
                      bp_stack[++(*stack_count)].i = ii;
                      bp_stack[(*stack_count)].j   = k;
                      *u = k + 1;
                      *i = ii;
                      *j = k;
                      return 1;
                    }
                  }

                  if (with_gquad){
                    if(fij == my_fc[k + 1] + my_ggg[idx[k] + ii]){
                      *u = k + 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, ii, k, bp_stack, stack_count);
                      return 1;
                    }
                  }
                }
                break;

      default:  for(k = ii + turn + 1; k <= jj; k++){
                  if(hc->matrix[idx[k] + ii] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    type = (unsigned char)ptype[idx[k] + ii];

                    if(type == 0)
                      type = 7;

                    if(fij == my_fc[k + 1] + my_c[idx[k] + ii] + E_ExtLoop(type, -1, -1, P)){
                      bp_stack[++(*stack_count)].i = ii;
                      bp_stack[(*stack_count)].j   = k;
                      *u = k + 1;
                      *i = ii;
                      *j = k;
                      return 1;
                    }
                    if(hc->up_ext[k + 1]){
                      mm3 = ON_SAME_STRAND(k, k + 1, cp) ? S1[k + 1] : -1;
                      en = my_c[idx[k] + ii];
                      if(sc)
                        if(sc->energy_up)
                          en += sc->energy_up[k + 1][1];

                      if(fij == my_fc[k + 2] + en + E_ExtLoop(type, -1, mm3, P)){
                        bp_stack[++(*stack_count)].i = ii;
                        bp_stack[(*stack_count)].j   = k;
                        *u = k + 2;
                        *i = ii;
                        *j = k;
                        return 1;
                      }
                    }
                  }

                  if (with_gquad){
                    if(fij == my_fc[k + 1] + my_ggg[idx[k] + ii]){
                      *u = k + 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, ii, k, bp_stack, stack_count);
                      return 1;
                    }
                  }

                  if(hc->matrix[idx[k] + ii + 1] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    if(hc->up_ext[ii]){
                      mm5   = ON_SAME_STRAND(ii, ii + 1,cp) ? S1[ii] : -1;
                      mm3   = ON_SAME_STRAND(k, k + 1,cp) ? S1[k + 1] : -1;
                      type  = ptype[idx[k] + ii + 1];

                      if(type == 0)
                        type = 7;

                      en    = my_c[idx[k] + ii + 1];
                      if(sc)
                        if(sc->energy_up)
                          en += sc->energy_up[ii][1];

                      if(fij == en + my_fc[k + 1] + E_ExtLoop(type, mm5, -1, P)){
                        bp_stack[++(*stack_count)].i = ii + 1;
                        bp_stack[(*stack_count)].j   = k;
                        *u = k + 1;
                        *i = ii + 1;
                        *j = k;
                        return 1;
                      }

                      if(k < jj){
                        if(hc->up_ext[k + 1]){
                          if(sc)
                            if(sc->energy_up)
                              en += sc->energy_up[k + 1][1];

                          if(fij == en + my_fc[k + 2] + E_ExtLoop(type, mm5, mm3, P)){
                            bp_stack[++(*stack_count)].i = ii + 1;
                            bp_stack[(*stack_count)].j   = k;
                            *u = k + 2;
                            *i = ii + 1;
                            *j = k;
                            return 1;
                          }
                        }
                      }
                    }
                  }
                }
                break;
    }
  } else { /* 'upper' part (fc[i=cut,j>cut]) */

    /* nibble off unpaired 3' bases */
    do{
      fij = my_fc[jj];
      fi  = (hc->up_ext[jj]) ? my_fc[jj - 1] : INF;

      if(sc){
        if(sc->energy_up)
          fi += sc->energy_up[jj][1];
      }

      if(--jj == ii)
        break;

    } while (fij == fi);
    jj++;

    if (jj < ii + turn + 2){ /* no more pairs */
      *u = *i = *j = -1;
      return 1;
    }

    /* j or j-1 is paired. Find pairing partner */
    mm3 = ((jj < length) && ON_SAME_STRAND(jj, jj + 1, cp)) ? S1[jj + 1] : -1;
    switch(dangle_model){
      case 0:   for (k = jj - turn - 1; k >= ii; k--){
                  if(with_gquad){
                    if(fij == my_fc[k - 1] + my_ggg[idx[jj] + k]){
                      *u = k - 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, k, jj, bp_stack, stack_count);
                      return 1;
                    }
                  }

                  if(hc->matrix[idx[jj] + k] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    type  = (unsigned char)ptype[idx[jj] + k];

                    if(type == 0)
                      type = 7;

                    en    = my_c[idx[jj] + k];
                    if(!ON_SAME_STRAND(k, jj, cp))
                      en += P->DuplexInit;

                    if(fij == my_fc[k - 1] + en + E_ExtLoop(type, -1, -1, P)){
                      bp_stack[++(*stack_count)].i = k;
                      bp_stack[(*stack_count)].j   = jj;
                      *u = k - 1;
                      *i = k;
                      *j = jj;
                      return 1;
                    }
                  }
                }
                break;
      case 2:   for(k = jj - turn - 1; k >= ii; k--){
                  if(with_gquad){
                    if(fij == my_fc[k - 1] + my_ggg[idx[jj] + k]){
                      *u = k - 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, k, jj, bp_stack, stack_count);
                      return 1;
                    }
                  }

                  if(hc->matrix[idx[jj] + k] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    mm5   = ((k > 1) && ON_SAME_STRAND(k - 1, k, cp)) ? S1[k - 1] : -1;
                    type  = (unsigned char)ptype[idx[jj] + k];

                    if(type == 0)
                      type = 7;

                    en    = my_c[idx[jj] + k];
                    if(!ON_SAME_STRAND(k, jj, cp))
                      en += P->DuplexInit;

                    if(fij == my_fc[k - 1] + en + E_ExtLoop(type, mm5, mm3, P)){
                      bp_stack[++(*stack_count)].i = k;
                      bp_stack[(*stack_count)].j   = jj;
                      *u = k - 1;
                      *i = k;
                      *j = jj;
                      return 1;
                    }
                  }
                }
                break;

      default:  for(k = jj - turn - 1; k >= ii; k--){

                  if(with_gquad){
                    if(fij == my_fc[k - 1] + my_ggg[idx[jj] + k]){
                      *u = k - 1;
                      *i = *j = -1;
                      vrna_BT_gquad_mfe(vc, k, jj, bp_stack, stack_count);
                      return 1;
                    }
                  }

                  if(hc->matrix[idx[jj] + k] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    type  = (unsigned char)ptype[idx[jj] + k];

                    if(type == 0)
                      type = 7;

                    en    = my_c[idx[jj] + k];
                    if(!ON_SAME_STRAND(k, jj, cp))
                      en += P->DuplexInit;

                    if(fij == my_fc[k - 1] + en + E_ExtLoop(type, -1, -1, P)){
                      bp_stack[++(*stack_count)].i = k;
                      bp_stack[(*stack_count)].j   = jj;
                      *u = k - 1;
                      *i = k;
                      *j = jj;
                      return 1;
                    }
                    if(hc->up_ext[k - 1]){
                      if((k > 1) && ON_SAME_STRAND(k - 1, k, cp)){
                        mm5 = S1[k - 1];
                        if(sc)
                          if(sc->energy_up)
                            en += sc->energy_up[k - 1][1];

                        if(fij == my_fc[k - 2] + en + E_ExtLoop(type, mm5, -1, P)){
                          bp_stack[++(*stack_count)].i = k;
                          bp_stack[(*stack_count)].j   = jj;
                          *u = k - 2;
                          *i = k;
                          *j = jj;
                          return 1;
                        }
                      }
                    }
                  }

                  if(hc->matrix[idx[jj - 1] + k] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                    type = (unsigned char)ptype[idx[jj - 1] + k];

                    if(type == 0)
                      type = 7;

                    if(hc->up_ext[jj]){
                      if(ON_SAME_STRAND(jj - 1, jj, cp)){
                        mm3 = S1[jj];
                        en = my_c[idx[jj - 1] + k];
                        if (!ON_SAME_STRAND(k, jj - 1, cp))
                          en += P->DuplexInit; /*???*/
                        if(sc)
                          if(sc->energy_up)
                            en += sc->energy_up[jj][1];

                        if (fij == en + my_fc[k - 1] + E_ExtLoop(type, -1, mm3, P)){
                          bp_stack[++(*stack_count)].i = k;
                          bp_stack[(*stack_count)].j   = jj - 1;
                          *u = k - 1;
                          *i = k;
                          *j = jj - 1;
                          return 1;
                        }

                        if(k > ii){
                          if(hc->up_ext[k - 1]){
                            mm5 = ON_SAME_STRAND(k - 1, k, cp) ? S1[k - 1] : -1;
                            if(sc)
                              if(sc->energy_up)
                                en += sc->energy_up[k - 1][1];

                            if (fij == my_fc[k - 2] + en + E_ExtLoop(type, mm5, mm3, P)){
                              bp_stack[++(*stack_count)].i = k;
                              bp_stack[(*stack_count)].j   = jj - 1;
                              *u = k - 2;
                              *i = k;
                              *j = jj - 1;
                              return 1;
                            }
                          }
                        }
                      }
                    }
                  }
                }
                break;
    }
  }

  return 0;
}

PUBLIC int
vrna_BT_mb_loop_split(vrna_fold_compound_t *vc,
                      int *i,
                      int *j,
                      int *k,
                      int *l,
                      int *component1,
                      int *component2,
                      vrna_bp_stack_t *bp_stack,
                      int *stack_count){

  unsigned char type, type_2;
  char          *ptype;
  short         *S1;
  int           ij, ii, jj, fij, fi, u, en, *my_c, *my_fML, *my_ggg,
                turn, *idx, with_gquad, dangle_model, *rtype;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_hc_t     *hc;
  vrna_sc_t     *sc;


  P             = vc->params;
  md            = &(P->model_details);
  hc            = vc->hc;
  sc            = vc->sc;
  idx           = vc->jindx;
  ptype         = vc->ptype;
  rtype         = &(md->rtype[0]);
  S1            = vc->sequence_encoding;

  my_c          = vc->matrices->c;
  my_fML        = vc->matrices->fML;
  my_ggg        = vc->matrices->ggg;
  turn          = md->min_loop_size;
  with_gquad    = md->gquad;
  dangle_model  = md->dangles;

  ii = *i;
  jj = *j;

  /* nibble off unpaired 3' bases */
  do{
    fij = my_fML[idx[jj] + ii];
    fi  = (hc->up_ml[jj]) ? my_fML[idx[jj - 1] + ii] + P->MLbase : INF;

    if(sc){
      if(sc->energy_up)
        fi += sc->energy_up[jj][1];
      if(sc->f)
        fi += sc->f(ii, jj, ii, jj-1, VRNA_DECOMP_ML_ML, sc->data);
    }

    if(--jj == 0)
      break;

  } while (fij == fi);
  jj++;

  /* nibble off unpaired 5' bases */
  do{
    fij = my_fML[idx[jj] + ii];
    fi  = (hc->up_ml[ii]) ? my_fML[idx[jj] + ii + 1] + P->MLbase : INF;

    if(sc){
      if(sc->energy_up)
        fi += sc->energy_up[ii][1];
      if(sc->f)
        fi += sc->f(ii, jj, ii+1, jj, VRNA_DECOMP_ML_ML, sc->data);
    }

    if(++ii == jj)
      break;

  } while (fij == fi);
  ii--;

  if (jj < ii + turn + 1){ /* no more pairs */
    return 0;
  }

  ij = idx[jj] + ii;

  *component1 = *component2 = 1; /* split into two multi loop parts by default */

  /* 1. test for single component */

  if(with_gquad){
    if(fij == my_ggg[ij] + E_MLstem(0, -1, -1, P)){
      *i = *j = -1;
      *k = *l = -1;
      vrna_BT_gquad_mfe(vc, ii, jj, bp_stack, stack_count);
      return 1;
    }
  }

  type  = (unsigned char)ptype[ij];

  en    = my_c[ij];
  if(sc){
    if(sc->f)
      en += sc->f(ii, jj, ii+1, jj-1, VRNA_DECOMP_ML_STEM, sc->data);
  }
  switch(dangle_model){
    case 0:   if(hc->matrix[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){

                if(type == 0)
                  type = 7;

                if(fij == en + E_MLstem(type, -1, -1, P)){
                  *i = *j = -1;
                  *k = ii;
                  *l = jj;
                  *component2 = 2;  /* 2nd part is structure enclosed by base pair */
                  return 1;
                }
              }
              break;

    case 2:   if(hc->matrix[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){

                if(type == 0)
                  type = 7;

                if(fij == en + E_MLstem(type, S1[ii - 1], S1[jj + 1], P)){
                  *i = *j = -1;
                  *k = ii;
                  *l = jj;
                  *component2 = 2;
                  return 1;
                }
              }
              break;

    default:  if(hc->matrix[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){

                if(type == 0)
                  type = 7;

                if(fij == en + E_MLstem(type, -1, -1, P)){
                  *i = *j = -1;
                  *k = ii;
                  *l = jj;
                  *component2 = 2;
                  return 1;
                }
              }
              if(hc->matrix[ij + 1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
                if(hc->up_ml[ii]){
                  int tmp_en = fij;
                  if(sc){
                    if(sc->energy_up)
                      tmp_en -= sc->energy_up[ii][1];
                    if(sc->f)
                      tmp_en -= sc->f(ii, jj, ii+1, jj, VRNA_DECOMP_ML_STEM, sc->data);
                  }
                  type = (unsigned char)ptype[ij + 1];

                  if(type == 0)
                    type = 7;

                  if(tmp_en == my_c[ij+1] + E_MLstem(type, S1[ii], -1, P) + P->MLbase){
                    *i = *j = -1;
                    *k = ii + 1;
                    *l = jj;
                    *component2 = 2;
                    return 1;
                  }
                }
              }
              if(hc->matrix[idx[jj - 1] + ii] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
                if(hc->up_ml[jj]){
                  int tmp_en = fij;
                  if(sc){
                    if(sc->energy_up)
                      tmp_en -= sc->energy_up[jj][1];
                    if(sc->f)
                      tmp_en -= sc->f(ii, jj, ii, jj-1, VRNA_DECOMP_ML_STEM, sc->data);
                  }
                  type = (unsigned char)ptype[idx[jj - 1] + ii];

                  if(type == 0)
                    type = 7;

                  if(tmp_en == my_c[idx[jj - 1] + ii] + E_MLstem(type, -1, S1[jj], P) + P->MLbase){
                    *i = *j = -1;
                    *k = ii;
                    *l = jj - 1;
                    *component2 = 2;
                    return 1;
                  }
                }
              }
              if(hc->matrix[idx[jj - 1] + ii + 1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
                if(hc->up_ml[ii] && hc->up_ml[jj]){
                  int tmp_en = fij;
                  if(sc){
                    if(sc->energy_up)
                      tmp_en -= sc->energy_up[ii][1] + sc->energy_up[jj][1];
                    if(sc->f)
                      tmp_en -= sc->f(ii, jj, ii+1, jj-1, VRNA_DECOMP_ML_STEM, sc->data);
                  }
                  type = (unsigned char)ptype[idx[jj - 1] + ii + 1];

                  if(type == 0)
                    type = 7;

                  if(tmp_en == my_c[idx[jj - 1] + ii + 1] + E_MLstem(type, S1[ii], S1[jj], P) + 2 * P->MLbase){
                    *i = *j = -1;
                    *k = ii + 1;
                    *l = jj - 1;
                    *component2 = 2;
                    return 1;
                  }
                }
              }
              break;
  }

  /* 2. Test for possible split point */
  for(u = ii + 1 + turn; u <= jj - 2 - turn; u++){
    en = my_fML[idx[u] + ii] + my_fML[idx[jj] + u + 1];
    if(sc){
      if(sc->f)
        en += sc->f(ii, jj, u, u+1, VRNA_DECOMP_ML_ML_ML, sc->data);
    }
    if(fij == en){
      *i = ii;
      *j = u;
      *k = u + 1;
      *l = jj;
      return 1;
    }
  }

  /* 3. last chance! Maybe coax stack */
  if(dangle_model==3){
    int ik, k1j, tmp_en;
    for (k1j = idx[jj] + ii + turn + 2, u = ii + 1 + turn; u <= jj - 2 - turn; u++, k1j++) {
      ik = idx[u] + ii;
      if((hc->matrix[ik] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC) && (hc->matrix[k1j] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC)){
        type    = rtype[(unsigned char)ptype[ik]];
        type_2  = rtype[(unsigned char)ptype[k1j]];

        if(type == 0)
          type = 7;
        if(type_2 == 0)
          type_2 = 7;

        tmp_en  = my_c[ik] + my_c[k1j] + P->stack[type][type_2] + 2*P->MLintern[1];
        if(sc){
          if(sc->f)
            tmp_en += sc->f(ii, u, u+1, jj, VRNA_DECOMP_ML_COAXIAL, sc->data);
        }
        if (fij == tmp_en){
          *i = ii;
          *j = u;
          *k = u + 1;
          *l = jj;
          *component1 = *component2 = 2;
          return 1;
        }
      }
    }
  }

  return 0;
}


PUBLIC int
vrna_BT_mb_loop(vrna_fold_compound_t *vc,
                int *i,
                int *j,
                int *k,
                int en,
                int *component1,
                int *component2){

  unsigned char type, type_2, tt;
  char          *ptype;
  short         s5, s3, *S1;
  int           ij, p, q, r, e, tmp_en, cp, *idx, turn, dangle_model,
                *my_c, *my_fML, *my_fc, *rtype;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_hc_t     *hc;
  vrna_sc_t     *sc;

  cp            = vc->cutpoint;
  idx           = vc->jindx;
  ij            = idx[*j] + *i;
  S1            = vc->sequence_encoding;
  P             = vc->params;
  md            = &(P->model_details);
  hc            = vc->hc;
  sc            = vc->sc;
  my_c          = vc->matrices->c;
  my_fML        = vc->matrices->fML;
  my_fc         = vc->matrices->fc;
  turn          = md->min_loop_size;
  ptype         = vc->ptype;
  rtype         = &(md->rtype[0]);
  type          = (unsigned char)ptype[ij];
  tt            = type;
  type          = rtype[type];
  dangle_model  = md->dangles;

  p = *i + 1;
  q = *j - 1;

  r = q - turn - 1;

  if(hc->matrix[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){

    if(type == 0)
      type = 7;
    if(tt == 0)
      tt = 7;

    /* is it a fake multi-loop? */
    if(!ON_SAME_STRAND(*i, *j, cp)){
      int ii, jj;
      ii = jj = 0;
      e = my_fc[p] + my_fc[q];
      if(sc){
        if(sc->energy_bp)
          e += sc->energy_bp[ij];
      }
      s5 = ON_SAME_STRAND(q, *j, cp) ? S1[q] : -1;
      s3 = ON_SAME_STRAND(*i, p, cp) ? S1[p] : -1;

      switch(dangle_model){
        case 0:   if(en == e + E_ExtLoop(type, -1, -1, P)){
                    ii=p, jj=q;
                  }
                  break;
        case 2:   if(en == e + E_ExtLoop(type, s5, s3, P)){
                    ii=p, jj=q;
                  }
                  break;
        default:  {
                    if(en == e + E_ExtLoop(type, -1, -1, P)){
                      ii=p, jj=q;
                      break;
                    }
                    if(hc->up_ext[p]){
                      e = my_fc[p + 1] + my_fc[q];
                      if(sc){
                        if(sc->energy_up)
                          e += sc->energy_up[p][1];
                        if(sc->energy_bp)
                          e += sc->energy_bp[ij];
                      }
                      if(en == e + E_ExtLoop(type, -1, s3, P)){
                        ii = p + 1; jj = q;
                        break;
                      }
                    }
                    if(hc->up_ext[q]){
                      e = my_fc[p] + my_fc[q - 1];
                      if(sc){
                        if(sc->energy_up)
                          e += sc->energy_up[q][1];
                        if(sc->energy_bp)
                          e += sc->energy_bp[ij];
                      }
                      if(en == e + E_ExtLoop(type, s5, -1, P)){
                        ii = p; jj = q - 1;
                        break;
                      }
                    }
                    if((hc->up_ext[q]) && (hc->up_ext[p])){
                      e = my_fc[p + 1] + my_fc[q - 1];
                      if(sc){
                        if(sc->energy_up)
                          e += sc->energy_up[p][1] + sc->energy_up[q][1];
                        if(sc->energy_bp)
                          e += sc->energy_bp[ij];
                      }
                      if(en == e + E_ExtLoop(type, s5, s3, P)){
                        ii = p + 1; jj = q - 1;
                        break;
                      }
                    }
                  }
                  break;
      }

      if(ii){ /* found a decomposition */
        *component1 = 3;
        *i          = ii;
        *k          = cp - 1;
        *j          = jj;
        *component2 = 4;
        return 1;
      }
    }

    /* true multi loop? */
    *component1 = *component2 = 1;  /* both components are MB loop parts by default */

    s5 = ON_SAME_STRAND(q, *j, cp) ? S1[q] : -1;
    s3 = ON_SAME_STRAND(*i, p, cp) ? S1[p] : -1;

    switch(dangle_model){
      case 0:   e = en - E_MLstem(type, -1, -1, P) - P->MLclosing;
                if(sc){
                  if(sc->energy_bp)
                    e -= sc->energy_bp[ij];
                  if(sc->f)
                    e -= sc->f(*i, *j, p, q, VRNA_DECOMP_PAIR_ML, sc->data);
                }
                for(r = *i + 2 + turn; r < *j - 2 - turn; ++r){
                  tmp_en = my_fML[idx[r] + p] + my_fML[idx[q] + r + 1];
                  if(sc){
                    if(sc->f)
                      tmp_en += sc->f(p, q, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                  }
                  if(e == tmp_en)
                    break;
                }
                break;

      case 2:   e = en - E_MLstem(type, s5, s3, P) - P->MLclosing;
                if(sc){
                  if(sc->energy_bp)
                    e -= sc->energy_bp[ij];
                  if(sc->f)
                    e -= sc->f(*i, *j, p, q, VRNA_DECOMP_PAIR_ML, sc->data);
                }
                for(r = p + turn + 1; r < q - turn - 1; ++r){
                  tmp_en = my_fML[idx[r] + p] + my_fML[idx[q] + r + 1];
                  if(sc){
                    if(sc->f)
                      tmp_en += sc->f(p, q, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                  }
                  if(e == tmp_en)
                    break;
                }
                break;

      default:  e = en - P->MLclosing;
                if(sc){
                  if(sc->energy_bp)
                    e -= sc->energy_bp[ij];
                }
                for(r = p + turn + 1; r < q - turn - 1; ++r){
                  tmp_en = my_fML[idx[r] + p] + my_fML[idx[q] + r + 1] + E_MLstem(type, -1, -1, P);
                  if(sc){
                    if(sc->f){
                      tmp_en += sc->f(*i, *j, p, q, VRNA_DECOMP_PAIR_ML, sc->data);
                      tmp_en += sc->f(p, q, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                    }
                  }
                  if(e == tmp_en){
                    break;
                  }
                  if(hc->up_ml[p]){
                    tmp_en = e;
                    if(sc){
                      if(sc->energy_up)
                        tmp_en -= sc->energy_up[p][1];
                      if(sc->f){
                        tmp_en -= sc->f(*i, *j, p+1, q, VRNA_DECOMP_PAIR_ML, sc->data);
                        tmp_en -= sc->f(p+1, q, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                      }
                    }
                    if(tmp_en == my_fML[idx[r] + p + 1] + my_fML[idx[q] + r + 1] + E_MLstem(type, -1, s3, P) + P->MLbase){
                      p += 1;
                      break;
                    }
                  }
                  if(hc->up_ml[q]){
                    tmp_en = e;
                    if(sc){
                      if(sc->energy_up)
                        tmp_en -= sc->energy_up[q][1];
                      if(sc->f){
                        tmp_en -= sc->f(*i, *j, p, q-1, VRNA_DECOMP_PAIR_ML, sc->data);
                        tmp_en -= sc->f(p, q-1, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                      }
                    }
                    if(tmp_en == my_fML[idx[r] + p] + my_fML[idx[q - 1] + r + 1] + E_MLstem(type, s5, -1, P) + P->MLbase){
                      q -= 1;
                      break;
                    }
                  }
                  if((hc->up_ml[p]) && (hc->up_ml[q])){
                    tmp_en = e;
                    if(sc){
                      if(sc->energy_up)
                        tmp_en -= sc->energy_up[p][1] + sc->energy_up[q][1];
                      if(sc->f){
                        tmp_en -= sc->f(*i, *j, p+1, q-1, VRNA_DECOMP_PAIR_ML, sc->data);
                        tmp_en -= sc->f(p+1, q-1, r, r+1, VRNA_DECOMP_ML_ML_ML, sc->data);
                      }
                    }
                    if(tmp_en == my_fML[idx[r] + p + 1] + my_fML[idx[q - 1] + r + 1] + E_MLstem(type, s5, s3, P) + 2 * P->MLbase){
                      p += 1;
                      q -= 1;
                      break;
                    }
                  }
                  /* coaxial stacking of (i.j) with (i+1.r) or (r.j-1) */
                  /* use MLintern[1] since coax stacked pairs don't get TerminalAU */
                  if(dangle_model == 3){
                    tmp_en = e;
                    if(hc->matrix[idx[r] + p] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
                      type_2 = rtype[(unsigned char)ptype[idx[r] + p]];

                      if(type_2 == 0)
                        type_2 = 7;

                      tmp_en = my_c[idx[r] + p] + P->stack[tt][type_2] + my_fML[idx[q] + r + 1];
                      if(sc){
                        if(sc->f){
                          tmp_en += sc->f(*i, *j, p, q, VRNA_DECOMP_PAIR_ML, sc->data);
                          tmp_en += sc->f(*i, *j, p, r, VRNA_DECOMP_ML_COAXIAL, sc->data);
                        }
                      }
                      if(e == tmp_en + 2 * P->MLintern[1]){
                        *component1 = 2;
                        break;
                      }
                    }

                    if(hc->matrix[idx[q] + r + 1] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){
                      type_2 = rtype[(unsigned char)ptype[idx[q] + r + 1]];

                      if(type_2 == 0)
                        type_2 = 7;

                      tmp_en = my_c[idx[q] + r + 1] + P->stack[tt][type_2] + my_fML[idx[r] + p];
                      if(sc){
                        if(sc->f){
                          tmp_en += sc->f(*i, *j, p, q, VRNA_DECOMP_PAIR_ML, sc->data);
                          tmp_en += sc->f(*i, *j, r+1, q, VRNA_DECOMP_ML_COAXIAL, sc->data);
                        }
                      }
                      if (e == tmp_en + 2 * P->MLintern[1]){
                        *component2 = 2;
                        break;
                      }
                    }
                  }
                }
                break;
    }
  }

  if(r <= *j - turn - 3){
    *i = p;
    *k = r;
    *j = q;
    return 1;
  } else {
#if 0
    /* Y shaped ML loops fon't work yet */
    if (dangle_model==3) {
      d5 = P->dangle5[tt][S1[j-1]];
      d3 = P->dangle3[tt][S1[i+1]];
      /* (i,j) must close a Y shaped ML loop with coax stacking */
      if (cij ==  fML[indx[j-2]+i+2] + mm + d3 + d5 + P->MLbase + P->MLbase) {
        i1 = i+2;
        j1 = j-2;
      } else if (cij ==  fML[indx[j-2]+i+1] + mm + d5 + P->MLbase)
        j1 = j-2;
      else if (cij ==  fML[indx[j-1]+i+2] + mm + d3 + P->MLbase)
        i1 = i+2;
      else /* last chance */
        if (cij != fML[indx[j-1]+i+1] + mm + P->MLbase)
          fprintf(stderr,  "backtracking failed in repeat");
      /* if we arrive here we can express cij via fML[i1,j1]+dangles */
      bt_stack[++s].i = i1;
      bt_stack[s].j   = j1;
    }
#endif
  }

  return 0;
}

