#ifndef MGREP_SRC_TRIE_H
#define MGREP_SRC_TRIE_H
#include <pthread.h>
#include <stdio.h>
#include "treenode.h"
class Trie {
	public:
		Trie(char *filename, class CaseFolding *casefolding);
		void Annotate(class WordDivider *worddivider, bool longest,
				char *input_buf, char **output_buf, int *output_size, int *output_tail,
				char *orig_input);
	private:
		TreeNode<int64_t> *root_node_;
	private:
		int Utf8LenOrDie(char *from, char *to);
		char *AnnotateRecursive(class WordDivider *worddivider, bool longest,
				char *input_buf, char **output_buf, int *output_size, int *output_tail,
				char *match_starting, TreeNode<int64_t> *tree_node, char *current_ptr,
				char *farthest_matching_ending, char *orig_input);
		const static int kBufferIncrement = 1048576;
};
#endif // MGREP_SRC_TRIE_H
