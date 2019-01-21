#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <bits/local_lim.h>
#include "global.h"
#include "bwt.h"
#include "worddivider.h"
void BWT::Search(int64_t word_id, char *line, char *word, WordDivider *worddivider,
    int max_parallel_depth, size_t mismatch_limit) {
  printf("#%s\n", line);
  fflush(stdout);
  size_t word_len = strlen(word);
  if (!word_len) return;
  char *word_buf = (char*)malloc(word_len + 7);
  size_t matched_size = word_len + mismatch_limit + 12;
  //1 core dumped, changed to 12 for 2*UTF8 chars
  char *matched_buf = (char*)malloc(matched_size);
  if (!matched_buf || !word_buf) Error(501, "failed to allocate memory");
  ResultSet *result_set = new ResultSet();
  memcpy(word_buf, word, word_len);
  for (int i=-2; i<worddivider->GetTotal(); i++) {
    int divider_len;
    if (i == -2) {
      word_buf[word_len] = '\n';
      divider_len = 1;
    } else if (i == -1) {
      word_buf[word_len] = 0;
      divider_len = 1;
    } else {
      divider_len = worddivider->CopyDivider(i, word_buf+word_len);
    }
    word_buf[word_len+divider_len] = 0;
    assert(divider_len <= 6);
    unsigned char c = word_buf[word_len + divider_len - 1];
    matched_buf[0] = c;
    FuzzySearch(max_parallel_depth, 0, 0, worddivider, word_buf,
        word_len, word_len + divider_len - 1, c?first_[c-1]:0, first_[c],
        matched_buf, matched_size, 1, mismatch_limit, 0, 0, divider_len,
            result_set);
  }
  result_set->Print(word_id);
  delete result_set;
  free(matched_buf);
  free(word_buf);
}
void BWT::FuzzySearchThreadOrNot(int max_parallel_depth, int cur_parallel_depth,
    char skipped, class WordDivider *worddivider, char *word_buf,
    size_t divider_pos, size_t word_tail, size_t last_from, size_t last_to,
    char *matched_buf, size_t matched_size, size_t matched_tail,
    size_t mismatch_limit, size_t mismatch_num, int leading_divider_len,
    int trailing_divider_len, class ResultSet *result_set, pthread_t **th_arr,
    int *th_size, int *th_tail) {
  if (cur_parallel_depth <= max_parallel_depth &&
      last_from + kMinRangePerThread < last_to) {
    size_t *arg;
    arg = (size_t*) malloc(18 * sizeof(size_t) + matched_size);
    if (!arg) Error(502, "Failed to allocate memory");
    arg[0] = (size_t) this;  
    arg[1] = max_parallel_depth;
    arg[2] = cur_parallel_depth;
    arg[3] = skipped;
    arg[4] = (size_t)worddivider;
    arg[5] = (size_t)word_buf;
    arg[6] = divider_pos;
    arg[7] = word_tail;
    arg[8] = last_from;
    arg[9] = last_to;
    arg[10] = (size_t)(arg + 18);
    memcpy(arg+18, matched_buf, matched_tail);
    arg[11] = matched_size;
    arg[12] = matched_tail;
    arg[13] = mismatch_limit;
    arg[14] = mismatch_num;
    arg[15] = leading_divider_len;
    arg[16] = trailing_divider_len;
    arg[17] = (size_t)result_set;
    if (*th_size == *th_tail) {
      *th_size = *th_size + 64;
      *th_arr = (pthread_t*)realloc(*th_arr, (*th_size) * sizeof(pthread_t*));
      if (!th_arr) Error(503, "failed to allocate memory");
    }
    int rtn = pthread_create((*th_arr) + (*th_tail)++, NULL,
        BWT::FuzzySearchThreadWorker, arg);
    if (!rtn) return;
    (*th_tail)--;
    free(arg);
    static bool reported = false;
    if (!reported) {
      reported = true;
      Message(502, "pthread_create returned error '%d', try smaller "
          "'parallel-depth'.\n", rtn);
    }
  } 
  FuzzySearch(max_parallel_depth, cur_parallel_depth, skipped, worddivider,
      word_buf, divider_pos, word_tail, last_from, last_to, matched_buf,
      matched_size, matched_tail, mismatch_limit, mismatch_num,
      leading_divider_len, trailing_divider_len, result_set);
}
void* BWT::FuzzySearchThreadWorker(void *arg) {
  BWT *bwt = (BWT*) ((size_t*)arg)[0];
  int max_parallel_depth = (int)((size_t*)arg)[1];
  int cur_parallel_depth = (int)((size_t*)arg)[2];
  char skipped = (char)((size_t*)arg)[3];
  WordDivider *worddivider = (WordDivider *) ((size_t*)arg)[4];
  char *word_buf = (char*) ((size_t*)arg)[5];
  size_t divider_pos = (size_t)((size_t*)arg)[6];
  size_t word_tail = (size_t)((size_t*)arg)[7];
  size_t last_from = (size_t)((size_t*)arg)[8];
  size_t last_to = (size_t)((size_t*)arg)[9];
  char *matched_buf = (char*) ((size_t*)arg)[10];
  size_t matched_size = (size_t)((size_t*)arg)[11];
  size_t matched_tail = (size_t)((size_t*)arg)[12];
  size_t mismatch_limit = (size_t)((size_t*)arg)[13];
  size_t mismatch_num = (size_t)((size_t*)arg)[14];
  int leading_divider_len = (int)((size_t*)arg)[15];
  int trailing_divider_len = (int)((size_t*)arg)[16];
  ResultSet *result_set = (ResultSet *) ((size_t*)arg)[17];
  bwt->FuzzySearch(max_parallel_depth, cur_parallel_depth, skipped, worddivider,
      word_buf, divider_pos, word_tail, last_from, last_to, matched_buf,
      matched_size, matched_tail, mismatch_limit, mismatch_num,
      leading_divider_len, trailing_divider_len, result_set);
  free(arg);
  pthread_exit(0);
}
void BWT::FuzzySearch(int max_parallel_depth, int cur_parallel_depth,
    char skipped, class WordDivider *worddivider, char *word_buf,
    size_t divider_pos, size_t word_tail, size_t last_from, size_t last_to,
    char *matched_buf, size_t matched_size, size_t matched_tail,
    size_t mismatch_limit, size_t mismatch_num, int leading_divider_len,
    int trailing_divider_len, class ResultSet *result_set) {
//matched_buf[matched_tail] = 0;
//printf("daimh2\t%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,[%s]\n", worddivider != NULL,
//    mismatch_limit, divider_pos, word_tail, last_from, last_to, matched_tail,
//    mismatch_num, matched_buf);
  if (mismatch_num > mismatch_limit) return;
  pthread_t *th_arr = NULL;
  int th_size = 0, th_tail = 0;
  if (word_tail) {
    //skip one character on word
    if (word_tail <= divider_pos && skipped != 'C')
      FuzzySearchThreadOrNot(max_parallel_depth, cur_parallel_depth+1, 'W',
          worddivider, word_buf, divider_pos, word_tail-1, last_from, last_to,
          matched_buf, matched_size, matched_tail, mismatch_limit,
          mismatch_num+1, leading_divider_len, trailing_divider_len, result_set,
          &th_arr, &th_size, &th_tail);
    size_t *rank_arr = (size_t*) calloc(256, sizeof(size_t));
    size_t *counter_arr = (size_t*) calloc(256, sizeof(size_t));
    if (!rank_arr || !counter_arr) Error(504, "failed to allocate memory");
    if (last_from >> bit_of_bucket_size_)
      memcpy(rank_arr, bucket_counter_[(last_from >> bit_of_bucket_size_)-1],
          sizeof(size_t) * 256);
    for (size_t i = last_from >> bit_of_bucket_size_ << bit_of_bucket_size_;
        i<last_from; i++)
      rank_arr[last_[i]]++;
    for (; last_from < last_to; last_from++) 
      counter_arr[last_[last_from]]++;
    for (int i=0; i<256; i++) {
      if (worddivider && (i == 0 || i == '\n')) continue;
      if (!counter_arr[i]) continue;
        size_t next_from = rank_arr[i];
        if (i) next_from += first_[i-1];
      assert(matched_tail < matched_size);
      matched_buf[matched_tail] = i;
      //skip one character on corpus
      if (skipped != 'W' && i != 0 && i != '\n')
        FuzzySearchThreadOrNot(max_parallel_depth, cur_parallel_depth+1, 'C',
            worddivider, word_buf, divider_pos, word_tail, next_from,
            next_from+counter_arr[i], matched_buf, matched_size, matched_tail+1,
            mismatch_limit, mismatch_num+1, leading_divider_len,
            trailing_divider_len, result_set, &th_arr, &th_size, &th_tail);
      //no skipping on either
      if (i==word_buf[word_tail-1] || word_tail <= divider_pos)
        FuzzySearchThreadOrNot(max_parallel_depth, cur_parallel_depth+1, 0,
            worddivider, word_buf, divider_pos, word_tail-1, next_from,
            next_from+counter_arr[i], matched_buf, matched_size, matched_tail+1,
            mismatch_limit, mismatch_num + (i!=word_buf[word_tail-1]?1:0),
            leading_divider_len, trailing_divider_len, result_set, &th_arr,
            &th_size, &th_tail);
    }
    free(rank_arr);
    free(counter_arr);
  } else if (worddivider) {
    for (int i=-2; i<worddivider->GetTotal(); i++) {
      char divider_buf[7];
      int divider_len;
      if (i == -2) {
        divider_buf[0] = '\n';
        divider_len = 1;
      } else if (i == -1) {
        divider_buf[0] = 0;
        divider_len = 1;
      } else {
        divider_len = worddivider->CopyDivider(i, divider_buf);
      }
      divider_buf[divider_len] = 0;
      FuzzySearchThreadOrNot(max_parallel_depth, cur_parallel_depth+1, skipped,
          NULL, divider_buf, 0, divider_len, last_from, last_to, matched_buf,
          matched_size, matched_tail, mismatch_limit, mismatch_num, divider_len,
          trailing_divider_len, result_set, &th_arr, &th_size, &th_tail);
    }
  } else {
    result_set->Append(matched_buf, matched_tail, last_to-last_from,
        mismatch_num, leading_divider_len, trailing_divider_len);
  }
  while (th_tail)
    pthread_join(th_arr[--th_tail], NULL);
  if (th_arr) free(th_arr);
}
void BWT::ReadIndex(FILE *findex) {
  if (fread(&bit_of_bucket_size_, sizeof(char), 1, findex) != 1)
    Error(505, "Failed to read from file");
  first_ = (size_t*) malloc(256 * sizeof(size_t));
  if (!first_) Error(506, "failed to allocate memory");
  if(fread(first_, sizeof(size_t), 256, findex) != 256)
    Error(507, "failed to read from file");
  text_length_ = 0;
  for (int i=0; i<256; i++) {
    text_length_ += first_[i];
    first_[i] = text_length_;
  }
  last_ = (unsigned char*)malloc(text_length_);
  if (!last_) Error(508, "failed to allocate memory");
  bucket_counter_ = NULL;
  if (text_length_ >> bit_of_bucket_size_ > 0) {
    bucket_counter_ = (size_t(*)[256]) malloc(
        (text_length_ >> bit_of_bucket_size_) * sizeof(size_t) * 256);
    if (!bucket_counter_) Error(509, "failed to allocate memory");
  }
  size_t unit = 1<<bit_of_bucket_size_;
  for (size_t i=0; i < text_length_ >> bit_of_bucket_size_; i++) {
    if (fread(last_ + (i<<bit_of_bucket_size_), 1, unit, findex)
        != unit)
      Error(510, "failed to read from file");
    if (fread(bucket_counter_+i, sizeof(size_t), 256, findex) != 256)
      Error(511, "failed to read from file");
  }
  size_t remaining = sizeof(size_t) * 8 - bit_of_bucket_size_;
  remaining = text_length_ << remaining >> remaining;
  if (fread(last_ + (text_length_>>bit_of_bucket_size_<<bit_of_bucket_size_),
      1, remaining, findex) != remaining)
    Error(512, "failed to read from file");
}
void BWT::BuildIndex(const char *text_buf, const char *path, FILE *findex,
    size_t memory_size, int max_parallel_depth, char bit_of_bucket_size) {
  size_t text_length = strlen(text_buf) + 1;
  if(fwrite(&bit_of_bucket_size, sizeof(char), 1, findex) != 1)
    Error(513, "failed to write to file");
  size_t *loc_arr = NULL;
  size_t *first = (size_t*) calloc(256, sizeof(size_t));
  if (!first) Error(514, "failed to allocate memory");
  for (size_t i=0; i<text_length; i++)
    first[(unsigned char)text_buf[i]]++;
  if(fwrite(first, sizeof(size_t), 256, findex) != 256)
    Error(515, "failed to write to file");
  if (!memory_size)
    memory_size = text_length;
  else {
    size_t memory_needed = 0;
    for (int i=0; i<256; i++)
      if (first[i] > memory_needed) memory_needed = first[i];
    memory_needed = memory_needed * sizeof(size_t) + text_length;
    if (memory_needed > memory_size) 
      Error(516, "memory_size need be '%lub' at least", memory_needed);
    memory_size = (memory_size - text_length) / sizeof(size_t);
  }
  size_t *bucket_count = (size_t*) calloc(256, sizeof(size_t));
  if (!bucket_count)
    Error(517, "failed to allocate memory");
  size_t loc_size = 0, loc_tail = 0;;
  int from = 0;
  size_t written_bytes = 0;
  size_t *counter_per_char = (size_t*) calloc(256, sizeof(size_t));
  if (!counter_per_char )
    Error(518, "failed to allocate memory");
  while (from < 256) {
    size_t total = first[from];
    int to = from + 1;
    while (to < 256 && total + first[to] <= memory_size) {
      total += first[to];
      to++;
    }
    if (total > loc_size) {
      loc_arr = (size_t*)realloc(loc_arr, sizeof(size_t) * total);
      if (!loc_arr) Error(519, "failed to allocate memory");
      loc_size = total;
    }
    size_t cnt = 0;
    for (size_t i=0; i<text_length; i++) {
      unsigned char c = text_buf[i];
      if (c >= from && c < to) loc_arr[cnt++] = i;
    }
    if (cnt != total)
      Error(520, "contact developer");
    QuickSort(text_length, text_buf, max_parallel_depth, loc_arr, total, 0, 0);
    for (size_t i=0; i<total; i++, loc_tail++) {
      unsigned char c = ReadChar(text_length, text_buf, loc_arr[i],
          text_length-1);
      WriteChar(findex, c, ++written_bytes, bit_of_bucket_size,
          counter_per_char);
    }
    from = to;
  }
  if (loc_tail != text_length) Error(521, "contact developer");
  free(loc_arr);
  assert(!memcmp(counter_per_char, first, sizeof(size_t) * 256));
  free(counter_per_char);
  free(first);
}
void BWT::WriteChar(FILE *findex, unsigned char c, size_t written_bytes,
    char bit_of_bucket_size, size_t *counter_per_char) {
  counter_per_char[c]++;
  if (fputc(c, findex) == EOF) Error(522, "failed to write to file");
  int shift = (sizeof(written_bytes) << 3) - bit_of_bucket_size;
  if (written_bytes << shift >> shift) return;
  if(fwrite(counter_per_char, sizeof(size_t), 256, findex) != 256)
    Error(523, "failed to write to file");
}
unsigned char BWT::ReadChar(const size_t text_length, const char *text_buf,
    size_t idx, size_t shift) {
  assert(idx < text_length);
  idx += shift;
  if (idx >= text_length) idx -= text_length;
  assert(idx < text_length);
  return (unsigned char)text_buf[idx];
}
void BWT::QuickSort(const size_t text_length, const char *text_buf,
    int max_parallel_depth, size_t *base,
    size_t nmemb, size_t shift, int depth) {
  pthread_t *th_arr = NULL;
  int th_size = 0, th_tail = 0;
  for (;;) {
/*
printf("A: %lx\t%ld\t%ld\n", (size_t)base, nmemb, shift);
if (nmemb < 100)
  for (size_t i=0; i<nmemb; i++)
    printf("%d,", ReadChar(base[i], shift));
printf("\n");
*/
    unsigned char pivot = ReadChar(text_length, text_buf, base[0], shift);
    size_t p_lt_pivot = 0, p_gt_pivot = nmemb - 1, p_head = 1,
        p_tail = nmemb - 1;
    int diff;
    for (;;) {
      while (p_head <= p_tail && (diff = (int)ReadChar(text_length, text_buf,
          base[p_head], shift) - pivot) <= 0) {
        if (diff < 0) {
          size_t tmp = base[p_head];
          base[p_head] = base[p_lt_pivot];
          base[p_lt_pivot++] = tmp;
      }
        p_head++;
      }
      while (p_head <= p_tail &&
          (diff = (int)ReadChar(text_length, text_buf, base[p_tail], shift)
              - pivot) >= 0) {
        if (diff > 0) {
          size_t tmp = base[p_tail];
          base[p_tail] = base[p_gt_pivot];
          base[p_gt_pivot--] = tmp;
        }
        p_tail--;
      }
      if (p_head > p_tail) break;
      size_t tmp = base[p_head];
      base[p_head] = base[p_lt_pivot];
      base[p_lt_pivot] = base[p_tail];
      base[p_tail] = base[p_gt_pivot];
      base[p_gt_pivot] = tmp;
      p_head++;
      p_tail--;
      p_lt_pivot++;
      p_gt_pivot--;
    }
    if (p_lt_pivot > 1) 
      QSThreadOrNot(text_length, text_buf, max_parallel_depth, base,
          p_lt_pivot, shift, depth+1, &th_arr, &th_size, &th_tail);
    if (nmemb - p_gt_pivot - 1 > 1) 
      QSThreadOrNot(text_length, text_buf, max_parallel_depth,
          base+p_gt_pivot+1, nmemb-p_gt_pivot-1, shift, depth+1, &th_arr,
              &th_size, &th_tail);
    if (p_gt_pivot <= p_lt_pivot || shift + 1 >= text_length)
      break;
    base += p_lt_pivot;
    nmemb = p_gt_pivot-p_lt_pivot+1;
    shift++;
    depth += 6;
  }
  while (th_tail)
    pthread_join(th_arr[--th_tail], NULL);
  if (th_arr) free(th_arr);
}
void BWT::QSThreadOrNot(const size_t text_length, const char *text_buf,
    int max_parallel_depth, size_t *base, size_t nmemb, size_t shift, int depth,
    pthread_t **th_arr, int *th_size, int *th_tail) {
  if (depth <= max_parallel_depth && nmemb >= kMinNmembPerThread) {
    size_t *arg;
    arg = (size_t*) malloc(8 * sizeof(size_t));
    if (!arg) Error(524, "Failed to allocate memory");
    arg[0] = (size_t) this;  
    arg[1] = text_length;
    arg[2] = (size_t) text_buf;
    arg[3] = max_parallel_depth;
    arg[4] = (size_t) base;  
    arg[5] = nmemb;
    arg[6] = shift;
    arg[7] = depth;
    if (*th_size == *th_tail) {
      *th_size = *th_size + 64;
      *th_arr = (pthread_t*)realloc(*th_arr, (*th_size) * sizeof(pthread_t*));
      if (!th_arr) Error(525, "failed to allocate memory");
    }
    int rtn = pthread_create((*th_arr) + (*th_tail)++, NULL,
        BWT::QSThreadWorker, arg);
    if (!rtn) return;
    (*th_tail)--;
    free(arg);
    static bool reported = false;
    if (!reported) {
      reported = true;
      Message(501, "pthread_create returned error '%d', try smaller "
          "'parallel-depth'.\n", rtn);
    }
  }
  QuickSort(text_length, text_buf, max_parallel_depth, base, nmemb, shift,
      depth);
}
void *BWT::QSThreadWorker(void *arg) {
  BWT *bwt = (BWT*) ((size_t*)arg)[0];
  size_t text_length = ((size_t*)arg)[1];
  const char *text_buf = (char*) ((size_t*)arg)[2];
  int max_parallel_depth = (int)((size_t*)arg)[3];
  size_t *base = (size_t*) ((size_t*)arg)[4];
  size_t nmemb = ((size_t*)arg)[5];
  size_t shift = ((size_t*)arg)[6];
  int depth = (int)((size_t*)arg)[7];
  free(arg);
  bwt->QuickSort(text_length, text_buf, max_parallel_depth, base, nmemb, shift,
      depth);
  pthread_exit(0);
}
//ResultSet
ResultSet::ResultSet() {
  result_arr_ = NULL;
  result_tail_ = result_size_ = 0;
  pthread_mutex_init(&mutex_, NULL);
}
ResultSet::~ResultSet() {
  if (result_tail_) {
    for (size_t i=0; i<result_tail_; i++)
      delete result_arr_[i];
    free(result_arr_);
  }
  pthread_mutex_destroy(&mutex_);
}
void ResultSet::Append(char *matched_buf, size_t matched_tail, size_t count,
    size_t mismatch, int leading_divider_len, int trailing_divider_len) {
  pthread_mutex_lock(&mutex_);
  if (result_tail_ == result_size_) {
    result_size_ += 128;
    result_arr_ = (Result**)realloc(result_arr_ ,
        sizeof(Result**)*result_size_);
    if (!result_arr_) Error(526, "failed to allocate memory");
  }
  result_arr_[result_tail_++] = new Result(matched_buf, matched_tail, count,
      mismatch, leading_divider_len, trailing_divider_len);
  pthread_mutex_unlock(&mutex_);
}
void ResultSet::Print(int64_t word_id) {
  if (!result_tail_) return;
  qsort(result_arr_, result_tail_, sizeof(Result*), Result::CompareBuffer);
  size_t new_tail = 0;
  for (size_t i=1; i<result_tail_; i++) {
    if (result_arr_[new_tail]->Diff(result_arr_[i])) {
      new_tail++;
      if (new_tail != i) {
        Result *swap = result_arr_[new_tail];
        result_arr_[new_tail] = result_arr_[i];
        result_arr_[i] = swap;
      }
    }
  }
  qsort(result_arr_, ++new_tail, sizeof(Result*), Result::CompareCount);
  for (size_t i=0; i<new_tail; i++) {
    result_arr_[i]->Print(word_id);
  }
}
Result::Result(char *matched_buf, size_t matched_tail, size_t count,
    size_t mismatch, int leading_divider_len, int trailing_divider_len) {
  matched_buf_ = (char*)malloc(matched_tail);
  if (!matched_buf_) Error(527, "Failed to allocate memory");
  for (size_t i=0; i<matched_tail; i++)
    matched_buf_[i] = matched_buf[matched_tail-i-1];
  matched_tail_ = matched_tail;
  count_ = count;
  mismatch_ = mismatch;
  leading_divider_len_ = leading_divider_len;
  trailing_divider_len_ = trailing_divider_len;
}
Result::~Result() {
  free(matched_buf_);
}
void Result::Print(int64_t word_id) {
  char *s = matched_buf_;
  int leading = ReadUnicodeOrDie(&s);
  if (s - matched_buf_ != leading_divider_len_) Error(528, "Contact developer");
  s = matched_buf_ + matched_tail_ - trailing_divider_len_;
  int trailing = ReadUnicodeOrDie(&s);
  if (s != matched_buf_ + matched_tail_) Error(529, "Contact developer");
  printf("%ld\t%lu\t%lu\t%d\t%d\t", word_id, count_, mismatch_, leading, trailing);
  for (size_t j=leading_divider_len_; j<matched_tail_ - trailing_divider_len_;
      j++) {
    switch (matched_buf_[j]) {
      case 0:
        fputc('$', stdout);
        break;
      case 10:
        printf("\\N");
        break;
      default:
        fputc(matched_buf_[j], stdout);
    }
  }
  printf("\n");
}
int Result::CompareBuffer(const void *p1, const void *p2) {
  Result *r1 = *((Result**)p1);
  Result *r2 = *((Result**)p2);
  int rtn = r1->matched_tail_ - r2->matched_tail_;
  if (rtn) return rtn;
  rtn = memcmp(r1->matched_buf_, r2->matched_buf_, r1->matched_tail_);
  if (rtn) return rtn;
  return r1->mismatch_ - r2->mismatch_;
}
int Result::CompareCount(const void *p1, const void *p2) {
  Result *r1 = *((Result**)p1);
  Result *r2 = *((Result**)p2);
  int rtn = r2->count_ - r1->count_;
  if (rtn) return rtn;
  rtn = r1->mismatch_ - r2->mismatch_;
  if (rtn) return rtn;
  rtn = r1->matched_tail_ - r2->matched_tail_;
  if (rtn) return rtn;
  return memcmp(r1->matched_buf_, r2->matched_buf_, r1->matched_tail_);
}
bool Result::Diff(Result *ano) {
  if (this->matched_tail_ != ano->matched_tail_)
    return true;
  return memcmp(this->matched_buf_, ano->matched_buf_, this->matched_tail_);
}
