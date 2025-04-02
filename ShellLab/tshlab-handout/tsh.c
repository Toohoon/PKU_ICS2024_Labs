/* 
 * tsh - A tiny shell program with job control
 * 
 * <郑斗薰 2100094820>
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE, //没有builtin命令
        BUILTIN_QUIT, //退出
        BUILTIN_JOBS,//显示出jobs
        BUILTIN_BG,//后台进行
        BUILTIN_FG,//前台进行
        BUILTIN_KILL,//kill jobs
        BUILTIN_NOHUP //忽略信号（SIGHUP）
        } builtins;
};

/* End global variables */

/* Function prototypes */
void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok);  /*解析命令行的函数，和书上一样*/ 
void sigquit_handler(int sig);/*quit信号处理*/

void clearjob(struct job_t *job);/*清理进程链表，退出的时候用*/
void initjobs(struct job_t *job_list); /*初始化进程链表*/
int maxjid(struct job_t *job_list);  /*找到最大的进程组号*/
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline); /*添加进程，这个是需要我们手动添加的*/
int deletejob(struct job_t *job_list, pid_t pid); /*删除进程，依然需要我们手动删除*/
pid_t fgpid(struct job_t *job_list);     /*如果有前台工作进程，返回1，否则返回0*/
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);  /*通过pid获得对于的进程结构体指针*/ 
struct job_t *getjobjid(struct job_t *job_list, int jid);  /*通过jid获得对于的进程结构体指针*/
int pid2jid(pid_t pid);   /*返回对于pid进程的jid*/
void listjobs(struct job_t *job_list, int output_fd);   /*打印进程信息*/

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
ssize_t sio_put(const char *fmt, ...);
void sio_error(char s[]);

typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);


/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);

    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
//一些包装函数：
pid_t Fork(void) {
    pid_t pid;
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}
int Open(const char* pathname, int flags, mode_t mode) {
    int rc;
    if ((rc = open(pathname, flags, mode)) < 0)
        unix_error("Open error");
    return rc;
}

int Sigemptyset(sigset_t* set) {
    int rc;
    if ((rc = sigemptyset(set)) < 0)
        unix_error("Sigemptyset error");
    return rc;
}

int Sigaddset(sigset_t* set, int signum) {
    int rc;
    if ((rc = sigaddset(set, signum)) < 0)
        unix_error("Sigaddset error");
    return rc;
}

int Sigfillset(sigset_t* set) {
    int rc;
    if ((rc = sigfillset(set)) < 0)
        unix_error("Sigfillset error");
    return rc;
}

int Sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    int rc;
    if ((rc = sigprocmask(how, set, oldset)) < 0)
        unix_error("Sigprocmask error");
    return rc;
}
int Execve(const char* filename, char* const argv[], char* const envp[]) {
    int rc;
    if ((rc = execve(filename, argv, envp)) < 0)
        unix_error("Execve error");
    return rc;
}

int Dup(int fd) {
    int rc;
    if ((rc = dup(fd)) < 0)
        unix_error("Dup error");
    return rc;
}

