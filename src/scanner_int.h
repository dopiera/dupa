#include "scanner.h"

#include <stack>
#include <thread>
#include <utility>

#include <boost/filesystem/convenience.hpp>

#include "synch_thread_pool.h"

template <class DirHandle>
void ScanDirectory(
		boost::filesystem::path const &root,
		ScanProcessor<DirHandle> &processor) {
	using boost::filesystem::path;

	std::stack<std::pair<path, DirHandle> > dirs_to_process;
	dirs_to_process.push(std::make_pair(root, processor.RootDir(root)));

	SyncThreadPool pool(4);  // FIXME: make configurable
	std::mutex mutex;

	while (!dirs_to_process.empty()) {
		path const dir = dirs_to_process.top().first;
		DirHandle const handle = dirs_to_process.top().second;

		dirs_to_process.pop();

		using boost::filesystem::directory_iterator;
		for (directory_iterator it(dir); it != directory_iterator(); ++it)
		{
			if (is_symlink(it->path()))
			{
				continue;
			}
			path const new_path = it->path();
			if (is_directory(it->status()))
			{
				std::lock_guard<std::mutex> lock(mutex);
				dirs_to_process.push(std::make_pair(
							new_path,
							processor.Dir(new_path, handle)));
			}
			if (boost::filesystem::is_regular(new_path))
			{
				pool.Submit([new_path, handle, &mutex, &processor] () mutable {
					cksum const sum = hash_cache::get()(new_path).sum;
					if (sum) {
						std::lock_guard<std::mutex> lock(mutex);
						processor.File(new_path, handle, sum);
					}
					});
			}
		}
	}
	pool.Stop();
}

