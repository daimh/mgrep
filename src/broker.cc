#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <config.h>
#include "global.h"
#include "broker.h"
#include "casefolding.h"
#include "trie.h"
Broker::ThreadArg::ThreadArg(Trie *trie, CaseFolding *casefolding,
		int worddivider_cnt, WordDivider **worddividers) {
	trie_ = trie;
	casefolding_ = casefolding;
	worddivider_cnt_ = worddivider_cnt;
	worddividers_ = worddividers;
}
Broker::Broker(int port) {
	server_sockfd_ = -1;
	if (port) {
		if ((server_sockfd_ = socket(PF_INET, SOCK_STREAM, 0)) == -1)
			Error(101, "Port: %d, Errno: %d, Errmsg: %s", port, errno,
					strerror(errno));
		int optval = 1;
		if (setsockopt(server_sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval,
				sizeof(optval)))
			Error(102, "Port: %d, Errno: %d, Errmsg: %s", port, errno,
					strerror(errno));
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons(port);
		if (bind(server_sockfd_, (struct sockaddr*)&sin, sizeof(sin)) == -1)
			Error(103, "Port: %d, Errno: %d, Errmsg: %s", port, errno,
					strerror(errno));
		if (listen(server_sockfd_, kBacklog) == -1)
			Error(104, "Port: %d, Errno: %d, Errmsg: %s", port, errno,
					strerror(errno));
	}
}
void Broker::Start(Trie *trie, CaseFolding *casefolding,
		bool longest, int worddivider_cnt, WordDivider **worddividers) {
	if (server_sockfd_ != -1) {
		pthread_attr_t attr;
		int rtn = pthread_attr_init(&attr);
		if (rtn) Error(105, "Errno: %d, Errmsg: %s", rtn, strerror(rtn));
		rtn = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (rtn) Error(106, "Errno: %d, Errmsg: %s", rtn, strerror(rtn));
		for (;;) {
			ThreadArg *parg = new ThreadArg(trie, casefolding,
					worddivider_cnt, worddividers);
			parg->client_sockfd_ = accept(server_sockfd_, NULL, NULL);
			if (parg->client_sockfd_ < 0 && errno != EINTR)
				Message(101, "Errno: %d, Errmsg: %s\n", errno, strerror(errno));
			else {
				pthread_t cthread;
				rtn = pthread_create(&cthread, &attr, Broker::Client, (void*)parg);
				if (rtn) Message(102, "Errno: %d, Errmsg: %s\n", rtn, strerror(rtn));
			}
		}
		rtn = pthread_attr_destroy(&attr);
		if (rtn) Error(107, "Errno: %d, Errmsg: %s", rtn, strerror(rtn));
	} else {
		char *line_buf = NULL;
		size_t line_size = 0;
		int line_tail;
		char *output_buf = (char*)malloc(kBufferIncrement);
		if (!output_buf)
			Error(108, "Failed to allocate memory");
		int output_size = kBufferIncrement;
		char *folding_buf = NULL;
		size_t folding_size = 0, line_num = 0;
		while ((line_tail = getline(&line_buf, &line_size, stdin)) != -1) {
			line_num++;
			if (line_tail > 0 && line_buf[line_tail-1] == '\n')
				line_buf[--line_tail] = 0;
			if (line_tail > 0 && line_buf[line_tail-1] == '\r')
				line_buf[--line_tail] = 0;
			if (!line_tail) continue;
			char *input_text = strrchr(line_buf, '\t');
			if (!input_text)
				input_text = line_buf;
			else
				*input_text++ = 0;
			if (!IsUtf8(input_text))
				Error(109, "Line %lu of stdin has non-UTF8 character", line_num);
			char *annotate_ptr = input_text;
			if (casefolding)
				annotate_ptr = casefolding->FoldOrDie(input_text, &folding_buf,
						&folding_size);
			*output_buf = 0;
			int output_tail = 0;
			trie->Annotate(worddividers[0], longest, 
					annotate_ptr, &output_buf, &output_size, &output_tail, input_text);
			for (char *s = output_buf; *s != 0; s++) {
				if (*s == '\n') {
					fputc('\t', stdout);
					fputs(line_buf, stdout);
				}
				fputc(*s, stdout);
			}
		}
		if (folding_buf) free(folding_buf);
		if (line_buf) free(line_buf);
		free(output_buf);
	}
}
bool Broker::Send(int sockfd, char *buf, int size) {
	for (int i = 0; i < size; ) {
		int len = write(sockfd, buf + i, size - i);
		if (len == -1)
			Message(103, "Errno: %d, Errmsg: %s\n", errno, strerror(errno));
		if (len <= 0) return false;
		i += len;
	}
	return true;
}
void *Broker::Client(void *arg) {
	ThreadArg *parg = (ThreadArg*)arg;
	char banner[128];
	if (strlen(PACKAGE_VERSION) != 8)
		Error(110, "PACKAGE_VESION is not 8-byte long, contact developer please");
	snprintf(banner, sizeof(banner), "mgrep\n%s %c\n", PACKAGE_VERSION,
			'0'+parg->worddivider_cnt_);
	if (Send(parg->client_sockfd_, banner, strlen(banner))) {
		int input_size = kBufferIncrement;
		char *input_buf = (char*)malloc(input_size);
		if (!input_buf)
			Error(111, "Failed to allocate memory");
		char *output_buf = (char*)malloc(kBufferIncrement);
		if (!output_buf)
			Error(112, "Failed to allocate memory");
		int output_size = kBufferIncrement;
		char *folding_buf = NULL;
		size_t folding_size = 0;
		for (;;) {
			int input_tail = 0;
			bool quit = false;
			while (!input_tail || input_buf[input_tail-1] != '\n') {
				int len = read(parg->client_sockfd_, input_buf+input_tail,
						input_size-input_tail);
				if (len == 0) {
					quit = true;
					break;
				} else if (len == -1) {
					Message(104, "Errno: %d, Errmsg: %s\n", errno, strerror(errno));
					quit = true;
					break;
				} else if (len > input_size-input_tail) {
					Message(105, "Contact developer please\n");
					quit = true;
					break;
				}
				input_tail += len;
				if (input_tail == input_size) {
					input_size += kBufferIncrement;
					input_buf = (char*)realloc(input_buf, input_size);
					if (!input_buf)
						Error(113, "Failed to allocate memory");
				}
			}
			if (quit) break;
			input_buf[--input_tail] = 0;
			if (input_tail <= 3 || *input_buf != 'A') {
				Message(106, "Wrong protocol, terminating connection.\n");
				break;
			}
			if (!IsUtf8(input_buf+3)) {
				Message(107, "Non-UTF8 character found, terminating connection\n");
				break;
			}
			char *annotate_ptr = input_buf+3;
			if (parg->casefolding_)
				annotate_ptr = parg->casefolding_->FoldOrDie(input_buf+3,
					&folding_buf, &folding_size);
			*output_buf = 0;
			int output_tail = 0;
			int wd_idx = input_buf[2] - '0';
			if (wd_idx < 0 || wd_idx >= parg->worddivider_cnt_) {
				Message(108, "Wrong word divider index %d, terminating connection\n",
						wd_idx);
				break;
			}
			parg->trie_->Annotate(parg->worddividers_[wd_idx], input_buf[1] == 'Y',
					annotate_ptr, &output_buf, &output_size, &output_tail, input_buf+3);
			output_buf[output_tail++] = '\n';
			if (!Send(parg->client_sockfd_, output_buf, output_tail)) break;
		}
		if (folding_buf) free(folding_buf);
		free(input_buf);
		free(output_buf);
	}
	shutdown(parg->client_sockfd_, SHUT_RDWR);
	close(parg->client_sockfd_);
	delete parg;
	pthread_exit(NULL);
	return NULL;
}
