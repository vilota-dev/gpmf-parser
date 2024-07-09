/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 2.5.0
 *
 *  (C) Copyright 2017-2020 GoPro Inc (http://gopro.com/).
 *
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "../GPMF_parser.h"
#include "GPMF_mp4reader.h"
#include "../GPMF_utils.h"

#define	SHOW_ALL_PAYLOADS			1
#define	SHOW_SCALED_DATA			1
#define	SHOW_THIS_FOUR_CC			STR2FOURCC("ACCL")
#define SHOW_COMPUTED_SAMPLERATES	1



extern void PrintGPMF(GPMF_stream* ms);


uint32_t show_all_payloads = SHOW_ALL_PAYLOADS;
uint32_t show_scaled_data = SHOW_SCALED_DATA;
uint32_t show_computed_samplerates = SHOW_COMPUTED_SAMPLERATES;
uint32_t show_this_four_cc = 0;

int mp4fuzzchanges = 0;
int gpmffuzzchanges = 4;
int resetfuzzloopcount = 0;
int fuzzloopcount = 0;

FILE* outputFile = NULL;

void openOutputFile(char* filename) {
	outputFile = fopen(filename, "w");
	if (outputFile == NULL) {
		printf("Error opening file %s for writing.\n", filename);
		exit(EXIT_FAILURE);
	}
}

// Function to close the output file
void closeOutputFile() {
	if (outputFile != NULL) {
		fclose(outputFile);
	}
}

// Function to write to the output file
void writeToFile(const char* format, ...) {
	if (outputFile != NULL) {
		va_list args;
		va_start(args, format);
		vfprintf(outputFile, format, args);
		va_end(args);
	}
}

GPMF_ERR readMP4File(char* filename);

int read_mp4(char* mp4Directory, char* flag_f)
{
	char outputFileName[8]; 
	snprintf(outputFileName, sizeof(outputFileName), "../../../vk_tools\src\govbag\python\fourccdata/%s.txt", flag_f);

    openOutputFile(outputFileName);

	GPMF_ERR ret = GPMF_OK;

	show_this_four_cc = STR2FOURCC(flag_f);

	do
	{
		ret = readMP4File(mp4Directory);

		if (fuzzloopcount) printf("%5d/%5d\b\b\b\b\b\b\b\b\b\b\b", resetfuzzloopcount - fuzzloopcount + 1, resetfuzzloopcount);
	} while (ret == GPMF_OK && --fuzzloopcount > 0);
	return 0;
	closeOutputFile();
}


char* CorruptTheMP4(char* filename)
{
	char fuzzname[256];
	uint8_t buffer[65536];
	uint64_t len, pos;
	FILE* fpr = NULL;
	FILE* fpw = NULL;

#ifdef _WINDOWS
	sprintf_s(fuzzname, sizeof(fuzzname), "%s-fuzz.mp4", filename);
	fopen_s(&fpr, filename, "rb");
	if (fpr)
	{
		_fseeki64(fpr, 0, SEEK_END);
		len = (uint64_t)_ftelli64(fpr);
		_fseeki64(fpr, 0, SEEK_SET);

		fopen_s(&fpw, fuzzname, "wb");
#else
	sprintf(fuzzname, "%s-fuzz.mp4", filename);
	fpr = fopen(filename, "rb");
	if (fpr)
	{
		fseeko(fpr, 0, SEEK_END);
		len = (uint64_t)ftell(fpr);
		fseeko(fpr, 0, SEEK_SET);
		fpw = fopen(fuzzname, "rb");
#endif
		if (fpw)
		{
			for (pos = 0; pos < len;)
			{
				uint64_t bytes = (uint64_t)fread(buffer, 1, sizeof(buffer), fpr);

				if (bytes == 0) break;

				//fuzz mp4 indexing - mess-up the end of the MP4
				if (pos >= (len - 120000))
				{
					srand(mp4fuzzchanges * resetfuzzloopcount + (resetfuzzloopcount - fuzzloopcount) + gpmffuzzchanges);
					int times;
					for (times = 0; times < mp4fuzzchanges; times++)
					{
						int offset = rand() % (bytes);
						buffer[offset] = rand() & 0xff;
					}
				}
				fwrite(buffer, 1, (size_t)bytes, fpw);
				pos += bytes;
			}
			fclose(fpw);
			filename = fuzzname;
		}
		fclose(fpr);
	}

	return filename;
	}


GPMF_ERR readMP4File(char* filename)
{
	openOutputFile("output.txt");

	GPMF_ERR ret = GPMF_OK;
	GPMF_stream metadata_stream = { 0 }, * ms = &metadata_stream;
	double metadatalength;
	uint32_t* payload = NULL;
	uint32_t payloadsize = 0;
	size_t payloadres = 0;
#if 1 // Search for GPMF Track
	size_t mp4handle = OpenMP4Source(filename, MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE, 0);
#else // look for a global GPMF payload in the moov header, within 'udta'
	size_t mp4handle = OpenMP4SourceUDTA(argv[1], 0);  //Search for GPMF payload with MP4's udta
#endif

	// FUZZ: Corrupt the MP4 index to test the parser and the mp4reader
	if (mp4fuzzchanges)
	{
		CloseSource(mp4handle);
		filename = CorruptTheMP4(filename);

		mp4handle = OpenMP4Source(filename, MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE, 0);
		if (mp4handle == 0)	return  GPMF_OK; // when fuzzing, errors reported are showing the system is working.
	}

	metadatalength = GetDuration(mp4handle);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberPayloads(mp4handle);
		//		printf("found %.2fs of metadata, from %d payloads, within %s\n", metadatalength, payloads, argv[1]);

		uint32_t fr_num, fr_dem;
		uint32_t frames = GetVideoFrameRateAndCount(mp4handle, &fr_num, &fr_dem);

		for (index = 0; index < payloads; index++)
		{
			double in = 0.0, out = 0.0; //times
			payloadsize = GetPayloadSize(mp4handle, index);
			payloadres = GetPayloadResource(mp4handle, payloadres, payloadsize);
			payload = GetPayload(mp4handle, payloadres, index);
			if (payload == NULL)
				goto cleanup;


			// FUZZ: Corrupt the GPMF playload to test the parser
			if (gpmffuzzchanges && fuzzloopcount)
			{
				srand(mp4fuzzchanges * resetfuzzloopcount + (resetfuzzloopcount - fuzzloopcount) + gpmffuzzchanges);
				for (int times = 0; times < gpmffuzzchanges; times++)
				{
					uint8_t* byteptr = (uint8_t*)payload;
					byteptr[rand() % payloadsize] = rand() & 0xff;
				}
			}


			ret = GetPayloadTime(mp4handle, index, &in, &out);
			if (ret != GPMF_OK)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;

			if (show_scaled_data)
			{
				if (show_all_payloads || index == 0)
				{
					while (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("STRM"), GPMF_RECURSE_LEVELS | GPMF_TOLERANT)) //GoPro Hero5/6/7 Accelerometer)
					{
						if (GPMF_VALID_FOURCC(show_this_four_cc))
						{
							if (GPMF_OK != GPMF_FindNext(ms, show_this_four_cc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
								continue;
						}
						else
						{
							ret = GPMF_SeekToSamples(ms);
							if (GPMF_OK != ret)
								continue;
						}

						char* rawdata = (char*)GPMF_RawData(ms);
						uint32_t key = GPMF_Key(ms);
						GPMF_SampleType type = GPMF_Type(ms);
						uint32_t samples = GPMF_Repeat(ms);
						uint32_t elements = GPMF_ElementsInStruct(ms);

						if (samples)
						{
							uint32_t buffersize = samples * elements * sizeof(double);
							GPMF_stream find_stream;
							double* ptr, * tmpbuffer = (double*)malloc(buffersize);

							#define MAX_UNITS	64
							#define MAX_UNITLEN	8
							char units[MAX_UNITS][MAX_UNITLEN] = { "" };
							uint32_t unit_samples = 1;

							char complextype[MAX_UNITS] = { "" };
							uint32_t type_samples = 1;

							if (tmpbuffer)
							{
								uint32_t i, j;

								//Search for any units to display
								GPMF_CopyState(ms, &find_stream);
								if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL | GPMF_TOLERANT) ||
									GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL | GPMF_TOLERANT))
								{
									char* data = (char*)GPMF_RawData(&find_stream);
									uint32_t ssize = GPMF_StructSize(&find_stream);
									if (ssize > MAX_UNITLEN - 1) ssize = MAX_UNITLEN - 1;
									unit_samples = GPMF_Repeat(&find_stream);

									for (i = 0; i < unit_samples && i < MAX_UNITS; i++)
									{
										memcpy(units[i], data, ssize);
										units[i][ssize] = 0;
										data += ssize;
									}
								}

								//Search for TYPE if Complex
								GPMF_CopyState(ms, &find_stream);
								type_samples = 0;
								if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL | GPMF_TOLERANT))
								{
									char* data = (char*)GPMF_RawData(&find_stream);
									uint32_t ssize = GPMF_StructSize(&find_stream);
									if (ssize > MAX_UNITLEN - 1) ssize = MAX_UNITLEN - 1;
									type_samples = GPMF_Repeat(&find_stream);

									for (i = 0; i < type_samples && i < MAX_UNITS; i++)
									{
										complextype[i] = data[i];
									}
								}

								//GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
								if (GPMF_OK == GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE))//Output scaled data as floats
								{

									ptr = tmpbuffer;
									int pos = 0;
									for (i = 0; i < samples; i++)
									{
										if (fuzzloopcount == 0);

										for (j = 0; j < elements; j++)
										{
											if (type == GPMF_TYPE_STRING_ASCII)
											{
												if (fuzzloopcount == 0) writeToFile("%c", rawdata[pos]);
												pos++;
												ptr++;
											}
											else if (type_samples == 0) //no TYPE structure
											{
												if (fuzzloopcount == 0) writeToFile("%.17f ", *ptr++);
											}
											else if (complextype[j] != 'F')
											{
												if (fuzzloopcount == 0) writeToFile("%.17f ", *ptr++);
												pos += GPMF_SizeofType((GPMF_SampleType)complextype[j]);
											}
											else if (type_samples && complextype[j] == GPMF_TYPE_FOURCC)
											{
												ptr++;
												if (fuzzloopcount == 0) writeToFile("%c%c%c%c, ", rawdata[pos], rawdata[pos + 1], rawdata[pos + 2], rawdata[pos + 3]);
												pos += GPMF_SizeofType((GPMF_SampleType)complextype[j]);
											}
										}

										if (fuzzloopcount == 0) writeToFile("\n");
									}
								}
								free(tmpbuffer);
							}
						}
					}
					GPMF_ResetState(ms);
				}
			}
		}


		if (show_computed_samplerates)
		{
			mp4callbacks cbobject;
			cbobject.mp4handle = mp4handle;
			cbobject.cbGetNumberPayloads = GetNumberPayloads;
			cbobject.cbGetPayload = GetPayload;
			cbobject.cbGetPayloadSize = GetPayloadSize;
			cbobject.cbGetPayloadResource = GetPayloadResource;
			cbobject.cbGetPayloadTime = GetPayloadTime;
			cbobject.cbFreePayloadResource = FreePayloadResource;
			cbobject.cbGetEditListOffsetRationalTime = GetEditListOffsetRationalTime;

			if (fuzzloopcount == 0) writeToFile("COMPUTED SAMPLERATES:\n");
			// Find all the available Streams and compute they sample rates
			while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
			{
				if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
				{
					double start, end;
					uint32_t fourcc = GPMF_Key(ms);

					double rate = GetGPMFSampleRate(cbobject, fourcc, STR2FOURCC("SHUT"), GPMF_SAMPLE_RATE_PRECISE, &start, &end);// GPMF_SAMPLE_RATE_FAST);
					if (fuzzloopcount == 0) writeToFile("  %c%c%c%c %.32f %.17f %.17f\n", PRINTF_4CC(fourcc), rate, start, end);
				}
			}
		}

	cleanup:
		if (payloadres) FreePayloadResource(mp4handle, payloadres);
		if (ms) GPMF_Free(ms);
		CloseSource(mp4handle);
	}

	if (fuzzloopcount == 0 && ret != GPMF_OK)
	{
		if (GPMF_ERROR_UNKNOWN_TYPE == ret)
			printf("Unknown GPMF Type within\n");
		else
			printf("GPMF data has corruption\n");
	}
	else
	{
		ret = GPMF_OK; // when fuzzing, errors reported are showing the system is working.
	}

	closeOutputFile();

	return ret;
}
