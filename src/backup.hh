#pragma once
#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>

namespace kv_store {
class payload;

class backuper {
public:
    backuper(seastar::shard_id shard);
    ~backuper();

    seastar::future<bool> insert(const payload& payload);
    seastar::future<std::optional<payload>> get(const std::string& key);
    seastar::future<bool> remove(const std::string& key);

private:
    void build_index();
    void compact();

private:
    std::string _db_filename;
    std::map<std::string, std::streampos> _index;
    std::map<std::string, bool> _deleted;
};
} // namespace kv_store
