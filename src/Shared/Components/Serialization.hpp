#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Components {

    inline void appendBytes(std::vector<std::uint8_t>& data, const void* value, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(value);
        data.insert(data.end(), bytes, bytes + size);
    }

    template <typename T>
    inline void append(std::vector<std::uint8_t>& data, const T& value) {
        appendBytes(data, &value, sizeof(T));
    }

    class CReader {
      public:
        explicit CReader(const std::vector<std::uint8_t>& data) : data_(data) {}

        template <typename T>
        bool read(T& value) {
            if (remaining() < sizeof(T))
                return false;
            std::memcpy(&value, data_.data() + offset_, sizeof(T));
            offset_ += sizeof(T);
            return true;
        }

        bool readString(std::string& value) {
            std::uint32_t size = 0;
            if (!read(size) || size > remaining())
                return false;
            value.assign(reinterpret_cast<const char*>(data_.data() + offset_), size);
            offset_ += size;
            return true;
        }

        [[nodiscard]] std::size_t remaining() const {
            return offset_ <= data_.size() ? data_.size() - offset_ : 0;
        }

        [[nodiscard]] bool finished() const {
            return offset_ == data_.size();
        }

      private:
        const std::vector<std::uint8_t>& data_;
        std::size_t                      offset_ = 0;
    };

    inline void appendString(std::vector<std::uint8_t>& data, const std::string& value) {
        append(data, static_cast<std::uint32_t>(value.size()));
        appendBytes(data, value.data(), value.size());
    }

} // namespace Components
