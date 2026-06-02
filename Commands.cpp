#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <regex>
#include "Commands.h"
#include <fcntl.h>
#include "sys/syscall.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isComplex(char** args) {
    if(args == nullptr)
        return false;
    for(int i = 0; args[i] != nullptr; i++) {
        if(strchr(args[i],'*') != nullptr || strchr(args[i],'?') != nullptr) {
            return true;
        }
    }
    return false;
}

// TODO: Add your implementation for classes in Commands.h

SmallShell::SmallShell() {
    og_name = (char*) malloc(string("smash").length() + 1);
    strcpy(og_name, string("smash").c_str());
    curr_name = (char*) malloc(string(og_name).length() + 1);
    strcpy(curr_name, og_name);
    pold_dir = nullptr;
    err_recover = -1;
    out_recover = -1;
    jobs = new JobsList();
    in_recover = -1;
}

SmallShell::~SmallShell() {
    free(og_name);
    free(curr_name);
    if(pold_dir != nullptr) free(pold_dir);
    delete jobs;
}

void SmallShell::ch_prompt(const char *name){
    free(curr_name);
    if(name == NULL){
        curr_name = (char*) malloc(string(og_name).length() + 1);
        strcpy(curr_name, og_name);
    }
    else{
        curr_name = (char*) malloc(string(name).length() + 1);
        strcpy(curr_name, name);
    }
}

const char* SmallShell::get_prompt(){
    return curr_name;
}

bool SmallShell::isAliasTaken(std::string alias){
    for(const std::string name : {"chprompt", "showpid", "pwd", "cd", "jobs",
        "fg", "quit", "kill", "alias", "unalias", "unsetenv", "sysinfo",
        "du", "whoami", "usbinfo"}) if(alias == name) return true;

    for(pair<const std::string, const std::string> p : aliases) if(alias == p.first) return true;
    return false;
}

void SmallShell::addAlias(const std::string& alias, const string& arg){
    if(this->isAliasTaken(alias)) throw std::invalid_argument("alias already in use");
    aliases[alias] = arg;
    alias_list.push_back(alias+"=\'"+arg+"\'\n");
}

void SmallShell::PrintAliases(){
    for(const std::string &to_print : alias_list){
        write(1, to_print.c_str(), to_print.length());
    }
}

void SmallShell::removeAlias(const std::string& alias) {
    if(!this->isAliasTaken(alias)) {
        throw std::invalid_argument("no such alias in use");
    }
    aliases.erase(alias);
}

void SmallShell::RedirectOut(std::string out_path, options option){
    char* p = getcwd(NULL, 0);
    if(p == NULL){
        const char *problem = "smash error: getcwd failed\n";
        perror(problem);
        throw runtime_error(problem);
    }
    string path = string(p);
    free(p);
    path += ("/" + out_path);
    this->out_recover = dup(1);
    if(out_recover < 0){
        const char *problem = "smash error: dup failed\n";
        perror(problem);
        this->out_recover = -1;
        throw runtime_error(problem);
    }
    int fd = option == append ? open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0666) : 
        open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(fd < 0){
        const char *problem = "smash error: open failed\n";
        perror(problem);
        throw runtime_error(problem);
    }
    dup2(fd, 1);
    close(fd);
}

void SmallShell::PipeOut(int fd, pipe_out out){
    if(out == SmallShell::out){
        this->out_recover = dup(out);
        if(this->out_recover == -1){
            const char *problem = "smash error: dup failed\n";
            perror(problem);
            throw runtime_error("open failed");
        }
    }
    else{
        this->err_recover = dup(out);
        if(this->err_recover == -1){
            const char *problem = "smash error: dup failed\n";
            perror(problem);
            throw runtime_error("open failed");
        }
    }
    int check = dup2(fd, out);
    if(check == -1){
        const char *problem = "smash error: dup2 failed\n";
        perror(problem);
        throw runtime_error("open failed");
    }
}

void SmallShell::PipeIn(int fd){
    this->in_recover = dup(0);
    if(this->in_recover == -1){
        const char *problem = "smash error: dup failed\n";
        perror(problem);
        throw runtime_error("open failed");
    }
    int check = dup2(fd, 0);
    if(check == -1){
        const char *problem = "smash error: dup2 failed\n";
        perror(problem);
        throw runtime_error("open failed");
    }
}

void SmallShell::RecoverIO(){
    if(err_recover != -1){
        dup2(err_recover, 2);
        close(err_recover);
        err_recover = -1;
    }
    if(out_recover != -1){
        dup2(out_recover, 1);
        close(out_recover);
        out_recover = -1;
    }
    if(in_recover != -1){
        dup2(in_recover, 0);
        close(in_recover);
        in_recover = -1;
    }
}

