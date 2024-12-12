//
//  HashFileThread.hpp
//  lldupdir
//
//  Created by dennis.lang on 12/3/24.
//  Copyright © 2024 Dennis Lang. All rights reserved.
//

#include <thread>
#include "md5.hpp"
#include "xxhash64.hpp"

// typedef lstring HashValue;       // md5
typedef uint64_t HashValue;         // xxHash64

class HashFileThread  {
public:
    HashValue hashValue;
    lstring fullPath;
    std::thread thread1;
    
    HashFileThread(const lstring& _fullPath) :
        fullPath(_fullPath),
        thread1(&HashFileThread::doWork, this)
    { }
 
                
    void doWork() {
        hashValue = XXHash64::compute(fullPath);
    }
    
    void join() {
        thread1.join();
    }
};
