#ifndef GPMF_DEMO_H
#define GPMF_DEMO_H

#include <stdint.h> 
#include "../GPMF_parser.h" 
#include "GPMF_mp4reader.h"
#include "../GPMF_utils.h"  

#ifdef __cplusplus
extern "C" {
#endif

void openOutputFile(const char* filename);
void closeOutputFile(void);
void writeToFile(const char* format, ...);
int read_mp4(const char* mp4Directory, const char* flag_f);
char* CorruptTheMP4(const char* filename);
GPMF_ERR readMP4File(const char* filename);
extern void PrintGPMF(GPMF_stream* ms);

#ifdef __cplusplus
}
#endif

#endif 