void SmallShell::AddToJobList(Command* cmd, bool isStopped, int pid) {
    this->Zakka();
    this->jobs->addJob(cmd, isStopped, pid);
}

void SmallShell::Zakka(){
    int pid;
    do{
        this->setPid(-1);
        pid = waitpid(-1, NULL, WNOHANG);
        if(pid > 0) this->jobs->RemoveJobByPid(pid);
    }
    while(pid > 0);
}

void SmallShell::setPid(int pid_num) {
    this->curr_pid = pid_num;
}




/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string og_line = cmd_line;
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    bool bg = _isBackgroundComamnd(cmd_line);
    //check for alias and replace if found
    try{
        firstWord = firstWord.substr(0, firstWord.find_first_of("&|>"));
        string alias = aliases.at(firstWord);
        cmd_s.replace(0, firstWord.length(), alias);
        cmd_s = _trim(cmd_s);
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
        cmd_line = cmd_s.c_str();
    }
    catch(std::out_of_range& e){}

    regex pattern = regex("^alias [a-zA-Z0-9_]+='[^']*'$");
    if(regex_match(cmd_line, pattern)){
        return new AliasCommand(cmd_line);
    }

    //redirection command
    std::regex redidect_pattern = regex("^(.*?)\\s*(>>|>)\\s*([a-z0-9./_-]+)\\s*&?\\s*$");
    cmatch redirect_parts;
    if(regex_match(cmd_s.c_str(), redirect_parts, redidect_pattern)){
        return new RedirectionCommand(string(redirect_parts[1]).c_str(),
            string(redirect_parts[3]), string(redirect_parts[2]));
    }

    //pipe command
    std::regex pipe_pattern = regex("^(.+)\\s*(\\| | \\|\\&)\\s*(.+)\\s*$");
    cmatch pipe_parts;
    if(regex_match(cmd_s.c_str(), pipe_parts, pipe_pattern)){
        return new PipeCommand(pipe_parts[0].str().c_str(), 
            pipe_parts[1].str().c_str(), pipe_parts[2].str().c_str(), 
            pipe_parts[3].str().c_str());
    }


    if(firstWord.compare("chprompt") == 0){
        return new ChPrompt(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
    }
    // else if(firstWord.compare("alias") == 0){
    //     return new AliasCommand(cmd_line);
    // }
    else if(firstWord.compare("unalias") == 0){
        return new UnAliasCommand(cmd_line);
    }
    else if(firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }
    else if(firstWord.compare("du") == 0){
        return new DiskUsageCommand(cmd_line, bg, og_line);
    }
    else if(firstWord.compare("jobs") == 0){
        return new JobsCommand(cmd_line, this->jobs);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line);
    }
    else if(firstWord.compare("quit") == 0){
        return new QuitCommand(cmd_line, jobs);
    }
    else if(firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, this->jobs);
    }
    else if(firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, this->jobs);
    }
    else if(firstWord.compare("whoami") == 0) {
        return new WhoAmICommand(cmd_line, bg, og_line);
    }
    else if(firstWord.compare("usbinfo") == 0) {
        return new USBInfoCommand(cmd_line, bg, og_line);
    }
    else {
        return new ExternalCommand(cmd_line, bg, og_line);
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    this->Zakka();
    Command* cmd = CreateCommand(cmd_line);
    if(ExternalCommand* extCmd = dynamic_cast<ExternalCommand*>(cmd)) {//if succeeds then cmd is external
        if(dynamic_cast<DiskUsageCommand*>(cmd) != nullptr || 
            dynamic_cast<WhoAmICommand*>(cmd) != nullptr || 
            dynamic_cast<USBInfoCommand*>(cmd) != nullptr){
                
            if(!extCmd->is_bg){
                extCmd->execute();
                delete extCmd;
                return;
            }
            else{
                int p = fork();
                if(p == -1){
                        perror("smash error: fork failed");
                        delete extCmd;
                        return;
                    }
                if(p == 0){
                        extCmd->execute();
                        exit(0);
                    }
                else{
                    this->Zakka();
                    this->AddToJobList(extCmd, false, p);
                    return;
                }
            }
        }
        const pid_t p = fork();
        if(p > 0) { //parent
            if(!extCmd->is_bg) {
                this->setPid(p);
                waitpid(p, NULL, 0);
                this->setPid(-1);
                delete cmd;
            }
            else {
                    this->Zakka();
                    this->AddToJobList(extCmd, false, p); //not sure what isStopped should be, when is it ever true and we want to add it?
            }
        }
        else { //child
            setpgrp();
            cmd->execute();
        }
    }
    else{
        cmd->execute();
        delete cmd;
    }
}





void JobsList::addJob(Command* cmd, bool isStopped, int pid){
    int id = jobs.size() == 0 ? 1 : jobs.back().get_id()+1;
    jobs.push_back(JobEntry(cmd, id, pid));
    job_ids.insert(id);
}

