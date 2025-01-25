//-------------------------------------------------------------------------------------------------
//
// File: dupscan.cpp   Author: Dennis Lang  Desc: Find duplicate files.
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

#include "ll_stdhdr.hpp"
#include "signals.hpp"
#include "dupscan.hpp"
#include "directory.hpp"
#include "hasher.hpp"

#include <assert.h>
#include <iostream>

// ---------------------------------------------------------------------------
template <class TT>
lstring& toString(lstring& buf, TT value) {
    buf.resize(buf.capacity());
    int len = std::snprintf((char*)buf.c_str(), buf.capacity(), "%ul", (unsigned long)value);
    // buf.c_str[len] = '\0';
    buf.resize(len);
    return buf;
}

// ---------------------------------------------------------------------------
static size_t fileLength(const lstring& path) {
    struct stat info;
    return (stat(path, &info) == 0) ? info.st_size : -1;
}

// ---------------------------------------------------------------------------
DupScan::DupScan(Command& _command) : command(_command)  {
}

// ---------------------------------------------------------------------------
bool DupScan::findDuplicates(unsigned level, const StringList& baseDirList, StringSet& subDirList) const {
    scanFiles(level, baseDirList, subDirList);

    StringSet outDirList;
    getDirs(level, baseDirList, subDirList, outDirList);
    subDirList.swap(outDirList);

    return subDirList.size() > 0;
}

// ---------------------------------------------------------------------------
void DupScan::done() {
    if (command.useThreads) {
        Hasher::waitForAsync(command);
    }
}

// ---------------------------------------------------------------------------
void DupScan::scanFiles(unsigned level, const StringList& baseDirList, const StringSet& nextDirList) const {
    StringSet files;
    getFiles(level, baseDirList, nextDirList, files);
    compareFiles(level, baseDirList, files);
}

// ---------------------------------------------------------------------------
void DupScan::getFiles(unsigned level, const StringList& baseDirList, const StringSet& nextDirList, StringSet& outFiles) const {
    lstring joinBuf;
    for (const lstring& nextDir : nextDirList) {
        for (const lstring& baseDir : baseDirList) {
            Directory_files directory(DirUtil::join(joinBuf, baseDir, nextDir));

            while (!Signals::aborted && directory.more()) {
                if (! directory.is_directory()) {
                    lstring name(directory.name());
                    lstring fullname;
                    directory.fullName(fullname);
                    if (command.validFile(name, fullname)) {
                        outFiles.insert(DirUtil::join(joinBuf, nextDir, name));
                    }
                }
            }
            if (Signals::aborted) {
                outFiles.clear();
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
void DupScan::getDirs(unsigned level, const StringList& baseDirList, const StringSet& nextDirList, StringSet& outDirList) const {
    lstring joinBuf;
    for (const lstring& nextDir : nextDirList) {
        for (const lstring& baseDir : baseDirList) {
            Directory_files directory(DirUtil::join(joinBuf, baseDir, nextDir));
            lstring fullname;

            while (!Signals::aborted && directory.more()) {
                directory.fullName(fullname);
                if (directory.is_directory() && command.validFile(directory.name(), fullname)) {
                    outDirList.insert(DirUtil::join(joinBuf, nextDir, directory.name()));
                }
            }
            if (Signals::aborted) {
                outDirList.clear();
                return;
            }
        }
    }
}

bool showIt = false;
template <typename TT>
void showValue(const lstring& path, const char* tag, TT value) {
    if (showIt) {
        if (value != 0 && value != -1)
            cerr << tag << value << " " << path << std::endl;
        else
            cerr << tag << "none " << path << std::endl;
    }
}

// ---------------------------------------------------------------------------
void DupScan::compareFiles(unsigned level, const StringList& baseDirList, const StringSet& files) const {
    lstring joinBuf1, joinBuf2;
    showIt = command.verbose;   // hack
    
    for (const lstring& file : files) {
        StringList::const_iterator dirIter = baseDirList.begin();
        size_t fileLen1 = fileLength(DirUtil::join(joinBuf1, *dirIter++, file));
        size_t fileLen2 = 0;
        bool matchingLen = true;

        showValue(joinBuf1, " len1=", fileLen1);
 
        while (!Signals::aborted && dirIter != baseDirList.end()) {
            fileLen2 = fileLength(DirUtil::join(joinBuf2, *dirIter++, file));

            showValue(joinBuf2, " len2=", fileLen2);

            if (command.justName) {
                if (fileLen1 == fileLen2)
                    command.showDuplicate(joinBuf1, joinBuf2);
                else if (fileLen1 != -1 && fileLen2 != -1)
                    command.showDifferent(joinBuf1, joinBuf2);
                else
                    command.showMissing((fileLen1 != -1), joinBuf1, (fileLen2 != -1), joinBuf2);
            } else {
                if (fileLen1 != fileLen2) {
                    matchingLen = false;  // currently only two items in baseDirList, so no need to exit early
                }
            }
        }

        if (fileLen1 == -1 && fileLen2 == -1)
            continue; // both file paths are missing
        
        if (command.justName)
            continue;

        if (matchingLen) {
            if (command.useThreads) {
                Hasher::findDupsAsync(command, baseDirList, file);
            } else {
                dirIter = baseDirList.begin();
                DirUtil::join(joinBuf1, *dirIter++, file);
                joinBuf1 = command.absOrRel(joinBuf1);
                HashValue hash1 = Hasher::compute(joinBuf1);  // hashValue = Md5::compute(joinBuf);

                showValue(joinBuf1, " hash1=", hash1);

                while (!Signals::aborted && dirIter != baseDirList.end()) {
                    DirUtil::join(joinBuf2, *dirIter++, file);
                    joinBuf2 = command.absOrRel(joinBuf2);
                    HashValue hash2 = Hasher::compute(joinBuf2); // hashValue = Md5::compute(joinBuf);

                    showValue(joinBuf2, " hash2=", hash2);

                    if (hash1 == hash2) {
                        command.showDuplicate(joinBuf1, joinBuf2);
                    } else {
                        command.showDifferent(joinBuf1, joinBuf2);
                    }
                }
            }
        } else {
            if (fileLen1 != -1 && fileLen2 != -1)
                command.showDifferent(joinBuf1, joinBuf2);
            else
                command.showMissing((fileLen1 != -1), joinBuf1, (fileLen2 != -1), joinBuf2);
        }
    }
}

