#include "neuronet.hpp"
#include <iostream>

int main() {
  Layer l1(3, std::vector<double>{0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1});
  Layer l2(3, std::vector<double>{0, 1, 1, 1, 0, -1, 1, 0});
  Net n(std::vector<Layer>{l1, l2});
  for (double o : n.evaluate(std::vector<double>{1, 2, -4})) {
    std::cout << o << ' ';
  }
  std::cout << std::endl;
}
