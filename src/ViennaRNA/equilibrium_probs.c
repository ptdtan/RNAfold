/*
                  partiton function for RNA secondary structures

                  Ivo L Hofacker + Ronny Lorenz
                  Vienna RNA package
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>    /* #defines FLT_MAX ... */
#include <limits.h>

#include "ViennaRNA/utils.h"
#include "ViennaRNA/energy_par.h"
#include "ViennaRNA/fold_vars.h"
#include "ViennaRNA/loop_energies.h"
#include "ViennaRNA/gquad.h"
#include "ViennaRNA/constraints.h"
#include "ViennaRNA/mfe.h"
#include "ViennaRNA/part_func.h"

/*
#################################
# GLOBAL VARIABLES              #
#################################
*/

/*
#################################
# PRIVATE VARIABLES             #
#################################
*/

/*
#################################
# PRIVATE FUNCTION DECLARATIONS #
#################################
*/
PRIVATE void  pf_create_bppm(vrna_fold_compound_t *vc, char *structure);
PRIVATE void  alipf_create_bppm(vrna_fold_compound_t *vc, char *structure);
PRIVATE INLINE void bppm_circ(vrna_fold_compound_t *vc);

PRIVATE FLT_OR_DBL  numerator_single(vrna_fold_compound_t *vc, int i, int j);
PRIVATE FLT_OR_DBL  numerator_comparative(vrna_fold_compound_t *vc, int i, int j);


PRIVATE double
wrap_mean_bp_distance(FLT_OR_DBL *p,
                      int length,
                      int *index,
                      int turn);

/*
#################################
# BEGIN OF FUNCTION DEFINITIONS #
#################################
*/

void
vrna_pairing_probs( vrna_fold_compound_t *vc,
                    char *structure){

  if(vc){
    switch(vc->type){
      case VRNA_VC_TYPE_SINGLE:     pf_create_bppm(vc, structure);
                                    break;

      case VRNA_VC_TYPE_ALIGNMENT:  alipf_create_bppm(vc, structure);
                                    break;

      default:                      vrna_message_warning("vrna_pf@part_func.c: Unrecognized fold compound type");
                                    break;
    }
  }
}

