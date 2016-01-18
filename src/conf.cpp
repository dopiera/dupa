#include "conf.h"

#include <cstdlib>

#include <iostream>

#include <boost/program_options.hpp>

#include "log.h"

static std::unique_ptr<GlobalConfig> conf;


void ParseArgv(int argc, char **argv) {
	conf.reset(new GlobalConfig);

	using namespace boost::program_options;
	variables_map vm;
	options_description hidden_desc("Hidden options");
	hidden_desc.add_options()
		("directory,d",
		 value<std::vector<std::string> >(&conf->dirs)->composing(),
		 "directory to analyze");
	options_description desc("usage: dup_ident dir1 [dir2]");
	desc.add_options()
		("help,h", "produce help message")
		("read_cache_from,c", value<std::string>(&conf->read_cache_from),
		 "path to the file from which to read checksum cache")
		("dump_cache_to,C", value<std::string>(&conf->dump_cache_to),
		 "path to which to dump the checksum cache")
		("cache_only,1", "only generate checksums cache")
		("verbose,v", "be verbose");

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

	if (vm.count("verbose")) {
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

	conf->cache_only = vm.count("cache_only");
}

GlobalConfig const &Conf() {
	assert(!!conf);
	return *conf;
}
