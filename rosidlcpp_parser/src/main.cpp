#include <rosidlcpp_parser/rosidlcpp_parser.hpp>

#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
  auto result = rosidlcpp_parser::parse_idl_file(argv[1]);

  if (argc == 2) {
    std::cout << result.dump(2) << std::endl;
    return 0;
  } else if (argc == 3) {
    std::ofstream output_file(argv[2]);
    output_file << result.dump(2) << std::endl;
    output_file.close();
    return 0;
  } else if (argc == 4) {
    auto ros_json = rosidlcpp_parser::convert_idljson_to_rosjson(result, argv[1]);
    std::ofstream output_file(argv[3]);
    output_file << ros_json.dump(2) << std::endl;
    output_file.close();
    return 0;
  } else {
    std::cerr << "Usage: rosidlcpp_parser <input_file> [output_file]" << std::endl;
    return 1;
  }

  return 0;
}