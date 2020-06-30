#ifndef _TASK_QUEUE_H_
#define _TASK_QUEUE_H_

#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <optional>

// TaskQueue implements a task execution interface using threads.
//
// Task provider shall add packaged_task<std::string()> to the
// TaskQueue, then call close after the last task is added. Without
// Close, the worker threads will wait indefinitely. The packaged
// tasks are executed in worker threads.
//
// The result consumer can call str = GetResult()
// repeatedly, until str does not contain value.
//
// All worker threads are waited for before destruction, to make sure
// all tasks are finished.
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
  void Add(std::function<std::string()> task);
  void Close();

  // For consumer
  std::optional<std::string> GetResult();
};

#endif  // _TASK_QUEUE_H_