/* calculate base pairing probs */
PRIVATE void
pf_create_bppm( vrna_fold_compound_t *vc,
                char *structure){

  int n, i,j,k,l, ij, kl, ii, u1, u2, ov=0;
  unsigned char type, type_2, tt;
  FLT_OR_DBL  temp, Qmax=0, prm_MLb;
  FLT_OR_DBL  prmt, prmt1;
  FLT_OR_DBL  *tmp;
  FLT_OR_DBL  tmp2;
  FLT_OR_DBL  expMLclosing;
  FLT_OR_DBL  *qb, *qm, *G, *probs, *scale, *expMLbase;
  FLT_OR_DBL  *q1k, *qln;

  char              *ptype;

  double            max_real;
  int               *rtype, with_gquad;
  short             *S, *S1;
  vrna_hc_t         *hc;
  vrna_sc_t         *sc;
  int               *my_iindx, *jindx;
  int               circular, turn;
  vrna_exp_param_t  *pf_params;
  vrna_mx_pf_t      *matrices;
  vrna_md_t         *md;

  pf_params         = vc->exp_params;
  md                = &(pf_params->model_details);
  S                 = vc->sequence_encoding2;
  S1                = vc->sequence_encoding;
  my_iindx          = vc->iindx;
  jindx             = vc->jindx;
  ptype             = vc->ptype;

  circular          = md->circ;
  with_gquad        = md->gquad;
  turn              = md->min_loop_size;

  hc                = vc->hc;
  sc                = vc->sc;

  matrices          = vc->exp_matrices;

  qb                = matrices->qb;
  qm                = matrices->qm;
  G                 = matrices->G;
  probs             = matrices->probs;
  q1k               = matrices->q1k;
  qln               = matrices->qln;
  scale             = matrices->scale;
  expMLbase         = matrices->expMLbase;

  FLT_OR_DBL  expMLstem         = (with_gquad) ? exp_E_MLstem(0, -1, -1, pf_params) : 0;
  char        *hard_constraints = hc->matrix;
  int         *hc_up_int        = hc->up_int;

  int           corr_size       = 5;
  int           corr_cnt        = 0;
  vrna_plist_t  *bp_correction  = vrna_alloc(sizeof(vrna_plist_t) * corr_size);

  max_real      = (sizeof(FLT_OR_DBL) == sizeof(float)) ? FLT_MAX : DBL_MAX;


  if((S != NULL) && (S1 != NULL)){

    expMLclosing  = pf_params->expMLclosing;
    with_gquad    = pf_params->model_details.gquad;
    rtype         = &(pf_params->model_details.rtype[0]);
    n             = S[0];

    /*
      The hc_local array provides row-wise access to hc->matrix, i.e.
      my_iindx. Using this in the cubic order loop for multiloops below
      results in way faster computation due to fewer cache misses. Also,
      it introduces only little memory overhead, e.g. ~450MB for
      sequences of length 30,000
    */
    char *hc_local = (char *)vrna_alloc(sizeof(char) * (((n + 1) * (n + 2)) /2 + 2));
    for(i = 1; i <= n; i++)
      for(j = i; j <= n; j++)
        hc_local[my_iindx[i] - j] = hard_constraints[jindx[j] + i];

    FLT_OR_DBL *prm_l  = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
    FLT_OR_DBL *prm_l1 = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
    FLT_OR_DBL *prml   = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));

    Qmax=0;

    /* 1. exterior pair i,j and initialization of pr array */
    if(circular){
      bppm_circ(vc);
    } else {
      for (i=1; i<=n; i++) {
        for (j=i; j<=MIN2(i+turn,n); j++)
          probs[my_iindx[i]-j] = 0.;

        for (j=i+turn+1; j<=n; j++) {
          ij = my_iindx[i]-j;
          if((hc_local[ij] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP) && (qb[ij] > 0.)){
            type      = (unsigned char)ptype[jindx[j] + i];

            if(type == 0)
              type = 7;

            probs[ij] = q1k[i-1]*qln[j+1]/q1k[n];
            probs[ij] *= exp_E_ExtLoop(type, (i>1) ? S1[i-1] : -1, (j<n) ? S1[j+1] : -1, pf_params);
            if(sc){
              if(sc->exp_f){
                probs[ij] *= sc->exp_f(1, n, i, j, VRNA_DECOMP_EXT_STEM_OUTSIDE, sc->data);
              }
            }
          } else
            probs[ij] = 0.;
        }
      }
    } /* end if(!circular)  */

    for (l = n; l > turn + 1; l--) {

      /* 2. bonding k,l as substem of 2:loop enclosed by i,j */
      for(k = 1; k < l - turn; k++){
        kl      = my_iindx[k]-l;
        type_2  = (unsigned char)ptype[jindx[l] + k];
        type_2  = rtype[type_2];

        if(type_2 == 0)
          type_2 = 7;

        if (qb[kl]==0.) continue;

        if(hc_local[kl] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP_ENC){
          for(i = MAX2(1, k - MAXLOOP - 1); i <= k - 1; i++){
            u1 = k - i - 1;
            if(hc_up_int[i+1] < u1) continue;

            for(j = l + 1; j <= MIN2(l + MAXLOOP - k + i + 2, n); j++){
              u2 = j-l-1;
              if(hc_up_int[l+1] < u2) break;

              ij = my_iindx[i] - j;
              if(hc_local[ij] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP){
                type = (unsigned char)ptype[jindx[j] + i];

                if(type == 0)
                  type = 7;

                if(probs[ij] > 0){
                  tmp2 =  probs[ij]
                          * scale[u1 + u2 + 2]
                          * exp_E_IntLoop(u1, u2, type, type_2, S1[i+1], S1[j-1], S1[k-1], S1[l+1], pf_params);

                  if(sc){
                    if(sc->exp_energy_up)
                      tmp2 *=   sc->exp_energy_up[i+1][u1]
                              * sc->exp_energy_up[l+1][u2];

                    if(sc->exp_energy_bp)
                      tmp2 *=   sc->exp_energy_bp[ij];

                    if(sc->exp_energy_stack){
                      if((i+1 == k) && (j-1 == l)){
                        tmp2 *=   sc->exp_energy_stack[i]
                                * sc->exp_energy_stack[k]
                                * sc->exp_energy_stack[l]
                                * sc->exp_energy_stack[j];
                      }
                    }

                    if(sc->exp_f){
                      tmp2 *= sc->exp_f(i, j, k, l, VRNA_DECOMP_PAIR_IL, sc->data);
                      if(sc->bt){ /* store probability correction for auxiliary pairs in interior loop motif */
                        vrna_basepair_t *ptr, *aux_bps;
                        aux_bps = sc->bt(i, j, k, l, VRNA_DECOMP_PAIR_IL, sc->data);
                        for(ptr = aux_bps; ptr && ptr->i != 0; ptr++){
                          bp_correction[corr_cnt].i = ptr->i;
                          bp_correction[corr_cnt].j = ptr->j;
                          bp_correction[corr_cnt++].p = tmp2 * qb[kl];
                          if(corr_cnt == corr_size){
                            corr_size += 5;
                            bp_correction = vrna_realloc(bp_correction, sizeof(vrna_plist_t) * corr_size);
                          }
                        }
                        free(aux_bps);
                      }
                    
                    }
                  }

                  probs[kl] += tmp2;
                }
              }
            }
          }
        }
      }

      if(with_gquad){
        /* 2.5. bonding k,l as gquad enclosed by i,j */
        double *expintern = &(pf_params->expinternal[0]);
        FLT_OR_DBL qe;

        if(l < n - 3){
          for(k = 2; k <= l - VRNA_GQUAD_MIN_BOX_SIZE; k++){
            kl = my_iindx[k]-l;
            if (G[kl]==0.) continue;
            tmp2 = 0.;
            i = k - 1;
            for(j = MIN2(l + MAXLOOP + 1, n); j > l + 3; j--){
              ij = my_iindx[i] - j;
              type = (unsigned char)ptype[jindx[j] + i];
              if(!type) continue;
              qe = (type > 2) ? pf_params->expTermAU : 1.;
              tmp2 +=   probs[ij]
                      * qe
                      * (FLT_OR_DBL)expintern[j-l-1]
                      * pf_params->expmismatchI[type][S1[i+1]][S1[j-1]]
                      * scale[2];
            }
            probs[kl] += tmp2 * G[kl];
          }
        }

        if (l < n - 1){
          for (k=3; k<=l-VRNA_GQUAD_MIN_BOX_SIZE; k++) {
            kl = my_iindx[k]-l;
            if (G[kl]==0.) continue;
            tmp2 = 0.;
            for (i=MAX2(1,k-MAXLOOP-1); i<=k-2; i++){
              u1 = k - i - 1;
              for (j=l+2; j<=MIN2(l + MAXLOOP - u1 + 1,n); j++) {
                ij = my_iindx[i] - j;
                type = (unsigned char)ptype[jindx[j] + i];
                if(!type) continue;
                qe = (type > 2) ? pf_params->expTermAU : 1.;
                tmp2 +=   probs[ij]
                        * qe
                        * (FLT_OR_DBL)expintern[u1+j-l-1]
                        * pf_params->expmismatchI[type][S1[i+1]][S1[j-1]]
                        * scale[2];
              }
            }
            probs[kl] += tmp2 * G[kl];
          }
        }

        if(l < n){
          for(k = 4; k <= l - VRNA_GQUAD_MIN_BOX_SIZE; k++){
            kl = my_iindx[k]-l;
            if (G[kl]==0.) continue;
            tmp2 = 0.;
            j = l + 1;
            for (i=MAX2(1,k-MAXLOOP-1); i < k - 3; i++){
              ij = my_iindx[i] - j;
              type = (unsigned char)ptype[jindx[j] + i];
              if(!type) continue;
              qe = (type > 2) ? pf_params->expTermAU : 1.;
              tmp2 +=   probs[ij]
                      * qe
                      * (FLT_OR_DBL)expintern[k - i - 1]
                      * pf_params->expmismatchI[type][S1[i+1]][S1[j-1]]
                      * scale[2];
            }
            probs[kl] += tmp2 * G[kl];
          }
        }
      }

      /* 3. bonding k,l as substem of multi-loop enclosed by i,j */
      prm_MLb = 0.;
      if (l<n)
        for (k = 2; k < l - turn; k++) {
          kl    = my_iindx[k] - l;
          i     = k - 1;
          prmt  = prmt1 = 0.0;

          ii = my_iindx[i];     /* ii-j=[i,j]     */
          tt = (unsigned char)ptype[jindx[l+1] + i];
          tt = rtype[tt];

          if(tt == 0)
            tt = 7;

          if(hc_local[ii - (l + 1)] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){
            prmt1 = probs[ii-(l+1)]
                    * expMLclosing
                    * exp_E_MLstem(tt, S1[l], S1[i+1], pf_params);

            if(sc){
              /* which decompositions are covered here? => (k-1, l+1) -> enclosing pair */
              if(sc->exp_energy_bp)
                prmt1 *= sc->exp_energy_bp[ii - (l+1)];

/*
              if(sc->exp_f)
                prmt1 *= sc->exp_f(i, l+1, k, l, , sc->data);
*/
            }
          }
          int lj;
          short s3;
          FLT_OR_DBL ppp;
          ij = my_iindx[i] - (l+2);
          lj = my_iindx[l+1]-(l+1);
          s3 = S1[i+1];
          for (j = l + 2; j<=n; j++, ij--, lj--){
            if(hc_local[ij] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){
              tt = (unsigned char)md->pair[S1[j]][S1[i]];
              /* which decomposition is covered here? =>
                i + 1 = k < l < j:
                (i,j)       -> enclosing pair
                (k, l)      -> enclosed pair
                (l+1, j-1)  -> multiloop part with at least one stem
                a.k.a. (k,l) is left-most stem in multiloop closed by (k-1, j)
              */
              ppp = probs[ij]
                    * exp_E_MLstem(tt, S1[j-1], s3, pf_params)
                    * qm[lj];

              if(sc){
                if(sc->exp_energy_bp)
                  ppp *= sc->exp_energy_bp[ij];
/*
                if(sc->exp_f)
                  ppp *= sc->exp_f(i, j, l+1, j-1, , sc->data);
*/
              }
              prmt += ppp;
            }
          }
          prmt *= expMLclosing;

          tt        =   ptype[jindx[l] + k];

          prml[ i]  =   prmt;

          /* l+1 is unpaired */
          if(hc->up_ml[l+1]){
            ppp = prm_l1[i] * expMLbase[1];
            if(sc){
              if(sc->exp_energy_up)
                ppp *= sc->exp_energy_up[l+1][1];

/*
              if(sc_exp_f)
                ppp *= sc->exp_f(, sc->data);
*/
            }
            prm_l[i] = ppp + prmt1;
          } else { /* skip configuration where l+1 is unpaired */
            prm_l[i] = prmt1;
          }

          /* i is unpaired */
          if(hc->up_ml[i]){
            ppp = prm_MLb*expMLbase[1];
            if(sc){
              if(sc->exp_energy_up)
                ppp *= sc->exp_energy_up[i][1];

/*
              if(sc->exp_f)
                ppp *= sc->exp_f(, sc->data);
*/
            }

            prm_MLb = ppp + prml[i];
            /* same as:    prm_MLb = 0;
               for (i=1; i<=k-1; i++) prm_MLb += prml[i]*expMLbase[k-i-1]; */

          } else { /* skip all configurations where i is unpaired */
            prm_MLb = prml[i];
          }

          prml[i] = prml[i] + prm_l[i];

          if(with_gquad){
            if ((!tt) && (G[kl] == 0.)) continue;
          } else {
            if (qb[kl] == 0.) continue;
          }

          if(hc_local[kl] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC){

            temp = prm_MLb;

            for (i=1;i<=k-2; i++)
              temp += prml[i]*qm[my_iindx[i+1] - (k-1)];

            if(with_gquad){
              if(tt)
                temp    *= exp_E_MLstem(tt, (k>1) ? S1[k-1] : -1, (l<n) ? S1[l+1] : -1, pf_params) * scale[2];
              else
                temp    *= G[kl] * expMLstem * scale[2];
            } else {

              if(tt == 0)
                tt = 7;

              temp    *= exp_E_MLstem(tt, (k>1) ? S1[k-1] : -1, (l<n) ? S1[l+1] : -1, pf_params) * scale[2];
            }

            probs[kl]  += temp;

            if (probs[kl]>Qmax) {
              Qmax = probs[kl];
              if (Qmax>max_real/10.)
                fprintf(stderr, "P close to overflow: %d %d %g %g\n",
                  i, j, probs[kl], qb[kl]);
            }
            if (probs[kl]>=max_real) {
              ov++;
              probs[kl]=FLT_MAX;
            }
          }
        } /* end for (k=..) */
      tmp = prm_l1; prm_l1=prm_l; prm_l=tmp;

    }  /* end for (l=..)   */

    if(sc && sc->f && sc->bt){
      for (i=1; i<=n; i++)
        for (j=i+turn+1; j<=n; j++) {
          ij = my_iindx[i]-j;
          /*  search for possible auxiliary base pairs in hairpin loop motifs to store
              the corresponding probability corrections
          */ 
          if(hc_local[ij] & VRNA_CONSTRAINT_CONTEXT_HP_LOOP){
            vrna_basepair_t *ptr, *aux_bps;
            aux_bps = sc->bt(i, j, i, j, VRNA_DECOMP_PAIR_HP, sc->data);
            if(aux_bps){
              FLT_OR_DBL qhp = vrna_exp_E_hp_loop(vc, i, j);
              for(ptr = aux_bps; ptr && ptr->i != 0; ptr++){
                bp_correction[corr_cnt].i = ptr->i;
                bp_correction[corr_cnt].j = ptr->j;
                bp_correction[corr_cnt++].p = probs[ij] * qhp;
                if(corr_cnt == corr_size){
                  corr_size += 5;
                  bp_correction = vrna_realloc(bp_correction, sizeof(vrna_plist_t) * corr_size);
                }
              }
            }
            free(aux_bps);
          }
        }

      /*  correct pairing probabilities for auxiliary base pairs from hairpin-, or interior loop motifs
          as augmented by the generalized soft constraints feature
      */
      for(i = 0; i < corr_cnt; i++){
        ij = my_iindx[bp_correction[i].i] - bp_correction[i].j;
        /* printf("correcting pair %d, %d by %f\n", bp_correction[i].i, bp_correction[i].j, bp_correction[i].p); */
        probs[ij] += bp_correction[i].p / qb[ij];
      }
    }
    free(bp_correction);

    for (i=1; i<=n; i++)
      for (j=i+turn+1; j<=n; j++) {
        ij = my_iindx[i]-j;

        if(with_gquad){
          if (qb[ij] > 0.)
            probs[ij] *= qb[ij];

          if (G[ij] > 0.){
            probs[ij] += q1k[i-1] * G[ij] * qln[j+1]/q1k[n];
          }
        } else {
          if (qb[ij] > 0.)
            probs[ij] *= qb[ij];
        }
      }

    if (structure!=NULL){
      char *s = vrna_db_from_probs(probs, (unsigned int)n);
      memcpy(structure, s, n);
      structure[n] = '\0';
      free(s);
    }
    if (ov>0) fprintf(stderr, "%d overflows occurred while backtracking;\n"
        "you might try a smaller pf_scale than %g\n",
        ov, pf_params->pf_scale);

    /* clean up */
    free(prm_l);
    free(prm_l1);
    free(prml);

    free(hc_local);
  } /* end if((S != NULL) && (S1 != NULL))  */
  else
    vrna_message_error("bppm calculations have to be done after calling forward recursion\n");

  return;
}

