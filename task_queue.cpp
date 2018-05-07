#include "task_queue.h"

TaskQueue::TaskQueue(int num_threads) {
  thread_.reserve(num_threads);
  for (int i = 0; i < num_threads; i++) {
    thread_.emplace_back(&TaskQueue::Worker, this);
  }
};

std::packaged_task<std::string()> *TaskQueue::GetTask() {
  std::unique_lock<std::mutex> lock(task_mutex_);
  while (task_.empty() && !closed_) {
    for_get_task_.wait(lock);
  }
  if (task_.empty()) {
    lock.unlock();
    return nullptr;
  }

  worked_task_.emplace_back(std::move(task_.front()));
  task_.pop_front();
  auto result = worked_task_.back().get();
  lock.unlock();
  return result;
}

void TaskQueue::Worker() {
  std::packaged_task<std::string()> *task = GetTask();
  while (task != nullptr) {
    (*task)();
    task = GetTask();
  }
};

void TaskQueue::Add(std::packaged_task<std::string()> task) {
  std::packaged_task<std::string()> *pt =
      new std::packaged_task<std::string()>(std::move(task));
  std::unique_ptr<std::packaged_task<std::string()>> v(pt);
  {
    std::lock_guard<std::mutex> g(task_mutex_);
    task_.emplace_back(std::move(v));
  }
  for_get_task_.notify_one();
};
