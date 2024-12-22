#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <iostream>

int main(int argc, char* argv[]) {
    auto result = rosidlcpp_parser::parse_ros_idl_file(argv[1]);

    std::cout << result.dump(2) << std::endl;

    return 0;
}