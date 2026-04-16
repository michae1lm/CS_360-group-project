# CS_360-group-project
# Log Analysis Tool for Ubuntu System Logs

## Project Overview & Goals

This tool analyzes system log files (e.g., `/var/log/syslog`) to detect anomalies such as error spikes and repeated warning messages. It supports **batch analysis** of existing log files and **real‑time monitoring** of live logs. The goal is to demonstrate core systems programming concepts from CPTS 360 while producing a useful utility for system administrators.

## Four Course Themes Used

1. **File I/O**  
   - Reading log files line by line with `std::ifstream` (batch mode).  
   - Writing CSV reports using `std::ofstream`.  
   - In monitor mode, the child process uses `tail -F` which continuously reads the file.

2. **Process Management**  
   - `fork()` creates a child process for real‑time monitoring.  
   - The child executes `tail` via `execlp()` – a classic fork‑exec pattern.  
   - The parent waits for the child with `wait()` on shutdown.

3. **Signals**  
   - `SIGUSR1` is handled by the parent to print an interim report without stopping.  
   - `SIGINT` (Ctrl+C) gracefully terminates the monitor mode.  
   - Signal handler uses a `volatile sig_atomic_t` flag for safe communication.

4. **Inter‑process Communication (Pipes)**  
   - An anonymous pipe connects the child’s `stdout` (from `tail`) to the parent’s input.  
   - The parent reads lines from the pipe using `getline()`.  
   - This demonstrates half‑duplex IPC with no temporary files.

## Design Decisions and Trade‑offs

- **Why C++ instead of C?**  
  C++ `std::string` and `std::vector` simplify dynamic memory management and reduce manual errors. However, we still use POSIX system calls (`fork`, `pipe`, `sigaction`) to stay close to the OS.

- **Why regex for parsing?**  
  Regular expressions make the code concise and resilient to small format variations. The trade‑off is performance – for very large log files (100k+ lines), a hand‑tuned parser might be faster. Since typical syslog files are moderate, clarity wins.

- **Why `tail -F` instead of re‑implementing file following?**  
  Re‑inventing file rotation detection (`-F` follows file across truncation/renaming) is complex and error‑prone. Using an external standard tool is a pragmatic design choice that demonstrates process execution and IPC.

- **Spike detection using mean + 2σ**  
  This simple statistical method works well for periodic logs (e.g., hourly). A more sophisticated approach (e.g., time‑series forecasting) would increase complexity without proportional benefit for the project scope.

- **No GUI**  
  A terminal interface keeps the tool lightweight and scriptable. The `--csv` option allows integration with external visualization tools.

## Challenges Encountered and Lessons Learned

- **Challenge 1: Real‑time parsing without blocking**  
  Initially we tried reading the pipe in a busy loop, which consumed 100% CPU. Switching to `getline()` which blocks naturally solved this.

- **Challenge 2: Signal safety**  
  Printing directly from the signal handler caused deadlocks. We learned to set only a flag and handle the action in the main loop.

- **Challenge 3: Handling log rotation**  
  The `tail -F` command (note uppercase `-F`) correctly follows a file even if it’s rotated, but our parser would miss lines written during the rotation. Adding a small buffer and re‑opening logic would solve this – left as future work.

- **Lesson learned:** Modular design pays off – adding monitor mode required only one new method (`parseLineFromString`) and changes to `main.cpp`. The core parsing logic remained untouched.

## Building and Running

```bash
make
./log_analyzer /var/log/syslog              # batch mode
./log_analyzer --monitor /var/log/syslog    # real-time monitoring
# Send SIGUSR1 to the parent PID to print a report
kill -USR1 $(pgrep log_analyzer)
