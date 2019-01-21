#ifndef __BWT_H__
#define __BWT_H__
class BWT {
public:
  void Search(int64_t word_id, char *line, char *word, class WordDivider *worddivider,
      int max_parallel_depth, size_t mismatch_limit);
  void ReadIndex(FILE *findex);
  void BuildIndex(const char *text_buf, const char *path, FILE *findex,
      size_t memory_size, int max_parallel_depth, char bit_of_bucket_size);
private:
  size_t *first_, text_length_, (*bucket_counter_)[256];
  unsigned char *last_;
  static const size_t kMinRangePerThread = 1024;
  char bit_of_bucket_size_;
  void FuzzySearch(int max_parallel_depth, int cur_parallel_depth, char skipped,
      class WordDivider *worddivider, char *word_buf, size_t divider_pos,
      size_t word_tail, size_t last_from, size_t last_to, char *matched_buf,
      size_t matched_size, size_t matched_tail, size_t mismatch_limit,
      size_t mismatch_num, int leading_divider_len, int trailing_divider_len,
      class ResultSet *result_set);
  void FuzzySearchThreadOrNot(int max_parallel_depth, int cur_parallel_depth,
      char skipped, class WordDivider *worddivider, char *word_buf,
      size_t divider_pos, size_t word_tail, size_t last_from, size_t last_to,
      char *matched_buf, size_t matched_size, size_t matched_tail,
      size_t mismatch_limit, size_t mismatch_num, int leading_divider_len,
      int trailing_divider_len, class ResultSet *result_set, pthread_t **th_arr,
      int *th_size, int *th_tail);
  static void *FuzzySearchThreadWorker(void *arg);
  //Following are for FM Index building.
  static const size_t kMinNmembPerThread = 2000000;
  void WriteChar(FILE *findex, unsigned char c, size_t written_bytes,
      char bit_of_bucket_size, size_t *counter_per_char);
  unsigned char ReadChar(const size_t text_length, const char *text_buf,
      size_t idx, size_t shift);
  void QuickSort(const size_t text_length, const char *text_buf,
      int max_parallel_depth, size_t *base, size_t nmemb, size_t shift,
      int depth);
  void QSThreadOrNot(const size_t text_length, const char *text_buf,
      int max_parallel_depth, size_t *base, size_t nmemb, size_t shift,
      int depth, pthread_t **th_arr, int *th_size, int *th_tail);
  static void *QSThreadWorker(void *arg);
};
class ResultSet {
public:
  ResultSet();
  ~ResultSet();
  void Append(char *matched_buf, size_t matched_tail, size_t count,
      size_t mismatch, int leading_divider_len, int trailing_divider_len);
  void Print(int64_t word_id);
private:
  class Result **result_arr_;
  size_t result_tail_, result_size_;
  pthread_mutex_t mutex_;
};
class Result {
public:
  Result(char *matched_buf, size_t matched_tail, size_t count, size_t mismatch,
      int leading_divider_len, int trailing_divider_len);
  ~Result();
  void Print(int64_t word_id);
  bool Diff(Result *ano);
  static int CompareBuffer(const void *p1, const void *p2);
  static int CompareCount(const void *p1, const void *p2);
  char *matched_buf_;
  size_t matched_tail_;
  size_t count_;
  size_t mismatch_;
  int leading_divider_len_;
  int trailing_divider_len_;
};
#endif
