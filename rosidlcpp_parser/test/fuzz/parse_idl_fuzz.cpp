#include <string>

#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::string buffer(reinterpret_cast<const char*>(data), size);
    rosidlcpp_parser::parse_idl(buffer);
    return 0;
}