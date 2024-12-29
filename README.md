lldupdir 
### Find duplicate files and optionally delete them.

### Builds
* OSX(M3)      Provided Xcode project
* Windows/DOS  Provided Visual Studio solution
 
### Visit home website
[https://landenlabs.com](https://landenlabs.com)

### Description

C++ v17 code to build either a windows/dos or Mac/Linux command line tool 
that can scan directories for duplicate files using a hash code comparison. 


**LLDupDir** has to modes to locate duplicates.
* Search two parallel directory tree structures for files with the same name, length and file contents hash value.
* Search one or many files and directories and compare hashed file content value, 
optionally matching file name with or without extension. 

The fastest search is when comparing two paralley directory tress.

<pre>
lldupdir -showAll  dir1 dir2
</pre>

The slower is to compute the file content hash value for all files
and report which are identical. 
<pre>
lldupdir -all -showAll dir1
or
lldupdir -all -showAll dir1 dir2 dir3 
</pre>

### Dependencies and acknowledgements 

The code contains two file hashing algorithms. 
Default is provided by 
<pre>
// xxhash64.h
// Copyright (c) 2016 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
</pre>


### Help Banner:
<pre>

lldupdir  Dennis Lang v2.4 (landenlabs.com) Dec 22 2024

Des: 'Find duplicate files by comparing length, hash value and optional name.
Use: lldupdir [options] directories...   or  files

Options (only first unique characters required, options can be repeated):


   -includeFile=&lt;filePattern>   ; -inc=*.java
   -excludeFile=&lt;filePattern>   ; -exc=*.bat -exe=*.exe
   Note: Capitalized I_x_nclude/E_x_xclude for full path pattern
   Note: Escape directory slash on windows
   -IncludePath=&lt;pathPattern>   ; -Inc=*\\code\\*.java
   -ExcludePath=&lt;pathPattern>   ; -Exc=*\\x64\\* -Exe=*\\build\\*
   -verbose
   -quiet

Options (default show only duplicates):
   -showAll            ; Compare all files for matching hash
   -showDiff           ; Show files that differ
   -showMiss           ; Show missing files
   -hideDup            ; Don't show duplicate files
   -showAbs            ; Show absolute file paths
   -preDup=&lt;text>      ; Prefix before duplicates, default nothing
   -preDiff=&lt;text>     ; Prefix before differences, default: "!= "
   -preMiss=&lt;text>     ; Prefix before missing, default: "--  "
   -postDivider=&lt;text> ; Divider for dup and diff, def: "__\n"
   -separator=&lt;text>   ; Separator, def: ", "

Options (when scanning two directories, dup if names and hash match) :
   -simple                      ; Show files no prefix or separators
   -log=[first|second]          ; Only show 1st or 2nd file for Dup or Diff
   -no                          ; DryRun, show delete but don't do delete
   -delete=[first|second|both]  ; If dup or diff, delete 1st, 2nd or both files
   -threads                     ; Compute file hashes in threads

Options (when comparing one dir or 3 or more directories)
        Default compares all files for matching length and hash value
   -justName                    ; Match duplicate name only, not contents
   -ignoreExtn                  ; With -justName, also ignore extension
   -all                         ; Find all matches, ignore name
   -delDupPat=pathPat           ; If dup   delete if pattern match

Examples:
  Find file matches by name and hash value (fastest with only 2 dirs)
   lldupdir  dir1 dir2/subdir
   lldupdir  -showMiss -showDiff dir1 dir2/subdir
   lldupdir  -hideDup -showMiss -showDiff dir1 dir2/subdir
   lldupdir -Exc=*\\.git\\* -exc=*.exe -exc=*.zip -hideDup -showMiss   dir1 dir2/subdir

  Find file matches by matching hash value, slower than above, 1 or three or more dirs
   lldupdir  -showAll  dir1
   lldupdir  -showAll  dir1   dir2/subdir   dir3

  Change how output appears
   lldupdir  -sep=" /  "  dir1 dir2/subdir dir3
</pre>
[Top](#top)