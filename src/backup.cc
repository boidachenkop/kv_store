#include "backup.hh"
#include "common.hh"

#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/thread.hh>

using namespace seastar;

namespace {
size_t next_power_of_two(size_t x) {
    if (x == 0)
        return 1;
    --x;
    for (size_t i = 1; i < sizeof(size_t) * 8; i *= 2) {
        x |= x >> i;
    }
    return x + 1;
}
} // namespace

namespace kv_store {

backuper::backuper(shard_id shard)
    : _db_filename(fmt::format("{}_backup", shard)) {
    fmt::print("start backup service for file: {}\n", _db_filename);
    compact();
    // build_index();
}

backuper::~backuper() {
    fmt::print("{}: closing backuper, compacting store...", this_shard_id());
}

// serialize data in format
// [key_len][key][status][value_len][value]
future<bool> backuper::insert(const payload& payload) {
    return seastar::async([&] {
        const auto& key = payload._key;
        const auto& value = payload._value;
        if (auto item = _index.find(key); item != _index.end()) {
            (void)remove(key).get();
        }
        std::ofstream outFile(_db_filename, std::ios::binary | std::ios::app);
        uint32_t keyLen = key.size();
        uint32_t valueLen = value.size();

        outFile.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        outFile.write(key.data(), keyLen);
        auto pos = outFile.tellp();
        outFile.write(reinterpret_cast<const char*>(&_alive_marker), sizeof(_alive_marker));
        outFile.write(reinterpret_cast<const char*>(&valueLen), sizeof(valueLen));
        outFile.write(value.data(), valueLen);

        _index[key] = pos;
    }).then([] {
        return make_ready_future<bool>(true);
    });
}

future<std::optional<payload>> backuper::get(const std::string& key) {
    auto it = _index.find(key);
    if (it == _index.end()) {
        return make_ready_future<std::optional<payload>>(std::nullopt);
    }

    std::ifstream in(_db_filename, std::ios::binary);
    in.seekg(it->second);

    uint32_t status;
    in.read(reinterpret_cast<char*>(&status), sizeof(status));

    uint32_t valueLen;
    in.read(reinterpret_cast<char*>(&valueLen), sizeof(valueLen));

    std::string value(valueLen, '\0');
    in.read(&value[0], valueLen);

    return make_ready_future<std::optional<payload>>(payload(key, value));
}

future<bool> backuper::remove(const std::string& key) {
    auto it = _index.find(key);
    if (it == _index.end()) {
        return make_ready_future<bool>(false);
    }

    std::ofstream out(_db_filename, std::ios::binary | std::ios::in | std::ios::out);
    out.seekp(it->second);
    out.write(reinterpret_cast<const char*>(&_delete_marker), sizeof(_delete_marker));

    _index.erase(key);
    return make_ready_future<bool>(true);
}

future<std::map<std::string, std::string>> backuper::get_all() {
    std::map<std::string, std::string> result;
    std::ifstream in(_db_filename, std::ios::binary);
    while (in) {
        uint32_t key_len, value_len, status;
        std::streampos pos = in.tellg();

        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (in.eof())
            break;
        std::string key(key_len, '\0');
        in.read(&key[0], key_len);
        in.read(reinterpret_cast<char*>(&status), sizeof(status));
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        if (status == _delete_marker) {
            in.seekg(value_len, std::ios::cur);
            continue;
        }

        std::string value(value_len, '\0');
        in.read(&value[0], value_len);
        result.emplace(key, value);
    }
    return make_ready_future<std::map<std::string, std::string>>(std::move(result));
}

void backuper::build_index() {
    std::ifstream in(_db_filename, std::ios::binary);
    if (!in)
        return;

    _index.clear();
    while (in) {
        uint32_t key_len, value_len, status;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (in.eof())
            break;

        std::string key(key_len, '\0');
        in.read(&key[0], key_len);
        std::streampos pos = in.tellg();

        in.read(reinterpret_cast<char*>(&status), sizeof(status));
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        if (status == _delete_marker) {
            in.seekg(value_len, std::ios::cur);
            continue;
        }

        std::string value(value_len, '\0');
        in.read(&value[0], value_len);

        _index[key] = pos;
    }
    fmt::print("backuper: finished building index, total: {}\n", _index.size());
}

void backuper::compact() {
    auto temp_file = fmt::format("{}.tmp", _db_filename);
    std::ifstream in(_db_filename, std::ios::binary);
    std::ofstream temp(temp_file, std::ios::binary | std::ios::trunc);

    if (!in || !temp) {
        temp.close();
        std::remove(temp_file.c_str());
        in.close();
        return;
    }

    while (in) {
        uint32_t key_len, value_len, status;
        std::streampos pos = in.tellg();

        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (in.eof())
            break;

        std::string key(key_len, '\0');
        in.read(&key[0], key_len);
        in.read(reinterpret_cast<char*>(&status), sizeof(status));
        in.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
        if (status == _delete_marker) {
            in.seekg(value_len, std::ios::cur);
            continue;
        }

        std::string value(value_len, '\0');
        in.read(&value[0], value_len);

        temp.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        temp.write(key.data(), key_len);
        temp.write(reinterpret_cast<const char*>(&status), sizeof(status));
        temp.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        temp.write(value.data(), value_len);
    }

    in.close();
    temp.close();
    std::rename(temp_file.c_str(), _db_filename.c_str());
    std::remove(temp_file.c_str());
    build_index();
}
} // namespace kv_store
