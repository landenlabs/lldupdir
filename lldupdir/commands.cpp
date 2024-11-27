//-------------------------------------------------------------------------------------------------
//
// File: commands.cpp   Author: Dennis Lang  Desc: Process file scan
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// http://landenlabs.com
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

#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <errno.h>
#include <map>
#include <vector>

#include "ll_stdhdr.hpp"
#include "commands.hpp"
#include "directory.hpp"
#include "md5.hpp"
#include "xxhash64.hpp"


const char EXTN_CHAR('.');
const lstring DECRYPT_EXE = "C:\\Program Files\\Axantum\\AxCrypt\\AxCrypt.exe";
const lstring AXX = ".axx";
const lstring EMPTY = "";

// ---------------------------------------------------------------------------
// Forward declaration
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec);
bool deleteFile(const char* path);

// ---------------------------------------------------------------------------
const string Q2 = "\"";
inline const string quote(const string& str) {
    return (str.find(" ") == string::npos) ? str : (Q2 + str + Q2);
}



// ---------------------------------------------------------------------------
class Decrypt {
public:
    const lstring cmd;
    lstring path;
    lstring extn;
    lstring outFile;
    struct stat pathStat;
    struct stat outStat;

    Decrypt(const lstring& CMD) : cmd(CMD) {}

    bool decryptFile(const lstring& fullname, const lstring& outPrefix);
};

// ---------------------------------------------------------------------------
bool Decrypt::decryptFile(const lstring& fullname, const lstring& outPrefix) {

    path = lstring(fullname);
    ReplaceAll(path, AXX, EMPTY);
    size_t pos = path.find_last_of("-");
    if (pos + 3 <= path.length()) {
        path[pos] = '.';
        extn = path + pos;
        int statErr = stat(fullname, &pathStat);

        if (statErr == 0) {
            outFile = outPrefix + extn;
            lstring fullCmd = cmd + quote(outFile) + " " + quote(fullname);
            DWORD exitCode = 0;

            deleteFile(outFile);
            if (RunCommand(fullCmd, &exitCode, 2000)) {
                if (exitCode == 0) {
                    int statErr = stat(outFile, &outStat);
                    return (statErr == 0);
                }
            }
        }
    }
    return false;
}

#if defined(_WIN32) || defined(_WIN64)


// ---------------------------------------------------------------------------
std::string GetErrorMsg(DWORD error) {
    std::string errMsg;
    if (error != 0) {
        LPTSTR pszMessage;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&pszMessage,
            0, NULL);

        errMsg = pszMessage;
        LocalFree(pszMessage);
        int eolPos = (int)errMsg.find_first_of('\r');
        errMsg.resize(eolPos);
        errMsg.append(" ");
    }
    return errMsg;
}

// ---------------------------------------------------------------------------
// Has statics - not thread safe.
const char* RunExtension(std::string& exeName) {
    // Cache results - return previous match
    static const char* s_extn = NULL;
    static std::string s_lastExeName;
    if (s_lastExeName == exeName)
        return s_extn;
    s_lastExeName = exeName;

    /*
    static char ext[_MAX_EXT];
    _splitpath(exeName.c_str(), NULL, NULL, NULL, ext);

    if (ext[0] == '.')
        return ext;
    */

    // Expensive - search PATH for executable.
    char fullPath[MAX_PATH];
    static const char* s_extns[] = { NULL, ".exe", ".com", ".cmd", ".bat", ".ps" };
    for (unsigned idx = 0; idx != ARRAYSIZE(s_extns); idx++) {
        s_extn = s_extns[idx];
        DWORD foundPathLen = SearchPath(NULL, exeName.c_str(), s_extn, ARRAYSIZE(fullPath), fullPath, NULL);
        if (foundPathLen != 0)
            return s_extn;
    }

    return NULL;
}

