#include "LogAnalyzer.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstring>

// flag set by signal handler
volatile sig_atomic_t print_report_flag = 0;

void signal_handler(int sig) {
    if (sig == SIGUSR1)
        print_report_flag = 1;
}

void printHelp() {
    std::cout << "Usage: ./log_analyzer [options] <logfile>\n"
              << "Options:\n"
              << "  -h, --help            Show this help\n"
              << "  -v, --verbose         Enable verbose output\n"
              << "  --csv <file>          Export report to CSV\n"
              << "  --repeat-threshold N  Set repeat detection threshold (default=5)\n"
              << "  --monitor             Real-time monitoring\n";
}

int main(int argc, char* argv[]) {
    std::string logfile;
    bool verbose = false;
    std::string csvFile;
    int repeatThreshold = 5;
    bool monitorMode = false;

    // parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--csv" && i+1 < argc) {
            csvFile = argv[++i];
        } else if (arg == "--repeat-threshold" && i+1 < argc) {
            repeatThreshold = std::stoi(argv[++i]);
        } else if (arg == "--monitor") {
            monitorMode = true;
        } else if (logfile.empty()) {
            logfile = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printHelp();
            return 1;
        }
    }

    if (logfile.empty()) {
        std::cerr << "Error: No log file specified.\n";
        printHelp();
        return 1;
    }

    if (monitorMode) {
        // setup signal handler
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &sa, nullptr);

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return 1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            // child: run tail -F and send output to pipe
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            execlp("tail", "tail", "-F", logfile.c_str(), nullptr);
            perror("execlp");
            return 1;
        } else {
            // parent: read from pipe
            close(pipefd[1]);
            FILE* pipeRead = fdopen(pipefd[0], "r");
            if (!pipeRead) {
                perror("fdopen");
                return 1;
            }

            LogAnalyzer analyzer("(real-time)", verbose, repeatThreshold);

            char* line = nullptr;
            size_t len = 0;
            ssize_t nread;

            std::cout << "Monitoring " << logfile
                      << " (kill -USR1 " << getpid()
                      << " to print report)\n";

            while (true) {
                nread = getline(&line, &len, pipeRead);

                // check signal flag
                if (print_report_flag) {
                    analyzer.printReport();
                    print_report_flag = 0;
                }

                if (nread == -1) break;

                if (nread > 0 && line[nread - 1] == '\n')
                    line[nread - 1] = '\0';

                analyzer.parseLineFromString(line);
            }

            free(line);
            fclose(pipeRead);
            wait(nullptr);
        }

    } else {
        // batch mode
        LogAnalyzer analyzer(logfile, verbose, repeatThreshold);
        if (!analyzer.parse())
            return 1;

        analyzer.printReport();

        if (!csvFile.empty())
            analyzer.exportCSV(csvFile);
    }

    return 0;
}