PRIVATE FLT_OR_DBL
numerator_single( vrna_fold_compound_t *vc,
                  int i,
                  int j){

  return 1.;
}

PRIVATE FLT_OR_DBL
numerator_comparative(vrna_fold_compound_t *vc,
                      int i,
                      int j){

  int     *pscore = vc->pscore;             /* precomputed array of pair types */                      
  double  kTn     = vc->exp_params->kT/10.; /* kT in cal/mol  */
  int     *jindx  = vc->jindx;

  return exp(pscore[jindx[j]+i]/kTn);
}

/* calculate base pairing probs */
PRIVATE INLINE void
bppm_circ(vrna_fold_compound_t *vc){

  unsigned char     type;
  char              *ptype, *sequence;
  char              *hard_constraints;
  short             *S, *S1;
  int               n, i,j,k,l, ij, *rtype, *my_iindx, *jindx, turn;
  FLT_OR_DBL        tmp2, expMLclosing, *qb, *qm, *qm1, *probs, *scale, *expMLbase, qo;
  vrna_hc_t         *hc;
  vrna_exp_param_t  *pf_params;
  vrna_mx_pf_t      *matrices;
  vrna_md_t         *md;
  FLT_OR_DBL (*numerator_f)(vrna_fold_compound_t *vc, int i, int j);

  pf_params         = vc->exp_params;
  md                = &(pf_params->model_details);
  S                 = vc->sequence_encoding2;
  S1                = vc->sequence_encoding;
  my_iindx          = vc->iindx;
  jindx             = vc->jindx;
  ptype             = vc->ptype;
  turn              = md->min_loop_size;
  hc                = vc->hc;
  matrices          = vc->exp_matrices;
  qb                = matrices->qb;
  qm                = matrices->qm;
  qm1               = matrices->qm1;
  probs             = matrices->probs;
  scale             = matrices->scale;
  expMLbase         = matrices->expMLbase;
  qo                = matrices->qo;
  hard_constraints  = hc->matrix;
  sequence          = vc->sequence;


  expMLclosing  = pf_params->expMLclosing;
  rtype         = &(pf_params->model_details.rtype[0]);
  n             = S[0];

  switch(vc->type){
    case  VRNA_VC_TYPE_SINGLE:    numerator_f = numerator_single;
                                  break;
    case  VRNA_VC_TYPE_ALIGNMENT: numerator_f = numerator_comparative;
                                  break;
    default:                      numerator_f = NULL;
                                  break;
  }

  /*
    The hc_local array provides row-wise access to hc->matrix, i.e.
    my_iindx. Using this in the cubic order loop for multiloops below
    results in way faster computation due to fewer cache misses. Also,
    it introduces only little memory overhead, e.g. ~450MB for
    sequences of length 30,000
  */
  char *hc_local = (char *)vrna_alloc(sizeof(char) * (((n + 1) * (n + 2)) /2 + 2));
  for(i = 1; i <= n; i++)
    for(j = i; j <= n; j++)
      hc_local[my_iindx[i] - j] = hard_constraints[jindx[j] + i];

  /* 1. exterior pair i,j */
  for (i=1; i<=n; i++) {
    for (j=i; j<=MIN2(i+turn,n); j++)
      probs[my_iindx[i]-j] = 0;
    for (j=i+turn+1; j<=n; j++) {
      ij = my_iindx[i]-j;
      if(qb[ij] > 0.){
        probs[ij] = numerator_f(vc, i, j)/qo;

        type = (unsigned char)ptype[jindx[j] + i];

        unsigned char rt = rtype[type];

        /* 1.1. Exterior Hairpin Contribution */
        tmp2 = vrna_exp_E_hp_loop(vc, j, i);

        /* 1.2. Exterior Interior Loop Contribution                     */
        /* 1.2.1. i,j  delimtis the "left" part of the interior loop    */
        /* (j,i) is "outer pair"                                        */
        for(k=1; k < i-turn-1; k++){
          int ln1, lstart;
          ln1 = k + n - j - 1;
          if(ln1>MAXLOOP) break;
          lstart = ln1+i-1-MAXLOOP;
          if(lstart<k+turn+1) lstart = k + turn + 1;
          for(l=lstart; l < i; l++){
            int ln2, type_2;
            type_2 = (unsigned char)ptype[jindx[l] + k];
            if (type_2==0) continue;
            ln2 = i - l - 1;
            if(ln1+ln2>MAXLOOP) continue;
            tmp2 += qb[my_iindx[k] - l]
                    * exp_E_IntLoop(ln1,
                                    ln2,
                                    rt,
                                    rtype[type_2],
                                    S1[j+1],
                                    S1[i-1],
                                    S1[k-1],
                                    S1[l+1],
                                    pf_params)
                    * scale[ln1 + ln2];
          }
        }
        /* 1.2.2. i,j  delimtis the "right" part of the interior loop  */
        for(k=j+1; k < n-turn; k++){
          int ln1, lstart;
          ln1 = k - j - 1;
          if((ln1 + i - 1)>MAXLOOP) break;
          lstart = ln1+i-1+n-MAXLOOP;
          if(lstart<k+turn+1) lstart = k + turn + 1;
          for(l=lstart; l <= n; l++){
            int ln2, type_2;
            type_2 = (unsigned char)ptype[jindx[l] + k];
            if (type_2==0) continue;
            ln2 = i - 1 + n - l;
            if(ln1+ln2>MAXLOOP) continue;
            tmp2 += qb[my_iindx[k] - l]
                    * exp_E_IntLoop(ln2,
                                    ln1,
                                    rtype[type_2],
                                    rt,
                                    S1[l+1],
                                    S1[k-1],
                                    S1[i-1],
                                    S1[j+1],
                                    pf_params)
                    * scale[ln1 + ln2];
          }
        }
        /* 1.3 Exterior multiloop decomposition */
        /* 1.3.1 Middle part                    */
        if((i>turn+2) && (j<n-turn-1))
          tmp2 += qm[my_iindx[1]-i+1]
                  * qm[my_iindx[j+1]-n]
                  * expMLclosing
                  * exp_E_MLstem(type, S1[i-1], S1[j+1], pf_params);

        /* 1.3.2 Left part  */
        for(k=turn+2; k < i-turn-2; k++)
          tmp2 += qm[my_iindx[1]-k]
                  * qm1[jindx[i-1]+k+1]
                  * expMLbase[n-j]
                  * expMLclosing
                  * exp_E_MLstem(type, S1[i-1], S1[j+1], pf_params);

        /* 1.3.3 Right part */
        for(k=j+turn+2; k < n-turn-1;k++)
          tmp2 += qm[my_iindx[j+1]-k]
                  * qm1[jindx[n]+k+1]
                  * expMLbase[i-1]
                  * expMLclosing
                  * exp_E_MLstem(type, S1[i-1], S1[j+1], pf_params);

        /* all exterior loop decompositions for pair i,j done  */
        probs[ij] *= tmp2;

      }
      else probs[ij] = 0;
    }
  }
}



