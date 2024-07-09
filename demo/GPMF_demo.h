#ifndef GPMF_DEMO_H
#define GPMF_DEMO_H

#include <stdint.h> 
#include "../GPMF_parser.h"

extern int read_mp4(char* mp4Directory, char* flag_f);
extern char* CorruptTheMP4(char* filename);
extern GPMF_ERR readMP4File(char* filename);
extern void PrintGPMF(GPMF_stream* ms);

#endif 
