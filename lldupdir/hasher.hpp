//
//  HashFileThread.hpp
//  lldupdir
//
//  Created by dennis.lang on 12/3/24.
//  Copyright © 2024 Dennis Lang. All rights reserved.
//

#include "ll_stdhdr.hpp"
#include "command.hpp"
#include <vector>


typedef std::vector<lstring> StringList;

#if 0
#include <thread>
#include "md5.hpp"
#include "xxhash64.hpp"
#endif
// typedef lstring HashValue;       // md5
typedef uint64_t HashValue;         // xxHash64



class Hasher  {
public:
    // Compute hash values of a set of files using threads. 
    static void findDupsAsync(Command& _command, const StringList& baseDirList, const string& file);
    static void waitForAsync(Command& command);

    // Compute hash value of a single file in caller's thread. 
    static HashValue compute(const string & path);
};
