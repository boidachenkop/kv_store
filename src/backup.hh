#pragma once

#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>
#include <seastar/coroutine/generator.hh>

namespace kv_store {
class payload;

class backuper {
public:
    backuper(seastar::shard_id shard);
    ~backuper();

    seastar::future<bool> insert(const payload& payload);
    seastar::future<std::optional<payload>> get(const std::string& key);
    seastar::future<bool> remove(const std::string& key);

    seastar::future<std::map<std::string, std::string>> get_all();
private:
    void compact();
    void build_index();

private:
    uint32_t _alive_marker{0xAA};
    uint32_t _delete_marker{0xFF};
    std::string _db_filename;
    std::unordered_map<std::string, std::streampos> _index;
};
} // namespace kv_store
