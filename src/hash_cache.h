#ifndef HASH_CACHE_H_6332
#define HASH_CACHE_H_6332

#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>

#include <unordered_map>

#include <boost/filesystem/path.hpp>

#include "sql_lib.h"

typedef uint64_t cksum;

class hash_cache
{
public:
	struct initializer {
		initializer(
			std::string const & read_cache_from,
			std::string const & dump_cache_to
			);
		~initializer();
	};
	static hash_cache & get();
	cksum operator()(boost::filesystem::path const & p);
private:
	hash_cache(
		std::string const & read_cache_from,
		std::string const & dump_cache_to
		);
	~hash_cache();
	static cksum compute_cksum(boost::filesystem::path const & p);
	void store_cksums();
	void read_cksums(std::string const & path);
	static void initialize(
		std::string const & read_cache_from,
		std::string const & dump_cache_to);
	static void finalize();

	static hash_cache * instance;

	typedef std::unordered_map<std::string, cksum> cache_map;
	cache_map cache;
	std::unique_ptr<SqliteScopedOpener> db_holder;
	std::mutex mutex;
};

#endif /* HASH_CACHE_H_6332 */
