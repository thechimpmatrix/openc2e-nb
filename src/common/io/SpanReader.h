#pragma once

#include "common/io/Reader.h"
#include "common/span.h"

#include <string_view>

#include <cstddef>
#include <cstdint>
#include <vector>

/*

A Reader implementation over a span<const uint8_t>. Useful
for passing in-memory regions of bytes to functions that
typically operate on files or other I/O.

*/

class SpanReader : public Reader {
  public:
	SpanReader();
	SpanReader(span<const uint8_t>);
	SpanReader(std::string_view);

	void seek_absolute(size_t n) override;
	void seek_relative(int64_t off) override;
	size_t tell() const override;
	bool has_data_left() override;
	std::vector<uint8_t> read_to_end() override;
	uint8_t peek_byte() override;

  private:
	void do_read(uint8_t* out, size_t n) override;

	size_t pos = 0;
	span<const uint8_t> buf;
};