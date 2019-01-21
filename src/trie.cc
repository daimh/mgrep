#include <errno.h>
#include <unistd.h>
#include "global.h"
#include "treenode.h"
#include "trie.h"
#include "casefolding.h"
#include "worddivider.h"
Trie::Trie(char *filename, class CaseFolding *casefolding) {
  FILE *fdict = fopen(filename, "r");
  if (!fdict) Error(201, "%s: No such a file", filename);
  root_node_ = NULL;
  size_t line_size = 0;
  ssize_t line_tail;
  char *line_buf = NULL;
  int line_num = 0;
  char *folding_buf = NULL;
  size_t folding_size = 0;
  while ((line_tail = getline(&line_buf, &line_size, fdict)) != -1) {
    line_num++;
    if (line_tail > 0 && line_buf[line_tail-1] == '\n')
      line_buf[--line_tail] = 0;
    if (line_tail > 0 && line_buf[line_tail-1] == '\r')
      line_buf[--line_tail] = 0;
    if (!line_tail) continue;
    char *word;
    errno = 0;
    int64_t id = strtol(line_buf, &word, 10);
    if (word == line_buf)
      Error(202, "Dictionary file line %d column 1 is invalid number",
          line_num);
    if (*word != '\t' or strchr(word+1, '\t') != NULL)
      Error(203, "Dictionary file line %d has no tab or multiple tabs",
          line_num);
    if (errno != 0)
      Error(204, "Dictionary file line %d column 1 is not in range "
          "[-9223372036854775808,9223372036854775807]", line_num);
    word++;
    if (!IsUtf8(word))
      Error(205, "Dictionary file line %d column 2 is not a UTF8 string",
          line_num, id);
    if (casefolding)
      word = casefolding->FoldOrDie(word, &folding_buf, &folding_size);
    if (root_node_)
      root_node_ = root_node_->InsertWord(word, &id, 1);
    else
      root_node_ = new TreeNode<int64_t>(word, &id, 1);
  }
  if (folding_buf) free(folding_buf);
  if (line_buf) free(line_buf);
  if (!root_node_)
    Error(206, "Empty dictionary file");
  fclose(fdict);
}
void Trie::Annotate(WordDivider *worddivider,
    bool longest, char *input_buf, char **output_buf,
    int *output_size, int *output_tail, char *orig_input) {
  char *current_ptr = input_buf;
  char *farthest_matching_ending = NULL;
  if (!worddivider->GetTotal()) {
    while (*current_ptr) {
      char *current_match_ending = AnnotateRecursive(worddivider, longest,
          input_buf, output_buf, output_size, output_tail,
          current_ptr, root_node_, current_ptr,
          farthest_matching_ending, orig_input);
      if (current_match_ending > farthest_matching_ending)
        farthest_matching_ending = current_match_ending;
      ReadUnicodeOrDie(&current_ptr);
    }
  } else {
    bool previous_is_divider = true;
    while (*current_ptr) {
      char *next_ptr = current_ptr;
      int code = ReadUnicodeOrDie(&next_ptr);
      bool current_is_divider = worddivider->IsDivider(code);
      if (!current_is_divider && previous_is_divider) {
        char *current_match_ending = AnnotateRecursive(worddivider, longest,
            input_buf, output_buf, output_size, output_tail,
            current_ptr, root_node_, current_ptr,
            farthest_matching_ending, orig_input);
        if (current_match_ending > farthest_matching_ending)
          farthest_matching_ending = current_match_ending;
      }
      previous_is_divider = current_is_divider;
      current_ptr = next_ptr;
    }
  }
}
char *Trie::AnnotateRecursive(WordDivider *worddivider,
    bool longest, char *input_buf, char **output_buf, int *output_size,
    int *output_tail, char *match_starting, TreeNode<int64_t> *tree_node,
    char *current_ptr, char *farthest_matching_ending, char *orig_input) {
  char *current_match_ending = NULL;
  char *original_current_ptr = current_ptr;
  char *word = tree_node->word();
  while (*current_ptr && *word && *current_ptr == *word) {
    current_ptr++;
    word++;
  }
  if (*word == 0) {
    if (*current_ptr && tree_node->right())
      current_match_ending = AnnotateRecursive(worddivider, longest,
          input_buf, output_buf, output_size, output_tail, match_starting,
          tree_node->right(), current_ptr, farthest_matching_ending,
          orig_input);
    if (tree_node->GetIdSize() > 0
// tree_node->GetIdSize()>0:  The matched dictionary word unit has IDs
        && (!longest ||
            (!current_match_ending && current_ptr > farthest_matching_ending) )
// !longest:    User doesn't force longest match,
// !current_match_ending:    Further longer search doesn't found anything
// current_ptr>farthest_matching_ending:  This search must exceed previous
// searches starting match_starting earlier locations.
        && (!worddivider->GetTotal() || worddivider->IsBoundary(current_ptr))) {
// !GetTotal(): User doesn't specify any word divider
// IsBoundary(*current_ptr): Current location is a delimiter
      for (int i = 0; i < tree_node->GetIdSize(); i++) {
        if (*output_size - *output_tail - (current_ptr - match_starting)
            <= 128) {
          *output_size += kBufferIncrement;
          *output_buf = (char*) realloc(*output_buf, *output_size);
          if (*output_buf == NULL)
            Error(207, "Failed to allocate memory for output buffer");
        }
        int len = snprintf(*output_buf + *output_tail,
            *output_size - *output_tail, "%ld\t%d\t%d\t",
            *tree_node->GetIdElement(i), 
            Utf8LenOrDie(input_buf, match_starting)+1,
            Utf8LenOrDie(input_buf, current_ptr));
        *output_tail += len;
        memcpy(*output_buf + *output_tail,
            orig_input + (match_starting - input_buf),
            current_ptr-match_starting);
        *output_tail += current_ptr-match_starting;
        *(*output_buf + *output_tail) = '\n';
        *output_tail += 1;
        *(*output_buf + *output_tail) = 0;
      }
      current_match_ending = current_ptr;
    }
  } else if (*word < *current_ptr && tree_node->down()) {
    current_match_ending = AnnotateRecursive(worddivider, longest,
        input_buf, output_buf, output_size, output_tail, match_starting,
        tree_node->down(), original_current_ptr, farthest_matching_ending,
        orig_input);
  }
  return current_match_ending;
}
int Trie::Utf8LenOrDie(char *from, char *to) {
  int len = 0;
  while (from < to) {
    if (*from >> 7 == 0)
      from++;
    else if (*from >> 5 == -2)
          from += 2;
    else if (*from >> 4 == -2)
          from += 3;
    else if (*from >> 3 == -2)
          from += 4;
    else if (*from >> 2 == -2)
          from += 5;
    else if (*from >> 1 == -2)
          from += 6;
    else
      Error(208, "Non-UTF8 character found");
    if (++len == INT_MAX)
      Error(209, "Input string is too long");
  }
  return len;
}