PRIVATE double
wrap_mean_bp_distance(FLT_OR_DBL *p,
                      int length,
                      int *index,
                      int turn){

  int         i,j;
  double      d = 0.;

  /* compute the mean base pair distance in the thermodynamic ensemble */
  /* <d> = \sum_{a,b} p_a p_b d(S_a,S_b)
     this can be computed from the pair probs p_ij as
     <d> = \sum_{ij} p_{ij}(1-p_{ij}) */

  for (i=1; i<=length; i++)
    for (j=i+turn+1; j<=length; j++)
      d += p[index[i]-j] * (1-p[index[i]-j]);

  return 2*d;
}


PUBLIC double
vrna_mean_bp_distance_pr( int length,
                          FLT_OR_DBL *p){

  int *index = vrna_idx_row_wise((unsigned int) length);
  double d;

  if (p==NULL)
    vrna_message_error("vrna_mean_bp_distance_pr: p==NULL. You need to supply a valid probability matrix");

  d = wrap_mean_bp_distance(p, length, index, TURN);

  free(index);
  return d;
}

PUBLIC double
vrna_mean_bp_distance(vrna_fold_compound_t *vc){

  if(!vc){
    vrna_message_error("vrna_mean_bp_distance: run vrna_pf_fold first!");
  } else if(!vc->exp_matrices){
    vrna_message_error("vrna_mean_bp_distance: exp_matrices==NULL!");
  } else if( !vc->exp_matrices->probs){
    vrna_message_error("vrna_mean_bp_distance: probs==NULL!");
  }

  return wrap_mean_bp_distance( vc->exp_matrices->probs,
                                vc->length,
                                vc->iindx,
                                vc->exp_params->model_details.min_loop_size);
}

