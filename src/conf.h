#ifndef CONF_H_112954
#define CONF_H_112954

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
};

void ParseArgv(int argc, const char* const argv[]);
void InitTestConf();
GlobalConfig const &Conf();



#endif /* CONF_H_112954 */
