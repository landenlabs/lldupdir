lldupdir
### OSX / Linux / DOS  Find duplicate files and optionally delete or hardlink them.

Nov-2024


### TODO - add notes and examples here

### NOTE - hardlink feature not yet implemented  (Nov-2024)

<hr>
Visit home website

[http://landenlabs.com](http://landenlabs.com)


<hr>
Help Banner:
<pre>
lldupdir  Dennis Lang v2.1 (landenlabs.com) Nov 25 2024

Des: 'Find duplicate files
Use: lldupdir [options] directories...   or  files

Options (only first unique characters required, options can be repeated):


   -includeFile=<filePattern>   ; -inc=*.java
   -excludeFile=<filePattern>   ; -exc=*.bat -exe=*.exe
   Note: Capitalized Include/Exclude for full path pattern
   -IncludePath=<pathPattern>   ; -Inc=*/code/*.java
   -ExcludePath=<pathPattern>   ; -Exc=*/bin/* -Exe=*/build/*
   -verbose

Options:
   -showDiff           ; Show files that differ
   -showMiss           ; Show missing files
   -sameAll            ; Compare all files for matching hash
   -hideDup            ; Don't show duplicate files
   -justName           ; Match duplicate name only, not contents
   -preDup=<text>      ; Prefix before duplicates, default nothing
   -preDiff=<text>     ; Prefix before diffences, default: "!= "
   -preMiss=<text>     ; Prefix before missing, default: "--  "
   -postDivider=<text> ; Divider for dup and diff, def: "__\n"
   -separator=<text>   ; Separator

Options (only when scanning two directories) :
   -simple             ; Only files no pre or separators
   -log=[1|2]          ; Only show 1st or 2nd file for Dup or Diff
   -delete=[1|2]       ; If dup, delete file from directory 1 or 2

Examples:
  Find file matches by name and hash value (fastest with only 2 dirs)
   lldupdir  dir1 dir2/subdir
   lldupdir  -showMiss -showDiff dir1 dir2/subdir
   lldupdir  -hideDup -showMiss -showDiff dir1 dir2/subdir
   lldupdir -Exc=*bin* -exc=*.exe -hideDup -showMiss   dir1 dir2/subdir

  Find file matches by matching hash value, slower than above, 1 or three or more dirs
   lldupdir  -showAll  dir1
   lldupdir  -showAll  dir1   dir2/subdir   dir3

  Change how output appears
   lldupdir  -sep=" /  "  dir1 dir2/subdir dir3
</pre>
