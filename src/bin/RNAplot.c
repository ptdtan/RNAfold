/*
  Plot RNA structures using different layout algorithms
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "ViennaRNA/utils.h"
#include "ViennaRNA/PS_dot.h"
#include "ViennaRNA/constraints.h"
#include "ViennaRNA/file_formats.h"
#include "RNAplot_cmdl.h"

#define PRIVATE static

int main(int argc, char *argv[]){
  struct RNAplot_args_info  args_info;
  char                      *structure, *pre, *post, fname[FILENAME_MAX_LENGTH], ffname[FILENAME_MAX_LENGTH],
                            *rec_sequence, *rec_id, **rec_rest, format[5]="ps";
  unsigned int              rec_type, read_opt;
  int                       i, length, istty;
  vrna_md_t                 md;

  structure = pre = post = NULL;
  length = 0;
  vrna_md_set_default(&md);

  /*
  #############################################
  # check the command line parameters
  #############################################
  */
  if(RNAplot_cmdline_parser (argc, argv, &args_info) != 0) exit(1);

  if(args_info.layout_type_given) rna_plot_type = args_info.layout_type_arg;
  if(args_info.pre_given)         pre           = strdup(args_info.pre_arg);
  if(args_info.post_given)        post          = strdup(args_info.post_arg);
  if(args_info.output_format_given){
    strncpy(format, args_info.output_format_arg, 4); format[4] = '\0';
  }

  /* free allocated memory of command line data structure */
  RNAplot_cmdline_parser_free (&args_info);

  /*
  #############################################
  # begin initializing
  #############################################
  */
  rec_type      = read_opt = 0;
  rec_id        = rec_sequence = NULL;
  rec_rest      = NULL;
  istty         = isatty(fileno(stdout)) && isatty(fileno(stdin));

  /* set options we wanna pass to vrna_file_fasta_read_record() */
  if(istty){
    read_opt |= VRNA_INPUT_NOSKIP_BLANK_LINES;
    vrna_message_input_seq("Input sequence (upper or lower case) followed by structure");
  }

  /*
  #############################################
  # main loop: continue until end of file
  #############################################
  */
  while(
    !((rec_type = vrna_file_fasta_read_record(&rec_id, &rec_sequence, &rec_rest, NULL, read_opt))
        & (VRNA_INPUT_ERROR | VRNA_INPUT_QUIT))){

    if(rec_id){
      (void) sscanf(rec_id, ">%" XSTR(FILENAME_ID_LENGTH) "s", fname);
    }
    else fname[0] = '\0';

    length = (int)strlen(rec_sequence);

    structure = vrna_extract_record_rest_structure((const char **)rec_rest, 0, (rec_id) ? VRNA_OPTION_MULTILINE : 0);

    if(!structure) vrna_message_error("structure missing");
    if((int)strlen(structure) != length) vrna_message_error("structure and sequence differ in length!");

    if (fname[0]!='\0'){
      strcpy(ffname, fname);
      strcat(ffname, "_ss");
    } else
      strcpy(ffname, "rna");

    structure = NULL;
    unsigned int struct_options = (rec_id) ? VRNA_OPTION_MULTILINE : 0;
    structure = vrna_extract_record_rest_structure((const char **)rec_rest, 0, struct_options);

    if(strlen(rec_sequence) != strlen(structure))
      vrna_message_error("sequence and structure have unequal length");

    switch (format[0]) {
      case 'p':
        strcat(ffname, ".ps");

        (void) vrna_file_PS_rnaplot_a(rec_sequence, structure, ffname, pre, post, &md);

        break;
      case 'g':
        strcat(ffname, ".gml");
        gmlRNA(rec_sequence, structure, ffname, 'x');
        break;
      case 'x':
        strcat(ffname, ".ss");
        xrna_plot(rec_sequence, structure, ffname);
        break;
      case 's':
        strcat(ffname, ".svg");
        svg_rna_plot(rec_sequence, structure, ffname);
        break;
      default:
        RNAplot_cmdline_parser_print_help(); exit(EXIT_FAILURE);
    }

    fflush(stdout);

    /* clean up */
    if(rec_id) free(rec_id);
    free(rec_sequence);
    free(structure);
    /* free the rest of current dataset */
    if(rec_rest){
      for(i=0;rec_rest[i];i++) free(rec_rest[i]);
      free(rec_rest);
    }
    rec_id = rec_sequence = structure = NULL;
    rec_rest = NULL;

    /* print user help for the next round if we get input from tty */
    if(istty){
      vrna_message_input_seq("Input sequence (upper or lower case) followed by structure");
    }
  }

  return EXIT_SUCCESS;
}