// ---------------------------------------------------------------------------
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec) {
    std::string tmpCommand(command);
    /*
    const char* pEndExe = strchr(command, ' ');
    if (pEndExe == NULL)
        pEndExe = strchr(command, '\0');
    std::string exeName(command, pEndExe);

    const char* exeExtn = RunExtension(exeName);
    static const char* s_extns[] = { ".cmd", ".bat", ".ps" };
    if (exeExtn != NULL)
    {
        for (unsigned idx = 0; idx != ARRAYSIZE(s_extns); idx++)
        {
            const char* extn = s_extns[idx];
            if (strcmp(exeExtn, extn) == 0)
            {
                // Add .bat or .cmd to executable name.
                tmpCommand = exeName + extn + pEndExe;
                break;
            }
        }
    }
    */

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    // Start the child process.
    if (! CreateProcess(
        NULL,   // No module name (use command line)
        (LPSTR)tmpCommand.c_str(), // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)) {         // Pointer to PROCESS_INFORMATION structure
        DWORD err = GetLastError();
        if (pExitCode)
            *pExitCode = err;
        std::cerr << "Failed " << tmpCommand << " " << GetErrorMsg(err) << std::endl;
        return false;
    }

    // Wait until child process exits.
    DWORD createStatus = WaitForSingleObject(pi.hProcess, waitMsec);

    if (pExitCode) {
        *pExitCode = createStatus;
        if (createStatus == 0 && ! GetExitCodeProcess(pi.hProcess, pExitCode))
            *pExitCode = (DWORD) -1;
    }

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}
#else
// ---------------------------------------------------------------------------
const char* GetErrorMsg(DWORD error) {
    return std::strerror(error);
}

// ---------------------------------------------------------------------------
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec) {
    std::vector<char> buffer(128);
    std::string result;

    auto pipe = popen(command, "r"); // get rid of shared_ptr
    if (! pipe)
        throw std::runtime_error("popen() failed!");

    while (! feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }

    auto rc = pclose(pipe);

    if (pExitCode != NULL) {
        *pExitCode = rc;
    }
    if (rc == EXIT_SUCCESS) { // == 0
    } else if (rc == EXIT_FAILURE) {  // EXIT_FAILURE is not used by all programs, maybe needs some adaptation.
    }
    return true;
}
#endif


// ---------------------------------------------------------------------------
// Extract name part from path.
lstring& getName(lstring& outName, const lstring& inPath) {
    size_t nameStart = inPath.rfind(SLASH_CHAR) + 1;
    if (nameStart == 0)
        outName = inPath;
    else
        outName = inPath.substr(nameStart);
    return outName;
}

// ---------------------------------------------------------------------------
// Extract name part from path.
lstring& removeExtn(lstring& outName, const lstring& inPath) {
    size_t extnPos = inPath.rfind(EXTN_CHAR);
    if (extnPos == std::string::npos)
        outName = inPath;
    else
        outName = inPath.substr(0, extnPos);
    return outName;
}

// ---------------------------------------------------------------------------
// Return true if inName matches pattern in patternList
bool FileMatches(const lstring& inName, const PatternList& patternList, bool emptyResult) {
    if (patternList.empty() || inName.empty())
        return emptyResult;

    for (size_t idx = 0; idx != patternList.size(); idx++)
        if (std::regex_match(inName.begin(), inName.end(), patternList[idx]))
            return true;

    return false;
}

// ---------------------------------------------------------------------------
static size_t fileLength(const lstring& path) {
    struct stat info;
    return (stat(path, &info) == 0) ? info.st_size : -1;
}

// ---------------------------------------------------------------------------
static struct stat  print(const lstring& path, struct stat* pInfo) {
    struct stat info;
    int result = 0;

    if (pInfo == NULL) {
        pInfo = &info;
        result = stat(path, pInfo);
    }

    char timeBuf[128];
    errno_t err = 0;

    if (result == 0) {
        // err = ctime_s(timeBuf, sizeof(timeBuf), &pInfo->st_mtime);
        struct tm TM;
#if defined(_WIN32) || defined(_WIN64)
        err = localtime_s(&TM, &pInfo->st_mtime);
#else
        TM = *localtime(&pInfo->st_mtime);
#endif
        strftime(timeBuf, sizeof(timeBuf), "%a %d-%b-%Y %h:%M %p", &TM);
        if (err) {
            std::cerr << "Invalid file " << path << std::endl;
        } else {
#if defined(_WIN32) || defined(_WIN64)
            bool isSymLink = false;
#else
            bool isSymLink = S_ISLNK(pInfo->st_flags);
#endif
            std::cout << std::setw(8) << pInfo->st_size
                << " " << timeBuf << " "
                << std::setw(10) << pInfo->st_ino
                << (isSymLink ? " S" : " ")
                << "Links " << pInfo->st_nlink
                << " " << path
                << std::endl;
        }
    }
    return *pInfo;
}


// ---------------------------------------------------------------------------
bool deleteFile(const char* path) {

#if defined(_WIN32) || defined(_WIN64)
    SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);
    if (0 == DeleteFile(path)) {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND) {  // 2 = ERROR_FILE_NOT_FOUND
            std::cerr << err << " error trying to delete " << path << std::endl;
            return false;
        }
    }
#else
    unlink(path);
#endif
    return true;
}

