#ifndef HASH_CACHE_H_6332
#define HASH_CACHE_H_6332

#include <stdint.h>

#include <map>
#include <string>

#include <boost/filesystem/path.hpp>

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
	cksum compute_cksum(boost::filesystem::path const & p);
	void store_cksums();
	void read_cksums(std::string const & path);
	static void initialize(
		std::string const & read_cache_from,
		std::string const & dump_cache_to);
	static void finalize();

	static hash_cache * instance;

	typedef std::map<std::string, cksum> cache_map;
	cache_map cache;
	int out_fd;
};

#endif /* HASH_CACHE_H_6332 */
