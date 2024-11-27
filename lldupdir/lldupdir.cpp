//-------------------------------------------------------------------------------------------------
//
//  lldupdir      Feb-2024       Dennis Lang
//
//  Find duplicate files - specialized using AxCrypt
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// http://landenlabs.com/
//
// This file is part of lldupdir project.
//
// ----- License ----
//
// Copyright (c) 2024 Dennis Lang
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

// 4291 - No matching operator delete found
#pragma warning( disable : 4291 )
#define _CRT_SECURE_NO_WARNINGS

// Project files
#include "ll_stdhdr.hpp"
#include "directory.hpp"
#include "split.hpp"
#include "commands.hpp"
#include "dupscan.hpp"
#include "colors.hpp"

#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <regex>
#include <exception>

using namespace std;

// Helper types
typedef std::vector<std::regex> PatternList;
typedef unsigned int uint;

uint optionErrCnt = 0;
uint patternErrCnt = 0;

#if defined(_WIN32) || defined(_WIN64)
    // const char SLASH_CHAR('\\');
    #include <assert.h>
    #define strncasecmp _strnicmp
    #if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
        #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #endif
#else
    // const char SLASH_CHAR('/');
#endif


// ---------------------------------------------------------------------------
// Recurse over directories, locate files.
static size_t InspectFiles(Command& command, const lstring& dirname) {
    Directory_files directory(dirname);
    lstring fullname;

    size_t fileCount = 0;

    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode)) {
            fileCount += command.add(dirname);
            return fileCount;
        }
    } catch (exception ex) {
        // Probably a pattern, let directory scan do its magic.
    }

    while (directory.more()) {
        directory.fullName(fullname);
        if (directory.is_directory()) {
            fileCount += InspectFiles(command, fullname);
        } else if (fullname.length() > 0) {
            fileCount += command.add(fullname);
        }
    }

    return fileCount;
}

// ---------------------------------------------------------------------------
// Return compiled regular expression from text.
std::regex getRegEx(const char* value) {
    try {
        std::string valueStr(value);
        return std::regex(valueStr);
        // return std::regex(valueStr, regex_constants::icase);
    } catch (const std::regex_error& regEx) {
        std::cerr << regEx.what() << ", Pattern=" << value << std::endl;
    }

    patternErrCnt++;
    return std::regex("");
}

// ---------------------------------------------------------------------------
// Validate option matchs and optionally report problem to user.
bool ValidOption(const char* validCmd, const char* possibleCmd, bool reportErr = true) {
    // Starts with validCmd else mark error
    size_t validLen = strlen(validCmd);
    size_t possibleLen = strlen(possibleCmd);

    if ( strncasecmp(validCmd, possibleCmd, std::min(validLen, possibleLen)) == 0)
        return true;

    if (reportErr) {
        std::cerr << "Unknown option:'" << possibleCmd << "', expect:'" << validCmd << "'\n";
        optionErrCnt++;
    }
    return false;
}
// ---------------------------------------------------------------------------
// Convert special characters from text to binary.
static std::string& ConvertSpecialChar(std::string& inOut) {
    uint len = 0;
    int x, n, scnt;
    const char* inPtr = inOut.c_str();
    char* outPtr = (char*)inPtr;
    while (*inPtr) {
        if (*inPtr == '\\') {
            inPtr++;
            switch (*inPtr) {
            case 'n': *outPtr++ = '\n'; break;
            case 't': *outPtr++ = '\t'; break;
            case 'v': *outPtr++ = '\v'; break;
            case 'b': *outPtr++ = '\b'; break;
            case 'r': *outPtr++ = '\r'; break;
            case 'f': *outPtr++ = '\f'; break;
            case 'a': *outPtr++ = '\a'; break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                scnt = sscanf(inPtr, "%3o%n", &x, &n);  // "\010" octal sets x=8 and n=3 (characters)
                assert(scnt == 1);
                inPtr += n - 1;
                *outPtr++ = (char)x;
                break;
            case 'x':                                // hexadecimal
                scnt = sscanf(inPtr + 1, "%2x%n", &x, &n);  // "\1c" hex sets x=28 and n=2 (characters)
                assert(scnt == 1);
                if (n > 0) {
                    inPtr += n;
                    *outPtr++ = (char)x;
                    break;
                }
            // seep through
            default:
                throw( "Warning: unrecognized escape sequence" );
            case '\\':
            case '\?':
            case '\'':
            case '\"':
                *outPtr++ = *inPtr;
                break;
            }
            inPtr++;
        } else
            *outPtr++ = *inPtr++;
        len++;
    }

    inOut.resize(len);
    return inOut;;
}

