#ifndef SRC_CONF_H_
#define SRC_CONF_H_

#include <string>
#include <vector>

struct GlobalConfig {
  std::string read_cache_from_;
  std::string dump_cache_to_;
  std::string sql_out_;
  std::vector<std::string> dirs_;
  int concurrency_;
  int tolerable_diff_pct_;
  bool verbose_;
  bool cache_only_;
  bool use_size_;
  bool ignore_db_prefix_;
  bool skip_renames_;
};

void ParseArgv(int argc, const char *const argv[]);
void InitTestConf();
GlobalConfig const &Conf();

#endif // SRC_CONF_H_
