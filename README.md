rvm
===

Recoverable Virtual Memory (CS 6210 Project 4)

Compilation
----------

1.  To compile librvm.a:
	
	make

2.  To compile the default tests:

	make tests

Logfiles for persistency and transaction semantics
-------------------------------------------------

The library as designed is capable of using multiple backing store directories
simultaneously, each associated with a different rvm\_t handle. Each such
backing store uses only a single log, named rvmlog. Each segment has its own
segment file. The name of the segment file is the segment name with .rvmseg
suffixed to it, to prevent name clashes. The log is not structurally aware of
transaction semantics; it is a collection of records, and transactional
guarantees are provided entirely by the design of the API.

Each record contains a segment name length, the segment name, and the number of
ranges within that segment to be written back to the segment file. Each range
record consists of an offset, a size, and the actual raw data to be written
back.

Log truncation occurs when a segment is destroyed (to ensure that the log file
retains no entries pointing to a nonexistent or invalid segment) and mapped
(to ensure correctness).

Transactional semantics are provided by means of the redo log. Each
rvm\_about\_to\_modify call causes the memory region about to be modified
to be written to the head of the in-memory transaction log. This log is
structured as a linked list. During an abort, this list is traversed, and all
backed-up regions written back. Older entries appear later in this list, and
thus overwrite newer ones in memory if they overlap. This is guaranteed correct
and simple, but comes at the cost of performance. A commit operation dumps the
entire in-memory transaction log onto the on-disk redo log, and blocks until
it's flushed through an fsync call. The in-memory transaction log has one list
per segment.

To ensure that edge cases are handled correctly, the transaction log also
contains a set of segbases, which identify which segments the transaction log
contains. When a segment is unmapped or destroyed during a transaction, the
lists in the transaction log remains, but the segbase is removed from the set.
During write-back, only the valid segments are written back to memory (for
abort) or disk (for commit). Destroying a segment also unmaps it.

As every map and destroy operation involves a log truncation, I do not expect
log files to grow without bound. A long-running session with many transactions
does lead to a significant increase in the size of the redo log, as each hint
call (about_to_modify) leads to a copy of its memory region being made and, if
the transaction is committed, being written back to disk. (One possible method
of alleviating this is to check the log size on each commit, and truncate it if
it goes above some preset size. I have not done this.)

General thoughts and comments
-----------------------------

The simplicity of the interface what works truly well. The tradeoff, however,
has been in performance. Though we are not being graded on performance on this
project, only correctness, the design I adopted is less performant than I had
hoped. Specifically, the structure of the redo log is very space-intensive.
I would have preferred to have a consolidated redo log which kept track of
ranges and did range-coalescing, but I found that it increased code complexity.

The simplicity the authors promised in the RVM paper, however, does show
through. The code is small, and took a remarkably short time to write.

What I realised while writing it was that significantly more robust guarantees
than the one we are being graded for could have been provided for with a little
more polishing. We do not need to consider the case, for example, where the
machine crashes in the middle of an API call. Adding this in would provide a
significantly stronger guarantee, with only slightly more effort. I think it is
the simplicity of the original LRVM design that allows for this.

Some other, more general thoughts also suggested themselves to me while doing
the project: is it possible to have a machine with *true* persistent memory,
whose the state of the memory hierarchy is invariant across reboots? How much
of a performance penalty would it impose? The availability of SSDs has already
led to the addition of another layer between memory and disk for speed-hungry
applications. Is it possible to design a memory system (and concomitantly
modified OS) that preserves consistent state between power failures, perhaps
losing no more than a certain amount of time or changes?
