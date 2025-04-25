#include <sched.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

struct ThreadInfo {
    pid_t tid;
    double cpu_time_seconds;
};

std::vector<ThreadInfo> get_thread_times_exclude_main() {
    std::vector<ThreadInfo> thread_times;
    DIR* task_dir = opendir("/proc/self/task");
    if (!task_dir) return thread_times;

    const pid_t main_tid = getpid();
    struct dirent* entry;
    
    while ((entry = readdir(task_dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;  // Fixed string comparison

        const pid_t tid = atoi(entry->d_name);
        if (tid == main_tid) continue;

        std::string stat_path = "/proc/self/task/" + std::to_string(tid) + "/stat";
        std::ifstream stat_file(stat_path);
        if (!stat_file) continue;

        std::string line;
        std::getline(stat_file, line);
        std::istringstream iss(line);

        std::string token;
        for (int i = 1; i <= 13; ++i) iss >> token;
        unsigned long utime, stime;
        iss >> utime >> stime;

        const double clock_ticks = sysconf(_SC_CLK_TCK);
        thread_times.push_back({tid, (utime + stime) / clock_ticks});
    }
    closedir(task_dir);

    // Explicit type in lambda parameters
    std::sort(thread_times.begin(), thread_times.end(),
        [](const ThreadInfo& a, const ThreadInfo& b) {
            return a.cpu_time_seconds > b.cpu_time_seconds;
        });

    return thread_times;
}

void set_affinity(int main_core, int worker_core) {
    // Set main thread affinity
    cpu_set_t main_set;
    CPU_ZERO(&main_set);
    CPU_SET(main_core, &main_set);
    if (sched_setaffinity(getpid(), sizeof(main_set), &main_set) == -1) {
        perror("Failed to set main thread affinity");
    }

    // Get sorted threads
    std::vector<ThreadInfo> threads = get_thread_times_exclude_main(); // Removed auto
    if (threads.empty()) return;

    // Set worker affinity (fixed threads.tid -> threads[0].tid)
    cpu_set_t worker_set;
    CPU_ZERO(&worker_set);
    CPU_SET(worker_core, &worker_set);
    if (sched_setaffinity(threads[0].tid, sizeof(worker_set), &worker_set) == -1) {
        perror("Failed to set worker thread affinity");
    }
}

