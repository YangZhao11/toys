#include "task_queue.h"
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char *argv[]) {
  int num_threads = 4;
  if (argc >= 2) {
    num_threads = std::stoi(argv[1]);
  }

  TaskQueue q(num_threads);

  std::thread writer([&q]() {
    std::string s;
    bool done;
    for (std::tie(s, done) = q.GetResult(); !done;
         std::tie(s, done) = q.GetResult()) {
      std::cout << s << std::endl;
    }
  });

  for (int i = 0; i < 20; i++) {
    q.Add(std::packaged_task<std::string()>([i]() -> std::string {
      std::this_thread::sleep_for(std::chrono::seconds(i));
      std::ostringstream stringStream;
      stringStream << "Hello" << i;
      return stringStream.str();
    }));
  }
  q.Close();

  writer.join();
}
