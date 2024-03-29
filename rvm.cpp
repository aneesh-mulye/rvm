#include "rvm.h"
#include <limits.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <string>
#include <string.h>
#include <stdio.h>
#include <errno.h>

namespace {
	int trans_counter = 0;
	std::list<rvm_struct_t> rvms;
	std::map<trans_t, rvm_t> trtorvm;
	int debug = 0;
	int ismapped(rvm_t rvm, const char *segname);
	std::list<segment_t>::iterator ispmapped(rvm_t rvm, void * segbase);
	std::list<segment_t>::iterator isnmapped(rvm_t, const char *);
	std::string getsegname(rvm_t, void *);

	int ismapped(rvm_t rvm, const char *segname) {

		std::list<segment_t>::iterator ii;
		std::string sname(segname);

		for(ii =  rvm->mapped_segs.begin();
				ii != rvm->mapped_segs.end(); ii++)
			if(sname == ii->segname)
				return 1;

		return 0;
	}

	std::list<segment_t>::iterator ispmapped(rvm_t rvm, void * segbase) {

		std::list<segment_t>::iterator ii;

		for(ii = rvm->mapped_segs.begin();
				ii != rvm->mapped_segs.end(); ii++)
			if(ii->segbase == segbase)
				break;

		return ii;
	}

	std::list<segment_t>::iterator isnmapped(rvm_t rvm,
			const char *segname) {

		std::list<segment_t>::iterator ii;
		std::string sname(segname);

		for(ii = rvm->mapped_segs.begin();
				ii != rvm->mapped_segs.end(); ii++)
			if(ii->segname == sname)
				break;

		return ii;
	}

	std::string getsegname(rvm_t rvm, void * segbase) {

		std::string retval;
		std::list<segment_t>::iterator ii;

		for(ii = rvm->mapped_segs.begin();
				ii != rvm->mapped_segs.end(); ii++)
			if(ii->segbase == segbase) {
				retval = ii->segname;
				break;
			}

		return retval;
	}
}

