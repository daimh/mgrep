#ifndef MGREP_SRC_GLOBAL_H
#define MGREP_SRC_GLOBAL_H
void Error(int code, const char *format, ...);
void Message(int code, const char *format, ...);
bool IsUtf8(char *s);
int ReadUnicodeOrDie(char **s);
int HexToUnicode(char *s);
int IntCompare(const void *m1, const void *m2);
#endif // MGREP_SRC_GLOBAL_H
