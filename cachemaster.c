#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>

#define US_PER_SEC (1000*1000)

enum FileType{
    REGFILE,
    DIRECTORY
};

enum CmdType{
    CLEAR,
    STAT,
    LOCK,
    WARM,
    RSTAT
};

typedef struct CmdParam{
    char path[256]; 
    long interval;
    bool is_daemon;
    bool is_suppress;
    enum CmdType  cmd_type;
    enum FileType file_type;    
}CmdParam;

typedef struct CacheStat{
    size_t page_count;
    size_t in_cache;
    bool   is_print;
}CacheStat;

char buf1[1024] = {0};
char buf2[1024] = {0};
char buf3[1024] = {0};

int getUsedTime(struct timeval *begin, struct timeval *end){
    return (end->tv_sec - begin->tv_sec)*1000 + (end->tv_usec-begin->tv_usec)/1000;
}

char *getTimeStr(){
    time_t timer;
    time(&timer);
    struct tm *tm = localtime(&timer);
    static char buf[128] = {0};
    sprintf(buf, "[%d-%d-%d %02d:%02d:%02d]", 
            tm->tm_year+1990,
            tm->tm_mon+1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec
            );
    return buf;
}

void usage(char *bin_name){
    fprintf(stderr, "Usage:%s [Option] [File] ...\n", bin_name);
    fprintf(stderr, "-c Clear Page Cache.\n");
    fprintf(stderr, "-l Lock file into memory.\n");
    fprintf(stderr, "-r Real Time Stat of Page Cache.\n");
    fprintf(stderr, "-s Stat of Page Cache.\n");
    fprintf(stderr, "-w Read ahead to page cache, for warmup.\n");
    fprintf(stderr, "-f Operation on a file.\n");
    fprintf(stderr, "-d Operation on a directory.\n");
    fprintf(stderr, "-m Lock in daemon, only used with -l.\n");
    fprintf(stderr, "-i Interval when in real time stat.\n");
    fprintf(stderr, "-c -s -r -l -w is mutual with each other; -f is mutual with -d\n");
    fprintf(stderr, "-S Suppress the unchanged line, only used with -r.\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "%s -c -f file_name | Clear cache of a file\n", bin_name);
    fprintf(stderr, "%s -c -d dir_name | Clear cache of a direcroty\n", bin_name);
    fprintf(stderr, "%s -s -f file_name | Stat cache of a file\n", bin_name);
    fprintf(stderr, "%s -s -d dir_name | Stat cache of a direcroty\n", bin_name);
    fprintf(stderr, "%s -r -f file_name | Stat real time cache of a file\n", bin_name);
    fprintf(stderr, "%s -r -d dir_name | Stat real time cache of a direcroty\n", bin_name);
    fprintf(stderr, "%s -l -f file_name | Lock file\n", bin_name);
    fprintf(stderr, "%s -l -d dir_name | Lock  direcroty\n", bin_name);
    fprintf(stderr, "%s -w -f file_name | Warmup file\n", bin_name);
    fprintf(stderr, "%s -w -d dir_name | Warmup  direcroty\n", bin_name);
    fprintf(stderr, "%s -l -f -m file_name | Lock file in daemon\n", bin_name);
    fprintf(stderr, "%s -l -d -m dir_name | Lock direcroty in daemon\n", bin_name);
    fprintf(stderr, "\n");
}

void daemonMe(){
    int pid = fork();
    if(pid == -1){
        fprintf(stderr, "Fork Error:%s\n", strerror(errno));
        exit(-1);
    }else{
        if(pid>0)
            exit(0); 
    }

    if(setsid() == -1){
        fprintf(stderr, "Setsid Error:%s\n", strerror(errno));
        exit(-1);
    }

    if (freopen("/dev/null", "r", stdin)  == NULL ||
        freopen("/dev/null", "w", stdout) == NULL ||
        freopen("/dev/null", "w", stderr) == NULL)
        fprintf(stderr, "Redirect Handles Error:%s\n", strerror(errno));
}

char* sizeFit(unsigned long size, char *buffer){
    if(size<1024){
        sprintf(buffer, "%luBytes", size);
        return buffer;
    }
    size = size/1024;
    if(size<1024){
        sprintf(buffer, "%luK", size);
        return buffer;
    }
    size = size/1024;
    if(size<1024){
        sprintf(buffer, "%luM", size);
        return buffer;
    }
    sprintf(buffer, "%.1fG", size/1024.0f);
    return buffer;
}

void parseArgs(int argc, char *argv[], CmdParam *pCmd){
    int i = 0;
    int loop =0;
    int check_count = 0;
    long l = 0;
    memset(pCmd, 0, sizeof(CmdParam));
    pCmd->interval = US_PER_SEC;

    while ((i = getopt(argc, argv, "d:f:i:clmrsSwh")) != EOF){
        switch(i){
            case 'c':
                pCmd->cmd_type = CLEAR;
                check_count++;
                break;
            case 'd':
                strcpy(pCmd->path, optarg);
                pCmd->file_type = DIRECTORY;
                break;
            case 'f':
                strcpy(pCmd->path, optarg);
                pCmd->file_type = REGFILE;
                break;
            case 'i':
                l = atof(optarg)*US_PER_SEC;
                pCmd->interval = l;
                if(pCmd->interval<=0)
                    pCmd->interval = US_PER_SEC;
                break;
            case 'l':
                pCmd->cmd_type = LOCK;
                check_count++;
                break;
            case 'm':
                pCmd->is_daemon = true;
                break;
            case 'r':
                pCmd->cmd_type = RSTAT;
                check_count++;
                break;
            case 's':
                pCmd->cmd_type = STAT;
                check_count++;
                break;
            case 'S':
                pCmd->is_suppress = true;
                break;
            case 'w':
                pCmd->cmd_type = WARM;
                check_count++;
                break;
            case 'h':
            default:
                usage(argv[0]);
                break;
        }
        ++loop;
    }


    if(check_count > 1){
        fprintf(stderr, "Clear Stat Real Lock is mutual with each other!\n");
        usage(argv[0]);
        exit(-1);
    }

    if(strlen(pCmd->path) == 0){
        fprintf(stderr, "Parameter Error\n");
        usage(argv[0]);
        exit(-1);
    }
}

bool doCmd(const char* path, enum CmdType cmd_type, enum FileType file_type, CacheStat* pStat){
    int  fd = 0;
    DIR  *dir = NULL;
    DIR  *soft_dir = NULL;
    struct dirent *dp = NULL;
    char file_name[256];
    char link_name[256];
    char real_path[256];

    void *pbase = NULL;
    char *vec = NULL;
    
    struct stat st;
    int in_cache = 0;
    int str_len  = 0;
    
    int page_index;
    int page_count;
    int pagesize = getpagesize();

    int time_used = 0;
    struct timeval begin, end;

    if(realpath(path, real_path) == NULL){
        goto ERROR;
    }

    if(file_type == REGFILE){
        fd = open(real_path, O_RDONLY);
        if(fd<0){
            goto ERROR;
        }

        if(stat(real_path, &st)<0){
            goto ERROR;
        }

        switch(cmd_type){
            case CLEAR:
                if(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_DONTNEED) != 0){
                    goto ERROR;
                }
                fprintf(stdout, "Release:%s\n", real_path);
                break;
            case STAT:
                if(st.st_size == 0)
                    goto EMPTYFILE;

                pbase = mmap((void *)0, st.st_size, PROT_NONE, MAP_SHARED, fd, 0);
                if(pbase == MAP_FAILED){
                    goto ERROR;
                }

                page_count = (st.st_size+pagesize-1)/pagesize;
                vec = (char*)calloc(1, page_count);
                if(mincore(pbase, st.st_size, (unsigned char *)vec) != 0){
                    goto ERROR; 
                }

                for(page_index=0; page_index<page_count; page_index++){
                    if(vec[page_index]&1 != 0){
                        ++in_cache;
                    } 
                }

                pStat->page_count += page_count;
                pStat->in_cache   += in_cache;

EMPTYFILE:
                if(pStat->is_print == true){
                    fprintf(stdout, "Stat:%s size:%s cached:%s\n", 
                            real_path,
                            sizeFit(st.st_size, buf1),
                            sizeFit(in_cache*(pagesize), buf2));
                }
                break;
            case LOCK:
                if(st.st_size == 0){
                    fprintf(stderr, "Empty file %s\n", real_path);
                    return true;
                }

                pbase = mmap((void *)0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
                if(pbase == MAP_FAILED){
                    goto ERROR;
                }

                if(mlock(pbase, st.st_size) == 0){
                    fprintf(stdout, "Lock %s succeed, size:%s\n", real_path, sizeFit(st.st_size, buf1));
                    return true;
                }else{
                    goto ERROR;
                }
            case WARM:
                gettimeofday(&begin, NULL);
                if(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_WILLNEED) != 0){
                    goto ERROR;
                }
                gettimeofday(&end, NULL);
                time_used = getUsedTime(&begin, &end);
                fprintf(stdout, "Warmup File:%s TimeUsed:%d ms\n", real_path, time_used);
                break;
            default:
                fprintf(stderr, "do not support cmd type %d\n", cmd_type);
                goto ERROR;
        }

        close(fd);
        if(vec)     free(vec);
        if(pbase)   munmap(pbase, st.st_size);
        return true;
    }else if(file_type == DIRECTORY){
        if((dir = opendir(real_path)) == NULL){
            goto ERROR;
        }

        gettimeofday(&begin, NULL);

        while((dp = readdir(dir)) != NULL){
            if(strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
                memset(file_name, 0, sizeof(file_name));
                strcat(file_name, real_path);
                strcat(file_name, "/");
                strcat(file_name, dp->d_name);
                if(dp->d_type == DT_REG){
                    doCmd(file_name, cmd_type, REGFILE,  pStat);
                }else if(dp->d_type == DT_DIR){
                    doCmd(file_name, cmd_type, DIRECTORY, pStat);
                }else if(dp->d_type == DT_LNK){
                    if(realpath(file_name, link_name) != NULL){
                        if(stat(link_name, &st)<0){
                            goto ERROR;
                        }

                        if(st.st_mode & S_IFREG){
                            doCmd(file_name, cmd_type, REGFILE,  pStat);
                        }else if(st.st_mode & S_IFDIR){
                            doCmd(file_name, cmd_type, DIRECTORY, pStat);
                        }
                    }
                }else{
                    fprintf(stdout, "%s:%c type unsupported!\n", dp->d_name, dp->d_type);
                }
            }
        }

        gettimeofday(&end, NULL);
        time_used = getUsedTime(&begin, &end);
        if(cmd_type == WARM){
            fprintf(stdout, "Warmup Dir:%s TimeUsed:%d ms\n", real_path, time_used);
        }

        closedir(dir);
        return true;
    }

ERROR:
    fprintf(stderr, "File:%s %s\n", real_path, strerror(errno));
    if(fd)      close(fd);
    if(dir)     closedir(dir);
    if(vec)     free(vec);
    if(pbase)   munmap(pbase, st.st_size);
    return false;
}

