// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <utility>
#include <vector>
#include <map>
#include <set>
#include <string.h>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    char* cmd_line;
public:
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed

    const char* get_cmd_line() {return cmd_line;}

    char** make_args();
    void free_args(char** args);
    int args_length(char **args);
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line){}

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    bool is_bg;
    std::string og_line;
    ExternalCommand(const char *cmd_line, bool bg, std::string og_line) : Command(cmd_line), 
        is_bg(bg), og_line(std::move(og_line)){}

    virtual ~ExternalCommand() {
    }

    void execute() override;

    std::string GetOgLine(){return og_line;}
};


class RedirectionCommand : public Command {
    std::string path;
    std::string op;
public:
    explicit RedirectionCommand(const char *cmd_line, std::string path, std::string op) :
        Command(cmd_line), path(path), op(op) {}

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    char *cmd1;
    char *op;
    char *cmd2;
public:
    PipeCommand(const char *cmd_line, const char *cmd1, const char *op, const char *cmd2);

    virtual ~PipeCommand();

    void execute() override;
};

class DiskUsageCommand : public Command {

    struct linux_dirent {
               unsigned long  d_ino;     /* Inode number */
               unsigned long  d_off;     /* Not an offset; see below */
               unsigned short d_reclen;  /* Length of this linux_dirent */
               char           d_name[1];  /* Filename (null-terminated) */
           }; //deff didn't copy past from the manpage

    int Rec(const char* path);
public:
    DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line) : Command(cmd_line){}

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class USBInfoCommand : public Command {
    struct linux_dirent {
        unsigned long  d_ino;     /* Inode number */
        unsigned long  d_off;     /* Not an offset; see below */
        unsigned short d_reclen;  /* Length of this linux_dirent */
        char           d_name[1];  /* Filename (null-terminated) */
    }; //deff didn't copy past from the manpage
    std::string ReadUsbProperty(const std::string& path);
    std::string GetUsbProperties(const std::string& bus_port);
public:
    USBInfoCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char** pold_dir;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), pold_dir(plastPwd){}
    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    JobsList *jobs;

    public:
    QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs){}

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        Command* cmd;
        int id;
        int pid;

        public:

            JobEntry(Command* cmd, int id, int pid) : cmd(cmd), id(id), pid(pid) {}
            ~JobEntry() = default;

            int get_id() {return id;}
            Command* get_cmd() {return cmd;}
            int get_pid() {return pid;}
    };

    std::vector<JobEntry> jobs;

    std::set<int> job_ids;
public:
    JobsList() = default;

    ~JobsList() = default;

    void addJob(Command *cmd, bool isStopped, int pid);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    void RemoveJobByPid(int pid);

    int GetSize();
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) :
        BuiltInCommand(cmd_line), jobs(jobs) {}

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs){}

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
    bool ExistsInEnv(std::string arg, std::vector<std::string> *allvars);
    void DeleteVar(const char* arg);
public:
    static std::vector<std::string>* ReadEnv(std::string path);
    UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};

class ChPrompt : public BuiltInCommand {
    public:
        ChPrompt(const char *cmd_line) : BuiltInCommand(cmd_line){}

        virtual ~ChPrompt() {}

        void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members'

    char* og_name;
    char* curr_name;
    int err_recover;
    int out_recover;
    int in_recover;
    int curr_pid;

    std::map<std::string, std::string> aliases;
    std::vector<std::string> alias_list;


    JobsList* jobs;

    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    void ch_prompt(const char *cmd_line = NULL);

    void AddToJobList(Command*, bool, int pid);

    const char* get_prompt();

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    bool isAliasTaken(const std::string alias);

    void addAlias(const std::string& alias, const std::string& arg);

    void removeAlias(const std::string& alias);

    void PrintAliases();

    enum options {append, no_append};

    void RedirectOut(std::string out_path, options option = no_append);

    enum pipe_out {out = 1, err = 2};

    void PipeOut(int fd, pipe_out out);

    void PipeIn(int fd);

    void RecoverIO();

    void Zakka();

    void setPid(int pid_num);

    int getPid() {return curr_pid;}
};

#endif //SMASH_COMMAND_H_