// ---------------------------------------------------------------------------
void showHelp(const char* arg0) {
    const char* helpMsg = "  Dennis Lang v2.2 (landenlabs.com) " __DATE__ "\n\n"
        "_p_Des: 'Find duplicate files\n"
        "_p_Use: lldupdir [options] directories...   or  files\n"
        "\n"
        "_p_Options (only first unique characters required, options can be repeated): \n"
        "\n"
        // "   -compareAxx       ; Find pair of encrypted path1/foo-xls.axx and path2/foo-xls.axx \n"
        // "   -duplicateAxx     ; Find pair of encrypted in raw foo-xls.axx and foo.xls \n"
        // "   -file             ; Find duplicate files by name \n"
        "\n"
        //    "   -invert           ; Invert test output "
        "   -_y_includeFile=<filePattern>   ; -inc=*.java \n"
        "   -_y_excludeFile=<filePattern>   ; -exc=*.bat -exe=*.exe \n"
        "   _p_Note: Capitalized _y_I_x_nclude/_y_E_x_xclude for full path pattern \n"
#ifdef HAVE_WIN
        "   _p_Note: Escape directory slash on windows \n"
        "   -_y_IncludePath=<pathPattern>   ; -Inc=*\\\\code\\\\*.java \n"
        "   -_y_ExcludePath=<pathPattern>   ; -Exc=*\\\\x64\\\\* -Exe=*\\\\build\\\\* \n"
#else
        "   -_y_IncludePath=<pathPattern>   ; -Inc=*/code/*.java \n"
        "   -_y_ExcludePath=<pathPattern>   ; -Exc=*/bin/* -Exe=*/build/* \n"
#endif
        "   -verbose \n"
        
        "\n"
        "_p_Options:\n"
        "   -_y_showDiff           ; Show files that differ\n"
        "   -_y_showMiss           ; Show missing files \n"
        "   -_y_sameAll            ; Compare all files for matching hash \n"
        "   -_y_hideDup            ; Don't show duplicate files \n"
        "   -_y_justName           ; Match duplicate name only, not contents \n"
        //      "   -ignoreExtn         ; With -justName, also ignore extension \n"
        "   -_y_preDup=<text>      ; Prefix before duplicates, default nothing  \n"
        "   -_y_preDiff=<text>     ; Prefix before diffences, default: \"!= \"  \n"
        "   -_y_preMiss=<text>     ; Prefix before missing, default: \"--  \" \n"
        // "   -_y_preDivider=<text>  ; Pre group divider output before groups  \n"
        "   -_y_postDivider=<text> ; Divider for dup and diff, def: \"__\\n\"  \n"
        "   -_y_separator=<text>   ; Separator  \n"
        //        "   -ignoreHardlink   ; \n"
        //        "   -ignoreSymlink    ; \n"
        "\n"
        "_p_Options (only when scanning two directories) :\n"
        "   -_y_simple             ; Only files no pre or separators \n"
        "   -_y_log=[1|2]          ; Only show 1st or 2nd file for Dup or Diff \n"
        "   -_y_delete=[1|2]       ; If dup, delete file from directory 1 or 2 \n"
        "\n"
        "_p_Examples: \n"
        "  Find file matches by name and hash value (fastest with only 2 dirs) \n"
        "   lldupdir  dir1 dir2/subdir  \n"
        "   lldupdir  -_y_showMiss -_y_showDiff dir1 dir2/subdir  \n"
        "   lldupdir  -_y_hideDup -_y_showMiss -_y_showDiff dir1 dir2/subdir  \n"
        "   lldupdir -_y_Exc=*bin* -_y_exc=*.exe -_y_hideDup -_y_showMiss   dir1 dir2/subdir  \n"
        "\n"
        "  Find file matches by matching hash value, slower than above, 1 or three or more dirs \n"
        "   lldupdir  -_y_showAll  dir1 \n"
        "   lldupdir  -_y_showAll  dir1   dir2/subdir   dir3 \n"
        "\n"
        "  Change how output appears \n"
        "   lldupdir  -_y_sep=\" /  \"  dir1 dir2/subdir dir3\n"
        //       "   lldupdir   -just -ignore . | grep png | le -I=-  '-c=lr ' \n"
        "\n"
        "\n";

    std::cerr << Colors::colorize("\n_W_") << arg0 << Colors::colorize(helpMsg);
}

//-------------------------------------------------------------------------------------------------
// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime(time_t& now) {
    now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}

