#ifndef SRC_CONF_H_
#define SRC_CONF_H_

#include <string>
#include <vector>

struct GlobalConfig {
  std::string read_cache_from;
  std::string dump_cache_to;
  std::string sql_out;
  std::vector<std::string> dirs;
  int concurrency;
  int tolerable_diff_pct;
  bool verbose;
  bool cache_only;
  bool use_size;
  bool ignore_db_prefix;
  bool skip_renames;
};

void ParseArgv(int argc, const char *const argv[]);
void InitTestConf();
GlobalConfig const &Conf();

#endif // SRC_CONF_H_