//doesn't delete finished jobs. will need to add this feature after doing background jobs.
void JobsList::printJobsList(){
    for(JobEntry j : jobs){
        ExternalCommand *ej = dynamic_cast<ExternalCommand*>(j.get_cmd());
        string out = "[" + std::to_string(j.get_id()) + "] " 
            + ej->GetOgLine() +"\n";
        write(1, out.c_str(), out.length());
    }
}

void JobsList::RemoveJobByPid(int pid){
    for(auto itr = this->jobs.begin(); itr != this->jobs.end(); itr++)
        if(itr->get_pid() == pid) {
            delete itr->get_cmd();
            job_ids.erase(itr->get_id());
            jobs.erase(itr);
            return;
        }
}

void JobsList::killAllJobs(){
    for(JobEntry j : jobs){
        ExternalCommand *je = dynamic_cast<ExternalCommand*>(j.get_cmd());
        string mssg = std::to_string(j.get_pid()) + ": " +
            je->GetOgLine() + "\n";
        write(1, mssg.c_str(), mssg.length());
        kill(j.get_pid(), SIGKILL);
        waitpid(j.get_pid(), NULL, 0);
        delete je;
    }
    jobs.clear();
}

int JobsList::GetSize() {return jobs.size();}

JobsList::JobEntry* JobsList::getJobById(int id){
    for(JobEntry j : jobs)
        if(j.get_id() == id) {
            JobEntry *jp = &j;
            return jp;
        }
    return nullptr;
}




Command::Command(const char* cmd_line){
    this->cmd_line = (char*) malloc(sizeof(char)*(string(cmd_line).length()+1));
    strcpy(this->cmd_line, cmd_line);
}

Command::~Command(){
    free(this->cmd_line);
}

char** Command::make_args(){
    char** args = (char**) malloc(sizeof(char*) * 20);
    char* copy_cmd_line = strdup(cmd_line);
    _removeBackgroundSign(copy_cmd_line);
    for(int i =_parseCommandLine(copy_cmd_line, args); i< 20; i++)
        args[i] = nullptr;
    return args;
}

void Command::free_args(char** args){
    for(int i = 0; args[i] != NULL; i++)
        free(args[i]);
    free(args);
}

int Command::args_length(char **args){
    int i=0;
    while(args[i]!=NULL) i++;
    return i;
}

void ChPrompt::execute(){
    SmallShell& s = SmallShell::getInstance();
    char** args = this->make_args();
    s.ch_prompt(args[1]);
    this->free_args(args);
}

void GetCurrDirCommand::execute(){
    char ** args = this->make_args();
    this->free_args(args);
    char* path = getcwd(NULL, 0);
    write(1, path, strlen(path));
    write(1, "\n", 1);
    free(path);
}

void ExternalCommand::execute() { //will always be the son
    char** args = this->make_args();
    if(!isComplex(args)) { //should not have *,? and & because of make_args
        execvp(args[0], args); //should leave automatically
        perror("smash error: execvp failed"); // there is no command like that/no fitting flags
        this->free_args(args);
        exit(1);
    }
    else { //complex, using bash
        char* copy_cmd_line = strdup(this->get_cmd_line());
        _removeBackgroundSign(copy_cmd_line);
        char* bash_args[] = {(char*)("/bin/bash"),(char*)("-c"), copy_cmd_line, nullptr};
        execv("/bin/bash", bash_args); //shouldn't return
        perror("smash error: execv failed");
        this->free_args(args);
        exit(1);
    }
}

void JobsCommand::execute(){
    SmallShell &s = SmallShell::getInstance();
    s.Zakka();
    jobs->printJobsList();
}

void ShowPidCommand::execute() {
    const std::string smash_pid_str = "smash pid is " + std::to_string(getpid()) + "\n";
    write(1,smash_pid_str.c_str(),smash_pid_str.length());
}

