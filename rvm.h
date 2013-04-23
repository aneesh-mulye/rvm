#ifndef _RVM_H
#define _RVM_H
#include <string>
#include <list>
#include <map>
#include <set>

typedef struct segment{
	std::string segname; /* ALWAYS relative to the pathname. */
	int size;
	void *segbase; /* The problem is, this cannot be used as a unique
			  key inside transactions, because we cannot guarantee
			  that malloc won't return another segbase pointer
			  identical to a previous one. */
	int busy; /* HACK! But fine for now. */
	~segment();
} segment_t;

typedef struct trans_range {
	int offset;
	int size;
	void *backup;

	~trans_range();
} trans_range_t;

typedef struct trans_base {
	int tid;
	std::set<void *> segbases; /* Remove the ones that are unmapped in
					   the
					   midde of the transaction; do not
					   write those back. */
	std::map<void *, std::list<trans_range_t> > mods;
} trans_base_t;

typedef int trans_t;

typedef struct rvm_struct {
	std::string pathname; /* Absolute path of the directory; is used as the
				 unique key. */
	std::list<segment_t> mapped_segs;
	std::list<trans_base_t> transactions;

} rvm_struct_t;

typedef rvm_struct_t* rvm_t;

/* Exact API follows. */
rvm_t rvm_init(const char *directory);
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create);
void rvm_unmap(rvm_t rvm, void *segbase);
void rvm_destroy(rvm_t rvm, const char *segname);
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size);
void rvm_commit_trans(trans_t tid);
void rvm_abort_trans(trans_t tid);
void rvm_truncate_log(rvm_t rvm);
#endif