rvm_t rvm_init(const char *directory) {
	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	/* Get the absolute pathname of the directory. */
	char *abspath;
	int found = 0;
	abspath = realpath(directory, 0x0);
	if(!abspath) {
		return 0x0;
	}
	std::string path(abspath);
	/* If is exists, destroy everything and return it. */
	std::list<rvm_struct_t>::iterator ii;

	for(ii = rvms.begin(); ii != rvms.end(); ii++)
		if(ii->pathname == path) {
			found = 1;
			break;
		}

	if(found) {
		rvms.erase(ii);
		std::string command;
		command = "rm -rf " + path + "/*.rvmseg " + path + "/rvmlog";
		system(command.c_str());
	}

	rvm_struct_t newrvm;
	newrvm.pathname = path;
	system(("touch "+path+"/rvmlog").c_str());

	rvms.push_back(newrvm);
	return &(rvms.back());
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {

	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	int fd;
	void * segbuf;
	struct stat segstat;
	char buf = 0;
	rvm_truncate_log(rvm);
	if(ismapped(rvm, segname))
		return 0x0;

	/* I presume here that size_to_create and the existing file size are
	 * both lower bounds on the final size of the segment. */

	system(("touch "+rvm->pathname+"/"+segname+".rvmseg").c_str());
	fd = open((rvm->pathname+"/"+segname+".rvmseg").c_str(),
			O_RDWR);
	if(-1 == fd)
		return 0x0;

	lseek(fd, size_to_create-1, SEEK_SET);
	read(fd, &buf, 1);
	lseek(fd, size_to_create-1, SEEK_SET);
	if(-1 == write(fd, &buf, 1)) {
		close(fd);
		return 0x0;
	}
	/* The above ensures that the file is AT LEAST as large as
	 * size_to_create. */
	if(-1 == fstat(fd, &segstat)) {
		close(fd);
		return 0x0;
	}

	if(segstat.st_size > size_to_create)
		size_to_create = segstat.st_size;

	lseek(fd, 0, SEEK_SET);
	segbuf = malloc(size_to_create);

	if(0x0 == segbuf) {
		close(fd);
		return 0x0;
	}

	if(-1 == read(fd, segbuf, size_to_create)) {
		close(fd);
		free(segbuf);
		return 0x0;
	}

	lseek(fd, 0, SEEK_SET);

	segment_t tempseg;
	tempseg.segname = std::string(segname);
	tempseg.size = size_to_create;
	tempseg.segbase = 0x0;
	tempseg.busy = 0;

	rvm->mapped_segs.push_back(tempseg);
	rvm->mapped_segs.back().segbase = segbuf;

	return segbuf;
}

void rvm_unmap(rvm_t rvm, void *segbase) {
	
	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	std::list<segment_t>::iterator segi = ispmapped(rvm, segbase);
	if(segi == rvm->mapped_segs.end())
		return;

	if(segi->busy) {

		std::list<trans_base_t>::iterator ii;

		/* Notify transactions that this segment has been unmapped;
		 * write-back should not occur. */
		for(ii = rvm->transactions.begin(); 
				ii != rvm->transactions.end(); ii++)
			ii->segbases.erase(segbase);
	}

	rvm->mapped_segs.erase(segi);

	return;
}

void rvm_destroy(rvm_t rvm, const char *segname) {

	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	rvm_truncate_log(rvm); /* Ensures that the redo log contains no
				  references to invalid segments. */
	std::list<segment_t>::iterator segi = isnmapped(rvm, segname);
	if(segi != rvm->mapped_segs.end())
		rvm_unmap(rvm, segi->segbase);

	system(("rm -rf "+rvm->pathname+"/"+segname+".rvmseg").c_str());
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
	trans_base_t temptrans;
	std::list<trans_range_t> temprangelist;
	std::list<segment_t>::iterator ii;
	int i;

	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	/* Code to check if all segments are free, and abort if not. */
	for(ii = rvm->mapped_segs.begin();
			ii != rvm->mapped_segs.end(); ii++)
		for(i = 0; i<numsegs; i++)
			if(ii->segbase == segbases[i] && ii->busy)
				return -1;

	for(ii = rvm->mapped_segs.begin();
			ii != rvm->mapped_segs.end(); ii++)
		for(i = 0; i<numsegs; i++)
			if(ii->segbase == segbases[i])
				ii->busy = 1;

	rvm->transactions.push_back(temptrans);
	rvm->transactions.back().tid = trans_counter++;

	trtorvm[rvm->transactions.back().tid] = rvm;

	for(i=0; i<numsegs; i++) {
		rvm->transactions.back().mods[segbases[i]] = temprangelist;
	}
	rvm->transactions.back().segbases.insert(segbases, segbases+numsegs);

	return rvm->transactions.back().tid;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {

	std::list<trans_base_t>::iterator ii;
	rvm_t last_rvm;
	trans_range_t temprange;
	temprange.offset = offset;
	temprange.size = size;
	temprange.backup = 0x0;

	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	if(trtorvm.count(tid) == 0)
		return;

	last_rvm = trtorvm[tid];

	for(ii = last_rvm->transactions.begin();
			ii != last_rvm->transactions.end(); ii++)
		if(tid == ii->tid)
			break;

	if(last_rvm->transactions.end() == ii)
		return;

	if(0 == ii->segbases.count(segbase))
		return;

	ii->mods[segbase].push_front(temprange);
	if(0x0 == (ii->mods[segbase].front().backup = malloc(size)))
		return;

	memcpy(ii->mods[segbase].front().backup, ((char *)segbase)+offset,
			size);

	return;
}

void rvm_abort_trans(trans_t tid) {

	trans_base_t  *trp = 0x0;
	rvm_t last_rvm;
	std::list<trans_base_t>::iterator ti;
	std::map<void *, std::list<trans_range_t> >::iterator mi;
	std::list<segment_t>::iterator si;
	
	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	if(trtorvm.count(tid) == 0)
		return;
	last_rvm = trtorvm[tid];
	trtorvm.erase(tid);

	for(ti = last_rvm->transactions.begin();
			ti != last_rvm->transactions.end(); ti++)
		if(tid == ti->tid) {
			trp = &*ti;
			break;
		}

	if(last_rvm->transactions.end() == ti)
		return;

	/* For every valid segment, restore memory. */
	for(mi = trp->mods.begin(); mi != trp->mods.end(); mi++) {
		if(0 == trp->segbases.count(mi->first))
			continue;
		std::list<trans_range_t>::iterator trit;
		for(trit = mi->second.begin(); trit != mi->second.end(); trit++)
			memcpy(((char *)mi->first) + trit->offset, trit->backup,
					trit->size);
	}

	/* Mark all valid segments non-busy. */
	for(si = last_rvm->mapped_segs.begin();
			si != last_rvm->mapped_segs.end(); si++)
		if(trp->segbases.count(si->segbase) == 1)
			si->busy = 0;

	last_rvm->transactions.erase(ti);
}

void rvm_commit_trans(trans_t tid) {

	trans_base_t  *trp = 0x0;
	rvm_t last_rvm;
	std::list<trans_base_t>::iterator ti;
	std::map<void *, std::list<trans_range_t> >::iterator mi;
	std::list<segment_t>::iterator si;
	int logfd;
	
	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	if(trtorvm.count(tid) == 0)
		return;
	last_rvm = trtorvm[tid];
	trtorvm.erase(tid);

	for(ti = last_rvm->transactions.begin();
			ti != last_rvm->transactions.end(); ti++)
		if(tid == ti->tid) {
			trp = &*ti;
			break;
		}

	if(last_rvm->transactions.end() == ti)
		return;

	logfd = open((last_rvm->pathname+"/rvmlog").c_str(), O_RDWR, O_APPEND);
	if(-1 == logfd)
		fprintf(stderr, "%s: %s (%d)\n",
				__func__, strerror(errno), logfd);
	lseek(logfd, 0, SEEK_END);
	/* For every valid segment, write record to the log. */
	for(mi = trp->mods.begin(); mi != trp->mods.end(); mi++) {
		if(0 == trp->segbases.count(mi->first))
			continue;
		std::string currsegname(getsegname(last_rvm, mi->first));
		int tempint = currsegname.length() + 1;
		write(logfd, &tempint, sizeof(int));
		write(logfd, currsegname.c_str(), tempint);
		tempint = mi->second.size();
		write(logfd, &tempint, sizeof(int));
		std::list<trans_range_t>::iterator trit;
		for(trit = mi->second.begin();trit != mi->second.end();trit++) {
			write(logfd, &(trit->offset),
					sizeof(int));
			write(logfd, &(trit->size),
					sizeof(int));
			write(logfd,
					((char*)mi->first) + trit->offset,
					trit->size);
		}
	}

	fsync(logfd);
	close(logfd);

	/* Mark all valid segments non-busy. */
	for(si = last_rvm->mapped_segs.begin();
			si != last_rvm->mapped_segs.end(); si++)
		if(trp->segbases.count(si->segbase) == 1)
			si->busy = 0;

	last_rvm->transactions.erase(ti);
}

void rvm_truncate_log(rvm_t rvm) {

	char *segname;
	int offset, size, segnamelen, currsegranges, logfd;
	int segfd;
	int i;
	void *data;
	struct stat statbuf;

	if(debug)
		fprintf(stderr, "%s called.\n", __func__);

	logfd = open((rvm->pathname+"/rvmlog").c_str(), O_RDONLY, O_CREAT);
	if(-1 == logfd) {
		fprintf(stderr, "%s: Failed to open log: %s\n",
				__func__, strerror(errno));
		return;
	}

	lseek(logfd, 0, SEEK_SET);
	if(-1 == fstat(logfd, &statbuf)) {
		fprintf(stderr, "%s: %s (%d) \n", __func__, strerror(errno),
				logfd);
		return;
	}

	if(0 == statbuf.st_size) {
		close(logfd);
		if(debug)
			fprintf(stderr,
				"Returning from %s at %d: nothing in log\n",
				__func__, __LINE__);
		return;
	}

	while(lseek(logfd, 0, SEEK_CUR) < statbuf.st_size) {

		//printf("%ld\n", lseek(logfd, 0, SEEK_CUR));
		//printf("%s\n", strerror(errno));
		read(logfd, &segnamelen, sizeof(int));
		segname = (char *)malloc(segnamelen);
		read(logfd, segname, segnamelen);
		segfd = open((rvm->pathname+"/"+segname+".rvmseg").c_str(),
				O_WRONLY);
		read(logfd, &currsegranges, sizeof(int));
		for(i=0; i<currsegranges; i++) {
			read(logfd, &offset, sizeof(int));
			read(logfd, &size, sizeof(int));
			data = malloc(size);
			read(logfd, data, size);
			lseek(segfd, offset, SEEK_SET);
			write(segfd, data, size);
			free(data);
		}
		fsync(segfd);
		close(segfd);
		free(segname);
	}
	close(logfd);
	system(("rm -rf "+rvm->pathname+"/rvmlog").c_str());
	system(("touch "+rvm->pathname+"/rvmlog").c_str());
}

segment::~segment() {
	if(segbase)
		free(segbase);
}

trans_range::~trans_range() {
	if(backup)
		free(backup);
}
