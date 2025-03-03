#pragma once
#include "data_store.hh"

#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>

namespace kv_store {
class payload;

class backuper : public data_storage{
public:
    backuper(seastar::shard_id shard);
    ~backuper();

    seastar::future<bool> insert(const payload& payload) override;
    seastar::future<std::optional<payload>> get(const std::string& key) override;
    seastar::future<bool> remove(const std::string& key) override;
private:
    void compact();
    void build_index();

private:
    uint32_t _alive_marker{0xAA};
    uint32_t _delete_marker{0xFF};
    std::string _db_filename;
    std::map<std::string, std::streampos> _index;
    std::unordered_map<std::string, bool> _deleted;
};
} // namespace kv_store
