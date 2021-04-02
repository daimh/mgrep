#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <config.h>
#include "treenode.h"
#include "global.h"
#include "broker.h"
#include "trie.h"
#include "worddivider.h"
#include "casefolding.h"
#include "bwt.h"
const static int kMaxWordDivider = 78;
const static size_t kBufferIncrement = 1048576;
int64_t StrToInt64(char *s, int64_t lower, int64_t upper) {
	char *tail;
	int64_t rtn = strtol(s, &tail, 10);
	if (*tail)
		Error(3, "Invalid integer, use '--help' for more information");
	if (rtn < lower || rtn > upper)
		Error(4, "Integer is not in acceptable range %ld..%ld)", lower, upper);
	return rtn;
}
void Version() {
	fprintf(stderr, "%s\n"
"Citation: https://github.com/daimh/mgrep\n"
"License AGPL: GNU AGPL version 3 or later <https://gnu.org/licenses/agpl-3.0.html>\n"
"Written by Manhong Dai, Fan Meng\n"
"Michigan Neuroscience Institute\n"
"University of Michigan\n"
"\n"
"There is NO WARRANTY; to the extent permitted by law.\n"
, PACKAGE_STRING);
	exit(0);
}
void Help() {
	fprintf(stderr, "Usage: mgrep <command> [<options>]... \n"
"\n"
"Commands:\n"
"    match\n"
"    extend\n"
"    index\n"
"\n");
	exit(0);
}
void HelpMatch() {
	fprintf(stderr, "Usage: mgrep match [<options>]... \n"
"\n"
"  mgrep searches the standard input or text sent through network from mgrep\n" 
"client for lines containing the concepts defined in the dictionary file. If the\n"
"standard input is tab-delimited format, mgrep searches the last column and\n"
"prints all other columns along with mapping result. Or if the standard input\n"
"has no TAB character, mgrep prints the whole line. Output columns are\n"
"  DICT_ID,FROM,TO,TEXT,LEADING_COLUMNS_OR_WHOLE_LINE_IN_STANDARD_INPUT\n"
"\n"
"Example:\n"
"  mgrep match -d dict.txt -w NonAlphanumeric < corpus.txt\n"
"  mgrep match -d dict.txt -w wd_english.txt -w wd_chinese.txt -p 55555\n"
"\n"
"    -d, --dictionary-file=FILE\n"
"      FILE is a two-column tab-delimited file. The first column is a\n"
"      signed 64-bit integer, the second column is the text\n"
"      associated with the concept\n"
"    -w, --word-divider=None|NonAlphanumeric|FILE\n"
"      'None' means no word-divider characters, all matches are\n"
"         printed\n"
"      'NonAlphanumeric' is a predefined character set, which\n"
"         includes all ASCII characters (1-127), excluding 0-9, a-z,\n"
"         A-Z\n"
"      FILE, each line is a hexadecimal code,\n"
"         followed by ';' and then comments. An example line is\n"
"         \"2b; ascii code 43, character '+'\"\n"
"    -c, --casefolding-file=FILE\n"
"      ignore case distinctions, FILE can be downloaded at\n"
"        http://unicode.org/Public/UNIDATA/CaseFolding.txt\n"
"    -l, --longest\n"
"      longest match only\n"
"    -p, --port=PORT\n"
"      run as a daemon listening at PORT, usually between 1024 and 65535.\n"
"        with daemon mode, mgrep search text sent from mgrep client program.\n"
"        multiple mgrep daemons can be deployed on single or multiple hosts for\n"
"        load balance. Currently there is a java version of mgrep client, other\n"
"        language versions are available upon request\n"
	);
	exit(0);
}
void HelpExtend() {
	fprintf(stderr, "Usage: mgrep extend [<options>]... \n"
"\n"
"  mgrep searches a pre-built binary index of a corpus file, prints strings\n"
"similar to the concepts defined in dictionary file, as well as their\n"
"occurrence count, then the output can be used to extend the dictionary for\n"
"following mgrep match. Output columns are\n"
"  DICT_ID,OCCURRENCE,MISMATCH,LEADING_UNICODE,TRAILING_UNICODE,STRING\n"
"Example:\n"
"  mgrep extend -w NonAlphanumeric -b index.dat < dict.txt\n"
"\n"
"    -w, --word-divider=None|NonAlphanumeric|FILE\n"
"      as in batch-mapping mode\n"
"    -b, --binary-index=FILE\n"
"      FILE is a binary file, created with 'mgrep index'\n"
"    -x, --maximum-mismatch=INT\n"
"      up to INT mismatch is allowed, between 0 and 100, default 5\n"
"    -P, --parallel-depth=DEPTH\n"
"      up to 512^DEPTH threads. Default 1\n"
);
	exit(0);
}
void HelpIndex() {
	fprintf(stderr, "Usage: mgrep <index> [<options>]... \n"
"\n"
"  mgrep takes a corpus file as input to build a binary index, which can be used\n"
"for 'mgrep extend'\n"
"Example:\n"
"  mgrep index -b index.dat < corpus.txt\n"
"\n"
"    -b, --binary-index=FILE\n"
"      FILE is name of the to-be-created binary index file\n"
"    -c, --casefolding-file=FILE\n"
"      as in batch-mapping mode\n"
"    -P, --parallel-depth=DEPTH\n"
"      up to 2^DEPTH threads. Default 10\n"
"    -M, --memory-size=SIZE\n"
"      soft limit of memory\n"
	);
	exit(0);
}
int MainExtend(int argc, char *argv[]) {
	char *binary_index = NULL;
	size_t maximum_mismatch = 5;
	char *worddivider_files[kMaxWordDivider];
	int worddivider_cnt = 0;
	int parallel_depth = -1;
	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"word-divider", 1, 0, 'w'},
			{"binary-index", 1, 0, 'b'},
			{"maximum-mismatch", 1, 0, 'x'},
			{"parallel-depth", 1, 0, 'P'},
			{0, 0, 0, 0}
		};
		char opt = getopt_long(argc, argv, "hb:x:", long_options,
				&option_index);
		if (opt == -1) break;
		switch (opt) {
			case 'h':
				HelpExtend();
			case 'w':
				if (worddivider_cnt == kMaxWordDivider)
					Error(6, "Up to %d '-w' parameters are supported", kMaxWordDivider);
				worddivider_files[worddivider_cnt++] = optarg;
				break;
			case 'b':
				binary_index = optarg;
				break;
			case 'x':
				maximum_mismatch = StrToInt64(optarg, 0, 100);
				break;
			case 'P':
				parallel_depth = StrToInt64(optarg, 0, 10);
				break;
			default:
				Error(9, "Wrong usage\nTry `mgrep' for more information.");
		}
	}
	if (optind + 1 != argc)
		Error(10, "Wrong usage\nTry `mgrep' for more information.");
	if (!binary_index) Error(25, "Missing --binary-index");
	if (worddivider_cnt == 0) Error(26, "Missing --word-divider");
	if (worddivider_cnt > 1)
		Error(27, "Only one --word-divider can be specified in this mode");
	FILE *findex = fopen(binary_index, "r");
	WordDivider *worddivider = new WordDivider(worddivider_files[0]);
	if (!findex)
		Error(28, "Failed top open file '%s' for read", binary_index);
	char header_buf[80];
	int header_tail = 0;
	for (; header_tail<(int)sizeof(header_buf); header_tail++) {
		header_buf[header_tail] = fgetc(findex);
		if (header_buf[header_tail] == EOF)
			Error(29, "Failed to read from file");
		if (header_buf[header_tail] == '\n') {
			header_buf[header_tail] = 0;
			break;
		}
	}
	if (header_tail == (int)sizeof(header_buf))
		Error(30, "File is not an mgrep index file built with 'mgrep index'");
	char *first_dot = strchr(header_buf, '.');
	if (!first_dot)
		Error(31, "File is not an mgrep index file built with 'mgrep index'");
	if (memcmp(header_buf, PACKAGE_STRING, first_dot - header_buf + 1))
		Error(32, "File is built with a different version of mgrep");
	CaseFolding *casefolding = NULL;
	if (header_buf[header_tail-1] == 'Y')
		casefolding = new CaseFolding(findex);
	else if (header_buf[header_tail-1] != 'N')
		Error(33, "File is built with a different version of mgrep");
	BWT *bwt = new BWT();
	bwt->ReadIndex(findex);
	char *line_buf = NULL;
	size_t line_size = 0;
	int line_tail, line_num = 0;
	char *folding_buf = NULL;
	size_t folding_size = 0;
	while ((line_tail = getline(&line_buf, &line_size, stdin)) != -1) {
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
			Error(34, "Dictionary file line %d column 1 is invalid number",
					line_num);
		if (*word != '\t' or strchr(word+1, '\t') != NULL)
			Error(35, "Dictionary file line %d has no tab or multiple tabs",
					line_num);
		if (errno != 0)
			Error(36, "Dictionary file line %d column 1 is not in range "
					"[-9223372036854775808,9223372036854775807]", line_num);
		word++;
		if (!IsUtf8(word))
			Error(37, "Dictionary file line %d column 2 is not a UTF8 string",
					line_num, id);
		if (casefolding)
			word = casefolding->FoldOrDie(word, &folding_buf, &folding_size);
		if (parallel_depth == -1) parallel_depth = 1;
		bwt->Search(id, line_buf, word, worddivider, parallel_depth,
				maximum_mismatch);
	}
	if (folding_buf) free(folding_buf);
	delete bwt;
	delete casefolding;
	fclose(findex);
	delete worddivider;
	return 0;
}
int MainMatch(int argc, char *argv[]) {
	char *dict_file = NULL, *casefolding_file = NULL;
	char *worddivider_files[kMaxWordDivider];
	int worddivider_cnt = 0;
	int port = 0;
	bool longest = false;
	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"dictionary-file", 1, 0, 'd'},
			{"word-divider", 1, 0, 'w'},
			{"casefolding-file", 1, 0, 'c'},
			{"longest", 0, 0, 'l'},
			{"port", 1, 0, 'p'},
			{0, 0, 0, 0}
		};
		char opt = getopt_long(argc, argv, "hd:w:c:lp:", long_options,
				&option_index);
		if (opt == -1) break;
		switch (opt) {
			case 'h':
				HelpMatch();
			case 'd':
				dict_file = optarg;
				break;
			case 'w':
				if (worddivider_cnt == kMaxWordDivider)
					Error(6, "Up to %d '-w' parameters are supported", kMaxWordDivider);
				worddivider_files[worddivider_cnt++] = optarg;
				break;
			case 'c':
				casefolding_file = optarg;
				break;
			case 'l':
				longest = true;
				break;
			case 'p':
				port = StrToInt64(optarg, 1, 65535);
				break;
			default:
				Error(9, "Wrong usage\nTry `mgrep' for more information.");
		}
	}
	if (optind + 1 != argc) 
		Error(10, "Wrong usage\nTry `mgrep' for more information. %d, %d, %d", optind + 1,  argc, optind + 1 -  argc);
	if (!dict_file) Error(12, "Missing --dictionary-file");
	if (worddivider_cnt == 0) Error(13, "Missing --word-divider");
	if (port == 0 && worddivider_cnt > 1)
		Error(14, "Only one --word-divider can be specified in daemon mode");
	WordDivider **worddividers =
			(WordDivider **)malloc(sizeof(WordDivider*) * worddivider_cnt);
	if (!worddividers) Error(17, "Failed to allocate memory");
	for (int i=0; i<worddivider_cnt; i++)
		worddividers[i] = new WordDivider(worddivider_files[i]);
	CaseFolding *casefolding = NULL;
	if (casefolding_file) casefolding = new CaseFolding(casefolding_file);
	Trie *trie = new Trie(dict_file, casefolding);
	Broker *broker = new Broker(port);
	if (port) {
		pid_t pid = fork();
		if (pid == -1)
			Error(18, "Errno: %d, Errmsg: %s", errno, strerror(errno));
		if (pid) {
			Message(1, "mgrep is now in daemon mode, pid is %d, all messages would"
					" be in syslog (file /var/log/message or command journalctl)\n", pid);
			exit(0);
		} else {
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
			stdin = stdout = stderr = NULL;
			Message(2, "mgrep is now in daemon mode, pid is %d", getpid());
		}
	}
	broker->Start(trie, casefolding, longest, worddivider_cnt, worddividers);
	delete broker;
	delete trie;
	delete casefolding;
	for (int i=0; i<worddivider_cnt; i++)
		delete worddividers[i];
	free(worddividers);
	return 0;
}
int MainIndex(int argc, char *argv[]) {
	char *binary_index = NULL;
	char *casefolding_file = NULL;
	int parallel_depth = -1;
	size_t memory_size = 0;
	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"binary-index", 1, 0, 'b'},
			{"casefolding-file", 1, 0, 'c'},
			{"parallel-depth", 1, 0, 'P'},
			{"memory-size", 1, 0, 'M'},
			{0, 0, 0, 0}
		};
		char opt = getopt_long(argc, argv, "hc:b:P:M:", long_options,
				&option_index);
		if (opt == -1) break;
		switch (opt) {
			case 'h':
				HelpIndex();
				break;
			case 'b':
				binary_index = optarg;
			case 'c':
				casefolding_file = optarg;
				break;
			case 'P':
				parallel_depth = StrToInt64(optarg, 0, 10);
				break;
			case 'M':
				{
					char *tail;
					memory_size = strtol(optarg, &tail, 10);
					if (!*tail) break; 
					switch (*tail++) {
						case 'b':
						case 'B':
							break;
						case 'k':
						case 'K':
							memory_size *= 1024;
							break;
						case 'm':
						case 'M':
							memory_size *= 1048576;
							break;
						case 'g':
						case 'G':
							memory_size *= 1073741824;
							break;
						case 't':
						case 'T':
							memory_size *= 1099511627776;
							break;
						default:
							Error(7, "Invalid suffix in -M argument");
					}
					if (*tail)
						Error(8, "Invalid suffix in -M argument");
				}
				break;
			default:
				Error(9, "Wrong usage\nTry `mgrep' for more information.");
		}
	}
	if (optind + 1 != argc)
		Error(10, "Wrong usage\nTry `mgrep' for more information.");
	if (!binary_index) Error(19, "Missing --binary-index");
	if (fopen(binary_index, "r"))
		Error(20, "--binary-index file '%s' exists", binary_index);
	FILE *findex;
	findex = fopen(binary_index, "w");
	if (!findex) Error(21, "cannot open binary index file '%s' for write",
			binary_index);
	char *file_buf = (char*)malloc(16777216);
	if (!file_buf) Error(22, "Failed to allocate memory");
	setbuffer(findex, file_buf, 16777216);
	fprintf(findex, "%s ", PACKAGE_STRING);
	char *line_buf = NULL;
	size_t line_size = 0;
	int line_tail, line_num = 0;
	char *text_buf = (char*)malloc(kBufferIncrement);
	size_t text_size = kBufferIncrement, text_tail = 0;
	while ((line_tail = getline(&line_buf, &line_size, stdin)) != -1) {
		line_num++;
		if (line_tail > 0 && line_buf[line_tail-1] == '\n')
			line_buf[--line_tail] = 0;
		if (line_tail > 0 && line_buf[line_tail-1] == '\r')
			line_buf[--line_tail] = 0;
		if (!line_tail) continue;
		char *line_text = strrchr(line_buf, '\t');
		if (!line_text)
			line_text = line_buf;
		else
			line_text++;
		size_t line_l = strlen(line_text);
		if (line_l >= kBufferIncrement)
			Error(23, "Line %d is too long", line_num);
		if (text_tail + line_l >= text_size) {
			text_size += kBufferIncrement;
			text_buf = (char*)realloc(text_buf, text_size);
			if (!text_buf) Error(24, "Failed to allocate memory");
		}
		memcpy(text_buf + text_tail, line_text, line_l);
		text_tail += line_l;
		text_buf[text_tail++] = '\n';
	}
	if (text_tail) text_tail--;
	text_buf[text_tail] = 0;
	if (!casefolding_file) {
		fprintf(findex, "N\n");
	} else {
		CaseFolding *casefolding = new CaseFolding(casefolding_file);
		fprintf(findex, "Y\n");
		casefolding->Dump(findex);
		char *folding_buf = NULL;
		size_t folding_size = 0;
		casefolding->FoldOrDie(text_buf, &folding_buf, &folding_size);
		delete casefolding;
		free(text_buf);
		text_buf = folding_buf;
	}
	BWT *bwt = new BWT();
	if (parallel_depth == -1) parallel_depth = 10;
	bwt->BuildIndex(text_buf, binary_index, findex, memory_size, parallel_depth,
			11);
	fclose(findex);
	delete bwt;
	free(text_buf);
	free(file_buf);
	return 0;
}
int main(int argc, char *argv[]) {
	if (argc <= 1 || 
		strcmp(argv[1], "-h") == 0 ||
		strcmp(argv[1], "--help") == 0
	)
		Help();
	else if (strcmp(argv[1], "--version") == 0)
		Version();
	else if (strcmp(argv[1], "match") == 0)
		return MainMatch(argc, argv);
	else if (strcmp(argv[1], "extend") == 0)
		return MainExtend(argc, argv);
	else if (strcmp(argv[1], "index") == 0)
		return MainIndex(argc, argv);
	else
		Error(10, "Wrong usage\nTry `mgrep' for more information.");
}
