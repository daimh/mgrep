#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <limits.h>
void Error(int code, const char *format, ...) {
	if (format) {
		va_list args;
		va_start(args, format);
		char message[1024];
		int tail = snprintf(message, sizeof(message), "ERR-%04d: ", code);
		vsnprintf(message+tail, sizeof(message)-tail, format, args);
		va_end(args);
		if (stderr) {
			fputs(message, stderr);
			fputc('\n', stderr);
		} else {
			syslog(LOG_USER | LOG_INFO, message);
		}
	}
	kill(0, SIGINT);
}
void Message(int code, const char *format, ...) {
	va_list args;
	va_start(args, format);
	char message[1024];
	int tail = snprintf(message, sizeof(message), "MSG-%04d: ", code);
	vsnprintf(message+tail, sizeof(message)-tail, format, args);
	va_end(args);
	if (stderr)
		fputs(message, stderr);
	else
		syslog(LOG_USER | LOG_INFO, message);
}
bool IsUtf8(char *s) {
	while (*s) {
		if (*s >> 7 == 0) {
			s++;
		} else if (*s >> 5 == -2) {
			s++;
			for (int i = 0; i < 1; i++)
				if (*s++ >> 6 != -2) return false;
		} else if (*s >> 4 == -2) {
			s++;
			for (int i = 0; i < 2; i++)
				if (*s++ >> 6 != -2) return false;
		} else if (*s >> 3 == -2) {
			s++;
			for (int i = 0; i < 3; i++)
				if (*s++ >> 6 != -2) return false;
		} else if (*s >> 2 == -2) {
			s++;
			for (int i = 0; i < 4; i++)
				if (*s++ >> 6 != -2) return false;
		} else if (*s >> 1 == -2) {
			s++;
			for (int i = 0; i < 5; i++)
				if (*s++ >> 6 != -2) return false;
		} else {
			return false;
		}
	}
	return true;
}
int ReadUnicodeOrDie(char **s) {
	int rtn = 0;
	if (**s >> 7 == 0) {
		rtn = *((*s)++);
	} else if (**s >> 5 == -2) {
		rtn = *((*s)++) & 0x1F;
		for (int i = 0; i < 1; i++, (*s)++)
			rtn = rtn << 6 | (**s & 0x3F);
	} else if (**s >> 4 == -2) {
		rtn = *((*s)++) & 0xF;
		for (int i = 0; i < 2; i++, (*s)++)
			rtn = rtn << 6 | (**s & 0x3F);
	} else if (**s >> 3 == -2) {
		rtn = *((*s)++) & 0x7;
		for (int i = 0; i < 3; i++, (*s)++)
			rtn = rtn << 6 | (**s & 0x3F);
	} else if (**s >> 2 == -2) {
		rtn = *((*s)++) & 0x3;
		for (int i = 0; i < 4; i++, (*s)++)
			rtn = rtn << 6 | (**s & 0x3F);
	} else if (**s >> 1 == -2) {
		rtn = *((*s)++) & 0x1;
		for (int i = 0; i < 5; i++, (*s)++)
			rtn = rtn << 6 | (**s & 0x3F);
	} else {
		Error(9001, "Non-unicode found");
	}
	return rtn;
}
int HexToUnicode(char *s) {
	char *tail;
	int64_t rtn = strtol(s, &tail, 16);
	if (*tail) return -1;
	if (rtn <= 0 || rtn > INT_MAX) return -1;
	return (int)rtn;
}
int IntCompare(const void *m1, const void *m2) {
	return *(int*)m1 -	*(int*)m2;
}
