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

#define SEC_PER_US (1000*1000)

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
    bool isDaemon;
    bool isSuppress;
    enum CmdType  cmd_type;
    enum FileType file_type;    
}CmdParam;

typedef struct CacheStat{
    size_t pageCount;
    size_t inCache;
    bool   isPrint;
}CacheStat;

char buf1[1024] = {0};
char buf2[1024] = {0};
char buf3[1024] = {0};

int getUsedTime(struct timeval *begin, struct timeval *end){
    assert(begin != NULL);
    assert(end   != NULL);
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
    pCmd->interval = SEC_PER_US;

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
                l = atof(optarg)*SEC_PER_US;
                pCmd->interval = l;
                if(pCmd->interval<=0)
                    pCmd->interval = SEC_PER_US;
                break;
            case 'l':
                pCmd->cmd_type = LOCK;
                check_count++;
                break;
            case 'm':
                pCmd->isDaemon = true;
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
                pCmd->isSuppress = true;
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

bool clear(const char* path, enum FileType file_type){
    int  fd = 0;
    DIR  *dir = NULL;
    char file_name[256];
    void *pbase = NULL;
    struct dirent *dp = NULL;
    struct stat st;

    if(file_type == REGFILE){
        fd = open(path, O_RDONLY);
        if(fd<0){
            goto ERROR;
        }

        if(stat(path, &st)<0){
            goto ERROR;
        }

        if(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_DONTNEED) != 0){
            goto ERROR;
        }

        fprintf(stdout, "Release:%s\n", path);
        return true;
    }else if(file_type == DIRECTORY){
        if((dir = opendir(path)) == NULL){
            goto ERROR;
        }

        while((dp = readdir(dir)) != NULL){
            if(strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
                memset(file_name, 0, sizeof(file_name));
                strcat(file_name, path);
                strcat(file_name, "/");
                strcat(file_name, dp->d_name);
                if(dp->d_type == DT_REG){
                    clear(file_name, REGFILE);
                }else if(dp->d_type == DT_DIR){
                    clear(file_name, DIRECTORY);
                }else{
                    fprintf(stdout, "%s:%c type unsupported!\n", dp->d_name, dp->d_type);
                }
            }
        }
        closedir(dir);
        return true;
    }

ERROR:
    fprintf(stderr, "File:%s %s\n", path, strerror(errno));
    if(pbase)   munmap(pbase, st.st_size);
    if(fd)      close(fd);
    if(dir)     closedir(dir);
    return false;
}

bool normalStat(const char* path, enum FileType file_type, struct CacheStat* stat_cache){
    int fd = 0;
    int pageIndex;
    int pageCount;
    int inCache = 0;
    DIR *dir = NULL;
    void *pbase = NULL;
    char *vec = NULL;
    struct stat st;
    struct dirent *dp = NULL;
    char file_name[256];
    int pagesize = getpagesize();

    if(file_type == REGFILE){
        fd = open(path, O_RDONLY);
        if(fd<0){
            goto ERROR;
        }

        if(stat(path, &st)<0){
            goto ERROR;
        }

        pbase = mmap((void *)0, st.st_size, PROT_NONE, MAP_SHARED, fd, 0);
        if(pbase == MAP_FAILED){
            goto ERROR;
        }

        pageCount = (st.st_size+pagesize-1)/pagesize;
        vec = (char*)calloc(1, pageCount);
        if(mincore(pbase, st.st_size, (unsigned char *)vec) != 0){
            goto ERROR; 
        }

        for(pageIndex=0; pageIndex<pageCount; pageIndex++){
            if(vec[pageIndex]&1 != 0){
                ++inCache;
            } 
        }

        stat_cache->pageCount += pageCount;
        stat_cache->inCache   += inCache;

        if(stat_cache->isPrint == true){
            fprintf(stdout, "Stat:%s size:%s cached:%s\n", 
                    path,
                    sizeFit(st.st_size, buf1),
                    sizeFit(inCache*(pagesize), buf2));
        }

        free(vec);
        munmap(pbase, st.st_size);
        close(fd);
        return true;
    }else if(file_type == DIRECTORY){
        if((dir = opendir(path)) == NULL){
            goto ERROR;
        }

        while((dp = readdir(dir)) != NULL){
            if(dp->d_name[0] != '.'){
                memset(file_name, 0, sizeof(file_name));
                strcat(file_name, path);
                strcat(file_name, "/");
                strcat(file_name, dp->d_name);
                if(dp->d_type == DT_REG){
                    normalStat(file_name, REGFILE,   stat_cache);
                }else if(dp->d_type == DT_DIR){
                    normalStat(file_name, DIRECTORY, stat_cache);
                }else{
                    fprintf(stdout, "%s:%c type unsupported!\n", dp->d_name, dp->d_type);
                }
            }
        }

        closedir(dir);
        return true;
    }

ERROR:
    if(stat_cache->isPrint == true){
        fprintf(stderr, "File:%s %s\n", path, strerror(errno));
    }
    if(vec)     free(vec);
    if(pbase)   munmap(pbase, st.st_size);
    if(fd>0)    close(fd);
    if(dir)     closedir(dir);
    return false;
}

