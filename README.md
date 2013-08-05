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

###kick page cache of file
>-bash-3.2$ ./cachemaster -s -f data  
>Stat:data size:488M cached:488M  
>-bash-3.2$ ./cachemaster -c -f data    
>Release:data  
>-bash-3.2$ ./cachemaster -s -f data   
>Stat:data size:488M cached:0Bytes  

###kick page cache of directory
>-bash-3.2$ ./cachemaster -s -d mydir/  
>Stat:mydir//file1 size:488M cached:0Bytes  
>Stat:mydir//file2 size:488M cached:488M  
>Stat:mydir//child/file3 size:488M cached:488M  
>Total Cache of Directory:mydir/ size:1.4G cached:976M  
>-bash-3.2$ ./cachemaster -c -d mydir/   
>Release:mydir//file1  
>Release:mydir//file2  
>Release:mydir//child/file3  
>-bash-3.2$ ./cachemaster -s -d mydir/  
>Stat:mydir//file1 size:488M cached:0Bytes  
>Stat:mydir//file2 size:488M cached:0Bytes  
>Stat:mydir//child/file3 size:488M cached:0Bytes  
>Total Cache of Directory:mydir/ size:1.4G cached:0Bytes  

###lock in mem of file
>-bash-3.2$ ./cachemaster -l -f data   
>Lock data succeed, size:488M  

###lock in mem of file in daemon
>-bash-3.2$ ./cachemaster -l -f data  -m  
>-bash-3.2$ ps xf | grep cachemaster | grep -v grep  
>28279 ?        SLs    0:00 ./cachemaster -l -f data -m  

###lock in mem of directory
>-bash-3.2$ ./cachemaster -l -d mydir/  
>Lock mydir//file1 succeed, size:488M  
>Lock mydir//file2 succeed, size:488M  
>Lock mydir//child/file3 succeed, size:488M  

###lock in mem of directory in daemon
>-bash-3.2$ ./cachemaster -l -d mydir/ -m  
>-bash-3.2$ ps xf | grep cachemaster | grep -v grep  
>28694 ?        SLs    0:00 ./cachemaster -l -d mydir/ -m  

###Real time stat of file(-i is interval, default 1s)
>-bash-3.2$ ./cachemaster -r -f data   
>file:data  
>time    pageCount       inCache change  
>[2103-8-5 16:03:14] 125000      125000  125000  
>[2103-8-5 16:03:15] 125000      125000  0  
>[2103-8-5 16:03:16] 125000      125000  0  
>[2103-8-5 16:03:17] 125000      125000  0  

###Real time stat of directory(-i is interval, default 1s)
>-bash-3.2$ ./cachemaster -r -d mydir/ -i 0.5  
>file:mydir/  
>time    pageCount       inCache change  
>[2103-8-5 16:05:12] 375000      375000  375000  
>[2103-8-5 16:05:13] 375000      375000  0  
>[2103-8-5 16:05:13] 375000      375000  0  
>[2103-8-5 16:05:14] 375000      375000  0  
>[2103-8-5 16:05:15] 375000      375000  0  

###Real time stat of directory(-s suppress the unchanged line)
>-bash-3.2$ ./cachemaster -r -d mydir/ -i 0.5 -S   
>file:mydir/  
>time    pageCount       inCache change  
>[2103-8-5 16:10:52] 375000      375000  375000  
>[2103-8-5 16:11:17] 375000      358841  -16159  
>[2103-8-5 16:11:17] 375000      0       -358841  

###Warmup/Readahead of file
>-bash-3.2$ ./cachemaster -c -f data   
>Release:data  
>-bash-3.2$ ./cachemaster -s -f data    
>Stat:data size:488M cached:0Bytes  
>-bash-3.2$ ./cachemaster -w -f data    
>Warmup File:data TimeUsed:6027 ms  
>-bash-3.2$ ./cachemaster -s -f data   
>Stat:data size:488M cached:488M  

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
