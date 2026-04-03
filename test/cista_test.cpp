#include <cassert>
#include <cista/cista.h>
#include <iostream>
int main() {
  namespace data = cista::offset;
  struct my_struct {  // Define your struct.
    int a_{0};
    struct inner {
        data::string b_;
    } j;
  };

  std::vector<unsigned char> buf;
  {  // Serialize.
    my_struct obj{1, {data::string{"test"}}};
    buf = cista::serialize(obj);
  }

  // Deserialize.
  auto deserialized = cista::deserialize<my_struct>(buf);
  assert(deserialized->j.b_ == data::string{"test"});
  std::cout << "Deserialized: " << deserialized->j.b_ << std::endl;
}