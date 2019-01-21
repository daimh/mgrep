#ifndef MGREP_SRC_BROKER_H
#define MGREP_SRC_BROKER_H
class Broker {
  public:
    explicit Broker(int port);
    void Start(class Trie *trie, class CaseFolding *casefolding,
        bool longest, int worddivider_cnt, class WordDivider **worddividers);
  private:
    static void *Client(void *arg);
    static bool Send(int sockfd, char *buf, int size);
    const static int kBacklog = 1024;
    int server_sockfd_;
    const static int kBufferIncrement = 1048576;
    class ThreadArg {
    public:
      ThreadArg(Trie *trie, CaseFolding *casefolding,
          int worddivider_cnt, WordDivider **worddividers);
    public:
      int client_sockfd_;
      Trie *trie_;
      CaseFolding *casefolding_;
      int worddivider_cnt_;
      WordDivider **worddividers_;
  };
};
#endif  // MGREP_SRC_BROKER_H