void AliasCommand::execute(){
    regex pattern = regex("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
    std::cmatch parts;
    if(! std::regex_match(this->get_cmd_line(), parts, pattern)){
        if(_trim(string(this->get_cmd_line())).compare("alias") == 0){
            SmallShell &e = SmallShell::getInstance();
            e.PrintAliases();
            return;
        }
        else{
            const char* problem = "smash error: alias: invalid alias format\n";
            write(2, problem, strlen(problem));
            return;
        }
    }

    try{
        SmallShell& s = SmallShell::getInstance();
        s.addAlias(parts[1].str(), parts[2].str());
    }
    catch(std::invalid_argument& e){
        const char *problem1 = "smash error: alias: ", *problem2 = " already exists or is a reserved command\n";
        write(2, problem1, strlen(problem1));
        write(2, parts[1].str().c_str(), parts[1].str().length());
        write(2, problem2, strlen(problem2));
    }
}

void UnAliasCommand::execute() {
    char** args = this->make_args();
    if(args[1] == nullptr) {
        const char* problem = "smash error: unalias: not enough arguments\n";
        write(2, problem, strlen(problem));
        free_args(args);
        return;
    }
    for(int i = 1; args[i] != nullptr; i++) {
        try {
            SmallShell& s = SmallShell::getInstance();
            s.removeAlias(args[i]);
        }
        catch (std::invalid_argument& e) {
            const std::string problem = "smash error: unalias: " + std::string(args[i]) + " alias does not exist\n";
            write(2, problem.c_str(), problem.length());
            free_args(args);
            return;
        }
    }
    this->free_args(args);
}

void UnSetEnvCommand::execute(){
    char** args = this->make_args();
    if(args[1] == NULL){
        const char* problem = "smash error: unsetenv: not enough arguments\n";
        write(2, problem, strlen(problem));
        free_args(args);
        return;
    }
    string path = "/proc/p/environ";
    string pid = std::to_string(getpid());
    path.replace(6, 1, pid);
    vector<std::string> *allvars;
    try{
        allvars = this->ReadEnv(path);
    }
    catch(runtime_error& e){return;}
    for(int i = 1; args[i] != NULL; ++i){
        if(this->ExistsInEnv((string(args[i]) + "="), allvars)) DeleteVar(args[i]);
        else{
            const char *problem1 = "smash error: unsetenv: ", *problem2 = " does not exist\n";
            write(2, problem1, strlen(problem1));
            write(2, args[i], strlen(args[i]));
            write(2, problem2, strlen(problem2));
            delete(allvars);
            free_args(args);
            return;
        }
    }
    delete(allvars);
    free_args(args);
}

vector<std::string>* UnSetEnvCommand::ReadEnv(std::string path){
    int fd = open(path.c_str(), O_RDONLY);
    if(fd < 0) {
        const char* problem = "smash error: open failed";
        perror(problem);
        close(fd);
        throw std::runtime_error(problem);
    }
    int size = 1024, realsize = 0;
    char *buff = (char*) malloc(sizeof(char)*1024), *place = buff;
    while(1){
        int amount = read(fd, place, 1024);
        realsize += amount;
        if(amount < 1024) break;
        char* temp = (char*) realloc(buff, ((size+=1024)*sizeof(char)));
        if(temp == NULL){
                const char* problem = "smash error: realloc failed\n";
                perror(problem);
                free(buff);
                close(fd);
                throw runtime_error(problem);
            }
        buff = temp;
        place = buff + realsize;
    }
    char* temp = (char*) realloc(buff, (realsize)*sizeof(char));
    if(temp == NULL){
        const char* problem = "smash error: realloc failed\n";
        perror(problem);
        free(buff);
        close(fd);
        throw runtime_error(problem);
    }
    buff = temp;
    int amount_read = 0;
    const char* curr_place = buff;
    vector<std::string> *Env = new vector<std::string>;
    while(amount_read < realsize){
        Env->push_back(string(curr_place));
        amount_read += (strlen(curr_place) + 1);
        curr_place += (strlen(curr_place) + 1);
    }
    close(fd);
    free(buff);
    return Env;
}

// vector<std::string>* WhoAmICommand::ReadEnv(std::string path){
//     int fd = open(path.c_str(), O_RDONLY);
//     if(fd < 0) {
//         const char* problem = "smash error: open failed";
//         perror(problem);
//         close(fd);
//         throw std::runtime_error(problem);
//     }
//     int size = 1024, realsize = 0;
//     char *buff = (char*) malloc(sizeof(char)*1024), *place = buff;
//     while(1){
//         int amount = read(fd, place, 1024);
//         realsize += amount;
//         if(amount < 1024) break;
//         char* temp = (char*) realloc(buff, ((size+=1024)*sizeof(char)));
//         if(temp == NULL){
//             const char* problem = "smash error: realloc failed\n";
//             perror(problem);
//             free(buff);
//             close(fd);
//             throw runtime_error(problem);
//         }
//         buff = temp;
//         place = buff + realsize;
//     }
//     char* temp = (char*) realloc(buff, (realsize)*sizeof(char));
//     if(temp == NULL){
//         const char* problem = "smash error: realloc failed\n";
//         perror(problem);
//         free(buff);
//         close(fd);
//         throw runtime_error(problem);
//     }
//     buff = temp;
//     int amount_read = 0;
//     const char* curr_place = buff;
//     vector<std::string> *Env = new vector<std::string>;
//     while(amount_read < realsize){
//         Env->push_back(string(curr_place));
//         amount_read += (strlen(curr_place) + 1);
//         curr_place += (strlen(curr_place) + 1);
//     }
//     close(fd);
//     free(buff);
//     return Env;
// }

bool UnSetEnvCommand::ExistsInEnv(std::string arg, vector<std::string> *allvars){
    for(std::string s : *allvars) if(s.find(arg) == 0) return true;
    return false;
}

//arg surly exists in __environ. oterwise, undefined behavior may occur.
void UnSetEnvCommand::DeleteVar(const char* arg){
    extern char** __environ;
    char **curr_place = __environ;
    while(true){
        if(string(*curr_place).find(string(arg) + "=") == 0) {
            break;
        }
        curr_place++;
    }
    while(*curr_place != NULL) {
        char **temp = curr_place++;
        *temp = *curr_place;
    }
}

void QuitCommand::execute(){
    char **args = make_args();
    if(args[1] != NULL && strcmp("kill", args[1]) == 0){
        string mssg = "smash: sending SIGKILL signal to " 
             + std::to_string(jobs->GetSize()) + " jobs:\n";
        write(1, mssg.c_str(), mssg.length());
        jobs->killAllJobs();
    }
    exit(0);
}



void RedirectionCommand::execute(){
    SmallShell &s = SmallShell::getInstance();
    SmallShell::options option = this->op.compare(">") == 0 ?
        SmallShell::no_append : SmallShell::append;
    try{
        s.RedirectOut(this->path, option);
    }
    catch(runtime_error &r) {return;}
    s.executeCommand(this->get_cmd_line());
    s.RecoverIO();
}

void ChangeDirCommand::execute() {
    SmallShell &s = SmallShell::getInstance();
    char** args = this->make_args();
    if (args[1] == nullptr) {
        this->free_args(args);
        return;
    }
    if(args[2] != nullptr) {
        const std::string error_msg = "smash error: cd: too many arguments\n";
        write(2, error_msg.c_str(), error_msg.length());
        this->free_args(args);
        return;
    }
    const char* path = args[1];
    if(strcmp(path, "-") == 0) {
        path = s.GetPold();
        if(path == nullptr) {
            const std::string error_msg = "smash error: cd: OLDPWD not set\n";
            write(2, error_msg.c_str(), error_msg.length());
            this->free_args(args);
            return;
        }
    }
    char* current_dir = getcwd(NULL, 0);
    if(chdir(path) == -1) {
        perror("smash error: chdir failed");
        free(current_dir);
    }
    else {
        s.SetPold(current_dir);
    }
    this->free_args(args);
}

void SysInfoCommand::execute() {
    std::string keys[5] = {"System", "Hostname", "Kernel", "Architecture", "Boot Time"};
    std::string values[5];
    values[3] = "x86_64\n";
    std::string suffixes[3] = {"ostype","hostname","osrelease"};
    std::string kernel_path = "/proc/sys/kernel/";
    std::string paths[4];
    for(int i = 0; i < 3; i++) {
        paths[i] = kernel_path + suffixes[i];
    }
    paths[3] = "/proc/stat";
    for(int i = 0; i < 4; i++) {
        char buffer[4096];
        int fd = open((paths[i]).c_str(), O_RDONLY);
        if(fd == -1) {
            perror("smash error: open failed");
            return;
        }
        int bytes_read = read(fd, buffer, sizeof(buffer)-1);
        if(bytes_read == -1) {
            perror("smash error: read failed");
            close(fd);
            return;
        }
        buffer[bytes_read] = '\0';
        if(i != 3)
            values[i] = buffer;
        else {
            time_t boot_time = 0;
            char *btime_ptr = strstr(buffer, "btime");
            if (btime_ptr != nullptr) {
                sscanf(btime_ptr, "btime %ld", &boot_time);
            }
            if (boot_time > 0) {
                struct tm *time_info = localtime(&boot_time);
                char time_string[64];
                strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);
                values[4] = std::string(time_string) + '\n';
            }
        }
        close(fd);
    }
    std::string ret = "";
    for(int i = 0;i < 5;i++) {
        ret += (keys[i] + ": " + values[i]);
    }
    write(1,ret.c_str(), ret.length());


}

