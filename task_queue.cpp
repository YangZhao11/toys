#include "task_queue.h"

TaskQueue::TaskQueue(int num_threads) {
  thread_.reserve(num_threads);
  for (int i = 0; i < num_threads; i++) {
    thread_.emplace_back(&TaskQueue::Worker, this);
  }
};

TaskQueue::~TaskQueue() {
  Close();
  for (auto &t : thread_) {
    t.join();
  }
};

std::packaged_task<std::string()> *TaskQueue::GetTask() {
  std::unique_lock<std::mutex> lock(task_mutex_);
  while (task_.empty() && !closed_) {
    has_more_task_.wait(lock);
  }
  if (task_.empty()) {
    lock.unlock();
    return nullptr;
  }

  worked_task_.emplace_back(std::move(task_.front()));
  task_.pop_front();
  auto result = worked_task_.back().get();
  lock.unlock();

  has_more_worked_task_.notify_one();
  return result;
};

void TaskQueue::Worker() {
  std::packaged_task<std::string()> *task = GetTask();
  while (task != nullptr) {
    (*task)();
    task = GetTask();
  }
};

void TaskQueue::Add(std::function<std::string()> task) {
  std::packaged_task<std::string()> *pt =
      new std::packaged_task<std::string()>(task);
  std::unique_ptr<std::packaged_task<std::string()>> v(pt);
  {
    std::lock_guard<std::mutex> g(task_mutex_);
    task_.emplace_back(std::move(v));
  }
  has_more_task_.notify_one();
};

void TaskQueue::Close() {
  {
    std::lock_guard<std::mutex> g(task_mutex_);
    closed_ = true;
  }
  has_more_task_.notify_all();
  has_more_worked_task_.notify_all();
}

std::optional<std::string> TaskQueue::GetResult() {
  std::unique_lock<std::mutex> lock(task_mutex_);
  while (worked_task_.empty() && !(closed_ && task_.empty())) {
    has_more_worked_task_.wait(lock);
  }
  if (worked_task_.empty()) {
    lock.unlock();
    return std::nullopt;
  }

  std::unique_ptr<std::packaged_task<std::string()>> t =
      std::move(worked_task_.front());
  worked_task_.pop_front();
  lock.unlock();

  auto fut = t.get()->get_future();
  return fut.get();
}
