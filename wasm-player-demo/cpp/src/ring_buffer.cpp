#include "ring_buffer.h"

#include <algorithm>
#include <cstring>

namespace wasm_player {

ByteRingBuffer::ByteRingBuffer(size_t capacity) : data_(capacity) {}

size_t ByteRingBuffer::write(const uint8_t* data, size_t len) {
  if (!data || len == 0 || data_.empty()) {
    return 0;
  }

  const size_t writable = std::min(len, data_.size() - size_);
  const size_t first = std::min(writable, data_.size() - tail_);
  std::memcpy(data_.data() + tail_, data, first);

  const size_t second = writable - first;
  if (second > 0) {
    std::memcpy(data_.data(), data + first, second);
  }

  tail_ = (tail_ + writable) % data_.size();
  size_ += writable;
  return writable;
}

size_t ByteRingBuffer::read(uint8_t* out, size_t len) {
  if (!out || len == 0 || size_ == 0 || data_.empty()) {
    return 0;
  }

  const size_t readable = std::min(len, size_);
  const size_t first = std::min(readable, data_.size() - head_);
  std::memcpy(out, data_.data() + head_, first);

  const size_t second = readable - first;
  if (second > 0) {
    std::memcpy(out + first, data_.data(), second);
  }

  head_ = (head_ + readable) % data_.size();
  size_ -= readable;
  return readable;
}

size_t ByteRingBuffer::size() const { return size_; }

size_t ByteRingBuffer::capacity() const { return data_.size(); }

bool ByteRingBuffer::empty() const { return size_ == 0; }

void ByteRingBuffer::clear() {
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

}  // namespace wasm_player
