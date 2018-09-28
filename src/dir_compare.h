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

#ifndef SRC_DIR_COMPARE_H_
#define SRC_DIR_COMPARE_H_

#include <boost/filesystem/path.hpp>

class CompareOutputStream {
 public:
  virtual ~CompareOutputStream() = default;

  virtual void OverwrittenBy(const std::string &f,
                             const std::vector<std::string> &candidates) = 0;
  virtual void CopiedFrom(const std::string &f,
                          const std::vector<std::string> &candidates) = 0;
  virtual void RenameTo(const std::string &f,
                        const std::vector<std::string> &to) = 0;
  virtual void ContentChanged(const std::string &f) = 0;
  virtual void Removed(const std::string &f) = 0;
  virtual void NewFile(const std::string &f) = 0;
};

class PrintingOutputStream : public CompareOutputStream {
 public:
  void OverwrittenBy(const std::string &f,
                     const std::vector<std::string> &candidates) override;
  void CopiedFrom(const std::string &f,
                  const std::vector<std::string> &candidates) override;
  void RenameTo(const std::string &f,
                const std::vector<std::string> &to) override;
  void ContentChanged(const std::string &f) override;
  void Removed(const std::string &f) override;
  void NewFile(const std::string &f) override;
};

class CompareOutputStreams : public CompareOutputStream {
 public:
  explicit CompareOutputStreams(
      std::vector<std::reference_wrapper<CompareOutputStream>> &&streams);
  void OverwrittenBy(const std::string &f,
                     const std::vector<std::string> &candidates) override;
  void CopiedFrom(const std::string &f,
                  const std::vector<std::string> &candidates) override;
  void RenameTo(const std::string &f,
                const std::vector<std::string> &to) override;
  void ContentChanged(const std::string &f) override;
  void Removed(const std::string &f) override;
  void NewFile(const std::string &f) override;

 private:
  std::vector<std::reference_wrapper<CompareOutputStream>> streams_;
};

void WarmupCache(const std::string &path);
void DirCompare(const std::string &dir1, const std::string &dir2,
                CompareOutputStream &stream);

#endif /* SRC_DIR_COMPARE_H_ */