// ---------------------------------------------------------------------------
void showUnknown(const char* argStr) {
    std::cerr << Colors::colorize("Use -h for help.\n_Y_Unknown option _R_") << argStr << Colors::colorize("_X_\n");
    optionErrCnt++;
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    DupFiles dupFiles;
    DupDecode  dupDecode;
    CompareAxxPair compareAxxPair;

    Command* commandPtr = &dupFiles;
    StringList fileDirList;

    if (argc == 1) {
        showHelp(argv[0]);
    } else {
        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++) {
            if (*argv[argn] == '-' && doParseCmds) {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (cmdValue.size() == 2) {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];
                    
                    if (cmd.length() > 1 && cmd[0] == '-')
                        cmd.erase(0);   // allow -- prefix on commands
                    
                    const char* cmdName = cmd + 1;
                    switch (cmd[1]) {
                    case 'd':   // delete=1|2
                        if (ValidOption("deleteFile", cmdName)) {
                            char* endPtr;
                            commandPtr->deletefile = (unsigned)strtoul(value, &endPtr, 10);
                            if (*endPtr == '\0' && commandPtr->deletefile < 3)
                                continue;
                        }
                        break;
                    case 'e':   // excludeFile=<pat>
                        if (ValidOption("excludeFile", cmdName)) {
                            ReplaceAll(value, "*", ".*");
                            ReplaceAll(value, "?", ".");
                            commandPtr->excludeFilePatList.push_back(getRegEx(value));
                            continue;
                        }
                        break;
                    case 'E': // ExcludePath=<pat>
                      if (ValidOption("ExcludePath", cmdName)) {
                        ReplaceAll(value, "*", ".*");
                        ReplaceAll(value, "?", ".");
                        commandPtr->excludePathPatList.push_back(
                            getRegEx(value));
                        continue;
                      }
                      break;
                    case 'i':   // includeFile=<pat>
                        if (ValidOption("includeFile", cmdName)) {
                            ReplaceAll(value, "*", ".*");
                            ReplaceAll(value, "?", ".");
                            commandPtr->includeFilePatList.push_back(getRegEx(value));
                            continue;
                        }
                        break;
                    case 'I': // IncludePath=<pat>
                      if (ValidOption("IncludePath", cmdName)) {
                        ReplaceAll(value, "*", ".*");
                        ReplaceAll(value, "?", ".");
                        commandPtr->includePathPatList.push_back(
                            getRegEx(value));
                        continue;
                      }
                      break;
                    case 'l':   // log=[1|2]
                        if (ValidOption("log", cmdName)) {
                            char* endPtr;
                            commandPtr->logfile = (unsigned)strtoul(value, &endPtr, 10);
                            if (*endPtr == '\0' && commandPtr->logfile < 3)
                                continue;
                        }
                        break;
                    case 'p':
                        if (ValidOption("postDivider", cmdName, false)) {
                            commandPtr->postDivider = ConvertSpecialChar(value);
                            continue;
                        } else if (ValidOption("preDivider", cmdName, false)) {
                            commandPtr->preDivider = ConvertSpecialChar(value);
                            continue;
                        } else if (ValidOption("preDuplicate", cmdName, false)) {
                            commandPtr->preDup = ConvertSpecialChar(value);
                            continue;
                        } else if (ValidOption("preDiffer", cmdName, false)) {
                            commandPtr->preDiff = ConvertSpecialChar(value);
                            continue;
                        } else if (ValidOption("preMissing", cmdName, false)) {
                            commandPtr->preMissing = ConvertSpecialChar(value);
                            continue;
                        }
                        break;
                    case 's':
                        if (ValidOption("separator", cmdName, false)) {
                            commandPtr->separator = ConvertSpecialChar(value);
                            continue;
                        }
                        break;

                    default:
                        // show error below,
                        break;
                    }

                    showUnknown(argStr);
                } else {
                    const char* cmdName = argStr + 1;
                    switch (argStr[1]) {
                    case 'a':
                        if (ValidOption("all", cmdName)) {
                            commandPtr->sameName = false;
                            continue;
                        }
                        break;
                    case 'f': // duplicated files
                        if (ValidOption("files", cmdName)) {
                            commandPtr = &dupFiles.share(*commandPtr);
                            continue;
                        }
                        break;
                    case '?':
                        showHelp(argv[0]);
                        return 0;
                    case 'h':
                        if (ValidOption("help", cmdName, false)) {
                            showHelp(argv[0]);
                            return 0;
                        } else if (ValidOption("hideDup", cmdName, false) || ValidOption("hideSame", cmdName, false)) {
                            commandPtr->showSame = false;
                            continue;
                        }
                        break;
                    case 'i':
                        if (ValidOption("invert", cmdName, false)) {
                            commandPtr->invert = true;
                            continue;
                        } else if (ValidOption("ignoreExtn", cmdName)) {
                            commandPtr->ignoreExtn = true;
                            continue;
                        }
                        break;
                    case 'j':
                        if (ValidOption("justName", cmdName)) {
                            commandPtr->justName = true;
                            continue;
                        }
                        break;
                    case 'n':   // no action, dry-run
                        std::cerr << "DryRun enabled\n";
                        commandPtr->dryrun = true;
                        continue;

                    case 's':
                        if (ValidOption("sameAll", cmdName, false)) {
                            commandPtr->sameName = false;
                        } else if (ValidOption("showAll", cmdName, false)) {
                            commandPtr->showSame = commandPtr->showDiff = commandPtr->showMiss = true;
                            continue;
                        } else if (ValidOption("showDiff", cmdName, false)) {
                            commandPtr->showDiff = true;
                            continue;
                        } else if (ValidOption("showMiss", cmdName, false)) {
                            commandPtr->showMiss = true;
                            continue;
                        } else if (ValidOption("showSame", cmdName, false)) {
                            commandPtr->showSame = true;
                            continue;
                        } else if (ValidOption("simple", cmdName, false)) {
                            commandPtr->preDup = commandPtr->preDiff = "";
                            commandPtr->separator = " ";
                            commandPtr->postDivider = "\n";
                            continue;
                        }
                        break;
                    case 'q':
                        if (ValidOption("quiet", cmdName)) {
                            commandPtr->quiet++;
                            commandPtr->showFile = commandPtr->showDiff = commandPtr->showMiss = false;
                            continue;
                        }
                        break;
                    case 'v':
                        if (ValidOption("verbose", cmdName)) {
                            commandPtr->verbose = true;
                            continue;
                        }
                        break;
                    }

                    if (endCmds == argv[argn]) {
                        doParseCmds = false;
                    } else {
                        showUnknown(argStr);
                    }
                }
            }  else {
             // Store file directories
                fileDirList.push_back(argv[argn]);
            }
        }

        time_t startT;

        if (commandPtr->quiet < 2)
            std::cerr << Colors::colorize("\n_G_ +Start ") << currentDateTime(startT) << Colors::colorize("_X_\n");

        if (commandPtr->begin(fileDirList)) {

            if (patternErrCnt == 0 && optionErrCnt == 0 && fileDirList.size() != 0) {
                if (fileDirList.size() == 1 && fileDirList[0] == "-") {
                    string filePath;
                    while (std::getline(std::cin, filePath)) {
                        if (commandPtr->quiet < 1)
                            std::cerr << "  Files Checked=" << InspectFiles(*commandPtr, filePath) << std::endl;
                    }
                } else if (commandPtr->ignoreExtn || !commandPtr->sameName || fileDirList.size() != 2) {
                    for (auto const& filePath : fileDirList) {
                        if (commandPtr->quiet < 1)
                            std::cerr << "  Files Checked=" << InspectFiles(*commandPtr, filePath) << std::endl;
                    }
                } else if (fileDirList.size() == 2) {
                    DupScan dupScan(*commandPtr);
                    StringSet nextDirList;
                    nextDirList.insert("");
                    unsigned level = 0;
                    while (dupScan.findDuplicates(level, fileDirList, nextDirList)) {
                        level++;
                    }

                    if (commandPtr->quiet < 2)
                        std::cerr << Colors::colorize("_G_ +Levels=") << level
                        << " Dup=" << commandPtr->sameCnt
                        << " Diff=" << commandPtr->diffCnt
                        << " Miss=" << commandPtr->missCnt
                        << " Skip=" << commandPtr->skipCnt
                        << " Files=" << commandPtr->sameCnt + commandPtr->diffCnt + commandPtr->missCnt + commandPtr->skipCnt
                        << Colors::colorize("_X_\n");
                }
            }

            commandPtr->end();
        }

        time_t endT;
        if (commandPtr->quiet < 2) {
            currentDateTime(endT);
            std::cerr << Colors::colorize("_G_ +End ")
                << currentDateTime(endT)
                << ", Elapsed "
                << std::difftime(endT, startT)
                << Colors::colorize(" (sec)_X_\n");
        }
    }

    return 0;
}
