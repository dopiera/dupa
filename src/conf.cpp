#include "conf.h"

#include <cstdlib>

#include <iostream>

#include <boost/program_options.hpp>

#include "log.h"

static std::unique_ptr<GlobalConfig> conf;


void ParseArgv(int argc, const char* const argv[]) {
	conf.reset(new GlobalConfig);

	using namespace boost::program_options;
	variables_map vm;
	options_description hidden_desc("Hidden options");
	hidden_desc.add_options()
		("directory,d",
		 value<std::vector<std::string> >(&conf->dirs)->composing(),
		 "directory to analyze");
	options_description desc("usage: dupa dir1 [dir2]");
	desc.add_options()
		("help,h", "produce help message")
		("read_cache_from,c", value<std::string>(&conf->read_cache_from),
		 "path to the file from which to read checksum cache")
		("dump_cache_to,C", value<std::string>(&conf->dump_cache_to),
		 "path to which to dump the checksum cache")
		("sql_out,o", value<std::string>(&conf->sql_out),
		 "if set, path to where SQLite3 results will be dumped")
		("cache_only,1", bool_switch(&conf->cache_only)->default_value(false),
		 "only generate checksums cache")
		("use_size,s", bool_switch(&conf->use_size)->default_value(false),
		 "use file size rather than number of files as a measure of directory "
		 "sizes")
		("ignore_db_prefix,r",
		 bool_switch(&conf->ignore_db_prefix)->default_value(false),
		 "when parsing directory name, ignore the \"db:\" prefix")
		("verbose,v", bool_switch(&conf->verbose)->default_value(false),
		 "be verbose")
		("concurrency,j", value<int>(&conf->concurrency)->default_value(4),
		 "number of concurrently computed checksums")
		("tolerable_diff_pct,t",
		 value<int>(&conf->tolerable_diff_pct)->default_value(20),
		 "directories different by this percent or less will be considered "
		 "duplicates")
		;

	try {
		options_description effective_desc;
		effective_desc.add(hidden_desc).add(desc);
		positional_options_description p;
		p.add("directory", 2);
		store(command_line_parser(argc, argv).options(effective_desc)
				.positional(p).run(), vm);
		notify(vm);
	} catch (error const &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << desc << std::endl;
		exit(1);
	}

	if (conf->verbose) {
		stderr_loglevel = DEBUG;
	}

	if (vm.count("help"))
	{
		std::cerr << desc << std::endl;
		exit(0);
	}
	if (Conf().dirs.empty())
	{
		std::cerr << desc << std::endl;
		exit(1);
	}
}

void InitTestConf() {
	// do whatever makes any sense so that default values are set
	char const *argv[] ={"test_binary", ".", nullptr};
	ParseArgv(2, argv);
}

GlobalConfig const &Conf() {
	assert(!!conf);
	return *conf;
}