void DiskUsageCommand::execute(){
    char **args = this->make_args();
    if(args_length(args) > 2){
        const char *problem = "smash error: du: too many arguments\n";
        write(2, problem, strlen(problem));
        free_args(args);
        return;
    }
    char *p = getcwd(NULL, 0);
    std::string path = args[1] != NULL ? string(args[1]) : string(p);
    free(p);

    int weight = Rec(path.c_str()), rem = weight%1024;
    weight = (weight/1024) + (rem != 0 ? 1 : 0); 
    std::string ans = "Total disk usage: " + std::to_string(weight) + " KB\n";
    write(1, ans.c_str(), ans.length());
}

int DiskUsageCommand::Rec(const char* path){
    struct stat root_st;
    if (lstat(path, &root_st) == -1) {
        perror("smash error: lstat failed");
        throw runtime_error("lstat failed");
    }
    int fd = open(path, O_RDONLY | O_DIRECTORY), 
        sum = root_st.st_blocks * 512;
    if(fd < 0){
        const char *problem = "smash error: open failed";
        perror(problem);
        throw runtime_error("open failed");
    }
    std::vector<std::string> paths_vec;
    char buff[1024];
    int nread = 2;
    while(nread != 0){
        nread = syscall(SYS_getdents, fd, buff,  1024);
        for (int bpos = 0; bpos < nread;){
            struct linux_dirent *d = (struct linux_dirent *) (buff + bpos);
            struct stat st;
            string curr = string(path) + "/" + string(d->d_name);
            lstat(curr.c_str(), &st);
            char d_type = *(buff + bpos + d->d_reclen - 1); //donno, took from man page
            if(d_type != DT_DIR) sum += st.st_blocks * 512;
            if(d_type == DT_DIR && strcmp(d->d_name, ".") != 0 
                && strcmp(d->d_name, "..") != 0){
                    paths_vec.push_back(curr);
                }
            bpos += d->d_reclen;
        }
    }
    close(fd);
    for(std::string str : paths_vec)
        sum+=Rec(str.c_str());
    return sum;
}