PUBLIC vrna_plist_t *
vrna_stack_prob(vrna_fold_compound_t *vc, double cutoff){

  vrna_plist_t             *pl;
  int               i, j, plsize, turn, length, *index, *jindx, *rtype, num;
  char              *ptype;
  FLT_OR_DBL        *qb, *probs, *scale, p;
  vrna_exp_param_t  *pf_params;
  vrna_mx_pf_t      *matrices;

  plsize  = 256;
  pl      = NULL;
  num     = 0;

  if(vc){
    pf_params = vc->exp_params;
    length    = vc->length;
    index     = vc->iindx;
    jindx     = vc->jindx;
    rtype     = &(pf_params->model_details.rtype[0]);
    ptype     = vc->ptype;
    matrices  = vc->exp_matrices;
    qb        = matrices->qb;
    probs     = matrices->probs;
    scale     = matrices->scale;
    turn      = pf_params->model_details.min_loop_size;

    pl        = (vrna_plist_t *) vrna_alloc(plsize*sizeof(vrna_plist_t));

    for (i=1; i<length; i++)
      for (j=i+turn+3; j<=length; j++) {
        if((p=probs[index[i]-j]) < cutoff) continue;
        if (qb[index[i+1]-(j-1)]<FLT_MIN) continue;
        p *= qb[index[i+1]-(j-1)]/qb[index[i]-j];
        p *= exp_E_IntLoop(0,0,(unsigned char)ptype[jindx[j]+i],rtype[(unsigned char)ptype[jindx[j-1] + i+1]],
                           0,0,0,0, pf_params)*scale[2];/* add *scale[u1+u2+2] */
        if (p>cutoff) {
          pl[num].i     = i;
          pl[num].j     = j;
          pl[num].type  = 0;
          pl[num++].p   = p;
          if (num>=plsize) {
            plsize *= 2;
            pl = vrna_realloc(pl, plsize*sizeof(vrna_plist_t));
          }
        }
      }
    pl[num].i=0;
  }

  return pl;
}


