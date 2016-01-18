#ifndef CONF_H_112954
#define CONF_H_112954

#include <string>
#include <vector>

struct GlobalConfig {
	std::string read_cache_from;
	std::string dump_cache_to;
	std::vector<std::string> dirs;
	bool verbose;
	bool cache_only;
};

void ParseArgv(int argc, char **argv);
GlobalConfig const &Conf();



#endif /* CONF_H_112954 */
