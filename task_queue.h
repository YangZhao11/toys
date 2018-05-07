#ifndef _TASK_QUEUE_H_
#define _TASK_QUEUE_H_

#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// task queue
class TaskQueue {
  bool closed_ = false;
  std::deque<std::unique_ptr<std::packaged_task<std::string()>>> task_;
  std::deque<std::unique_ptr<std::packaged_task<std::string()>>> worked_task_;
  std::mutex task_mutex_;  // guards the above three
  std::condition_variable for_get_task_;

  std::packaged_task<std::string()> *GetTask();
  void Worker();
  std::vector<std::thread> thread_;

 public:
  explicit TaskQueue(int num_threads);

  // For provider
  void Add(std::packaged_task<std::string()> task);
  void Close();

  // For consumer
  std::string GetResult();
};

#endif  // _TASK_QUEUE_H_