// ---------------------------------------------------------------------------
bool Command::validFile(const lstring& name, const lstring& fullname)  {
  bool isValid =
      (!name.empty() && !FileMatches(name, excludeFilePatList, false) &&
       FileMatches(name, includeFilePatList, true) &&
       !FileMatches(fullname, excludePathPatList, false) &&
       FileMatches(fullname, includePathPatList, true));

    if (! isValid)
        skipCnt++;
    return isValid;
}

// ---------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
// Locate pair of files one encrypt with AXX and the native file
//  foo-xls.axx and foo.xls
//  Decrypt the axx and compare to native.
size_t DupDecode::add(const lstring& fullname) {
    size_t fileCount = 0;
    lstring name;
    getName(name, fullname);

    if (validFile(name, fullname)) {
        const lstring DECRYPT_CMD = quote(DECRYPT_EXE) + " -f -c -k " + quote(DECRYPT_KEY) + " -d -n ";
        const lstring OUTFILE = "d:\\out";

        if (name.find(AXX) != lstring::npos) {   //  chry-towncountry-2007-xlsx.axx
            lstring dup1 = fullname;
            lstring dup2 = lstring(fullname);
            ReplaceAll(dup2, AXX, EMPTY);
            size_t pos = dup2.find_last_of("-");
            if (pos + 3 <= dup2.length()) {
                dup2[pos] = '.';
                lstring extn = dup2 + pos;

                struct stat info1;
                struct stat info2;

                int result1 = stat(dup1.c_str(), &info1);
                int result2 = stat(dup2.c_str(), &info2);

                if (result1 == 0 && result2 == 0) {
                    print(dup1, &info1);
                    print(dup2, &info2);

                    lstring outFile = OUTFILE + extn;
                    lstring decrypCmd = DECRYPT_CMD + outFile + " " + quote(dup1);
                    DWORD exitCode = 0;

                    deleteFile(outFile);
                    if (RunCommand(decrypCmd, &exitCode, 2000)) {
                        if (exitCode == 0) {
                            struct stat info3 = print(outFile, NULL);
                            if (info3.st_size == info2.st_size) {
                                lstring compare = "D:\\opt\\bin\\cmp.exe -q ";
                                lstring cmpCmd = compare + quote(dup2) + " " + quote(outFile);
                                if (RunCommand(cmpCmd, &exitCode, 2000)) {
                                    if (exitCode == 0) {
                                        std::cout << "  Identical\n";
                                        std::cout << "lr -f " << quote(dup2) << std::endl;
                                    }
                                } else {
                                    std::cout << "Compare execute failed " << exitCode << std::endl;
                                }
                            }
                        } else {
                            std::cout << decrypCmd << " ExitCode=" << exitCode << std::endl;
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }

        fileCount++;
        if (showFile)
            std::cout << fullname.c_str() << std::endl;

    }

    return fileCount;
}


// ---------------------------------------------------------------------------
bool CompareAxxPair::begin(StringList& fileDirList) {
    // Clean list - remove comma separator
    for (lstring& str : fileDirList) {
        ReplaceAll(str, ",", "");
    }

    if (fileDirList.size() == 2) {
        ref1Path = fileDirList.front();
        ref2Path = fileDirList.back();
        fileDirList.pop_back();
        return true;
    }
    std::cerr << "Must provide two files or two directories to compare" << std::endl;
    return false;
}

// ---------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
// Locate matching pair into directory trees
// Decrypt and compare
//   path1/foo-xls.axx  path2/foo-xls.axx
size_t CompareAxxPair::add(const lstring& fullname) {
    size_t fileCount = 0;
    lstring name;
    getName(name, fullname);

    if (validFile(name, fullname)) {
        const lstring DECRYPT_CMD = quote(DECRYPT_EXE) + " -f -c -k " + quote(DECRYPT_KEY) + " -d -n ";
        const lstring OUTFILE1 = "d:\\out1";
        const lstring OUTFILE2 = "d:\\out2";
        // const lstring COPY_CMD = "cmd /c copy /y ";
        const lstring COPY_CMD = "d:\\opt\\bin\\lc -vfO ";
        Decrypt decrypt1(DECRYPT_CMD);
        Decrypt decrypt2(DECRYPT_CMD);

        if (name.find(AXX) != lstring::npos) {   //  chry-towncountry-2007-xlsx.axx
            if (decrypt1.decryptFile(fullname, OUTFILE1)) {
                lstring file2 = fullname;
                ReplaceAll(file2, ref1Path, ref2Path);
                if (decrypt2.decryptFile(file2, OUTFILE2)) {
                    fileCount++;
                    print(decrypt1.path, &decrypt1.outStat);
                    print(decrypt2.path, &decrypt2.outStat);

                    lstring compare = "D:\\opt\\bin\\cmp.exe -q ";
                    lstring cmpCmd = compare + quote(decrypt1.outFile) + " " + quote(decrypt2.outFile);
                    DWORD exitCode = 0;
                    if (RunCommand(cmpCmd, &exitCode, 2000)) {
                        if (exitCode == 0) {
                            std::cout << "  Identical\n";
                            if (true) {
                                // std::cout << "lr -f " << quote(decrypt2.path) << std::endl;
                                lstring copyCmd = COPY_CMD + quote(fullname) + " " + quote(file2);
                                std::cout << copyCmd << std::endl;
                                if (RunCommand(copyCmd, &exitCode, 2000)) {
                                } else {
                                    std::cerr << "Copy execute failed " << exitCode << std::endl;
                                }
                            }
                        }
                    } else {
                        if (exitCode <= 10)
                            std::cout << "Compare execute failed " << exitCode << std::endl;
                        else
                            std::cout << "Compare execute failed " << exitCode
                                << " " << GetErrorMsg(exitCode) << std::endl;

                    }
                }
            }
        }

        if (showFile)
            std::cout << fullname.c_str() << std::endl;
    }

    return fileCount;
}



// map<std::string name, vector<int> path>

map<std::string, IntList> fileList;
std::vector<std::string> pathList;
std::string lastPath;
unsigned lastPathIdx = 0;

// ---------------------------------------------------------------------------
bool DupFiles::begin(StringList& fileDirList) {
    fileList.clear();
    pathList.clear();
    lastPathIdx = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
// Locate duplcate files.
size_t DupFiles::add(const lstring& fullname) {
    size_t fileCount = 0;
    lstring name;
    getName(name, fullname);

    if (validFile(name, fullname)) {
        std::string path = fullname.substr(0, fullname.length() - name.length());
        if (lastPath != path) {
            if (lastPath.rfind(path, 0) == 0) {
                for (lastPathIdx--; lastPathIdx < pathList.size(); lastPathIdx--) {
                    lastPath = pathList[lastPathIdx];
                    if (lastPath == path) {
                        break;
                    }
                }
            } else {
                lastPathIdx = -1;
            }

            if (lastPathIdx >= pathList.size() ) {
                lastPathIdx = (unsigned)pathList.size();
                lastPath = path;
                pathList.push_back(lastPath);
            }

            string testPath = pathList[lastPathIdx] + name;
            if (testPath != fullname) {
                assert(false);
            }
        }
        fileList[name].push_back(lastPathIdx);
        fileCount = 1;
    }

    return fileCount;
}

class PathParts {
public:
    unsigned pathIdx;
    const string& name;
    PathParts(unsigned _pathIdx, const string& _name) :
        pathIdx(_pathIdx), name(_name) {}
};

void DupFiles::printPaths(const IntList& pathListIdx, const std::string& name) {
    for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
        lstring fullPath = pathList[pathListIdx[plIdx]] + name;
        if (verbose) {
            print(fullPath, NULL);
        } else {
            if (plIdx != 0) std::cout << separator;
            std::cout << fullPath;
        }
    }
}

// ---------------------------------------------------------------------------
bool DupFiles::end() {
    // typedef lstring HashValue;       // md5
    typedef uint64_t HashValue;         // xxHash64

    if (justName && ignoreExtn) {
        lstring noExtn;
        std::map<lstring, std::vector<const string* >> noExtnList;       // TODO - make string& not string
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const string& fullname = it->first;
            getName(noExtn, fullname);
            removeExtn(noExtn, noExtn);
            noExtnList[noExtn].push_back(&fullname);
        }

        for (auto it = noExtnList.cbegin(); it != noExtnList.cend(); it++) {
            unsigned outCnt = 0;
            if (it->second.size() > 1) {

                for (auto itNames = it->second.cbegin(); itNames != it->second.cend(); itNames++) {
                    const IntList& pathListIdx = fileList[*(*itNames)];
                    if (outCnt == 0) std::cout << preDivider;
                    if (outCnt++ != 0) std::cout << separator;
                    printPaths(pathListIdx, *(*itNames));
                }
            }
            if (outCnt != 0) std::cout << postDivider;
        }

    } else if (justName) {
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            if (it->second.size() > 1) {
                std::cout << preDivider;
                printPaths(pathListIdx, it->first);
                std::cout << postDivider;
            }
        }
    } else if (sameName)  {
        std::map<HashValue, unsigned> hashDups;
        std::map<lstring, HashValue> fileHash;
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            if (it->second.size() > 1) {
                hashDups.clear();
                fileHash.clear();

                for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                    // std::cout << pathList[pathListIdx[plIdx]] << it->first << std::endl;
                    lstring fullPath = pathList[pathListIdx[plIdx]] + it->first;
                    // HashValue hashValue = Md5::compute(fullPath);
                    HashValue hashValue = XXHash64::compute(fullPath);
                    hashDups[hashValue] = hashDups[hashValue] + 1;
                    fileHash[fullPath] = hashValue;
                }

                std::map<HashValue, std::vector<unsigned >> hashFileList;
                for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                    // std::cout << pathList[pathListIdx[plIdx]] << it->first << std::endl;
                    unsigned plPos = pathListIdx[plIdx];
                    lstring fullPath = pathList[plPos] + it->first;
                    HashValue hashValue = fileHash[fullPath];
                    bool isDup = (hashDups[hashValue] != 1);
                    if (verbose) {
                        std::cout << (isDup ? "dup " : "    ") << fileHash[fullPath] << " ";
                        print(fullPath, NULL);
                        // std::cout << endl;
                    } else if (isDup != invert) {
                        hashFileList[hashValue].push_back(plPos);
                    }
                }

                if (! verbose) {
                    for (auto hashFileListIter = hashFileList.cbegin(); hashFileListIter != hashFileList.cend(); hashFileListIter++) {
                        if (hashFileListIter->second.size() > 1) {
                            std::cout << preDivider;
                            const auto& matchList = hashFileListIter->second;
                            for (unsigned mIdx = 0; mIdx < matchList.size(); mIdx++) {
                                string fullPath = pathList[matchList[mIdx]] + it->first;
                                if (mIdx != 0) std::cout << separator;
                                std::cout << fullPath;
                            }
                            std::cout << postDivider;
                        }
                    }
                }
            } else if (invert) {
                std::cout << preDivider;
                lstring fullPath = pathList[pathListIdx[0]] + it->first;
                std::cout << fullPath << postDivider;
            }
        }
    } else {
        // Compare all files by size and hash
        //  1. Create map of file length and name
        //  2. For duplicate file length - compute hash
        //  3. For duplicate hash print

        // 1. Create map of file length and name
        std::map<size_t, std::vector<PathParts >> sizeFileList;
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                unsigned plPos = pathListIdx[plIdx];
                lstring fullPath = pathList[plPos] + it->first;
                size_t fileLen = fileLength(fullPath);
                fileLen = (fileLen != 0) ? fileLen : std::hash<std::string> {}(fullPath);
                const string& name = it->first;
                PathParts pathParts(plPos, name);
                sizeFileList[fileLen].push_back(pathParts);
            }
        }

        // 2. Compute hash on duplicate length files.
        std::map<HashValue, std::vector<const PathParts* >> hashFileList;
        for (auto sizeFileListIter = sizeFileList.cbegin(); sizeFileListIter != sizeFileList.cend(); sizeFileListIter++) {
            if ((sizeFileListIter->second.size() > 1) != invert) {
                const auto& sizeList = sizeFileListIter->second;
                for (unsigned sIdx = 0; sIdx < sizeList.size(); sIdx++) {
                    const PathParts& pathParts = sizeList[sIdx];
                    lstring fullPath = pathList[pathParts.pathIdx];
                    fullPath += pathParts.name;
                    // HashValue hashValue = Md5::compute(fullPath);
                    HashValue hashValue = XXHash64::compute(fullPath);
                    hashFileList[hashValue].push_back(&pathParts);
                }
            }
        }

        // 3. Find duplicate hash
        for (auto hashFileListIter = hashFileList.cbegin(); hashFileListIter != hashFileList.cend(); hashFileListIter++) {
            if ((hashFileListIter->second.size() > 1) != invert) {
                std::cout << preDivider;
                const auto& matchList = hashFileListIter->second;
                for (unsigned mIdx = 0; mIdx < matchList.size(); mIdx++) {
                    const PathParts& pathParts = *matchList[mIdx];
                    lstring fullPath = pathList[pathParts.pathIdx];
                    fullPath += pathParts.name;
                    if (verbose) {
                        std::cout << matchList.size() << " Hash " << hashFileListIter->first << " ";
                        print(fullPath, NULL);
                    } else {
                        if (mIdx != 0) std::cout << separator;
                        std::cout << fullPath;
                    }
                }
                std::cout << postDivider;
            }
        }
    }
    return true;
}
