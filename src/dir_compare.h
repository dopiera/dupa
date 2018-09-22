#ifndef SRC_DIR_COMPARE_H_
#define SRC_DIR_COMPARE_H_

#include <boost/filesystem/path.hpp>

class CompareOutputStream {
 public:
  virtual ~CompareOutputStream() = default;

  virtual void OverwrittenBy(
      const std::string &f,
      const std::vector<std::string> &candidates) const = 0;
  virtual void CopiedFrom(const std::string &f,
                          const std::vector<std::string> &candidates) const = 0;
  virtual void Rename(const std::string &f,
                      const std::vector<std::string> &to) const = 0;
  virtual void ContentChanged(const std::string &f) const = 0;
  virtual void Removed(const std::string &f) const = 0;
  virtual void NewFile(const std::string &f) const = 0;
};

class PrintingOutputStream : public CompareOutputStream {
 public:
  virtual ~PrintingOutputStream() = default;
  virtual void OverwrittenBy(const std::string &f,
                             const std::vector<std::string> &candidates) const;
  virtual void CopiedFrom(const std::string &f,
                          const std::vector<std::string> &candidates) const;
  virtual void Rename(const std::string &f,
                      const std::vector<std::string> &to) const;
  virtual void ContentChanged(const std::string &f) const;
  virtual void Removed(const std::string &f) const;
  virtual void NewFile(const std::string &f) const;
};

void WarmupCache(const std::string &path);
void DirCompare(const std::string &dir1, const std::string &dir2,
                const CompareOutputStream &stream);

#endif /* SRC_DIR_COMPARE_H_ */
