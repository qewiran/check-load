#include "json.hpp"

using nlohmann::json;

namespace linux_metrics {

struct CPUdata;
CPUdata read_cpu_stats(); 
float calculate_cpu_usage(const CPUdata &prev, const CPUdata &curr); 
auto read_memory_stats(); 
json calculate_memory_usage();
json get_disk_metrics(bool verbose);

json get_system_metrics(bool); 
} // namespace linux_metrics
