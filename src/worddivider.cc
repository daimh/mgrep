#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "global.h"
#include "worddivider.h"
WordDivider::WordDivider(const char *filename) {
  worddivider_tail_ = 0;
  worddivider_buf_ = NULL;
  if (!filename ) Error(301, "contact developer");
  if (!strcmp(filename, "NonAlphanumeric")) {
    worddivider_tail_ = 65;
    worddivider_buf_ = (int*) malloc(worddivider_tail_ * sizeof(int));
    int i=0;
    for (int c=1; c<128; c++) {
      if (c >= '0' && c <= '9') continue;
      if (c >= 'a' && c <= 'z') continue;
      if (c >= 'A' && c <= 'Z') continue;
      worddivider_buf_[i++] = c;
    }
    if (i != worddivider_tail_) Error(302, "contact developer");
  } else if (strcmp(filename, "None")) {
    FILE *file = fopen(filename, "r");
    if (!file ) Error(303, "%s: No such a file", filename);
    size_t line_size = 0;
    ssize_t line_tail;
    char *line_buf = NULL;
    int line_num = 0, worddivider_size = 0;
    while ((line_tail = getline(&line_buf, &line_size, file)) != -1) {
      line_num++;
      if (line_tail > 0 && line_buf[line_tail-1] == '\n')
        line_buf[--line_tail] = 0;
      if (line_tail > 0 && line_buf[line_tail-1] == '\r')
        line_buf[--line_tail] = 0;
      if (!line_tail || line_buf[0] == ';') continue;
      char *delim_loc = strchr(line_buf, ';');
      if (delim_loc) *delim_loc = 0;
      int code = HexToUnicode(line_buf);
      if (code == -1)
        Error(304, "Worddivider file line '%d' has invalid hex", line_num);
      if (worddivider_size == worddivider_tail_) {
        worddivider_size += 1024;
        worddivider_buf_ = (int*) realloc(worddivider_buf_,
            worddivider_size * sizeof(int));
      }
      worddivider_buf_[worddivider_tail_++] = code;
      if (worddivider_tail_ == INT_MAX)
        Error(305, "Not a correct word-divider file");
    }
    if (worddivider_tail_ == 0)
      Error(306, "Empty word-divider file is not allowed");
    if (line_buf) free(line_buf);
    fclose(file);
    qsort(worddivider_buf_, worddivider_tail_, sizeof(int), IntCompare);
  }
}
WordDivider::~WordDivider() {
  if (worddivider_buf_) free(worddivider_buf_);
}
bool WordDivider::IsBoundary(char *s) {
  char *original = s;
  int code = ReadUnicodeOrDie(&s);
  if (IsDivider(code)) return true;
  original--;
  while (*original >> 6 == -2) original--;
  code = ReadUnicodeOrDie(&original);
  return IsDivider(code);
}
bool WordDivider::IsDivider(int code) {
  return !code || bsearch(&code, worddivider_buf_, worddivider_tail_,
      sizeof(int), IntCompare);
}
int WordDivider::GetTotal() {
  return worddivider_tail_;
}
int WordDivider::CopyDivider(int idx, char *buf) {
  assert(idx < worddivider_tail_);
  int utf8 = worddivider_buf_[idx];
  int bytes = 0;
  if (utf8 >> 7 == 0) {
    buf[0] = utf8;
    bytes = 1;
  } else if (utf8 >> 11 == 0) {
    buf[0] = utf8 >> 6 | 0xC0;
    buf[1] = (utf8 & 0x3F) | 0x80;
    bytes = 2;
  } else if (utf8 >> 16 == 0) {
    buf[0] = utf8 >> 12 | 0xE0;
    buf[1] = (utf8 >> 6 & 0x3F) | 0x80;
    buf[2] = (utf8 & 0x3F) | 0x80;
    bytes = 3;
  } else if (utf8 >> 21 == 0) {
    buf[0] = utf8 >> 18 | 0xF0;
    buf[1] = (utf8 >> 12 & 0x3F) | 0x80;
    buf[2] = (utf8 >> 6 & 0x3F) | 0x80;
    buf[3] = (utf8 & 0x3F) | 0x80;
    bytes = 4;
  } else if (utf8 >> 26 == 0) {
    buf[0] = utf8 >> 24 | 0xF8;
    buf[1] = (utf8 >> 18 & 0x3F) | 0x80;
    buf[2] = (utf8 >> 12 & 0x3F) | 0x80;
    buf[3] = (utf8 >> 6 & 0x3F) | 0x80;
    buf[4] = (utf8 & 0x3F) | 0x80;
    bytes = 5;
  } else if (utf8 >> 31 == 0) {
    buf[0] = utf8 >> 30 | 0xFC;
    buf[1] = (utf8 >> 24 & 0x3F) | 0x80;
    buf[2] = (utf8 >> 18 & 0x3F) | 0x80;
    buf[3] = (utf8 >> 12 & 0x3F) | 0x80;
    buf[4] = (utf8 >> 6 & 0x3F) | 0x80;
    buf[5] = (utf8 & 0x3F) | 0x80;
    bytes = 6;
  } else {
    Error(307, "contact developer please");
  }
  return bytes;
}
