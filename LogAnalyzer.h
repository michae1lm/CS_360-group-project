#ifndef LOGANALYZER_H
#define LOGANALYZER_H

#include <string>
#include <vector>
#include <map>
#include <ctime>

struct LogEntry {
    std::time_t timestamp;
    std::string severity;
    std::string process;
    std::string message;
};

class LogAnalyzer {
public:
    LogAnalyzer(const std::string& filename, bool verbose, int repeatThreshold);
    bool parse();
    bool parseLineFromString(const std::string& line);
    void printReport() const;
    void exportCSV(const std::string& csvFile) const;

private:
    std::string filename;
    bool verbose;
    int repeatThreshold;
    std::vector<LogEntry> entries;

    bool parseLine(const std::string& line, LogEntry& entry);
    std::string extractSeverity(const std::string& line);
    std::time_t parseTimestamp(const std::string& token);
    void computeStatistics(size_t& total, size_t& errors, size_t& warnings,
                           size_t& info, size_t& other) const;
    void detectSpikes(std::map<std::string, size_t>& hourlyErrors) const;
    void detectRepeatedMessages() const;
};

#endif
