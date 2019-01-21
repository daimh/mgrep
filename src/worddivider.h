#ifndef MGREP_SRC_WORDDIVIDER_H
#define MGREP_SRC_WORDDIVIDER_H
class WordDivider {
  public:
    explicit WordDivider(const char *filename);
    ~WordDivider();
    bool IsBoundary(char *s);
    bool IsDivider(int code);
    int GetTotal();
    int CopyDivider(int idx, char *buf);
  private:
    int worddivider_tail_;
    int* worddivider_buf_;
};
#endif  // MGREP_SRC_WORDDIVIDER_H
