
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "ViennaRNA/fold_vars.h"
#include "ViennaRNA/energy_par.h"
#include "ViennaRNA/utils.h"
#include "ViennaRNA/constraints.h"
#include "ViennaRNA/gquad.h"
#include "ViennaRNA/exterior_loops.h"

#ifdef __GNUC__
# define INLINE inline
#else
# define INLINE
#endif

#ifdef ON_SAME_STRAND
#undef ON_SAME_STRAND
#endif

#define ON_SAME_STRAND(I,J,C)  (((I)>=(C))||((J)<(C)))

/*
#################################
# BEGIN OF FUNCTION DEFINITIONS #
#################################
*/

PUBLIC int
E_ext_loop( int i,
            int j,
            vrna_fold_compound_t *vc){

  char          eval_loop, *ptype, *hard_constraints;
  short         *S;
  int           ij, en, e, type, cp, *idx;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_sc_t     *sc;
#ifdef WITH_GEN_HC
  vrna_callback_hc_evaluate *f;
#endif

  cp                = vc->cutpoint;
  S                 = vc->sequence_encoding;
  idx               = vc->jindx;
  ptype             = vc->ptype;
  P                 = vc->params;
  md                = &(P->model_details);
  hard_constraints  = vc->hc->matrix;
  sc                = vc->sc;

#ifdef WITH_GEN_HC
  f = vc->hc->f;
#endif

  e     = INF;
  ij    = idx[j] + i;
  type  = ptype[ij];

  if((cp < 0) || (((i)>=cp)||((j)<cp))){ /* regular exterior loop */
    eval_loop = hard_constraints[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
    if(f)
      eval_loop = (f(i, j, i, j, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
    if(eval_loop){

      if(type == 0)
        type = 7;

      switch(md->dangles){
        case 2:   e = E_ExtLoop(type, S[i-1], S[j+1], P);
                  break;
        case 0:   /* fall through */
        default:  e = E_ExtLoop(type, -1, -1, P);
                  break;
      }
      if(sc){
        if(sc->f)
          e += sc->f(i, j, i, j, VRNA_DECOMP_EXT_STEM, sc->data);
      }
    }

    if(md->dangles % 2){
      ij = idx[j - 1] + i;
      eval_loop = hard_constraints[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(i, j, i, j - 1, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
      if(eval_loop){
        type = vc->ptype[ij];

        if(type == 0)
          type = 7;

        en = E_ExtLoop(type, -1, S[j], P);
        if(sc){
          if(sc->f)
            en += sc->f(i, j, i, j - 1, VRNA_DECOMP_EXT_STEM, sc->data);
        }
        e = MIN2(e, en);
      }

      ij = idx[j] + i + 1;
      eval_loop = hard_constraints[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(i, j, i+1, j, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
      if(eval_loop){
        type = vc->ptype[ij];

        if(type == 0)
          type = 7;

        en = E_ExtLoop(type, S[i], -1, P);
        if(sc){
          if(sc->f)
            en += sc->f(i, j, i + 1, j, VRNA_DECOMP_EXT_STEM, sc->data);
        }
        e = MIN2(e, en);
      }
    }
  }

  return e;
}


PUBLIC void
E_ext_loop_5( vrna_fold_compound_t *vc){

  char          eval_loop, el, *ptype, *hc;
  short         *S;
  int           en, i, j, ij, type, length, *indx, *hc_up, *f5, *c, dangle_model,
                *ggg, with_gquad, turn;
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
  hc_up         = vc->hc->up_ext;
  sc            = vc->sc;
  f5            = vc->matrices->f5;
  c             = vc->matrices->c;
  P             = vc->params;
  dangle_model  = P->model_details.dangles;
  ggg           = vc->matrices->ggg;
  with_gquad    = P->model_details.gquad;
  turn          = P->model_details.min_loop_size;

#ifdef WITH_GEN_HC
  f = vc->hc->f;
#endif

  f5[0] = 0;
  for(i = 1; i <= turn + 1; i++){
    if(f5[i-1] != INF){
      eval_loop = (hc_up[i] > 0) ? (char)1 : (char)0;
#ifdef WITH_GEN_HC
      if(f)
        eval_loop = (f(1, i, 1, i - 1, VRNA_DECOMP_EXT_EXT, vc->hc->data)) ? eval_loop : (char)0;
#endif
      if(eval_loop){
        f5[i] = f5[i-1];
        if(sc){
          if(sc->energy_up)
            f5[i] += sc->energy_up[i][1];
          if(sc->f)
            f5[i] += sc->f(1, i, 1, i - 1, VRNA_DECOMP_EXT_EXT, sc->data);
        }
      } else {
        f5[i] = INF;
      }
    } else {
      f5[i] = INF;
    }
  }

  /* duplicated code may be faster than conditions inside loop ;) */
  switch(dangle_model){
    /* dont use dangling end and mismatch contributions at all */
    case 0:   for(j=turn+2; j<=length; j++){
                /* initialize with INF */
                f5[j] = INF;

                /* check for 3' extension with one unpaired nucleotide */
                if(f5[j-1] != INF){
                  eval_loop = (hc_up[j] > 0) ? (char)1 : (char)0;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j-1, VRNA_DECOMP_EXT_EXT, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    f5[j] = f5[j-1];
                    if(sc){
                      if(sc->energy_up)
                        f5[j] += sc->energy_up[j][1];
                      if(sc->f)
                        f5[j] += sc->f(1, j, i, j - 1, VRNA_DECOMP_EXT_EXT, sc->data);
                    }
                  }
                }

                /* check for possible stems branching off the exterior loop */
                if(sc && sc->f){
                  for (i=j-turn-1; i>1; i--){
                    if(f5[i-1] != INF){
                      ij = indx[j]+i;

                      if(with_gquad){
                        f5[j] = MIN2(f5[j], f5[i-1] + ggg[ij]);
                      }

                      if(c[ij] != INF){
                        eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                        if(f)
                          eval_loop = (f(1, j, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                        if(eval_loop){
                          type = ptype[ij];

                          if(type == 0)
                            type = 7;

                          en  = f5[i-1] + c[ij] + E_ExtLoop(type, -1, -1, P);
                          en += sc->f(1, j, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                          f5[j] = MIN2(f5[j], en);
                        }
                      }
                    }
                  }
                } else {
                  for (i=j-turn-1; i>1; i--){
                    if(f5[i-1] != INF){
                      ij = indx[j]+i;

                      if(with_gquad){
                        f5[j] = MIN2(f5[j], f5[i-1] + ggg[ij]);
                      }

                      if(c[ij] != INF){
                        eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                        if(f)
                          eval_loop = (f(1, j, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                        if(eval_loop){
                          type = ptype[ij];

                          if(type == 0)
                            type = 7;

                          en    = f5[i-1] + c[ij] + E_ExtLoop(type, -1, -1, P);
                          f5[j] = MIN2(f5[j], en);
                        }
                      }
                    }
                  }
                }
                ij = indx[j] + 1;

                if(with_gquad){
                  f5[j] = MIN2(f5[j], ggg[ij]);
                }

                if(c[ij] != INF){
                  eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    type = ptype[ij];

                    if(type == 0)
                      type = 7;

                    en    = c[ij] + E_ExtLoop(type, -1, -1, P);
                    if(sc){
                      if(sc->f)
                        en += sc->f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, sc->data);
                    }
                    f5[j] = MIN2(f5[j], en);
                  }
                }
              }
              break;

    /* always use dangles on both sides */
    case 2:   for(j=turn+2; j<length; j++){
                f5[j] = INF;

                if(f5[j-1] != INF){
                  eval_loop = (hc_up[j] > 0) ? (char)1 : (char)0;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j-1, VRNA_DECOMP_EXT_EXT, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    f5[j] = f5[j-1];
                    if(sc){
                      if(sc->energy_up)
                        f5[j] += sc->energy_up[j][1];
                      if(sc->f)
                        f5[j] += sc->f(1, j, 1, j - 1, VRNA_DECOMP_EXT_EXT, sc->data);
                    }
                  }
                }
                if(sc && sc->f){
                  for (i=j-turn-1; i>1; i--){
                    if(f5[i-1] != INF){
                      ij = indx[j] + i;

                      if(with_gquad){
                        f5[j] = MIN2(f5[j], f5[i-1] + ggg[ij]);
                      }

                      if(c[ij] != INF){
                        eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                        if(f)
                          eval_loop = (f(1, j, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                        if(eval_loop){
                          type = ptype[ij];

                          if(type == 0)
                            type = 7;

                          en    = f5[i-1] + c[ij] + E_ExtLoop(type, S[i-1], S[j+1], P);
                          en   += sc->f(1, j, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                          f5[j] = MIN2(f5[j], en);
                        }
                      }
                    }
                  }
                } else {
                  for (i=j-turn-1; i>1; i--){
                    if(f5[i-1] != INF){
                      ij = indx[j] + i;

                      if(with_gquad){
                        f5[j] = MIN2(f5[j], f5[i-1] + ggg[ij]);
                      }

                      if(c[ij] != INF){
                        eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                        if(f)
                          eval_loop = (f(1, j, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                        if(eval_loop){
                          type = ptype[ij];

                          if(type == 0)
                            type = 7;

                          en    = f5[i-1] + c[ij] + E_ExtLoop(type, S[i-1], S[j+1], P);
                          f5[j] = MIN2(f5[j], en);
                        }
                      }
                    }
                  }
                }
                ij = indx[j] + 1;

                if(with_gquad){
                  f5[j] = MIN2(f5[j], ggg[ij]);
                }

                if(c[ij] != INF){
                  eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    type = ptype[ij];

                    if(type == 0)
                      type = 7;

                    en    = c[ij] + E_ExtLoop(type, -1, S[j+1], P);
                    if(sc){
                      if(sc->f)
                        en += sc->f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, sc->data);
                    }
                    f5[j] = MIN2(f5[j], en);
                  }
                }
              }

              f5[length] = INF;
              if(f5[length-1] != INF){
                eval_loop = (hc_up[length] > 0) ? (char)1 : (char)0;
#ifdef WITH_GEN_HC
                if(f)
                  eval_loop = (f(1, length, 1, length-1, VRNA_DECOMP_EXT_EXT, vc->hc->data)) ? eval_loop : (char)0;
#endif
                if(eval_loop){
                  f5[length] = f5[length-1];
                  if(sc){
                    if(sc->energy_up)
                      f5[length] += sc->energy_up[length][1];
                    if(sc->f)
                      f5[length] += sc->f(1, length, 1, length - 1, VRNA_DECOMP_EXT_EXT, sc->data);
                  }
                }
              }
              if(sc && sc->f){

                for (i=length-turn-1; i>1; i--){
                  if(f5[i-1] != INF){
                    ij = indx[length] + i;

                    if(with_gquad){
                      f5[length] = MIN2(f5[length], f5[i-1] + ggg[ij]);
                    }

                    if(c[ij] != INF){
                      eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                      if(f)
                        eval_loop = (f(1, length, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                      if(eval_loop){
                        type = ptype[ij];

                        if(type == 0)
                          type = 7;

                        en          = f5[i-1] + c[ij] + E_ExtLoop(type, S[i-1], -1, P);
                        en         += sc->f(1, length, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                        f5[length]  = MIN2(f5[length], en);
                      }
                    }
                  }
                }
              } else {
                for (i=length-turn-1; i>1; i--){
                  if(f5[i-1] != INF){
                    ij = indx[length] + i;

                    if(with_gquad){
                      f5[length] = MIN2(f5[length], f5[i-1] + ggg[ij]);
                    }

                    if(c[ij] != INF){
                      eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                      if(f)
                        eval_loop = (f(1, length, i-1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                      if(eval_loop){
                        type = ptype[ij];

                        if(type == 0)
                          type = 7;

                        en          = f5[i-1] + c[ij] + E_ExtLoop(type, S[i-1], -1, P);
                        f5[length]  = MIN2(f5[length], en);
                      }
                    }
                  }
                }
              }
              ij = indx[length] + 1;

              if(with_gquad){
                f5[length] = MIN2(f5[length], ggg[ij]);
              }

              if(c[ij] != INF){
                eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                if(f)
                  eval_loop = (f(1, length, 1, length, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                if(eval_loop){
                  type = ptype[ij];

                  if(type == 0)
                    type = 7;

                  en          = c[ij] + E_ExtLoop(type, -1, -1, P);
                  if(sc){
                    if(sc->f)
                      en += sc->f(1, length, 1, length, VRNA_DECOMP_EXT_STEM, sc->data);
                  }
                  f5[length]  = MIN2(f5[length], en);
                }
              }
              break;

    /* normal dangles, aka dangle_model = 1 || 3 */
    default:  for(j=turn+2; j<=length; j++){
                f5[j] = INF;
                if(f5[j-1] != INF){
                  eval_loop = (hc_up[j] > 0) ? (char)1 : (char)0;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j-1, VRNA_DECOMP_EXT_EXT, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    f5[j] = f5[j-1];
                    if(sc){
                      if(sc->energy_up)
                        f5[j] += sc->energy_up[j][1];
                      if(sc->f)
                        f5[j] += sc->f(1, j, 1, j - 1, VRNA_DECOMP_EXT_EXT, sc->data);
                    }
                  }
                }

                for (i=j-turn-1; i>1; i--){
                  ij = indx[j] + i;
                  if(f5[i-1] != INF){

                    if(with_gquad){
                      f5[j] = MIN2(f5[j], f5[i-1] + ggg[ij]);
                    }

                    if(c[ij] != INF){
                      eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                      if(f)
                        eval_loop = (f(1, j, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                      if(eval_loop){
                        type = ptype[ij];

                        if(type == 0)
                          type = 7;

                        en    = f5[i-1] + c[ij] + E_ExtLoop(type, -1, -1, P);
                        if(sc){
                          if(sc->f)
                            en += sc->f(1, j, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                        }
                        f5[j] = MIN2(f5[j], en);
                      }
                    }
                  }

                  if((f5[i-2] != INF) && c[ij] != INF){
                    eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
                    eval_loop = (hc_up[i-1] > 0) ? eval_loop : (char)0;
#ifdef WITH_GEN_HC
                    if(f)
                      eval_loop = (f(1, j, i-2, i, VRNA_DECOMP_EXT_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                    if(eval_loop){
                      type = ptype[ij];

                      if(type == 0)
                        type = 7;

                      en    = f5[i-2] + c[ij] + E_ExtLoop(type, S[i-1], -1, P);

                      if(sc){
                        if(sc->energy_up)
                          en += sc->energy_up[i-1][1];
                        if(sc->f)
                          en += sc->f(1, j, i - 2, i, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                      }
                      f5[j] = MIN2(f5[j], en);
                    }
                  }

                  ij = indx[j-1] + i;
                  if(c[ij] != INF){
                    if(f5[i - 1] != INF){
                      eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
                      eval_loop = (hc_up[j] > 0) ? eval_loop : (char)0;
#ifdef WITH_GEN_HC
                      if(f)
                        eval_loop = (f(1, j, i-1, i, VRNA_DECOMP_EXT_EXT_STEM1, vc->hc->data)) ? eval_loop : (char)0;
#endif
                      if(eval_loop){
                        type = ptype[ij];

                        if(type == 0)
                          type = 7;

                        en    = f5[i-1] + c[ij] + E_ExtLoop(type, -1, S[j], P);

                        if(sc){
                          if(sc->energy_up)
                            en += sc->energy_up[j][1];
                          if(sc->f)
                            en += sc->f(1, j, i - 1, i, VRNA_DECOMP_EXT_EXT_STEM1, sc->data);
                        }
                        f5[j] = MIN2(f5[j], en);
                      }
                    }

                    if(f5[i - 2] != INF){
                      el = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
                      el = (hc_up[i-1] > 0) ? el : (char)0;
#ifdef WITH_GEN_HC
                      if(f)
                        el = (f(1, j, i-2, i, VRNA_DECOMP_EXT_EXT_STEM1, vc->hc->data)) ? el : (char)0;
#endif
                      if(el){
                        type = ptype[ij];

                        if(type == 0)
                          type = 7;

                        en    = f5[i-2] + c[ij] + E_ExtLoop(type, S[i-1], S[j], P);

                        if(sc){
                          if(sc->energy_up)
                            en += sc->energy_up[i-1][1] + sc->energy_up[j][1];
                          if(sc->f)
                            en += sc->f(1, j, i - 2, i, VRNA_DECOMP_EXT_EXT_STEM1, sc->data);
                        }
                        f5[j] = MIN2(f5[j], en);
                      }
                    }
                  }
                }

                ij = indx[j] + 1;

                if(with_gquad){
                  f5[j] = MIN2(f5[j], ggg[ij]);
                }

                if(c[ij] != INF){

                  eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    type = ptype[ij];

                    if(type == 0)
                      type = 7;

                    en    = c[ij] + E_ExtLoop(type, -1, -1, P);
                    if(sc){
                      if(sc->f)
                        en += sc->f(1, j, 1, j, VRNA_DECOMP_EXT_STEM, sc->data);
                    }
                    f5[j] = MIN2(f5[j], en);
                  }
                }
                ij = indx[j-1] + 1;
                if(c[ij] != INF){
                  eval_loop = hc[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP;
                  eval_loop = (hc_up[j] > 0) ? eval_loop : (char)0;
#ifdef WITH_GEN_HC
                  if(f)
                    eval_loop = (f(1, j, 1, j-1, VRNA_DECOMP_EXT_STEM, vc->hc->data)) ? eval_loop : (char)0;
#endif
                  if(eval_loop){
                    type = ptype[ij];

                    if(type == 0)
                      type = 7;

                    en    = c[ij] + E_ExtLoop(type, -1, S[j], P);

                    if(sc){
                      if(sc->energy_up)
                        en += sc->energy_up[j][1];
                      if(sc->f)
                        en += sc->f(1, j, 1, j - 1, VRNA_DECOMP_EXT_STEM, sc->data);
                    }
                    f5[j] = MIN2(f5[j], en);
                  }
                }
              }
  }
}

PUBLIC int
E_Stem( int type,
        int si1,
        int sj1,
        int extLoop,
        vrna_param_t *P){

  int energy = 0;
  int d5 = (si1 >= 0) ? P->dangle5[type][si1] : 0;
  int d3 = (sj1 >= 0) ? P->dangle3[type][sj1] : 0;

  if(type > 2)
    energy += P->TerminalAU;

  if(si1 >= 0 && sj1 >= 0)
    energy += (extLoop) ? P->mismatchExt[type][si1][sj1] : P->mismatchM[type][si1][sj1];
  else
    energy += d5 + d3;

  if(!extLoop) energy += P->MLintern[type];
  return energy;
}

PUBLIC int
E_ExtLoop(int type,
          int si1,
          int sj1,
          vrna_param_t *P){

  int energy = 0;

  if(si1 >= 0 && sj1 >= 0){
    energy += P->mismatchExt[type][si1][sj1];
  }
  else if (si1 >= 0){
    energy += P->dangle5[type][si1];
  }
  else if (sj1 >= 0){
    energy += P->dangle3[type][sj1];
  }

  if(type > 2)
    energy += P->TerminalAU;

  return energy;
}

PUBLIC FLT_OR_DBL
exp_E_Stem( int type,
            int si1,
            int sj1,
            int extLoop,
            vrna_exp_param_t *P){

  double energy = 1.0;
  double d5 = (si1 >= 0) ? P->expdangle5[type][si1] : 1.;
  double d3 = (sj1 >= 0) ? P->expdangle3[type][sj1] : 1.;

  if(si1 >= 0 && sj1 >= 0)
    energy = (extLoop) ? P->expmismatchExt[type][si1][sj1] : P->expmismatchM[type][si1][sj1];
  else
    energy = d5 * d3;

  if(type > 2)
    energy *= P->expTermAU;

  if(!extLoop) energy *= P->expMLintern[type];
  return (FLT_OR_DBL)energy;
}

PUBLIC FLT_OR_DBL
exp_E_ExtLoop(int type,
              int si1,
              int sj1,
              vrna_exp_param_t *P){

  double energy = 1.0;

  if(si1 >= 0 && sj1 >= 0){
    energy = P->expmismatchExt[type][si1][sj1];
  }
  else if(si1 >= 0){
    energy = P->expdangle5[type][si1];
  }
  else if(sj1 >= 0){
    energy = P->expdangle3[type][sj1];
  }

  if(type > 2)
    energy *= P->expTermAU;

  return (FLT_OR_DBL)energy;
}

PUBLIC int
vrna_BT_ext_loop_f5(vrna_fold_compound_t *vc,
                    int *k,
                    int *i,
                    int *j,
                    vrna_bp_stack_t *bp_stack,
                    int *stack_count){

  unsigned char type;
  char          *ptype;
  short         mm5, mm3, *S1;
  int           length, cp, fij, fi, jj, u, en, e, *my_f5, *my_c, *my_ggg, *idx,
                dangle_model, turn, with_gquad;
  vrna_param_t  *P;
  vrna_md_t     *md;
  vrna_hc_t     *hc;
  vrna_sc_t     *sc;

  length        = vc->length;
  cp            = vc->cutpoint;
  P             = vc->params;
  md            = &(P->model_details);
  hc            = vc->hc;
  sc            = vc->sc;
  my_f5         = vc->matrices->f5;
  my_c          = vc->matrices->c;
  my_ggg        = vc->matrices->ggg;
  idx           = vc->jindx;
  ptype         = vc->ptype;
  S1            = vc->sequence_encoding;
  dangle_model  = md->dangles;
  turn          = md->min_loop_size;
  with_gquad    = md->gquad;

  jj = *k;

  /* nibble off unpaired 3' bases */
  do{
    fij = my_f5[jj];
    fi  = (hc->up_ext[jj]) ? my_f5[jj-1] : INF;

    if(sc){
      if(sc->energy_up)
        fi += sc->energy_up[jj][1];
      if(sc->f)
        fi += sc->f(1, jj, 1, jj - 1, VRNA_DECOMP_EXT_EXT, sc->data);
    }

    if(--jj == 0)
      break;

  } while (fij == fi);
  jj++;

  if (jj < turn + 2){ /* no more pairs */
    *i = *j = -1;
    *k = 0;
    return 1;
  }

  /* must have found a decomposition */
  switch(dangle_model){
    case 0:   /* j is paired. Find pairing partner */
              for(u = jj - turn - 1; u >= 1; u--){

                if(with_gquad){
                  if(fij == my_f5[u - 1] + my_ggg[idx[jj] + u]){
                    *i = *j = -1;
                    *k = u - 1;
                    vrna_BT_gquad_mfe(vc, u, jj, bp_stack, stack_count);
                    return 1;
                  }
                }

                if(hc->matrix[idx[jj] + u] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                  type = (unsigned char)ptype[idx[jj] + u];

                  if(type == 0)
                    type = 7;

                  en = my_c[idx[jj] + u];
                  if(sc){
                    if(sc->f)
                      en += sc->f(1, jj, u - 1, u, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                  }
                  if(!ON_SAME_STRAND(u, jj, cp))
                    en += P->DuplexInit;
                  if(fij == E_ExtLoop(type, -1, -1, P) + en + my_f5[u - 1]){
                    *i = u;
                    *j = jj;
                    *k = u - 1;
                    bp_stack[++(*stack_count)].i = u;
                    bp_stack[(*stack_count)].j   = jj;
                    return 1;
                  }
                }
              }
              break;

    case 2:   mm3 = ((jj<length) && ON_SAME_STRAND(jj, jj + 1, cp)) ? S1[jj+1] : -1;
              for(u = jj - turn - 1; u >= 1; u--){

                if(with_gquad){
                  if(fij == my_f5[u - 1] + my_ggg[idx[jj] + u]){
                    *i = *j = -1;
                    *k = u - 1;
                    vrna_BT_gquad_mfe(vc, u, jj, bp_stack, stack_count);
                    return 1;
                  }
                }

                if(hc->matrix[idx[jj] + u] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                  mm5   = ((u > 1) && ON_SAME_STRAND(u - 1, u, cp)) ? S1[u - 1] : -1;
                  type  = (unsigned char)ptype[idx[jj] + u];

                  if(type == 0)
                    type = 7;

                  en    = my_c[idx[jj] + u];
                  if(sc){
                    if(sc->f)
                      en += sc->f(1, jj, u - 1, u, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                  }
                  if(!ON_SAME_STRAND(u, jj, cp))
                    en += P->DuplexInit;
                  if(fij == E_ExtLoop(type, mm5, mm3, P) + en + my_f5[u - 1]){
                    *i = u;
                    *j = jj;
                    *k = u - 1;
                    bp_stack[++(*stack_count)].i = u;
                    bp_stack[(*stack_count)].j   = jj;
                    return 1;
                  }
                }
              }
              break;

    default:  if(with_gquad){
                if(fij == my_ggg[idx[jj] + 1]){
                  *i = *j = -1;
                  *k = 0;
                  vrna_BT_gquad_mfe(vc, 1, jj, bp_stack, stack_count);
                  return 1;
                }
              }

              if(hc->matrix[idx[jj] + 1] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                type  = (unsigned char)ptype[idx[jj] + 1];

                if(type == 0)
                  type = 7;

                en    = my_c[idx[jj] + 1];
                if(sc){
                  if(sc->f)
                    en += sc->f(1, jj, 1, jj, VRNA_DECOMP_EXT_STEM, sc->data);
                }
                if(!ON_SAME_STRAND(1, jj, cp))
                  en += P->DuplexInit;
                if(fij == en + E_ExtLoop(type, -1, -1, P)){
                  *i = 1;
                  *j = jj;
                  *k = 0;
                  bp_stack[++(*stack_count)].i = 1;
                  bp_stack[(*stack_count)].j   = jj;
                  return 1;
                }
              }

              if(hc->matrix[idx[jj - 1] + 1] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                if(hc->up_ext[jj]){
                  if(ON_SAME_STRAND(jj - 1, jj, cp)){
                    mm3   = S1[jj];
                    type  = (unsigned char)ptype[idx[jj - 1] + 1];

                    if(type == 0)
                      type = 7;

                    en    = my_c[idx[jj - 1] + 1];
                    if(sc){
                      if(sc->energy_up)
                        en += sc->energy_up[jj][1];
                      if(sc->f)
                        en += sc->f(1, jj, 1, jj - 1, VRNA_DECOMP_EXT_STEM, sc->data);
                    }
                    if(!ON_SAME_STRAND(1, jj - 1, cp))
                      en += P->DuplexInit;

                    if(fij == en + E_ExtLoop(type, -1, mm3, P)){
                      *i = 1;
                      *j = jj - 1;
                      *k = 0;
                      bp_stack[++(*stack_count)].i = 1;
                      bp_stack[(*stack_count)].j   = jj - 1;
                      return 1;
                    }
                  }
                }
              }

              for(u = jj - turn - 1; u > 1; u--){

                if(with_gquad){
                  if(fij == my_f5[u - 1] + my_ggg[idx[jj] + u]){
                    *i = *j = -1;
                    *k = u - 1;
                    vrna_BT_gquad_mfe(vc, u, jj, bp_stack, stack_count);
                    return 1;
                  }
                }

                if(hc->matrix[idx[jj] + u] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                  type  = (unsigned char)ptype[idx[jj] + u];

                  if(type == 0)
                    type = 7;

                  en    = my_c[idx[jj] + u];
                  
                  if(!ON_SAME_STRAND(u, jj, cp))
                    en += P->DuplexInit;

                  e = my_f5[u - 1] + en + E_ExtLoop(type, -1, -1, P);
                  if(sc){
                    if(sc->f)
                      e += sc->f(1, jj, u - 1, u, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                  }
                  if(fij == e){
                    *i = u;
                    *j = jj;
                    *k = u - 1;
                    bp_stack[++(*stack_count)].i = u;
                    bp_stack[(*stack_count)].j   = jj;
                    return 1;
                  }
                  if(hc->up_ext[u - 1]){
                    if(ON_SAME_STRAND(u - 1, u, cp)){
                      mm5 = S1[u - 1];
                      e = my_f5[u - 2] + en + E_ExtLoop(type, mm5, -1, P);
                      if(sc){
                        if(sc->energy_up)
                          e += sc->energy_up[u - 1][1];
                        if(sc->f)
                          e += sc->f(1, jj, u - 2, u, VRNA_DECOMP_EXT_EXT_STEM, sc->data);
                      }
                      if(fij == e){
                        *i = u;
                        *j = jj;
                        *k = u - 2;
                        bp_stack[++(*stack_count)].i = u;
                        bp_stack[(*stack_count)].j   = jj;
                        return 1;
                      }
                    }
                  }
                }
                if(hc->matrix[idx[jj-1] + u] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP){
                  if(hc->up_ext[jj])
                    if(ON_SAME_STRAND(jj - 1, jj, cp)){
                      mm3   = S1[jj];
                      type  = (unsigned char)ptype[idx[jj - 1] + u];

                      if(type == 0)
                        type = 7;

                      en    = my_c[idx[jj - 1] + u];
                      if(!ON_SAME_STRAND(u, jj - 1, cp))
                        en += P->DuplexInit;

                      e = my_f5[u - 1] + en + E_ExtLoop(type, -1, mm3, P);

                      if(sc){
                        if(sc->energy_up)
                          e += sc->energy_up[jj][1];
                        if(sc->f)
                          e += sc->f(1, jj, u - 1, u, VRNA_DECOMP_EXT_EXT_STEM1, sc->data);
                      }

                      if(fij == e){
                        *i = u;
                        *j = jj - 1;
                        *k = u - 1;
                        bp_stack[++(*stack_count)].i = u;
                        bp_stack[(*stack_count)].j   = jj - 1;
                        return 1;
                      }

                      if(hc->up_ext[u - 1]){
                        mm5 = ON_SAME_STRAND(u - 1, u, cp) ? S1[u - 1] : -1;
                        e = my_f5[u - 2] + en + E_ExtLoop(type, mm5, mm3, P);
                        if(sc){
                          if(sc->energy_up)
                            e +=    sc->energy_up[jj][1]
                                  + sc->energy_up[u - 1][1];
                          if(sc->f)
                            e += sc->f(1, jj, u - 2, u, VRNA_DECOMP_EXT_EXT_STEM1, sc->data);
                        }
                        if(fij == e){
                          *i = u;
                          *j = jj - 1;
                          *k = u - 2;
                          bp_stack[++(*stack_count)].i = u;
                          bp_stack[(*stack_count)].j   = jj - 1;
                          return 1;
                        }
                      }
                    }
                }
              }
              break;
  }

  return 0;
}