PRIVATE void
alipf_create_bppm(vrna_fold_compound_t *vc,
                  char *structure){

  int s;
  int i,j,k,l, ij, kl, ii, ll, tt, *type, ov=0;
  FLT_OR_DBL temp, prm_MLb;
#ifdef USE_FLOAT_PF
  FLT_OR_DBL Qmax=0.;
#endif
  FLT_OR_DBL prmt,prmt1;
  FLT_OR_DBL qbt1, *tmp, tmp2, tmp3;

  int             n_seq         = vc->n_seq;
  int             n             = vc->length;


  short             **S           = vc->S;                                                                   
  short             **S5          = vc->S5;     /*S5[s][i] holds next base 5' of i in sequence s*/            
  short             **S3          = vc->S3;     /*Sl[s][i] holds next base 3' of i in sequence s*/            
  char              **Ss          = vc->Ss;
  unsigned short    **a2s         = vc->a2s;                                                                   
  vrna_exp_param_t  *pf_params    = vc->exp_params;
  vrna_mx_pf_t      *matrices     = vc->exp_matrices;
  vrna_md_t         *md           = &(pf_params->model_details);
  vrna_hc_t         *hc           = vc->hc;
  vrna_sc_t         **sc          = vc->scs;
  int               *my_iindx     = vc->iindx;
  int               *jindx        = vc->jindx;
  FLT_OR_DBL        *q            = matrices->q;
  FLT_OR_DBL        *qb           = matrices->qb;
  FLT_OR_DBL        *qm           = matrices->qm;
  FLT_OR_DBL        *qm1          = matrices->qm1;
  FLT_OR_DBL        qo            = matrices->qo;
  int               *pscore       = vc->pscore;     /* precomputed array of pair types */                      
  int               *rtype        = &(md->rtype[0]);
  int               circular      = md->circ;
  FLT_OR_DBL        *scale        = matrices->scale;
  FLT_OR_DBL        *expMLbase    = matrices->expMLbase;
  FLT_OR_DBL        expMLclosing  = pf_params->expMLclosing;
  FLT_OR_DBL        *probs        = matrices->probs;
  char              *hard_constraints = hc->matrix;

  double kTn;
  FLT_OR_DBL pp;

  FLT_OR_DBL *prm_l   = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
  FLT_OR_DBL *prm_l1  = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
  FLT_OR_DBL *prml    = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
  type                = (int *)vrna_alloc(sizeof(int) * n_seq);

  if((matrices->q1k == NULL) || (matrices->qln == NULL)){
    free(matrices->q1k);
    matrices->q1k = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+1));
    free(matrices->qln);
    matrices->qln = (FLT_OR_DBL *) vrna_alloc(sizeof(FLT_OR_DBL)*(n+2));
  }

  FLT_OR_DBL *q1k    = matrices->q1k;
  FLT_OR_DBL *qln    = matrices->qln;

  for (k=1; k<=n; k++) {
    q1k[k] = q[my_iindx[1] - k];
    qln[k] = q[my_iindx[k] - n];
  }
  q1k[0] = 1.0;
  qln[n+1] = 1.0;


  kTn = pf_params->kT/10.;   /* kT in cal/mol  */

  for (i=0; i<=n; i++)
    prm_l[i]=prm_l1[i]=prml[i]=0;

  /* 1. exterior pair i,j and initialization of pr array */
  if(circular){
    for (i=1; i<=n; i++) {
      for (j=i; j<=MIN2(i+TURN,n); j++) probs[my_iindx[i]-j] = 0;
      for (j=i+TURN+1; j<=n; j++) {
        ij = my_iindx[i]-j;
        if (qb[ij]>0.) {
          probs[ij] =  exp(pscore[jindx[j]+i]/kTn)/qo;

          /* get pair types  */
          for (s=0; s<n_seq; s++) {
            type[s] = md->pair[S[s][j]][S[s][i]];
            if (type[s]==0) type[s]=7;
          }

          tmp2 = 0.;

          /* 1.1. Exterior Hairpin Contribution */
          tmp2 += vrna_exp_E_hp_loop(vc, j, i);

          /* 1.2. Exterior Interior Loop Contribution */
          /* recycling of k and l... */
          if(hard_constraints[jindx[j] + i] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP){

            /* 1.2.1. first we calc exterior loop energy with constraint, that i,j  */
            /* delimtis the "right" part of the interior loop                       */
            /* (l,k) is "outer pair"                                                */
            for(k=1; k < i-TURN-1; k++){
              /* so first, lets calc the length of loop between j and k */
              int ln1, lstart;
              ln1 = k + n - j - 1;
              if(ln1>MAXLOOP)
                break;
              if(hc->up_int[j+1] < ln1)
                break;

              lstart = ln1+i-1-MAXLOOP;
              if(lstart<k+TURN+1) lstart = k + TURN + 1;
              for(l=lstart; l < i; l++){
                int ln2,ln2a,ln1a, type_2;
                ln2 = i - l - 1;
                if(ln1+ln2>MAXLOOP)
                  continue;
                if(hc->up_int[l+1] < ln2)
                  continue;
                if(!(hard_constraints[jindx[l] + k] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP))
                  continue;
                
                FLT_OR_DBL qloop=1.;
                if(qb[my_iindx[k]-l]==0.){
                  qloop=0.;
                  continue;
                }

                for (s=0; s<n_seq; s++){
                  ln2a= a2s[s][i-1];
                  ln2a-=a2s[s][l];
                  ln1a= a2s[s][n]-a2s[s][j];
                  ln1a+=a2s[s][k-1];
                  type_2 = md->pair[S[s][l]][S[s][k]];
                  if (type_2 == 0) type_2 = 7;
                  qloop *= exp_E_IntLoop(ln1a, ln2a, type[s], type_2,
                              S[s][j+1],
                              S[s][i-1],
                              S[s][(k>1) ? k-1 : n],
                              S[s][l+1], pf_params);
                }
                if(sc)
                  for(s = 0; s < n_seq; s++){
                    if(sc[s]){
                      ln2a= a2s[s][i-1];
                      ln2a-=a2s[s][l];
                      ln1a= a2s[s][n]-a2s[s][j];
                      ln1a+=a2s[s][k-1];

                      if(sc[s]->exp_energy_up)
                        qloop *=    sc[s]->exp_energy_up[a2s[s][l]+1][ln2a]
                                  * ((j < n) ? sc[s]->exp_energy_up[a2s[s][j]+1][a2s[s][n] - a2s[s][j]] : 1.)
                                  * ((k > 1) ? sc[s]->exp_energy_up[1][a2s[s][k]-1] : 1.);

                      if((ln1a + ln2a == 0) && sc[s]->exp_energy_stack){
                        if(S[s][i] && S[s][j] && S[s][k] && S[s][l]){ /* don't allow gaps in stack */
                          qloop *=    sc[s]->exp_energy_stack[a2s[s][k]]
                                    * sc[s]->exp_energy_stack[a2s[s][l]]
                                    * sc[s]->exp_energy_stack[a2s[s][i]]
                                    * sc[s]->exp_energy_stack[a2s[s][j]];
                        }
                      }
                    }
                  }
                tmp2 += qb[my_iindx[k] - l] * qloop * scale[ln1+ln2];
              }
            }

            /* 1.2.2. second we calc exterior loop energy with constraint, that i,j */
            /* delimtis the "left" part of the interior loop                        */
            /* (j,i) is "outer pair"                                                */
            for(k=j+1; k < n-TURN; k++){
              /* so first, lets calc the length of loop between l and i */
              int ln1, lstart;
              ln1 = k - j - 1;
              if((ln1 + i - 1)>MAXLOOP)
                break;
              if(hc->up_int[j+1] < ln1)
                break;

              lstart = ln1+i-1+n-MAXLOOP;
              if(lstart<k+TURN+1) lstart = k + TURN + 1;
              for(l=lstart; l <= n; l++){
                int ln2, type_2;
                ln2 = i - 1 + n - l;
                if(ln1+ln2>MAXLOOP)
                  continue;
                if(hc->up_int[l+1] < ln2)
                  continue;
                if(!(hard_constraints[jindx[l] + k] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP))
                  continue;

                FLT_OR_DBL qloop=1.;
                if(qb[my_iindx[k]-l]==0.){
                  qloop=0.;
                  continue;
                }

                for (s=0; s<n_seq; s++){
                  ln1 = a2s[s][k] - a2s[s][j+1];
                  ln2 = a2s[s][i-1] + a2s[s][n] - a2s[s][l];
                  type_2 = md->pair[S[s][l]][S[s][k]];
                  if (type_2 == 0) type_2 = 7;
                  qloop *= exp_E_IntLoop(ln2, ln1, type_2, type[s],
                          S3[s][l],
                          S5[s][k],
                          S5[s][i],
                          S3[s][j], pf_params);
                }
                if(sc)
                  for(s = 0; s < n_seq; s++){
                    if(sc[s]){
                      ln1 = a2s[s][k] - a2s[s][j+1];
                      ln2 = a2s[s][i-1] + a2s[s][n] - a2s[s][l];

                      if(sc[s]->exp_energy_up)
                        qloop *=    sc[s]->exp_energy_up[a2s[s][j]+1][ln1]
                                  * ((l < n) ? sc[s]->exp_energy_up[a2s[s][l]+1][a2s[s][n] - a2s[s][l]] : 1.)
                                  * ((i > 1) ? sc[s]->exp_energy_up[1][a2s[s][i]-1] : 1.);

                      if((ln1 + ln2 == 0) && sc[s]->exp_energy_stack){
                        if(S[s][i] && S[s][j] && S[s][k] && S[s][l]){ /* don't allow gaps in stack */
                          qloop *=    sc[s]->exp_energy_stack[a2s[s][k]]
                                    * sc[s]->exp_energy_stack[a2s[s][l]]
                                    * sc[s]->exp_energy_stack[a2s[s][i]]
                                    * sc[s]->exp_energy_stack[a2s[s][j]];
                        }
                      }
                    }
                  }
                tmp2 += qb[my_iindx[k] - l] * qloop * scale[ln1+ln2];
              }
            }
          }
          /* 1.3 Exterior multiloop decomposition */
          if(hard_constraints[jindx[j] + i] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){
            /* 1.3.1 Middle part                    */
            if((i>TURN+2) && (j<n-TURN-1)){

              for (tmp3=1, s=0; s<n_seq; s++){
                tmp3 *= exp_E_MLstem(rtype[type[s]], S5[s][i], S3[s][j], pf_params);
              }
              tmp2 += qm[my_iindx[1]-i+1] * qm[my_iindx[j+1]-n] * tmp3 * pow(expMLclosing,n_seq);
            }
            /* 1.3.2 Left part    */
            for(k=TURN+2; k < i-TURN-2; k++){
              if(hc->up_ml[j+1] < n-j)
                break;

              for (tmp3=1, s=0; s<n_seq; s++){
                tmp3 *= exp_E_MLstem(rtype[type[s]], S5[s][i], S3[s][j], pf_params);
              }

              if(sc)
                for(s = 0; s < n_seq; s++){
                  if(sc[s]){
                    if(sc[s]->exp_energy_bp)
                      tmp3 *= sc[s]->exp_energy_bp[jindx[j] + i];

                    if(sc[s]->exp_energy_up)
                      tmp3 *= sc[s]->exp_energy_up[a2s[s][j]+1][a2s[s][n]-a2s[s][j]];
                  }
                }

              tmp2 += qm[my_iindx[1]-k] * qm1[jindx[i-1]+k+1] * tmp3 * expMLbase[n-j] * pow(expMLclosing,n_seq);
            }
            /* 1.3.3 Right part    */
            for(k=j+TURN+2; k < n-TURN-1;k++){
              if(hc->up_ml[1] < i-1)
                break;

              for (tmp3=1, s=0; s<n_seq; s++){
                tmp3 *= exp_E_MLstem(rtype[type[s]], S5[s][i], S3[s][j], pf_params);
              }

              if(sc)
                for(s = 0; s < n_seq; s++){
                  if(sc[s]){
                    if(sc[s]->exp_energy_bp)
                      tmp3 *= sc[s]->exp_energy_bp[jindx[j] + i];

                    if(sc[s]->exp_energy_up)
                      tmp3 *= sc[s]->exp_energy_up[a2s[s][1]][a2s[s][i]-a2s[s][1]];
                  }
                }

              tmp2 += qm[my_iindx[j+1]-k] * qm1[jindx[n]+k+1] * tmp3 * expMLbase[i-1] * pow(expMLclosing,n_seq);
            }
          }
          probs[ij] *= tmp2;
        }
        else probs[ij] = 0;
      }  /* end for j=..*/
    }  /* end or i=...  */
  } /* end if(circular)  */
  else{
    for (i=1; i<=n; i++) {
      for (j=i; j<=MIN2(i+TURN,n); j++)
        probs[my_iindx[i]-j] = 0;

      for (j=i+TURN+1; j<=n; j++) {
        ij = my_iindx[i]-j;
        if ((qb[ij] > 0.) && (hard_constraints[jindx[j] + i] & VRNA_CONSTRAINT_CONTEXT_EXT_LOOP)){
          probs[ij] = q1k[i-1] * qln[j+1]/q1k[n] * exp(pscore[jindx[j]+i]/kTn);
          for (s=0; s<n_seq; s++) {
            int typ;
            typ = md->pair[S[s][i]][S[s][j]]; if (typ==0) typ=7;
            probs[ij] *= exp_E_ExtLoop(typ, (i>1) ? S5[s][i] : -1, (j<n) ? S3[s][j] : -1, pf_params);
          }
        } else
          probs[ij] = 0;
      }
    }
  } /* end if(!circular)  */
  for (l=n; l>TURN+1; l--) {

    /* 2. bonding k,l as substem of 2:loop enclosed by i,j */
    for (k=1; k<l-TURN; k++) {
      pp = 0.;
      kl = my_iindx[k]-l;
      if (qb[kl] == 0.) continue;
      if(!(hard_constraints[jindx[l] + k] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP_ENC)) continue;

      for (s=0; s<n_seq; s++) {
        type[s] = md->pair[S[s][l]][S[s][k]];
        if (type[s]==0) type[s]=7;
      }

      for (i=MAX2(1,k-MAXLOOP-1); i<=k-1; i++){
        if(hc->up_int[i+1] < k - i - 1)
          continue;

        for (j=l+1; j<=MIN2(l+ MAXLOOP -k+i+2,n); j++) {
          FLT_OR_DBL qloop=1;
          ij = my_iindx[i] - j;

          if(probs[ij] == 0.) continue;
          if(!(hard_constraints[jindx[j] + i] & VRNA_CONSTRAINT_CONTEXT_INT_LOOP)) continue;
          if(hc->up_int[l+1] < j - l - 1) break;

          for (s=0; s<n_seq; s++) {
            int typ, u1, u2;
            u1 = a2s[s][k-1] - a2s[s][i];
            u2 = a2s[s][j-1] - a2s[s][l];
            typ = md->pair[S[s][i]][S[s][j]]; if (typ==0) typ=7;
            qloop *=  exp_E_IntLoop(u1, u2, typ, type[s], S3[s][i], S5[s][j], S5[s][k], S3[s][l], pf_params);
          }

          if(sc){
            for(s = 0; s < n_seq; s++){
              if(sc[s]){
                int u1, u2;
                u1 = a2s[s][k-1] - a2s[s][i];
                u2 = a2s[s][j-1] - a2s[s][l];
/*
                u1 = k - i - 1;
                u2 = j - l - 1;
*/
                if(sc[s]->exp_energy_bp)
                  qloop *= sc[s]->exp_energy_bp[jindx[j] + i];

                if(sc[s]->exp_energy_up)
                  qloop *=    sc[s]->exp_energy_up[a2s[s][i]+1][u1]
                              * sc[s]->exp_energy_up[a2s[s][l]+1][u2];

                if(sc[s]->exp_energy_stack)
                  if(u1 + u2 == 0){
                    if(S[s][i] && S[s][j] && S[s][k] && S[s][l]){ /* don't allow gaps in stack */
                      qloop *=    sc[s]->exp_energy_stack[i]
                                * sc[s]->exp_energy_stack[k]
                                * sc[s]->exp_energy_stack[l]
                                * sc[s]->exp_energy_stack[j];
                    }
                  }
              }
            }
          }
          pp += probs[ij]*qloop*scale[k-i + j-l];
        }
      }
      probs[kl] += pp * exp(pscore[jindx[l]+k]/kTn);
    }
    /* 3. bonding k,l as substem of multi-loop enclosed by i,j */
    prm_MLb = 0.;
    if (l<n)
      for (k=2; k<l-TURN; k++) {
      i = k-1;
      prmt = prmt1 = 0.;

      if(1 /* hard_constraints[jindx[l] + k] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP_ENC */){
        ii = my_iindx[i];     /* ii-j=[i,j]     */
        ll = my_iindx[l+1];   /* ll-j=[l+1,j-1] */
        if(hard_constraints[jindx[l+1] + i] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP){
          prmt1 = probs[ii-(l+1)];
          for (s=0; s<n_seq; s++) {
            tt = md->pair[S[s][l+1]][S[s][i]]; if (tt==0) tt=7;
            prmt1 *= exp_E_MLstem(tt, S5[s][l+1], S3[s][i], pf_params) * expMLclosing;
          }

          if(sc)
            for(s = 0; s < n_seq; s++){
              if(sc[s]){
                if(sc[s]->exp_energy_bp)
                  prmt1 *= sc[s]->exp_energy_bp[jindx[l+1] + i];
              }
            }
        }

        for (j=l+2; j<=n; j++){
          pp = 1.;
          if(probs[ii-j]==0) continue;
          if(!(hard_constraints[jindx[j] + i] & VRNA_CONSTRAINT_CONTEXT_MB_LOOP)) continue;

          for (s=0; s<n_seq; s++) {
            tt = md->pair[S[s][j]][S[s][i]]; if (tt==0) tt=7;
            pp *=  exp_E_MLstem(tt, S5[s][j], S3[s][i], pf_params) * expMLclosing;
          }

          if(sc)
            for(s = 0; s < n_seq; s++){
              if(sc[s]){
                if(sc[s]->exp_energy_bp)
                  pp *= sc[s]->exp_energy_bp[jindx[j] + i];
              }
            }

          prmt +=  probs[ii-j] * pp * qm[ll-(j-1)];
        }
        kl = my_iindx[k]-l;

        prml[ i] = prmt;

        pp = 0.;
        if(hc->up_ml[l+1]){
          pp = prm_l1[i] * expMLbase[1];
          if(sc)
            for(s = 0; s < n_seq; s++){
              if(sc[s]){
                if(sc[s]->exp_energy_up)
                  pp *= sc[s]->exp_energy_up[a2s[s][l+1]][1];
              }
            }
        }
        prm_l[i] = pp + prmt1; /* expMLbase[1]^n_seq */

        pp = 0.;
        if(hc->up_ml[i]){
          pp = prm_MLb * expMLbase[1];
          if(sc)
            for(s = 0; s < n_seq; s++){
              if(sc[s]){
                if(sc[s]->exp_energy_up)
                  pp *= sc[s]->exp_energy_up[a2s[s][i]][1];
              }
            }
        }
        prm_MLb = pp + prml[i];

        /* same as:    prm_MLb = 0;
           for (i=1; i<=k-1; i++) prm_MLb += prml[i]*expMLbase[k-i-1]; */

        prml[i] = prml[ i] + prm_l[i];

        if (qb[kl] == 0.) continue;

        temp = prm_MLb;

        for (i=1;i<=k-2; i++)
          temp += prml[i]*qm[my_iindx[i+1] - (k-1)];

        for (s=0; s<n_seq; s++) {
          tt=md->pair[S[s][k]][S[s][l]]; if (tt==0) tt=7;
          temp *= exp_E_MLstem(tt, S5[s][k], S3[s][l], pf_params);
        }
        probs[kl] += temp * scale[2] * exp(pscore[jindx[l]+k]/kTn);
      } else { /* (k,l) not allowed to be substem of multiloop closed by (i,j) */
        prml[i] = prm_l[i] = prm_l1[i] = 0.;
      }

#ifdef USE_FLOAT_PF
      if (probs[kl]>Qmax) {
        Qmax = probs[kl];
        if (Qmax>FLT_MAX/10.)
          fprintf(stderr, "%d %d %g %g\n", i,j,probs[kl],qb[kl]);
      }
      if (probs[kl]>FLT_MAX) {
        ov++;
        probs[kl]=FLT_MAX;
      }
#endif
    } /* end for (k=2..) */
    tmp = prm_l1; prm_l1=prm_l; prm_l=tmp;

  }  /* end for (l=..)   */

  for (i=1; i<=n; i++)
    for (j=i+TURN+1; j<=n; j++) {
      ij = my_iindx[i]-j;
      probs[ij] *= qb[ij] *exp(-pscore[jindx[j]+i]/kTn);
    }

  if (structure!=NULL){
    char *s = vrna_db_from_probs(probs, (unsigned int)n);
    memcpy(structure, s, n);
    structure[n] = '\0';
    free(s);
  }

  if (ov>0) fprintf(stderr, "%d overflows occurred while backtracking;\n"
        "you might try a smaller pf_scale than %g\n",
        ov, pf_params->pf_scale);

  free(type);
  free(prm_l);
  free(prm_l1);
  free(prml);
}

