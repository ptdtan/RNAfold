#ifndef VIENNA_RNA_PACKAGE_MFE_H
#define VIENNA_RNA_PACKAGE_MFE_H

#include <stdio.h>
#include <ViennaRNA/data_structures.h>

/**
 *  @addtogroup mfe_fold
 *  @ingroup folding_routines
 *  @brief This section covers all functions and variables related to the calculation
 *  of minimum free energy (MFE) structures.
 *
 *  The library provides a fast dynamic programming minimum free energy
 *  folding algorithm as described in @cite zuker:1981.
 *  All relevant parts that directly implement the "Zuker & Stiegler" algorithm for single
 *  sequences are described in this section.
 *
 *  Folding of circular RNA sequences is handled as a post-processing step of the forward
 *  recursions. See @cite hofacker:2006 for further details.
 *
 *  Nevertheless, the RNAlib also
 *  provides interfaces for the prediction of consensus MFE structures of sequence alignments,
 *  MFE structure for two hybridized sequences, local optimal structures and many more. For
 *  those more specialized variants of MFE folding routines, please consult the appropriate
 *  subsections (Modules) as listed above.
 *
 *  @file mfe.h
 *  @brief MFE calculations for single RNA sequences
 *
 *  This file includes (almost) all function declarations within the RNAlib that are related to
 *  MFE folding...
 */

/**
 *  @brief Compute minimum free energy and an appropriate secondary
 *  structure of an RNA sequence, or RNA sequence alignment
 *
 *  Depending on the type of the provided #vrna_fold_compound_t, this function
 *  predicts the MFE for a single sequence, or a corresponding averaged MFE for
 *  a sequence alignment. If backtracking is activated, it also constructs the
 *  corresponding secondary structure, or consensus structure.
 *  Therefore, the second parameter, @a structure, has to point to an allocated
 *  block of memory with a size of at least @f$\mathrm{strlen}(\mathrm{sequence})+1@f$ to
 *  store the backtracked MFE structure. (For consensus structures, this is the length of
 *  the alignment + 1. If @p NULL is passed, no backtracking will be performed.
 *
 *  @ingroup mfe_fold
 *
 *  @note This function is polymorphic. It accepts #vrna_fold_compound_t of type
 *        #VRNA_VC_TYPE_SINGLE, and #VRNA_VC_TYPE_ALIGNMENT.
 *
 *  @see #vrna_fold_compound_t, vrna_fold_compound(), vrna_fold(), vrna_circfold(),
 *        vrna_fold_compound_comparative(), vrna_alifold(), vrna_circalifold()
 *
 *  @param vc             fold compound
 *  @param structure      A pointer to the character array where the
 *                        secondary structure in dot-bracket notation will be written to (Maybe NULL)
 *
 *  @return the minimum free energy (MFE) in kcal/mol
 */
float
vrna_mfe(vrna_fold_compound_t *vc,
          char *structure);

/**
 *  @brief Compute the minimum free energy of two interacting RNA molecules
 *
 *  The code is analog to the vrna_mfe() function.
 *
 *  @ingroup mfe_cofold
 *
 *  @param    vc  fold compound
 *  @param    structure Will hold the barcket dot structure of the dimer molecule
 *  @return   minimum free energy of the structure
 */
float vrna_mfe_dimer( vrna_fold_compound_t *vc,
                      char *structure);

/**
 *  @brief Local MFE prediction using a sliding window approach.
 *
 *  Computes minimum free energy structures using a sliding window
 *  approach, where base pairs may not span outside the window.
 *  In contrast to vrna_mfe(), where a maximum base pair span
 *  may be set using the #vrna_md_t.max_bp_span attribute and one
 *  globally optimal structure is predicted, this function uses a
 *  sliding window to retrieve all locally optimal structures within
 *  each window.
 *  The size of the sliding window is set in the #vrna_md_t.window_size
 *  attribute, prior to the retrieval of the #vrna_fold_compound_t
 *  using vrna_fold_compound() with option #VRNA_OPTION_WINDOW
 *
 *  The predicted structures are written on-the-fly, either to
 *  stdout, if a NULL pointer is passed as file parameter, or to
 *  the corresponding filehandle.
 *
 *  @ingroup local_mfe_fold
 * 
 *  @see  vrna_fold_compound(), vrna_mfe_window_zscore(), vrna_mfe(),
 *        vrna_Lfold(), vrna_Lfoldz(),
 *        #VRNA_OPTION_WINDOW, #vrna_md_t.max_bp_span, #vrna_md_t.window_size
 *
 *  @param  vc        The #vrna_fold_compound_t with preallocated memory for the DP matrices
 *  @param  file      The output file handle where predictions are written to (maybe NULL)
 */
float vrna_mfe_window( vrna_fold_compound_t *vc, FILE *file);

#ifdef USE_SVM
/**
 *  @brief Local MFE prediction using a sliding window approach (with z-score cut-off)
 *
 *  Computes minimum free energy structures using a sliding window
 *  approach, where base pairs may not span outside the window.
 *  This function is the z-score version of vrna_mfe_window(), i.e.
 *  only predictions above a certain z-score cut-off value are
 *  printed.
 *  As for vrna_mfe_window(), the size of the sliding window is set in
 *  the #vrna_md_t.window_size attribute, prior to the retrieval of
 *  the #vrna_fold_compound_t using vrna_fold_compound() with option
 *  #VRNA_OPTION_WINDOW.
 *
 *  The predicted structures are written on-the-fly, either to
 *  stdout, if a NULL pointer is passed as file parameter, or to
 *  the corresponding filehandle.
 *
 *  @ingroup local_mfe_fold
 * 
 *  @see  vrna_fold_compound(), vrna_mfe_window_zscore(), vrna_mfe(),
 *        vrna_Lfold(), vrna_Lfoldz(),
 *        #VRNA_OPTION_WINDOW, #vrna_md_t.max_bp_span, #vrna_md_t.window_size
 *
 *  @param  vc        The #vrna_fold_compound_t with preallocated memory for the DP matrices
 *  @param  min_z     The minimal z-score for a predicted structure to appear in the output
 *  @param  file      The output file handle where predictions are written to (maybe NULL)
 */
float vrna_mfe_window_zscore(vrna_fold_compound_t *vc, double min_z, FILE *file);
#endif

void
vrna_backtrack_from_intervals(vrna_fold_compound_t *vc,
                              vrna_bp_stack_t *bp_stack,
                              sect bt_stack[],
                              int s);


#endif
