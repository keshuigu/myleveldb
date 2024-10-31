
#include "leveldb/options.h"

#include "leveldb/comparator.h"
#include "leveldb/env.h"

namespace leveldb {
Options::Options() : comparator(BytewiseComparator()), env(Env::Default()) {}
}  // namespace leveldb