PipeCommand::PipeCommand(const char *cmd_line, const char *cmd1, const char *op, const char *cmd2) : 
    Command(cmd_line)
{
    this->cmd1 = (char*) malloc(sizeof(char)*(strlen(cmd1)+1));
    strcpy(this->cmd1, cmd1);
    this->op = (char*) malloc(sizeof(char)*(strlen(op)+1));
    strcpy(this->op, op);
    this->cmd2 = (char*) malloc(sizeof(char)*(strlen(cmd2)+1));
    strcpy(this->cmd2, cmd2);
} 

PipeCommand::~PipeCommand(){
    free(cmd1); free(op); free(cmd2);
}

void PipeCommand::execute(){
    int fd = strcmp(this->op, "|") ? 1 : 2;
    SmallShell &s = SmallShell::getInstance();
    Command *left = s.CreateCommand(this->cmd1);
    Command *right = s.CreateCommand(this->cmd2);

    ExternalCommand *e_right = dynamic_cast<ExternalCommand*>(right);
    ExternalCommand *e_left = dynamic_cast<ExternalCommand*>(left);

    int pipe_fd[2];
    if(pipe(pipe_fd) == -1){
        const char *problem = "smash error: pipe failed\n";
        perror(problem);
        free(right); free(left);
        return;
    }

    if(e_right != nullptr && e_left != nullptr){
        int fork_left = fork();
        if(fork_left == -1){
            const char *problem = "smash error: fork failed\n";
            perror(problem);
            free(right); free(left);
            close(pipe_fd[0]); close(pipe_fd[1]);
            return;
        }
        else if(fork_left == 0){
            setpgrp();
            dup2(pipe_fd[1], fd);
            close(pipe_fd[0]); close(pipe_fd[1]);
            left->execute();
            exit(1);
        }
        int fork_right = fork();
        if(fork_right == -1){
            const char *problem = "smash error: fork failed\n";
            perror(problem);
            free(right); free(left);
            close(pipe_fd[0]); close(pipe_fd[1]);
            s.setPid(fork_left);
            kill(fork_left, 9);
            waitpid(fork_left, NULL, 0);
            s.setPid(-1);
            return;
        }
        if(fork_right == 0){
            setpgrp();
            dup2(pipe_fd[0], 0);
            close(pipe_fd[0]); close(pipe_fd[1]);
            right->execute();
            exit(1);
        }
        close(pipe_fd[0]); close(pipe_fd[1]);
        s.setPid(fork_left);
        waitpid(fork_left, NULL, 0);
        s.setPid(-1);
        s.setPid(fork_right);
        waitpid(fork_right, NULL, 0);
        s.setPid(-1);
    }

    else if(e_right != nullptr){
        int fork_right = fork();
        if(fork_right < 0){
            const char *problem = "smash error: fork failed\n";
            perror(problem);
            free(right); free(left);
            close(pipe_fd[0]); close(pipe_fd[1]);
            return;
        }
        else if(fork_right == 0){
            setpgrp();
            dup2(pipe_fd[0], 0);
            close(pipe_fd[0]); close(pipe_fd[1]);
            right->execute();
        }
        else{
            try{
                s.PipeOut(pipe_fd[1], SmallShell::pipe_out(fd));
            }
            catch(runtime_error &e){
                free(right); free(left);
                close(pipe_fd[0]); close(pipe_fd[1]);
                s.setPid(fork_right);
                kill(fork_right, 9);
                waitpid(fork_right, NULL, 0);
                s.setPid(-1);
                return;
            }
            close(pipe_fd[0]); close(pipe_fd[1]);
            left->execute();
            s.RecoverIO();
            s.setPid(fork_right);
            waitpid(fork_right, NULL, 0);
            s.setPid(-1);
        }
    }
    
    else if(e_left != nullptr){
        int fork_left = fork();
        if(fork_left < 0){
            const char *problem = "smash error: fork failed\n";
            perror(problem);
            free(right); free(left);
            close(pipe_fd[0]); close(pipe_fd[1]);
            return;
        }
        else if(fork_left == 0){
            setpgrp();
            dup2(pipe_fd[1], fd);
            close(pipe_fd[0]); close(pipe_fd[1]);
            left->execute();
        }
        else{
            try{
                s.PipeIn(pipe_fd[0]);
            }
            catch(runtime_error &e){
                free(right); free(left);
                close(pipe_fd[0]); close(pipe_fd[1]);
                s.setPid(fork_left);
                kill(fork_left, 9); waitpid(fork_left, NULL, 0);
                s.setPid(-1);
                return;
            }
            close(pipe_fd[0]); close(pipe_fd[1]);
            right->execute();
            s.RecoverIO();
            s.setPid(fork_left);
            waitpid(fork_left, NULL, 0);
            s.setPid(-1);
        }
    }
     
    //not piping for built-in commands. risk of deadlock (isn't there a video game with taht name?)
    else{
        close(pipe_fd[0]); close(pipe_fd[1]);
        left->execute();
        right->execute();
    }

    free(right); free(left);
}

