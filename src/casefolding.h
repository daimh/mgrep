#ifndef MGREP_SRC_CASEFOLDING_H
#define MGREP_SRC_CASEFOLDING_H
class CaseFolding {
	public:
		explicit CaseFolding(const char *filename);
		explicit CaseFolding(FILE *file);
		~CaseFolding();
	public:
		void Dump(FILE *file);
		char *FoldOrDie(char *input_buf, char **output_buf, size_t *output_size);
	private:
		int UnicodeToChar(char *buf, int code);
	private:
//0: source unicode
//1: start location on mapping_buf_ of folded unicode
//2: end location 
		int (*code_arr_)[3]; 
		char *mapping_buf_;
		int code_tail_, mapping_tail_;
};
#endif // MGREP_SRC_CASEFOLDING_H
