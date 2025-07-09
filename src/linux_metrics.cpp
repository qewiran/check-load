#include "linux_metrics.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <sys/statvfs.h>

namespace linux_metrics {
struct CPUdata {
  unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
};

CPUdata read_cpu_stats() {
  std::ifstream file("/proc/stat");
  std::string line;
  std::getline(file, line);

  std::istringstream iss(line);
  CPUdata data;
  std::string cpu_label;
  iss >> cpu_label >> data.user >> data.nice >> data.system >> data.idle >>
      data.iowait >> data.irq >> data.softirq >> data.steal;

  return data;
}

float calculate_cpu_usage(const CPUdata &prev, const CPUdata &curr) {
  const unsigned long prev_total = prev.user + prev.nice + prev.system +
                                   prev.idle + prev.iowait + prev.irq +
                                   prev.softirq + prev.steal;
  const unsigned long curr_total = curr.user + curr.nice + curr.system +
                                   curr.idle + curr.iowait + curr.irq +
                                   curr.softirq + curr.steal;

  const unsigned long total_diff = curr_total - prev_total;
  const unsigned long idle_diff =
      (curr.idle + curr.iowait) - (prev.idle + prev.iowait);
  const unsigned long active_diff = total_diff - idle_diff;

  return total_diff == 0
             ? 0.0f
             : static_cast<float>(active_diff) / total_diff * 100.0f;
}

auto read_memory_stats() {
  std::unordered_map<std::string, unsigned long> memory_stats{};
  std::ifstream meminfo("/proc/meminfo");
  std::string line;
  while (std::getline(meminfo, line)){
    std::istringstream iss (line);
    std::string key{}, unit{};
    unsigned long value{};
    iss>>key>>value>>unit;
    key.pop_back();
    memory_stats[key] = value;
  }
  
  return memory_stats;
  
}

json calculate_memory_usage(){
  json memory_usage = read_memory_stats();
  unsigned long memTotal = memory_usage["MemTotal"];
  unsigned long memAvailable = memory_usage["MemAvailable"];
  memory_usage = {
    {"total_kb", memory_usage["MemTotal"]},
    {"available_kb", memory_usage["MemAvailable"]},
    {"used_percent", static_cast<float>(memTotal-memAvailable) / memTotal * 100.0f} 
  };

  return memory_usage;
}

json get_disk_metrics(bool verbose){
  json disk_metrics=json::array();
  std::vector<std::tuple<std::string,unsigned long, unsigned long>> disk_sizes;
  std::ifstream mounts_file("/proc/mounts");
  std::string line{};

  while (std::getline(mounts_file, line)){
    std::istringstream iss(line);
    std::string device, mount_point, fs_type;
    iss >> device >> mount_point >> fs_type;
    // Пропускаем специальные файловые системы
    if (mount_point == "/dev" || mount_point == "/proc" ||
        mount_point == "/sys" || mount_point == "/run")
      continue;

    if (fs_type == "tmpfs" || fs_type == "devtmpfs" || fs_type == "proc" ||
        fs_type == "sysfs" || fs_type == "cgroup" || fs_type == "debugfs") 
      continue;

    struct statvfs vfs;
    if (statvfs(mount_point.c_str(), &vfs)==0){
      unsigned long block_size = vfs.f_frsize;
      unsigned long total = vfs.f_blocks * block_size;
      unsigned long free = vfs.f_bfree * block_size;
      unsigned long available = vfs.f_bavail * block_size;
      unsigned long used = total - free;

      if (verbose)
        disk_metrics.push_back(
            {{"device", device},
             {"mount_point", mount_point},
             {"fs_type", fs_type},
             {"total_bytes", total},
             {"used_bytes", used},
             {"free_bytes", free},
             {"available_bytes", available},
             {"usage_percent", 100.0 - (100.0 * free / total)}});
      else
        disk_sizes.push_back(std::make_tuple(mount_point, total, used));

    }
  }

  auto max_size_disc = *std::max_element(
      disk_sizes.begin(), disk_sizes.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) < std::get<2>(b);
      });

  if (verbose)
    return {{"disks", disk_metrics}};
  else
    return {{"mount_point", std::get<0>(max_size_disc)},
            {"total_Gbytes", static_cast<double>(std::get<1>(max_size_disc)) /
                                 (1024 * 1024 * 1024.)},
            {"used_Gbytes", static_cast<double>(std::get<2>(max_size_disc)) /
                                (1024 * 1024 * 1024.)}};
}

json get_system_metrics(bool verbose) {
  CPUdata cpu_data1 = read_cpu_stats();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  CPUdata cpu_data2 = read_cpu_stats();
  json metrics{};
  metrics["cpu_usage"] = calculate_cpu_usage(cpu_data1, cpu_data2);
  json memory_metrics = calculate_memory_usage();
  
  metrics["memory_percent_usage"] = memory_metrics["used_percent"];
  metrics["memory_total_kb"] = memory_metrics["total_kb"];
  metrics["memory_available_kb"] = memory_metrics["available_kb"];

  metrics["disks"] = get_disk_metrics(verbose);
  

  return metrics;
}
}
