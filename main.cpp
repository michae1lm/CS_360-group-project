#include "LogAnalyzer.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstring>

// Flag set by signal handler to safely trigger report printing
volatile sig_atomic_t print_report_flag = 0;

// Signal handler: only sets a flag
void signal_handler(int sig) {
    if (sig == SIGUSR1)
        print_report_flag = 1;
}

// Prints usage information
void printHelp() {
    std::cout << "Usage: ./log_analyzer [options] <logfile>\n"
              << "Options:\n"
              << "  -h, --help            Show this help\n"
              << "  -v, --verbose         Enable verbose output\n"
              << "  --csv <file>          Export report to CSV\n"
              << "  --repeat-threshold N  Set repeat detection threshold (default=5)\n"
              << "  --monitor             Real-time monitoring (forks child, uses signals & pipe)\n";
}

int main(int argc, char* argv[]) {
    std::string logfile;
    bool verbose = false;
    std::string csvFile;
    int repeatThreshold = 5;
    bool monitorMode = false;

    // Parse command-line arguments
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
        // Set up SIGUSR1 handler
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &sa, nullptr);

        // Create pipe for IPC between parent and child
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
            // Child: redirect stdout to pipe and run tail -F
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                return 1;
            }
            close(pipefd[1]);
            execlp("tail", "tail", "-F", logfile.c_str(), nullptr);
            perror("execlp");
            return 1;
        } else {
            // Parent: read from pipe and process lines
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
                      << ". Send SIGUSR1 (kill -USR1 " << getpid()
                      << ") to print report. Press Ctrl+C to exit.\n";

            // Read incoming log lines continuously
            while ((nread = getline(&line, &len, pipeRead)) != -1) {
                if (line[nread-1] == '\n') line[nread-1] = '\0';
                analyzer.parseLineFromString(line);

                // Print report when signal is received
                if (print_report_flag) {
                    analyzer.printReport();
                    print_report_flag = 0;
                }
            }

            free(line);
            fclose(pipeRead);
            wait(nullptr);
        }
    } else {
        // Batch mode: process entire file at once
        LogAnalyzer analyzer(logfile, verbose, repeatThreshold);
        if (!analyzer.parse()) {
            return 1;
        }
        analyzer.printReport();

        if (!csvFile.empty()) {
            analyzer.exportCSV(csvFile);
        }
    }

    return 0;
}
