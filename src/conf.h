/*
 * (C) Copyright 2018 Marek Dopiera
 *
 * This file is part of dupa.
 *
 * dupa is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dupa is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dupa. If not, see http://www.gnu.org/licenses/.
 */

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
const GlobalConfig &Conf();

#endif  // SRC_CONF_H_
