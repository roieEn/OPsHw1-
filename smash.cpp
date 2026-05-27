#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }


    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        write(1, smash.get_prompt(), strlen(smash.get_prompt()));
        write(1, "> ", 2);
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}