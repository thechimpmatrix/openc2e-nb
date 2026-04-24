#include "StringWriter.h"

#include <cstddef>
#include <cstdint>
#include <string>

void StringWriter::flush() {
}

const std::string& StringWriter::string() const {
	return buf;
}

void StringWriter::do_write(const uint8_t* out, size_t n) {
	buf.insert(buf.end(), out, out + n);
}