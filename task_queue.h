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
  std::condition_variable has_more_task_;
  std::condition_variable has_more_worked_task_;

  std::packaged_task<std::string()> *GetTask();
  void Worker();
  std::vector<std::thread> thread_;

 public:
  explicit TaskQueue(int num_threads);
  ~TaskQueue();

  // For provider
  void Add(std::packaged_task<std::string()> task);
  void Close();

  // For consumer
  std::pair<std::string, bool> GetResult();
};

#endif  // _TASK_QUEUE_H_
