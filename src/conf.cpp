#include "conf.h"

#include <cstdlib>

#include <iostream>

#include <boost/program_options.hpp>
#include <memory>

#include "log.h"

static std::unique_ptr<GlobalConfig> conf;

void ParseArgv(int argc, const char *const argv[]) {
  conf = std::make_unique<GlobalConfig>();

  namespace po = boost::program_options;

  po::variables_map vm;
  po::options_description hidden_desc("Hidden options");
  hidden_desc.add_options()(
      "directory,d",
      po::value<std::vector<std::string>>(&conf->dirs_)->composing(),
      "directory to analyze");
  po::options_description desc("usage: dupa dir1 [dir2]");
  desc.add_options()("help,h", "produce help message")(
      "read_cache_from,c", po::value<std::string>(&conf->read_cache_from_),
      "path to the file from which to read checksum cache")(
      "dump_cache_to,C", po::value<std::string>(&conf->dump_cache_to_),
      "path to which to dump the checksum cache")(
      "sql_out,o", po::value<std::string>(&conf->sql_out_),
      "if set, path to where SQLite3 results will be dumped")(
      "cache_only,1", po::bool_switch(&conf->cache_only_)->default_value(false),
      "only generate checksums cache")(
      "use_size,s", po::bool_switch(&conf->use_size_)->default_value(false),
      "use file size rather than number of files as a measure of directory "
      "sizes")("ignore_db_prefix,r",
               po::bool_switch(&conf->ignore_db_prefix_)->default_value(false),
               "when parsing directory name, ignore the \"db:\" prefix")(
      "skip_renames,w",
      po::bool_switch(&conf->skip_renames_)->default_value(false),
      "when comparing directories, don't print renames")(
      "verbose,v", po::bool_switch(&conf->verbose_)->default_value(false),
      "be verbose")("concurrency,j",
                    po::value<int>(&conf->concurrency_)->default_value(4),
                    "number of concurrently computed checksums")(
      "tolerable_diff_pct,t",
      po::value<int>(&conf->tolerable_diff_pct_)->default_value(20),
      "directories different by this percent or less will be considered "
      "duplicates");

  try {
    po::options_description effective_desc;
    effective_desc.add(hidden_desc).add(desc);
    po::positional_options_description p;
    p.add("directory", 2);
    po::store(po::command_line_parser(argc, argv)
                  .options(effective_desc)
                  .positional(p)
                  .run(),
              vm);
    po::notify(vm);
  } catch (const po::error &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << desc << std::endl;
    exit(1);
  }

  if (conf->verbose_) {
    stderr_loglevel = DEBUG;
  }

  if (vm.count("help")) {
    std::cerr << desc << std::endl;
    exit(0);
  }
  if (Conf().dirs_.empty()) {
    std::cerr << desc << std::endl;
    exit(1);
  }
}

void InitTestConf() {
  // do whatever makes any sense so that default values are set
  const char *argv[] = {"test_binary", ".", nullptr};
  ParseArgv(2, argv);
}

const GlobalConfig &Conf() {
  assert(!!conf);
  return *conf;
}
