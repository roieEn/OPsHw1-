#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <regex>
#include "Commands.h"
#include <fcntl.h>

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

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() {
    og_name = (char*) malloc(string("smash").length() + 1);
    strcpy(og_name, string("smash").c_str());
    curr_name = (char*) malloc(string(og_name).length() + 1);
    strcpy(curr_name, og_name);
}

SmallShell::~SmallShell() {
    free(og_name);
    free(curr_name);
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

void SmallShell::addAlias(const std::string alias, const string arg){
    if(this->isAliasTaken(alias)) throw std::invalid_argument("alias already in use");
    aliases[alias] = arg;
}

const string SmallShell::get_alias(string word){
    try{
        return aliases.at(word);
    }
    catch(std::out_of_range& e){
        return "";
    }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
    
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    try{
        firstWord = aliases.at(firstWord);
        firstWord = firstWord.substr(0, firstWord.find_first_of(" \n"));
    }
    catch(std::out_of_range& e){}

    if(firstWord.compare("chprompt") == 0){
        return new ChPrompt(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if(firstWord.compare("alias") == 0){
        return new AliasCommand(cmd_line);
    }
    else if(firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }
    /*
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if ...
    .....
    else {
      return new ExternalCommand(cmd_line);
    }
    */
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}






Command::Command(const char* cmd_line){
    this->cmd_line = (char*) malloc(sizeof(char)*(string(cmd_line).length()+1));
    strcpy(this->cmd_line, cmd_line);
}

Command::~Command(){
    free(this->cmd_line); 
}

char** Command::make_args(){
    const char* line;
    std::string cmd_s, alias = _trim(string(this->cmd_line));
    alias = alias.substr(0, alias.find_first_of(" \n"));
    SmallShell& s = SmallShell::getInstance();
    alias = s.get_alias(alias);
    if(alias == "") line = this->cmd_line;
    else{
        cmd_s = _trim(string(cmd_line));
        cmd_s.replace(0, cmd_s.find_first_of(" \n"), alias);
        line = cmd_s.c_str();
    }
    char** args = (char**) malloc(sizeof(char*) * 20);
    _parseCommandLine(line, args);
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
    char* path = getcwd(NULL, 0);
    write(1, path, strlen(path));
    write(1, "\n", 1);
    free(path);
}

void AliasCommand::execute(){
    regex pattern = regex("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
    std::cmatch parts;
    if(! std::regex_match(this->get_cmd_line(), parts, pattern)){
        const char* problem = "smash error: alias: invalid alias format\n";
        write(2, problem, strlen(problem));
        return;
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
