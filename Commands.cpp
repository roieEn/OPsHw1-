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
    in_recover = -1;
    out_recover = -1;
    jobs = new JobsList();
}

SmallShell::~SmallShell() {
    free(og_name);
    free(curr_name);
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

void SmallShell::RedirectIn(std::string in_path){
    //TODO
}

void SmallShell::removeAlias(const std::string& alias) {
    if(!this->isAliasTaken(alias)) {
        throw std::invalid_argument("no such alias in use");
    }
    aliases.erase(alias);
}



void SmallShell::RecoverIO(){
    if(in_recover != -1){
        dup2(in_recover, 0);
        close(in_recover);
    }
    if(out_recover != -1){
        dup2(out_recover, 1);
        close(out_recover);
    }
}

void *SmallShell::AddToJobList(Command* cmd, bool isStopped) {
    this->jobs->addJob(cmd, isStopped);
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    bool bg = false;
    std::string cmd = cmd_line;
    if(_isBackgroundComamnd(cmd_line)) {
        bg = true;
    }

    //check for alias and replace if found
    try{
        string alias = aliases.at(firstWord);
        cmd_s.replace(0, cmd_s.find_first_of(" \n"), alias);
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
        cmd_line = cmd_s.c_str();
    }
    catch(std::out_of_range& e){}

    //redirection command
    std::regex redidect_pattern = regex("^(.*?)\\s*(>>|>)\\s*([a-z0-9./_-]+)\\s*&?\\s*$");
    cmatch redirect_parts;
    if(regex_match(cmd_s.c_str(), redirect_parts, redidect_pattern)){
        return new RedirectionCommand(string(redirect_parts[1]).c_str(),
            string(redirect_parts[3]), string(redirect_parts[2]));
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
    else if(firstWord.compare("alias") == 0){
        return new AliasCommand(cmd_line);
    }
    else if(firstWord.compare("unalias") == 0){
        return new UnAliasCommand(cmd_line);
    }
    else if(firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }
    else if(firstWord.compare("jobs") == 0){
        return new JobsCommand(cmd_line, this->jobs);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, nullptr);
    }
    else {
        if(bg) {
            return new ExternalCommand(cmd_line, true);
        }
        else {
            return new ExternalCommand(cmd_line, false);
        }
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    delete cmd;
    if(ExternalCommand* extCmd = dynamic_cast<ExternalCommand*>(cmd)) {//if succeeds then cmd is external
        const pid_t p = fork();
        if(p > 0) { //parent
            if(!extCmd->is_bg) {
                wait(NULL);
            }
            else {
                  this->AddToJobList(extCmd, false); //not sure what isStopped should be, when is it ever true and we want to add it?
            }
        }
        else { //child
            setpgrp();
            cmd->execute();
        }
    }
    else
        cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}



void JobsList::addJob(Command* cmd, bool isStopped){
    int id = jobs.size() == 0 ? 1 : jobs.front().get_id(); //should be +1 ?
    jobs.push_back(JobEntry(cmd, id));
}

//doesn't delete finished jobs. will need to add this feature after doing background jobs.
void JobsList::printJobsList(){
    for(JobEntry j : jobs)
        std::cout << "[" << j.get_id() << "] " << j.get_cmd()->get_cmd_line() <<std::endl;
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
    _parseCommandLine(copy_cmd_line, args);
    return args;
}

void Command::free_args(char** args){
    for(int i = 0; args[i] != NULL; i++)
        free(args[i]);
    free(args);
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
    }
    else { //complex, using bash
        char* copy_cmd_line = strdup(this->get_cmd_line());
        _removeBackgroundSign(copy_cmd_line);
        char* bash_args[] = {(char*)("/bin/bash"),(char*)("-c"), copy_cmd_line, nullptr};
        execv("/bin/bash", bash_args); //shouldn't return
        perror("smash error: execv failed");
    }
}


void JobsCommand::execute(){
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
        }
    }
    delete(allvars);
    free_args(args);
}

vector<std::string>* UnSetEnvCommand::ReadEnv(std::string path){
    int fd = open(path.c_str(), O_RDONLY);
    if(fd < 0) {
        const char* problem = "smash error: open failed";
        write(2, problem, strlen(problem));
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
                write(2, problem, strlen(problem));
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
        write(2, problem, strlen(problem));
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

bool UnSetEnvCommand::ExistsInEnv(std::string arg, vector<std::string> *allvars){
    for(std::string s : *allvars) if(s.find(arg) == 0) return true;
    return false;
}

//arg surly exists in __environ. oterwise, undefined behavior may occur.
void UnSetEnvCommand::DeleteVar(const char* arg){
    extern char** __environ;
    char **curr_place = __environ;
    std::cout << "before" << std::endl;
    {
        for(char **temp = __environ; *temp != NULL; temp++) std::cout << *temp << endl;
    }
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
    std::cout << "after" << std::endl;
    {
        for(char **temp = __environ; *temp != NULL; temp++) std::cout << *temp << endl;
    }
}

void RedirectionCommand::execute(){
    SmallShell &s = SmallShell::getInstance();
    SmallShell::options option = this->op.compare(">") == 0 ?
        SmallShell::no_append : SmallShell::append;
    s.RedirectOut(this->path, option);
    Command *cmd = s.CreateCommand(this->get_cmd_line());
    cmd->execute();
    delete cmd;
    s.RecoverIO();
}

void ChangeDirCommand::execute() {
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
    char* path = args[1];
    if(strcmp(path, "-") == 0) {
        path = *this->pold_dir;
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
        if(*this->pold_dir != nullptr) {
            free(*this->pold_dir);
        }
        *this->pold_dir = current_dir;
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
