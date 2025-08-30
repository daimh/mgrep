#ifndef MGREP_SRC_TREENODE_H
#define MGREP_SRC_TREENODE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
template <class T>
class TreeNode {
	public:
		TreeNode(char *word, T *id_ptr, int id_size) {
			if (!word || !*word) {
				fprintf(stderr, "Empty/Null word is not allowed\n");
				exit(1);
			}
			word_ = strdup(word);
			right_ = down_ = NULL;
			InitIdBuf(id_ptr, id_size);
		}
		~TreeNode() {
			if (down_) delete down_;
			if (right_) delete right_;
			if (id_buf_) free(right_);
			free(word_);
		}
		TreeNode<T> *InsertWord(char *word, T *id_ptr, int id_size) {
			TreeNode *pRtn = this;
			int idx = 0;
			char me, you;
			for (;;) {
				me = word_[idx];
				you = word[idx];
				if (me != you || me == 0) break;
				idx++;
			}
			if (idx == 0) {
				if (me < you) {
					if (down_)
						down_ = down_->InsertWord(word, id_ptr, id_size);
					else
						down_ = NewInstance(word, id_ptr, id_size);
				} else {
					pRtn = NewInstance(word, id_ptr, id_size);
					pRtn->down_ = this;
				}
			} else if (me != 0) {
				TreeNode<T> *pTmp = NewInstance(word_ + idx, NULL, 0);
				pTmp->id_buf_ = id_buf_;
				pTmp->right_ = right_;
				word_ = (char*) realloc(word_, sizeof(*word_) * (idx+1));
				word_[idx] = 0;
				right_ = pTmp;
				if (you == 0) {
					InitIdBuf(id_ptr, id_size);
				} else {
					id_buf_ = NULL;
					right_ = right_->InsertWord(word+idx, id_ptr, id_size);
				}
			} else if (you == 0) {
				int original_id_size = GetIdSize();
				id_buf_ = (char*)realloc(id_buf_,
						sizeof(T) * (id_size + original_id_size) + sizeof(int));
				*((int*)id_buf_) = id_size + original_id_size;
				CopyIdBuf(original_id_size, id_ptr, id_size);
			} else if (right_) {
				right_ = right_->InsertWord(word+idx, id_ptr, id_size);
			} else {
				right_ = NewInstance(word+idx, id_ptr, id_size);
			}
			return pRtn;
		}
		char *word() {
			return word_;
		}
		TreeNode *down() {
			return down_;
		}
		TreeNode *right() {
			return right_;
		}
		int GetIdSize() {
			return id_buf_ ? *((int*)id_buf_) : 0;
		}
		T *GetIdElement(int idx) {
			return ((T*)(id_buf_ + sizeof(int))) + idx;
		}
/*
		void DEBUG(char *sPrev) {
			int iLen = strlen(sPrev);
			iLen += strlen(word_);
			char *str = new char[iLen];
			strcpy(str, sPrev);
			strcat(str, word_);
			for (int i=0; i<GetIdSize(); i++)
				fprintf(stderr, "%d\t%s\n", *GetIdElement(i), str);
			if (right_) right_->DEBUG(str);
			delete [] str;
			if (down_) down_->DEBUG(sPrev);
		}
*/
	private:
		TreeNode *right_, *down_;
		char *word_;
		char *id_buf_;

	private:
		TreeNode<T> *NewInstance(char *word, T *id_ptr, int id_size) {
			return new TreeNode<T> (word, id_ptr, id_size);
		}
		void CopyIdBuf(int to_location, T *copy_from, int id_size) {
			T *copy_to = (T*)(id_buf_+sizeof(int));
//		for (int idx=0; idx<id_size; idx++)
//			copy_to[to_location+idx] = copy_from[idx];
			memcpy(copy_to+to_location, copy_from, sizeof(T) * id_size);
		}
		void InitIdBuf(T *id_ptr, int id_size) {
			if (id_size) {
				id_buf_ = (char*)malloc(sizeof(T) * id_size + sizeof(int));
				*((int*)id_buf_) = id_size;
				CopyIdBuf(0, id_ptr, id_size);
			} else {
				id_buf_ = NULL;
			}
		}
};
#endif // MGREP_SRC_TREENODE_H
