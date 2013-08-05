cachemaster
===========

##Introduction
Cachemaster is similar to VMTOUCH, but with more functions. Such as kick page cache, warmup/readahead data, lock data in mem, stat page cache, stat page cache in realtime mode, all by file or directory! 

##Contributors
henshao@taobao.com,tiechou@taobao.com

##Examples
###stat page cache of file
>-bash-3.2$ ./cachemaster -s -f data  
>Stat:data size:488M cached:488M  
###stat page cache of directory
>-bash-3.2$ ./cachemaster -s -d mydir/  
>Stat:mydir//file1 size:488M cached:0Bytes  
>Stat:mydir//file2 size:488M cached:488M  
>Stat:mydir//child/file3 size:488M cached:488M  
>Total Cache of Directory:mydir/ size:1.4G cached:976M  

##Help
*   Usage:./cachemaster [Option] [File] ...
*   -c Clear Page Cache.
*   -l Lock file into memory.
*   -r Real Time Stat of Page Cache.
*   -s Stat of Page Cache.
*   -w Read ahead to page cache, for warmup.
*   -f Operation on a file.
*   -d Operation on a directory.
*   -m Lock in daemon, only used with -l.
*   -i Interval when in real time stat.
*   -c -s -r -l -w is mutual with each other; -f is mutual with -d
*   -S Suppress the unchanged line, only used with -r.
*   Example:
*   ./cachemaster -c -f file_name | Clear cache of a file
*   ./cachemaster -c -d dir_name | Clear cache of a direcroty
*   ./cachemaster -s -f file_name | Stat cache of a file
*   ./cachemaster -s -d dir_name | Stat cache of a direcroty
*   ./cachemaster -r -f file_name | Stat real time cache of a file
*   ./cachemaster -r -d dir_name | Stat real time cache of a direcroty
*   ./cachemaster -l -f file_name | Lock file
*   ./cachemaster -l -d dir_name | Lock  direcroty
*   ./cachemaster -w -f file_name | Warmup file
*   ./cachemaster -w -d dir_name | Warmup  direcroty
*   ./cachemaster -l -f -m file_name | Lock file in daemon
*   ./cachemaster -l -d -m dir_name | Lock direcroty in daemon