int realStat(const char *path, long interval, enum FileType file_type, bool isSuppress){
    CacheStat stat;
    int pre = 0;
    bool first = true;
    bool ret = false;

    memset(&stat, 0, sizeof(stat));
    fprintf(stdout, "file:%s\n", path);
    fprintf(stdout, "time\tpageCount\tinCache\tchange\n");

    while(1){
        pre = stat.inCache;
        memset(&stat, 0, sizeof(stat));
        ret = normalStat(path, file_type, &stat);

        if(pre==stat.inCache && isSuppress && !first) continue;
        first = false;
        if(ret == false){
            fprintf(stderr, "real stat error %s\n", path);
            exit(0);
        }else{
            fprintf(stdout, "%s %d\t%d\t%d\n", 
               getTimeStr(), stat.pageCount, stat.inCache, stat.inCache-pre);
        } 
        usleep(interval);
    }
}

bool lock(const char* path, enum FileType file_type){
    int fd = 0;
    DIR *dir = NULL;
    void *pbase = NULL;
    struct dirent *dp = NULL;
    struct stat st;
    char file_name[256];

    if(file_type == REGFILE){
        fd = open(path, O_RDONLY);
        if(fd<0){
            goto ERROR;
        }

        if(stat(path, &st)<0){
            goto ERROR;
        }

        pbase = mmap((void *)0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if(pbase == MAP_FAILED){
            goto ERROR;
        }

        if(mlock(pbase, st.st_size) == 0){
            fprintf(stdout, "Lock %s succeed, size:%s\n", path, sizeFit(st.st_size, buf1));
            return true;
        }else{
            goto ERROR;
        }
    }else if(file_type == DIRECTORY){
        if((dir = opendir(path)) == NULL){
            goto ERROR;
        }

        while((dp = readdir(dir)) != NULL){
            if(strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
                memset(file_name, 0, sizeof(file_name));
                strcat(file_name, path);
                strcat(file_name, "/");
                strcat(file_name, dp->d_name);
                if(dp->d_type == DT_REG){
                    lock(file_name, REGFILE);
                }else if(dp->d_type == DT_DIR){
                    lock(file_name, DIRECTORY);
                }else{
                    fprintf(stdout, "%s:%c type unsupported!\n", dp->d_name, dp->d_type);
                }
            }
        }
        closedir(dir);
        return true;
    }

ERROR:
    fprintf(stderr, "File:%s %s\n", path, strerror(errno));
    if(pbase)   munmap(pbase, st.st_size);
    if(fd)      close(fd);
    if(dir)     closedir(dir);
    return false;
}

bool warmup(const char *path, enum FileType file_type){
    int fd = 0;
    DIR *dir = NULL;
    int time_used = 0;
    char file_name[256];
    struct stat st;
    struct timeval begin, end;
    struct dirent *dp = NULL;

    if(stat(path, &st)<0){
        goto ERROR;
    }

    gettimeofday(&begin, NULL);
    if(file_type == REGFILE){
        fd = open(path, O_RDONLY);
        if(fd<0){
            goto ERROR;
        }

        if(posix_fadvise(fd, 0, st.st_size, POSIX_FADV_WILLNEED) != 0){
            goto ERROR;
        }
    }else if(file_type == DIRECTORY){
        if((dir = opendir(path)) == NULL){
            goto ERROR;
        }

        while((dp = readdir(dir)) != NULL){
            if(dp->d_name[0] != '.'){
                memset(file_name, 0, sizeof(path));
                strcat(file_name, path);
                strcat(file_name, "/");
                strcat(file_name, dp->d_name);
                if(dp->d_type == DT_REG){
                    warmup(file_name, REGFILE);
                }else if(dp->d_type == DT_DIR){
                    warmup(file_name, DIRECTORY);
                }else{
                    fprintf(stdout, "%s:%c type unsupported!\n", dp->d_name, dp->d_type);
                }
            }
        }
    }

    gettimeofday(&end, NULL);
    time_used = getUsedTime(&begin, &end);

    if(file_type == REGFILE){
        fprintf(stdout, "Warmup File:%s TimeUsed:%d ms\n", path, time_used);
        close(fd);
    }else if(file_type == DIRECTORY){
        fprintf(stdout, "Warmup Dir:%s TimeUsed:%d ms\n", path, time_used);
        closedir(dir);
    }
    return true;

ERROR:
    fprintf(stderr, "File:%s %s\n", path, strerror(errno));
    if(fd)  close(fd);
    if(dir) closedir(dir);
    return false;
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
    stat.isPrint = true; //default true
    parseArgs(argc, argv, &cmd);

    if(cmd.isDaemon)
        daemonMe();

    if(cmd.cmd_type == CLEAR){
        clear(cmd.path, cmd.file_type);
    }else if(cmd.cmd_type == STAT){
        normalStat(cmd.path, cmd.file_type, &stat);
        if(cmd.file_type == DIRECTORY){
            fprintf(stdout, "\nTotal Cache of Directory:%s size:%s cached:%s\n", 
                    cmd.path,
                    sizeFit(stat.pageCount*pagesize, buf1), 
                    sizeFit(stat.inCache*pagesize, buf2));
        }
    }else if(cmd.cmd_type == RSTAT){
        realStat(cmd.path, cmd.interval, cmd.file_type, cmd.isSuppress);
    }else if(cmd.cmd_type == LOCK){
        lock(cmd.path, cmd.file_type);
        select(0, NULL, NULL, NULL, NULL);
    }else if(cmd.cmd_type == WARM){
        warmup(cmd.path, cmd.file_type);
    }

    return 0;
}