bool isDigit(const char c) {
    return c >= '0' && c <= '9';
}

int parseNum(const char* numString, int size) { //assuming natural numbers (including zero)
    int ret = 0;
    for(int i = 0; i < size; i++) {
        if(!isDigit(numString[i])) {
            if(numString[i] == '\0') {
                return ret;
            }
            return -1; //if reached then numString[i] isn't a digit and isn't end of string
        }
        ret *= 10;
        ret += numString[i] - '0';
    }
    return ret;
}

void ForegroundCommand::execute() {
    char** args = this->make_args();
    int id;
    if(args[2] != nullptr) { //more than one argument given
        const char *problem = "smash error: fg: invalid arguments\n";
        write(2, problem, strlen(problem));
        this->free_args(args);
        return;
        // throw runtime_error(problem);
    }
    if (args[1] == nullptr) { //no job id specified
        if(this->jobs->GetSize() == 0) {
            const char *problem = "smash error: fg: jobs list is empty\n";
            write(2, problem, strlen(problem));
            this->free_args(args);
            return;
            // throw runtime_error(problem);
        }
        id = this->jobs->jobs.back().get_id(); //we need the last actual job
    }
    else {
        //const char* arg = args[1]; //should be an "int" like "87"
        id = parseNum(args[1], strlen(args[1]));
        if(id == -1) {
            const string problem_str = "smash error: fg: invalid arguments\n";
            write(2, problem_str.c_str(), strlen(problem_str.c_str()));
            this->free_args(args);
            return;
        }
        if (this->jobs->job_ids.find(id) == this->jobs->job_ids.end()) {
            const string problem_str = "smash error: fg: job-id " + to_string(id) +" does not exist\n";
            write(2, problem_str.c_str(), strlen(problem_str.c_str()));
            this->free_args(args);
            return;
        }
    }
    //if reached, id holds the correct JobId to bring forward
    JobsList::JobEntry* job_entry = this->jobs->getJobById(id);
    int job_pid = job_entry->get_pid();
    const string print_str = std::string(job_entry->get_cmd()->get_cmd_line()) + " " + to_string(job_pid) + "\n";
    write(1, print_str.c_str(), strlen(print_str.c_str()));
    SmallShell& smash = SmallShell::getInstance();
    smash.setPid(job_pid);
    kill(job_pid, SIGCONT); //will be created along with the kill smash command
    waitpid(job_pid, NULL, 0);
    this->jobs->RemoveJobByPid(job_pid);
    smash.setPid(-1);
    this->free_args(args);

}