int Dup2(int fd1, int fd2) {
    int rc;
    if ((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    return rc;
}

int Close(int fd) {
    int rc;
    if ((rc = close(fd)) < 0)
        unix_error("Close error");
    return rc;
}

int Kill(pid_t pid, int signum) {
    int rc;
    if ((rc = kill(pid, signum)) < 0)
        unix_error("Kill error");
    return rc;
}

void eval_BUILTIN_NONE(struct cmdline_tokens tok, int bg, char* cmdline) {
    
    
    pid_t pid;
    sigset_t maskFull, maskPrevious;
    // 设置信号屏蔽集，阻塞所有信号以保护临界区
    Sigfillset(&maskFull);
    Sigprocmask(SIG_BLOCK, &maskFull, &maskPrevious);

    

    // 创建子进程
    if ((pid = Fork()) == 0) {
        // 子进程逻辑
        setpgid(0, 0); // 将子进程设置为新进程组组长
        Sigprocmask(SIG_SETMASK, &maskPrevious, NULL); // 恢复信号屏蔽状态

        // 尝试执行命令
        if (Execve(tok.argv[0], tok.argv, environ) < 0) {
            printf("%s: Command not found\n", tok.argv[0]); // 输出错误信息
            exit(0); // 子进程退出
        }
    } else {
        // 父进程逻辑
        if (bg) {
            // 后台任务添加到作业列表
            addjob(job_list, pid, BG, cmdline);
        } else {
            // 前台任务添加到作业列表
            addjob(job_list, pid, FG, cmdline);
        }
        Sigprocmask(SIG_SETMASK, &maskPrevious, NULL); // 恢复信号屏蔽状态

        if (!bg) {
            // 前台作业处理逻辑
            while (pid == fgpid(job_list)) {
                sigsuspend(&maskPrevious); // 暂停父进程，等待信号到来
            }
        } else {
            // 后台作业输出信息
            printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
        }
    }
    return;
};

void eval_BUILTIN_KILL(struct cmdline_tokens tok) {
    pid_t pid;
    int jid;
    struct job_t * job;

    // 检查是否提供了PID或JID
    if (tok.argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", tok.argv[0]);
        return;
    }

    // 处理以'%'开头的JID
    if (tok.argv[1][0] == '%') {
        jid = atoi(&tok.argv[1][1]);  // 提取JID
        jid = abs(jid);                // 确保JID为正
        job = getjobjid(job_list, jid);  // 获取对应作业

        // 检查是否找到了作业
        if (job == NULL) {
            printf("%%%d: No such job\n", jid);
            return;
        }
    }
    // 处理PID
    else {
        pid = atoi(tok.argv[1]);      // 提取PID
        job = getjobpid(job_list, pid);  // 获取对应作业

        // 检查是否找到了作业
        if (job == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
    }

    // 向目标进程发送SIGTERM信号
    Kill(-abs(job->pid), SIGTERM);
    return;
}

void eval_BUILTIN_BG(struct cmdline_tokens tok) {
    pid_t pid;
    sigset_t maskFull, maskPrevious,singleSignalMask;

    // 初始化信号集
    Sigemptyset(&singleSignalMask);
    Sigaddset(&singleSignalMask, SIGCHLD);
    Sigfillset(&maskFull);

    struct job_t *currentJob;

    // 检查命令参数有效性
    if (tok.argv[1] == NULL || tok.argc != 2) {
        printf("%s command requires PID or %%jobid argument\n", tok.argv[0]);
        return;
    }

    // 根据参数类型确定是Job ID还是PID
    if (tok.argv[1][0] == '%') {
        int jobId = atoi(&tok.argv[1][1]);
        currentJob = getjobjid(job_list, jobId);
        if (currentJob == NULL) {
            printf("%%%d: No such job\n", jobId);
            return;
        }
    } else {
        pid = atoi(tok.argv[1]);
        currentJob = getjobpid(job_list, pid);
        if (currentJob == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
    }

    // 阻塞信号以保护临界区
    Sigprocmask(SIG_BLOCK, &maskFull, &maskPrevious);

    // 根据任务状态执行操作
    if (currentJob->state == ST) {
        currentJob->state = BG;  // 切换为后台运行
        Kill(-(currentJob->pid), SIGCONT);
        printf("[%d] (%d) %s", currentJob->jid, currentJob->pid, currentJob->cmdline);
    }

    // 恢复信号屏蔽状态
    Sigprocmask(SIG_SETMASK, &maskPrevious, NULL);
    return;
}

void eval_BUILTIN_FG(struct cmdline_tokens tok) {
    struct job_t *job;
    pid_t pid;
    sigset_t maskFull, maskPrevious;

    // 初始化信号集并阻塞所有信号
    Sigfillset(&maskFull);
    Sigprocmask(SIG_BLOCK, &maskFull, &maskPrevious);

    // 检查命令格式是否正确，确保有PID或%%jobid
    if (tok.argv[1] == NULL || tok.argc != 2) {
        printf("%s command requires PID or %%jobid argument\n", tok.argv[0]);
        Sigprocmask(SIG_SETMASK, &maskPrevious, NULL); // 恢复信号屏蔽
        return;
    }

    // 判断输入是JID还是PID并获取对应的作业
    if (tok.argv[1][0] == '%') {
        int jid = atoi(&tok.argv[1][1]);
        job = getjobjid(job_list, jid);

        // 如果没有找到对应作业，打印错误信息并退出
        if (job == NULL) {
            printf("%%%d: No such job\n", jid);
            Sigprocmask(SIG_SETMASK, &maskPrevious, NULL); // 恢复信号屏蔽
            return;
        }
    } else {
        pid = atoi(tok.argv[1]);
        job = getjobpid(job_list, pid);

        // 如果没有找到对应作业，打印错误信息并退出
        if (job == NULL) {
            printf("(%d): No such process\n", pid);
            Sigprocmask(SIG_SETMASK, &maskPrevious, NULL); // 恢复信号屏蔽
            return;
        }
    }

    // 获取目标进程的PID
    pid = job->pid;

    // 根据作业的状态进行处理
    if (job->state == ST) {
        job->state = FG;  // 将作业状态更改为前台
        kill(-pid, SIGCONT);  // 向作业发送继续执行信号
        while (pid == fgpid(job_list)) {
            sigsuspend(&maskPrevious);  // 等待前台作业结束
        }
    } else if (job->state == BG) {
        job->state = FG;  // 将后台作业切换为前台
        while (job->pid == fgpid(job_list)) {
            sigsuspend(&maskPrevious);  // 等待前台作业结束
        }
    }

    // 恢复信号屏蔽
    Sigprocmask(SIG_SETMASK, &maskPrevious, NULL);
    return;
}
void eval_BUILTIN_NOHUP(struct cmdline_tokens tok,char *cmdline){
    int bg;              // 是否是后台执行任务
    pid_t pid;           // 子进程的 PID

    // 解析命令行，确定是后台还是前台任务
    bg = parseline(cmdline, &tok); 

    // 信号集定义与初始化
    //sigset_t maskFull, maskPrevious;
    sigset_t maskFull,  maskPrevious, maskHup, maskTemp, maskChild, maskCombined;

    Sigemptyset(&maskChild);
    Sigaddset(&maskChild, SIGCHLD);
    sigfillset(&maskFull);
    Sigemptyset(&maskHup);
    Sigemptyset(&maskCombined);
    Sigaddset(&maskHup, SIGHUP);
    Sigaddset(&maskCombined, SIGHUP);
    Sigaddset(&maskCombined, SIGCHLD);

    // 阻塞 SIGHUP 信号
    Sigprocmask(SIG_BLOCK, &maskHup, &maskPrevious);

    // 阻塞 SIGCHLD 信号
    Sigprocmask(SIG_BLOCK, &maskChild, &maskTemp);

    // 创建子进程
    pid = fork();
    if (pid == 0) {
        // 子进程中解除信号屏蔽
        Sigprocmask(SIG_SETMASK, &maskTemp, NULL);

        // 设置进程组
        if (setpgid(0, 0) < 0) {
            exit(0);
        }

        // 执行命令
        if (Execve(tok.argv[1], tok.argv + 1, environ) < 0) {
            printf("%s: Command not found.\n", tok.argv[0]);
            exit(0);
        }
    } else {
        // 父进程中阻塞所有信号
        Sigprocmask(SIG_BLOCK, &maskFull, NULL);

        // 将任务添加到作业列表
        if (bg == 0)
            addjob(job_list, pid, FG, cmdline);  // 前台任务
        else
            addjob(job_list, pid, BG, cmdline);  // 后台任务

        // 恢复信号屏蔽
        sigprocmask(SIG_SETMASK, &maskFull, NULL);

        if (bg == 0) {
            // 如果是前台任务，等待其完成
            while (pid == fgpid(job_list))
                sigsuspend(&maskTemp);
        } else {
            // 如果是后台任务，输出作业信息
            Sigprocmask(SIG_BLOCK, &maskFull, NULL);
            struct job_t *job = getjobpid(job_list, pid);
            printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
        }

        // 恢复原来的信号屏蔽
        Sigprocmask(SIG_SETMASK, &maskPrevious, NULL);
    }
    return;
}
void 
eval(char *cmdline) 
{
    int bg; 
    struct cmdline_tokens tok;
    int input_file = STDIN_FILENO;
    int output_file = STDOUT_FILENO;
    /* Parse command line */
    bg = parseline(cmdline, &tok);

    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;
    
    if (tok.infile) {
        input_file = Open(tok.infile, O_RDONLY, 0);
    }
    if (tok.outfile) {
        output_file = Open(tok.outfile, O_WRONLY, 0);
    }
    int std_input = Dup(STDIN_FILENO);
    int std_output = Dup(STDOUT_FILENO);
    Dup2(input_file, STDIN_FILENO);
    Dup2(output_file, STDOUT_FILENO);
    switch (tok.builtins) {

    case BUILTIN_NONE:
        eval_BUILTIN_NONE(tok, bg, cmdline);

        break;

    case BUILTIN_QUIT:
        exit(0);

    case BUILTIN_JOBS:
        listjobs(job_list, output_file);
        break;

    case BUILTIN_BG:
        eval_BUILTIN_BG(tok);
        break;

    case BUILTIN_FG:
        eval_BUILTIN_FG(tok);
        break;

    case BUILTIN_KILL:
        eval_BUILTIN_KILL(tok);
        break;

    case BUILTIN_NOHUP:
       eval_BUILTIN_NOHUP(tok,cmdline);
       break;

    default:
        break;
    }
    if (tok.infile) {
        Close(input_file);
        Dup2(std_input, STDIN_FILENO);
    }
    if (tok.outfile) {
        Close(output_file);
        Dup2(std_output, STDOUT_FILENO);
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters 
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else if (!strcmp(tok->argv[0], "kill")) {          /* kill command */
        tok->builtins = BUILTIN_KILL;
    } else if (!strcmp(tok->argv[0], "nohup")) {            /* kill command */
        tok->builtins = BUILTIN_NOHUP;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void 
sigchld_handler(int sig) 
{
    int saved_errno = errno;   // 保存原有的 errno
    pid_t child_pid;
    int status;

    // 等待所有已更改状态的子进程
    while ((child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status)) {
            // 如果进程正常退出
            deletejob(job_list, child_pid);
        }
        if (WIFSTOPPED(status)) {
            // 如果进程被暂停
            struct job_t *stopped_job = getjobpid(job_list, child_pid);
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(child_pid), child_pid, WSTOPSIG(status));
            if (stopped_job != NULL) {
                stopped_job->state = ST;
            }
        }
        if (WIFSIGNALED(status)) {
            // 如果进程被信号终止
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(child_pid), child_pid, WTERMSIG(status));
            deletejob(job_list, child_pid);
        }
    }

    // 恢复原始的 errno 值
    errno = saved_errno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
    //return;
    int saved_errno = errno;
    pid_t pid = fgpid(job_list);
    if (pid) {
        Kill(pid, SIGINT);
    }
    errno = saved_errno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
    //return;
    int saved_errno = errno;
    pid_t pid = fgpid(job_list);
    if(pid!=0){
        if(Kill(-pid, SIGTSTP) != 0){
            unix_error("Error during stop");
        }
    }
    errno = saved_errno;
    return;
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void 
sigquit_handler(int sig) 
{
    sio_error("Terminating after receipt of SIGQUIT signal\n");
}



/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int 
pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE << 2];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void 
usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void 
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/* Private sio_functions */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    
    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);
    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(const char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}

/* sio_copy - Copy len chars from fmt to s (by Ding Rui) */
void sio_copy(char *s, const char *fmt, size_t len)
{
    if(!len)
        return;

    for(size_t i = 0;i < len;i++)
        s[i] = fmt[i];
}

/* Public Sio functions */
ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s));
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */ 
    return sio_puts(s);
}

