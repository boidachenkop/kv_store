#include "backup.hh"
#include "common.hh"

using namespace seastar;

namespace kv_store {

backuper::backuper(shard_id shard)
    : _db_filename(fmt::format("{}_backup", shard)) {
    fmt::print("start backup service for file: {}\n", _db_filename);
    build_index();
}

backuper::~backuper() {
    compact();
}

future<bool> backuper::insert(const payload& payload) {
    const auto& key = payload._key;
    const auto& value = payload._value;
    std::optional<future<bool>> remove_future;
    if (auto item = _index.find(key); item != _index.end()) {
        remove_future = remove(key);
    }
    std::ofstream outFile(_db_filename, std::ios::binary | std::ios::app);
    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();

    outFile.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
    outFile.write(key.data(), keyLen);
    outFile.write(reinterpret_cast<const char*>(&valueLen), sizeof(valueLen));
    outFile.write(value.data(), valueLen);
    if (!remove_future) { // TODO code repetition
        _index[key] = static_cast<std::streamoff>(outFile.tellp()) - static_cast<std::streamoff>(sizeof(valueLen) + valueLen);
        return make_ready_future<bool>(true);
    }
    return remove_future->then([&](bool success) {
        _index[key] = static_cast<std::streamoff>(outFile.tellp()) - static_cast<std::streamoff>(sizeof(valueLen) + valueLen);
        return make_ready_future<bool>(true);
    });
}

future<std::optional<payload>> backuper::get(const std::string& key) {
    auto it = _index.find(key);
    if (it == _index.end()) {
        return make_ready_future<std::optional<payload>>(std::nullopt);
    }

    std::ifstream inFile(_db_filename, std::ios::binary);
    inFile.seekg(it->second);

    uint32_t valueLen;
    inFile.read(reinterpret_cast<char*>(&valueLen), sizeof(valueLen));

    std::string value(valueLen, '\0');
    inFile.read(&value[0], valueLen);

    return make_ready_future<std::optional<payload>>(payload(key, value));
}

future<bool> backuper::remove(const std::string& key) {
    if (_index.find(key) == _index.end()) {
        return make_ready_future<bool>(false);
    }

    std::ofstream outFile(_db_filename, std::ios::binary | std::ios::app);
    uint32_t keyLen = key.size();
    uint32_t deleteMarker = 0xFFFFFFFF;

    outFile.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
    outFile.write(key.data(), keyLen);
    outFile.write(reinterpret_cast<const char*>(&deleteMarker), sizeof(deleteMarker));

    _index.erase(key);
    _deleted[key] = true;
    return make_ready_future<bool>(true);
}

void backuper::build_index() {
    std::ifstream inFile(_db_filename, std::ios::binary);
    if (!inFile)
        return;

    _index.clear();
    while (inFile) {
        uint32_t keyLen, valueLen;
        inFile.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        if (inFile.eof())
            break;

        std::string key(keyLen, '\0');
        inFile.read(&key[0], keyLen);
        std::streampos pos = inFile.tellg();

        inFile.read(reinterpret_cast<char*>(&valueLen), sizeof(valueLen));
        std::string value(valueLen, '\0');
        inFile.read(&value[0], valueLen);

        _index[key] = pos;
    }
}

void backuper::compact() {
    auto temp_file = fmt::format("{}.tmp", _db_filename);
    std::ifstream inFile(_db_filename, std::ios::binary);
    std::ofstream tempFile(temp_file, std::ios::binary | std::ios::trunc);
    if (!inFile || !tempFile)
        return;

    while (inFile) {
        uint32_t keyLen, valueLen;
        std::streampos pos = inFile.tellg();

        inFile.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        if (inFile.eof())
            break;

        std::string key(keyLen, '\0');
        inFile.read(&key[0], keyLen);

        inFile.read(reinterpret_cast<char*>(&valueLen), sizeof(valueLen));
        if (valueLen == 0xFFFFFFFF || _deleted.count(key)) {
            inFile.seekg(valueLen, std::ios::cur);
            continue; // Skip deleted entries
        }

        std::string value(valueLen, '\0');
        inFile.read(&value[0], valueLen);

        tempFile.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        tempFile.write(key.data(), keyLen);
        tempFile.write(reinterpret_cast<const char*>(&valueLen), sizeof(valueLen));
        tempFile.write(value.data(), valueLen);
    }

    inFile.close();
    tempFile.close();
    std::rename(temp_file.c_str(), _db_filename.c_str());
    build_index();
}
} // namespace kv_store
