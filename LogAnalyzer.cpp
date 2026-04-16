#include "LogAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <map>
#include <numeric>

// Constructor initializes configuration values
LogAnalyzer::LogAnalyzer(const std::string& filename, bool verbose, int repeatThreshold)
    : filename(filename), verbose(verbose), repeatThreshold(repeatThreshold) {}

// Parses entire log file (batch mode)
bool LogAnalyzer::parse() {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return false;
    }

    std::string line;
    int lineNum = 0;

    // Read file line by line and parse each entry
    while (std::getline(file, line)) {
        lineNum++;
        LogEntry entry;
        if (parseLine(line, entry)) {
            entries.push_back(entry);
            if (verbose) {
                std::cout << "[Parsed] " << line << std::endl;
            }
        } else if (verbose) {
            std::cerr << "Warning: Could not parse line " << lineNum << ": " << line << std::endl;
        }
    }

    file.close();
    return true;
}

// Parses a single line from real-time input (monitor mode)
bool LogAnalyzer::parseLineFromString(const std::string& line) {
    LogEntry entry;
    if (parseLine(line, entry)) {
        entries.push_back(entry);
        if (verbose) {
            std::cout << "[Parsed] " << line << std::endl;
        }
        return true;
    } else if (verbose) {
        std::cerr << "Warning: Could not parse line: " << line << std::endl;
    }
    return false;
}

// Extracts structured data from a log line using regex
bool LogAnalyzer::parseLine(const std::string& line, LogEntry& entry) {
    std::regex syslogRegex(R"(^(\w+\s+\d+\s+\d+:\d+:\d+)\s+\S+\s+([^:]+):\s*(.*)$)");
    std::smatch match;

    if (!std::regex_search(line, match, syslogRegex)) {
        return false;
    }

    // Parse timestamp (assumes current year)
    std::string timeStr = match[1].str();
    entry.timestamp = parseTimestamp(timeStr);
    if (entry.timestamp == -1) return false;

    // Extract severity, process name, and message
    std::string rest = match[2].str() + ": " + match[3].str();
    entry.severity = extractSeverity(rest);
    entry.process = match[2].str();
    entry.message = match[3].str();

    return true;
}

// Determines severity level using keyword matching
std::string LogAnalyzer::extractSeverity(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("error") != std::string::npos) return "ERROR";
    if (lower.find("warning") != std::string::npos) return "WARNING";
    if (lower.find("info") != std::string::npos) return "INFO";

    return "OTHER";
}

// Converts timestamp string into time_t (uses current year)
std::time_t LogAnalyzer::parseTimestamp(const std::string& token) {
    std::tm tm = {};
    std::istringstream ss(token);
    ss >> std::get_time(&tm, "%b %d %H:%M:%S");

    if (ss.fail()) return -1;

    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);

    tm.tm_year = now_tm->tm_year;
    tm.tm_isdst = -1;

    return std::mktime(&tm);
}

// Counts occurrences of each severity type
void LogAnalyzer::computeStatistics(size_t& total, size_t& errors, size_t& warnings,
                                    size_t& info, size_t& other) const {
    total = entries.size();
    errors = warnings = info = other = 0;

    for (const auto& e : entries) {
        if (e.severity == "ERROR") errors++;
        else if (e.severity == "WARNING") warnings++;
        else if (e.severity == "INFO") info++;
        else other++;
    }
}

// Detects hours with unusually high error counts (mean + 2*stddev)
void LogAnalyzer::detectSpikes(std::map<std::string, size_t>& hourlyErrors) const {
    // Group errors by hour
    for (const auto& e : entries) {
        if (e.severity != "ERROR") continue;

        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H", std::localtime(&e.timestamp));
        hourlyErrors[buffer]++;
    }

    if (hourlyErrors.empty()) return;

    // Compute mean and standard deviation
    std::vector<size_t> counts;
    for (const auto& p : hourlyErrors) counts.push_back(p.second);

    double sum = std::accumulate(counts.begin(), counts.end(), 0.0);
    double mean = sum / counts.size();

    double sq_sum = std::inner_product(counts.begin(), counts.end(), counts.begin(), 0.0);
    double stddev = std::sqrt(sq_sum / counts.size() - mean * mean);

    // Report anomalies
    bool spikeFound = false;
    for (const auto& p : hourlyErrors) {
        if (p.second > mean + 2 * stddev) {
            if (!spikeFound) {
                std::cout << "\n--- Spike Anomalies ---\n";
                spikeFound = true;
            }
            std::cout << "  Hour " << p.first << " had " << p.second
                      << " errors (mean=" << std::fixed << std::setprecision(1)
                      << mean << ", stddev=" << stddev << ")\n";
        }
    }
}

// Detects messages that repeat frequently (above threshold)
void LogAnalyzer::detectRepeatedMessages() const {
    std::map<std::string, int> msgCount;

    for (const auto& e : entries) {
        if (e.severity == "ERROR" || e.severity == "WARNING") {
            msgCount[e.message]++;
        }
    }

    bool repeatedFound = false;
    for (const auto& p : msgCount) {
        if (p.second >= repeatThreshold) {
            if (!repeatedFound) {
                std::cout << "\n--- Repeated Messages ---\n";
                repeatedFound = true;
            }
            std::cout << "  \"" << p.first << "\" appears " << p.second << " times\n";
        }
    }
}

// Prints summary report and detected anomalies
void LogAnalyzer::printReport() const {
    size_t total, errors, warnings, info, other;
    computeStatistics(total, errors, warnings, info, other);

    std::cout << "\n=== Log Analysis Report ===\n";
    std::cout << "File: " << filename << "\n";
    std::cout << "Total lines parsed: " << total << "\n";
    std::cout << "Errors:   " << errors << " (" << (100.0 * errors / total) << "%)\n";
    std::cout << "Warnings: " << warnings << " (" << (100.0 * warnings / total) << "%)\n";
    std::cout << "Info:     " << info << " (" << (100.0 * info / total) << "%)\n";
    std::cout << "Other:    " << other << " (" << (100.0 * other / total) << "%)\n";

    std::map<std::string, size_t> hourlyErrors;
    detectSpikes(hourlyErrors);
    detectRepeatedMessages();
}

// Exports parsed log entries to CSV format
void LogAnalyzer::exportCSV(const std::string& csvFile) const {
    std::ofstream out(csvFile);
    if (!out) {
        std::cerr << "Error: Could not create CSV file " << csvFile << std::endl;
        return;
    }

    out << "Timestamp,Severity,Process,Message\n";

    for (const auto& e : entries) {
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&e.timestamp));

        out << "\"" << buffer << "\","
            << e.severity << ","
            << "\"" << e.process << "\","
            << "\"" << e.message << "\"\n";
    }

    out.close();
    std::cout << "CSV report exported to " << csvFile << std::endl;
}
