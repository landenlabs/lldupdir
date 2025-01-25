//-------------------------------------------------------------------------------------------------
//
// File: hasher.cpp   Author: Dennis Lang  Desc: Compute hash value of file contents
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com
//
//  
//
// ----- License ----
//
// Copyright (c) 2024  Dennis Lang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "hasher.hpp"
#include "directory.hpp"

#include <assert.h>
#include <thread>
#include <shared_mutex>
#include <atomic>
#include <deque>
#include <list>
#include <iostream>


#ifdef USE_MD5
#include "md5.hpp"
// typedef lstring HashValue;
#else
typedef unsigned int uint;  // required by xxhash64
#include "xxhash64.hpp"
// typedef uint64_t HashValue;
#endif

typedef unsigned int ThreadCnt;
std::atomic<ThreadCnt> threadCnt(0);
const ThreadCnt MAX_THREADS = 8;
std::shared_timed_mutex  lock1;   // locked while thread count is increasing

// -----
class ThreadJob  {
public:
    lstring path;
    HashValue hashValue;
    volatile bool isDone;
    std::thread thread1;

    ThreadJob(const lstring& _path): path(_path), hashValue(0), isDone(false), 
        thread1(&ThreadJob::doWork, this) // starts thread
    {
    }
    void join();
    void doWork();
};

void ThreadJob::doWork() {
    try { 
        hashValue = Hasher::compute(path);  // hashValue = Md5::compute(joinBuf);
    } catch (...) {
    }
    threadCnt--;
    isDone = true;
}

void ThreadJob::join()  {
    if (thread1.joinable())
        thread1.join();
}
// ----

typedef std::vector<ThreadJob*> ThreadGroup;
typedef std::list<ThreadGroup> ThreadGroups;
static ThreadGroups threadGroups;
// static ThreadGroups doneGroups;

// Forward declaration
void anyFinishedGroups(Command& command);
void finishGroup(Command& command, ThreadGroup& group);

void Hasher::findDupsAsync(Command& command, const StringList& baseDirList, const string& file) {
    lstring joinBuf1;
    const char* fileStr = file.c_str();
    ThreadGroup group;

    anyFinishedGroups(command);
    while (threadCnt > MAX_THREADS || (threadCnt > 0 && (threadCnt + baseDirList.size()) > MAX_THREADS)) {
        // cerr << "Waiting, threadCnt=" << threadCnt << std::endl;
        (void) lock1.try_lock_shared_for(std::chrono::seconds(1));
        // cerr << "Resume, threadCnt=" << threadCnt << std::endl;
        // anyFinishedGroups(command);
    }

    (void) lock1.try_lock_shared();
    for (StringList::const_iterator dirIter = baseDirList.begin(); dirIter != baseDirList.end(); dirIter++) {
        DirUtil::join(joinBuf1, *dirIter, fileStr);
        joinBuf1 = command.absOrRel(joinBuf1);
        threadCnt++;
        group.push_back(new ThreadJob(joinBuf1));
    }
    threadGroups.push_back(group);
}

void Hasher::waitForAsync(Command& command) {
    while (threadCnt > 0) {
        // std::cerr << "waiting for all threads to finish, cnt=" << threadCnt << std::endl;
        anyFinishedGroups(command);
        if (threadCnt != 0) 
            (void)lock1.try_lock_shared_for(std::chrono::seconds(1));
    }
    // std::cerr << "Done using " << MAX_THREADS << " threads\n";
}

void anyFinishedGroups(Command& command) {
    uint dbgIdx = 0;
    ThreadGroups::iterator grpIter = threadGroups.begin();
    while ( grpIter != threadGroups.end()) {
        ThreadGroup& group = *grpIter;
        size_t cntRunning = group.size();
        for (ThreadGroup::iterator jobIter = group.begin(); jobIter != group.end(); ++jobIter) {
            ThreadJob* jobPtr = *jobIter;
            if (jobPtr->isDone) {
                jobPtr->join();
                cntRunning--;
            }
        }

        // cerr << threadCnt <<  " Groups=" << threadGroups.size() << " item[" << dbgIdx++ << "] Running=" << cntRunning << std::endl;

        if (cntRunning == 0) {
            finishGroup(command, group);
            grpIter = threadGroups.erase(grpIter);
            lock1.unlock_shared();
        } else {
            ++grpIter;
        }
    }
}

void finishGroup(Command& command, ThreadGroup& group) {
   ThreadGroup::iterator jobIter = group.begin(); 
   ThreadJob* firstPtr = *jobIter++;
   if (command.verbose)
       cerr << firstPtr->path << " hash=" << firstPtr->hashValue << std::endl;

   while (jobIter != group.end()) {
       ThreadJob* secondPtr  = *jobIter++;
       if (command.verbose)
           cerr << secondPtr->path << " hash=" << secondPtr->hashValue << std::endl;

       if (firstPtr->hashValue == secondPtr->hashValue) {
           command.showDuplicate(firstPtr->path, secondPtr->path);
       } else {
           command.showDifferent(firstPtr->path, secondPtr->path);
       }

       delete secondPtr;
   }
   delete firstPtr;
   group.clear();
}

const uint BUFFER_SIZE = 4096 * 16;
const uint NUM_BUFFERS = MAX_THREADS * 2;
char BUFFERS[NUM_BUFFERS][BUFFER_SIZE];
std::deque<uint> bufferIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
mutex bufferLock;
uint nextBufIdx()   {
    lock_guard<std::mutex> lock(bufferLock);
    assert(!bufferIndexes.empty());
    uint returnVal = bufferIndexes.front();
    bufferIndexes.pop_front();
    return returnVal;
}
void doneWithBufIdx(uint val) {
    lock_guard<std::mutex> lock(bufferLock);
    bufferIndexes.push_back(val);
}
HashValue Hasher::compute(const string& path) {
    uint bufferIdx = nextBufIdx();
    char* buffer = BUFFERS[bufferIdx];
    HashValue hashValue = XXHash64::compute(path.c_str(), buffer, BUFFER_SIZE);
    doneWithBufIdx(bufferIdx);
    return hashValue;
}