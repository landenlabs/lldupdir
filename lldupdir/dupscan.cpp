//-------------------------------------------------------------------------------------------------
//
// File: dupscan.cpp   Author: Dennis Lang  Desc: Find duplicate files.
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com
//
// This file is part of lldupdir project.
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
#include "dupscan.hpp"
#include "directory.hpp"

#ifdef USE_MD5
#include "md5.hpp"
typedef lstring HashValue;
#else
#include "xxhash64.hpp"
typedef uint64_t HashValue;
#endif

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
bool DupScan::findDuplicates(unsigned level, const StringList& baseDirList, StringSet& subDirList) const {
    scanFiles(level, baseDirList, subDirList);

    StringSet outDirList;
    getDirs(level, baseDirList, subDirList, outDirList);
    subDirList.swap(outDirList);

    return subDirList.size() > 0;
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
            Directory_files directory(Directory_files::join(joinBuf, baseDir, nextDir));

            while (directory.more()) {
                if (! directory.is_directory()) {
                    lstring name(directory.name());
                    lstring fullname;
                    directory.fullName(fullname);
                    if (command.validFile(name, fullname)) {
                        outFiles.insert(Directory_files::join(joinBuf, nextDir, name));
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
void DupScan::getDirs(unsigned level, const StringList& baseDirList, const StringSet& nextDirList, StringSet& outDirList) const {
    lstring joinBuf;
    for (const lstring& nextDir : nextDirList) {
        for (const lstring& baseDir : baseDirList) {
            Directory_files directory(Directory_files::join(joinBuf, baseDir, nextDir));
            lstring fullname;

            while (directory.more()) {
                if (directory.is_directory()) {
                    outDirList.insert(Directory_files::join(joinBuf, nextDir, directory.name()));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
void DupScan::compareFiles(unsigned level, const StringList& baseDirList, const StringSet& files) const {
    lstring joinBuf1, joinBuf2;

    for (const lstring& file : files) {
        StringList::const_iterator dirIter = baseDirList.begin();
        size_t fileLen1 = fileLength(Directory_files::join(joinBuf1, *dirIter++, file));
        size_t fileLen2 = 0;
        bool matchingLen = true;

        if (command.verbose)
            cerr << joinBuf1 << " len=" << fileLen1 << std::endl;
 
        while (dirIter != baseDirList.end()) {
            fileLen2 = fileLength(Directory_files::join(joinBuf2, *dirIter++, file));

            if (command.verbose)
                cerr << joinBuf2 << " len=" << fileLen2 << std::endl;

            if (command.justName) {
                if (fileLen1 == fileLen2)
                    showDuplicate(joinBuf1, joinBuf2);
                else if (fileLen1 != -1 && fileLen2 != -1)
                    showDifferent(joinBuf1, joinBuf2);
                else
                    showMissing((fileLen1 != -1), joinBuf1, (fileLen2 != -1), joinBuf2);
            } else {
                if (fileLen1 != fileLen2) {
                    matchingLen = false;  // currently only two items in baseDirList, so no need to exit early
                }
            }
        }

        if (command.justName)
            continue;

        if (matchingLen) {
            dirIter = baseDirList.begin();
            Directory_files::join(joinBuf1, *dirIter++, file);
            HashValue hash1 = XXHash64::compute(joinBuf1);  // hashValue = Md5::compute(joinBuf);

            if (command.verbose)
                cerr << joinBuf1 << " hash=" << hash1 << std::endl;

            while (dirIter != baseDirList.end()) {
                Directory_files::join(joinBuf2, *dirIter++, file);
                HashValue hash2 = XXHash64::compute(joinBuf2); // hashValue = Md5::compute(joinBuf);

                if (command.verbose)
                    cerr << joinBuf2 << " hash=" << hash2 << std::endl;

                if (hash1 == hash2) {
                    showDuplicate(joinBuf1, joinBuf2);
                } else {
                    showDifferent(joinBuf1, joinBuf2);
                }
            }
        } else {
            if (fileLen1 != -1 && fileLen2 != -1)
                showDifferent(joinBuf1, joinBuf2);
            else
                showMissing((fileLen1 != -1), joinBuf1, (fileLen2 != -1), joinBuf2);
        }
    }
}

// ---------------------------------------------------------------------------
void DupScan::showDuplicate(const lstring& filePath1, const lstring& filePath2) const {
    command.sameCnt++;
    if (command.showSame) {
        std::cout << command.preDup;
        if (command.showFiles == Command::Both || command.showFiles == Command::First)
            std::cout << filePath1;
        if (command.showFiles == Command::Both)
            std::cout << command.separator;
        if (command.showFiles == Command::Both || command.showFiles == Command::Second)
            std::cout << filePath2;
        std::cout << command.postDivider;
        
        if (command.hardlink) {
            assert(true);  // TODO - implement hardlnk
            // Directory_files::deleteFile(command.dryrun, filePath2);
            // Directory_files::hardlink(filePath1, filePath2);
        } else if (command.deleteFiles != Command::None) {
            switch (command.deleteFiles) {
            case Command::None:
                break;
            case Command::First:
                Directory_files::deleteFile(command.dryrun, filePath1);
                break;
            case Command::Second:
                Directory_files::deleteFile(command.dryrun, filePath2);
                break;
            case Command::Both:
                Directory_files::deleteFile(command.dryrun, filePath1);
                Directory_files::deleteFile(command.dryrun, filePath2);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
void DupScan::showDifferent(const lstring& filePath1, const lstring& filePath2) const {
    command.diffCnt++;
    if (command.showDiff) {
        std::cout << command.preDiff;
        if (command.showFiles != Command::Second)
            std::cout << filePath1;
        if (command.showFiles != Command::None)
            std::cout << command.separator;
        if (command.showFiles != Command::First)
            std::cout << filePath2;
        std::cout << command.postDivider;
    }
}

// ---------------------------------------------------------------------------
void DupScan::showMissing(bool have1,  const lstring& filePath1, bool have2, const lstring& filePath2) const {
    command.missCnt++;
    if (command.showMiss) {
        std::cout << command.preMissing;
        if (have1 != command.invert)
            std::cout << filePath1;
        else
            std::cout << filePath2;
        std::cout << command.postDivider;
    }
}
