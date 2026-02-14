#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wasm_player {

class ByteRingBuffer {
 public:
  explicit ByteRingBuffer(size_t capacity);

  size_t write(const uint8_t* data, size_t len);
  size_t read(uint8_t* out, size_t len);

  size_t size() const;
  size_t capacity() const;
  bool empty() const;

  void clear();

 private:
  std::vector<uint8_t> data_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
};

}  // namespace wasm_player
