#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "global.h"
#include "casefolding.h"
CaseFolding::CaseFolding(const char *filename) {
	assert(filename);
	code_tail_ = mapping_tail_ = 0;
	code_arr_ = NULL;
	mapping_buf_ = NULL;
	FILE *file = fopen(filename, "r");
	if (!file ) Error(401, "%s: No such a file", filename);
	size_t line_size = 0;
	ssize_t line_tail;
	char *line_buf = NULL;
	int line_num = 0;
	int mapping_size = 0;
	int code_size = 0;
	while ((line_tail = getline(&line_buf, &line_size, file)) != -1) {
		line_num++;
		if (line_tail > 0 && line_buf[line_tail-1] == '\n')
			line_buf[--line_tail] = 0;
		if (line_tail > 0 && line_buf[line_tail-1] == '\r')
			line_buf[--line_tail] = 0;
		if (!line_tail || line_buf[0] == '#') continue;
		char *status_ptr = strchr(line_buf, ';');
		if (!status_ptr)
			Error(402, "Casefolding file line '%d' parsing error", line_num);
		*status_ptr++ = 0;
		char *mapping_ptr = strchr(status_ptr, ';');
		if (!mapping_ptr)
			Error(403, "Casefolding file line '%d' parsing error", line_num);
		*mapping_ptr++ = 0;
		char *ending = strchr(mapping_ptr, ';');
		if (!ending)
			Error(404, "Casefolding file line '%d' parsing error", line_num);
		*ending = 0;
		while (*status_ptr == ' ') status_ptr++;
		//"F" will break printing original text 
		if (!strcmp(status_ptr, "C") || !strcmp(status_ptr, "S")) {
			if (code_size == code_tail_) {
				code_size += 1024;
				code_arr_ = (int(*)[3]) realloc(code_arr_,
						code_size * sizeof(int) * 3);
			}
			code_arr_[code_tail_][0] = HexToUnicode(line_buf);
			if (code_arr_[code_tail_][0] == -1)
				Error(405, "Casefolding file line %d has invalid hex",
						line_num);
			code_arr_[code_tail_][1] = mapping_tail_;
			while (true) {
				char *tail;
				int64_t rtn = strtol(mapping_ptr, &tail, 16);
				if ((*tail != ' ' && *tail) || rtn <= 0 || rtn > INT_MAX)
					Error(406, "Casefolding file line %d has non-unicode hex",
							line_num);
				char buf[6];
				int increment = UnicodeToChar(buf, (int)rtn);
				if (!increment)
					Error(407,
							 "Failed to convert line %d of casefolding file to UTF8",
							 line_num);
				if (mapping_tail_ + increment > mapping_size) {
					mapping_size += 1024;
					mapping_buf_ = (char*) realloc(mapping_buf_, mapping_size);
				}
				for (int i = 0; i < increment; i++)
					mapping_buf_[mapping_tail_++] = buf[i];
				if (!*tail) break;
				mapping_ptr = tail+1;
			}
			code_arr_[code_tail_++][2] = mapping_tail_;
		} else if (strcmp(status_ptr, "F") && strcmp(status_ptr, "T")) {
			Error(408, "Casefolding file line '%d' column 2 is not C/F/S/T",
					line_num);
		}
	}
	if (line_buf) free(line_buf);
	fclose(file);
	qsort(code_arr_, code_tail_, sizeof(int)*3, IntCompare);
}
CaseFolding::CaseFolding(FILE *file) {
	if (fread(&code_tail_, sizeof(code_tail_), 1, file) != 1)
		Error(409, "Failed to read from file");
	if (fread(&mapping_tail_, sizeof(mapping_tail_), 1, file) != 1)
		Error(410, "Failed to read from file");
	code_arr_ = (int(*)[3]) malloc(code_tail_ * sizeof(int) * 3);
	mapping_buf_ = (char*) malloc(mapping_tail_);
	if (!code_arr_ || !mapping_buf_)
		Error(411, "Failed to allocate memory");
	if (fread(code_arr_, sizeof(int)*3, code_tail_, file) != (size_t)code_tail_)
		Error(412, "Failed to read from file");
	if (fread(mapping_buf_, 1, mapping_tail_, file) != (size_t)mapping_tail_)
		Error(413, "Failed to read from file");
}
CaseFolding::~CaseFolding() {
	if (code_arr_) free(code_arr_);
	if (mapping_buf_) free(mapping_buf_);
}
void CaseFolding::Dump(FILE *file) {
	if (fwrite(&code_tail_, sizeof(code_tail_), 1, file) != 1)
		Error(414, "Failed to write to file");
	if (fwrite(&mapping_tail_, sizeof(mapping_tail_), 1, file) != 1)
		Error(415, "Failed to write to file");
	if (fwrite(code_arr_, sizeof(int)*3, code_tail_, file) != (size_t)code_tail_)
		Error(416, "Failed to write to file");
	if (fwrite(mapping_buf_, 1, mapping_tail_, file) != (size_t)mapping_tail_)
		Error(417, "Failed to write to file");
}
char *CaseFolding::FoldOrDie(char *input_buf, char **output_buf,
		size_t *output_size) {
	char **current_ptr = &input_buf;
	int code;
	char *previous_ptr = *current_ptr;
	size_t output_tail = 0;
	while ((code = ReadUnicodeOrDie(current_ptr)) != 0) {
		int *code_ptr = (int*)bsearch(&code, code_arr_, code_tail_, sizeof(int) * 3,
				IntCompare);
		size_t increment = code_ptr ? code_ptr[2] - code_ptr[1] :
				*current_ptr - previous_ptr;
		if (output_tail + increment >= *output_size) {
			*output_size += 1024;
			*output_buf = (char*) realloc(*output_buf, *output_size * sizeof(char));
		}
		if (code_ptr)
			memcpy(*output_buf+output_tail, mapping_buf_+code_ptr[1], increment);
		else
			memcpy(*output_buf+output_tail, previous_ptr, increment);
		output_tail += increment;
		previous_ptr = *current_ptr;
	}
	(*output_buf)[output_tail] = 0;
	return *output_buf;
}
int CaseFolding::UnicodeToChar(char *buf, int code) {
	if (!(code >> 7)) {
		buf[0] = code;
		return 1;
	} else if (!(code >> 11)) {
		buf[1] = (code & 0x3F) | 0x80;
		buf[0] = code >> 6 | 0xC0;
		return 2;
	} else if (!(code >> 16)) {
		buf[2] = (code & 0x3F) | 0x80;
		buf[1] = (code >> 6 & 0x3F) | 0x80;
		buf[0] = code >> 12 | 0xE0;
		return 3;
	} else if (!(code >> 21)) {
		buf[3] = (code & 0x3F) | 0x80;
		buf[2] = (code >> 6 & 0x3F) | 0x80;
		buf[1] = (code >> 12 & 0x3F) | 0x80;
		buf[0] = code >> 18 | 0xF0;
		return 4;
	} else if (!(code >> 26)) {
		buf[4] = (code & 0x3F) | 0x80;
		buf[3] = (code >> 6 & 0x3F) | 0x80;
		buf[2] = (code >> 12 & 0x3F) | 0x80;
		buf[1] = (code >> 18 & 0x3F) | 0x80;
		buf[0] = code >> 24 | 0xF8;
		return 5;
	} else if (!(code >> 31)) {
		buf[5] = (code & 0x3F) | 0x80;
		buf[4] = (code >> 6 & 0x3F) | 0x80;
		buf[3] = (code >> 12 & 0x3F) | 0x80;
		buf[2] = (code >> 18 & 0x3F) | 0x80;
		buf[1] = (code >> 24 & 0x3F) | 0x80;
		buf[0] = code >> 30 | 0xFC;
		return 6;
	} else {
		return 0;
	}
}