void KillCommand::execute() {
    char** args = this->make_args();
    if(args[1] == nullptr || args[2] == nullptr || args[3] != nullptr || args[1][0] != '-') {
        const char *problem = "smash error: kill: invalid arguments\n";
        write(2, problem, strlen(problem));
        this->free_args(args);
        return;
    }
    const int signum = parseNum(args[1]+1, strlen(args[1] + 1)); //args[1][0] should be '-'
    if(signum == -1) { //not a number
        const string problem_str = "smash error: kill: invalid arguments\n";
        write(2, problem_str.c_str(), strlen(problem_str.c_str()));
        this->free_args(args);
        return;
    }
    int id = parseNum(args[2], strlen(args[2]));
    if(id == -1) { //not a number
        const string problem_str = "smash error: kill: invalid arguments\n";
        write(2, problem_str.c_str(), strlen(problem_str.c_str()));
        this->free_args(args);
        return;
    }
    if (this->jobs->job_ids.find(id) == this->jobs->job_ids.end()) {
        const string problem_str = "smash error: kill: job-id " + to_string(id) +" does not exist\n";
        write(2, problem_str.c_str(), strlen(problem_str.c_str()));
        this->free_args(args);
        return;
    }
    JobsList::JobEntry* job_entry = this->jobs->getJobById(id); //if reached then id is a valid jobId
    int job_pid = job_entry->get_pid();
    const string message = "signal number " + to_string(signum) + " was sent to pid " + to_string(job_pid) + "\n";
    if(kill(job_pid, signum) == -1) {
        perror("smash error: kill failed");
    }
    else
        write(1, message.c_str(), strlen(message.c_str()));

    this->free_args(args);
}

void WhoAmICommand::execute() {
    const int uid = getuid();
    const int gid = getgid();
    string path = "/proc/p/environ";
    string pid = std::to_string(getpid());
    path.replace(6, 1, pid);
    vector<std::string> *allvars;
    try{
        allvars = UnSetEnvCommand::ReadEnv(path);
    }
    catch(runtime_error& e) {

        return;
    }
    std::string username;
    std::string home_dir;

    for(const std::string& current_entry : *allvars) {
        if (current_entry.rfind("USER=", 0) == 0) {
            username = current_entry.substr(5);
        }
        else if (current_entry.rfind("HOME=", 0) == 0) {
            home_dir = current_entry.substr(5);
        }
    }
    const std::string message =username + "\n" + to_string(uid) + "\n" + to_string(gid) + "\n" + home_dir + "\n";
    write(1, message.c_str(), strlen(message.c_str()));
    delete(allvars);
}

std::string USBInfoCommand::ReadUsbProperty(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if(fd == -1) {
        // perror("smash error: open failed"); it is okay for open to fail if the file doesnt exist, we don't want to crash
        return "-1";
    }
    char buffer[1024];
    int bytes_read = read(fd, buffer, sizeof(buffer)-1);
    if(bytes_read == -1) {
        perror("smash error: read failed");
        close(fd);
        return "-1";
    }
    buffer[bytes_read] = '\0';
    if (buffer[bytes_read - 1] == '\n') //files ends with \n but we need them to be in the same line
        buffer[bytes_read - 1] = '\0';

    return std::string(buffer);
}

std::string USBInfoCommand::GetUsbProperties(const std::string& bus_port) { //bus port is from the shape [0-9]*-[0-9]*
    const int PROPERTIES_NUM = 6;
    std::string suffixes[PROPERTIES_NUM] = {"devnum", "idVendor", "idProduct", "manufacturer", "product", "bMaxPower"};
    std::string path = "/sys/bus/usb/devices/"; //found from tutorial 4 page 24 in the sysfs man page
    std::string properties[6];
    for(int i = 0; i < PROPERTIES_NUM; i++) {
        std::string property = ReadUsbProperty(path + bus_port + "/" + suffixes[i]);
        if(i <= 2 && property == "-1") { //those cannot be unknown if it is a USB
            return "-1";
        }
        if(property == "-1") { //i >= 3
            property[i] = *"N/A";
        }
        properties[i] = property;
    }
    std::string ret = "Device " + properties[0] + ": ID " + properties[1] + ":" + properties[2];
    ret += " " + properties[3] + " " + properties[4] + " MaxPower: " + properties[5] + "\n";
    return ret;

}

void USBInfoCommand::execute() {
    const char* path = "/sys/bus/usb/devices";
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if(fd < 0){
        const char *problem = "smash error: open failed";
        perror(problem);
        throw runtime_error("open failed");
    }
    std::vector<std::string> paths_vec;
    char buff[1024];
    int nread = 1;
    std::string output = "";
    while(nread != 0){
        nread = syscall(SYS_getdents, fd, buff,  1024);
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent *d = (struct linux_dirent *) (buff + bpos);
            if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
                std::string check_vendor_file = std::string(path) + "/" + std::string(d->d_name) + "/idVendor";
                int fd_check = open(check_vendor_file.c_str(), O_RDONLY);
                if(fd_check != -1) { //if file doesn't have idVendor it is not a USB device
                    close(fd_check); //check ended
                    std::string device_info = GetUsbProperties(std::string(d->d_name));
                    if(device_info != "-1") { //read properties correctly
                        output += device_info;
                    }
                }
            }
            bpos += d->d_reclen;
        }
    }
    close(fd);
    if(output == "") {
        std::string message = "smash error: usbinfo: no USB devices found\n";
        write(2, message.c_str(), strlen(message.c_str()));
        return;
    }
    write(1, output.c_str(), strlen(output.c_str()));
}