ssize_t sio_put(const char *fmt, ...) // Put to the console. only understands %d
{
  va_list ap;
  char str[MAXLINE]; // formatted string
  char arg[128];
  const char *mess = "sio_put: Line too long!\n";
  int i = 0, j = 0;
  int sp = 0;
  int v;

  if (fmt == 0)
    return -1;

  va_start(ap, fmt);
  while(fmt[j]){
    if(fmt[j] != '%'){
        j++;
        continue;
    }

    sio_copy(str + sp, fmt + i, j - i);
    sp += j - i;

    switch(fmt[j + 1]){
    case 0:
    		va_end(ap);
        if(sp >= MAXLINE){
            write(STDOUT_FILENO, mess, sio_strlen(mess));
            return -1;
        }
        
        str[sp] = 0;
        return write(STDOUT_FILENO, str, sp);

    case 'd':
        v = va_arg(ap, int);
        sio_ltoa(v, arg, 10);
        sio_copy(str + sp, arg, sio_strlen(arg));
        sp += sio_strlen(arg);
        i = j + 2;
        j = i;
        break;

    case '%':
        sio_copy(str + sp, "%", 1);
        sp += 1;
        i = j + 2;
        j = i;
        break;

    default:
        sio_copy(str + sp, fmt + j, 2);
        sp += 2;
        i = j + 2;
        j = i;
        break;
    }
  } // end while

  sio_copy(str + sp, fmt + i, j - i);
  sp += j - i;

	va_end(ap);
  if(sp >= MAXLINE){
    write(STDOUT_FILENO, mess, sio_strlen(mess));
    return -1;
  }
  
  str[sp] = 0;
  return write(STDOUT_FILENO, str, sp);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t 
*Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