int realStat(const char *path, long interval, enum FileType file_type, bool is_suppress){
    CacheStat stat;
    int pre = 0;
    bool first = true;
    bool ret = false;

    memset(&stat, 0, sizeof(stat));
    fprintf(stdout, "file:%s\n", path);
    fprintf(stdout, "time\tpage_count\tin_cache\tchange\n");

    while(1){
        pre = stat.in_cache;
        memset(&stat, 0, sizeof(stat));
        ret = doCmd(path, STAT, file_type, &stat);

        if(pre==stat.in_cache && is_suppress && !first) continue;
        first = false;
        if(ret == false){
            fprintf(stderr, "real stat error %s\n", path);
            exit(0);
        }else{
            fprintf(stdout, "%s %d\t%d\t%d\n", 
               getTimeStr(), stat.page_count, stat.in_cache, stat.in_cache-pre);
        } 
        usleep(interval);
    }
}

int main(int argc, char *argv[]){
    if(argc<=2){
        usage(argv[0]);
        exit(-1);
    } 

    int pagesize = getpagesize();
    CmdParam cmd;
    CacheStat stat;
    memset(&stat, 0, sizeof(stat));
    memset(&cmd, 0, sizeof(cmd));
    stat.is_print = true; //default true
    parseArgs(argc, argv, &cmd);

    if(cmd.is_daemon)
        daemonMe();

    if(cmd.cmd_type == CLEAR){
        doCmd(cmd.path, cmd.cmd_type, cmd.file_type, NULL);
    }else if(cmd.cmd_type == STAT){
        doCmd(cmd.path, cmd.cmd_type, cmd.file_type, &stat);
        if(cmd.file_type == DIRECTORY){
            fprintf(stdout, "\nTotal Cache of Directory:%s size:%s cached:%s\n", 
                    cmd.path,
                    sizeFit(stat.page_count*pagesize, buf1), 
                    sizeFit(stat.in_cache*pagesize, buf2));
        }
    }else if(cmd.cmd_type == RSTAT){
        realStat(cmd.path, cmd.interval, cmd.file_type, cmd.is_suppress);
    }else if(cmd.cmd_type == LOCK){
        doCmd(cmd.path, cmd.cmd_type, cmd.file_type, NULL);
        select(0, NULL, NULL, NULL, NULL);
    }else if(cmd.cmd_type == WARM){
        doCmd(cmd.path, cmd.cmd_type, cmd.file_type, NULL);
    }

    return 0;
}
