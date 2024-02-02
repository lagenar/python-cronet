#include "executor.h"


Executor::Executor()
    : executor_thread_(Executor::ThreadLoop, this),
      executor_(Cronet_Executor_CreateWith(Executor::Execute)) {
  Cronet_Executor_SetClientContext(executor_, this);
}

Executor::~Executor() {
  ShutdownExecutor();
  Cronet_Executor_Destroy(executor_);
}

Cronet_ExecutorPtr Executor::GetExecutor() {
  return executor_;
}

void Executor::ShutdownExecutor() {
  // Break tasks loop.
  {
    std::lock_guard<std::mutex> lock(lock_);
    stop_thread_loop_ = true;
  }
  task_available_.notify_one();
  // Wait for executor thread.
  executor_thread_.join();
}

void Executor::RunTasksInQueue() {
  // Process runnables in |task_queue_|.
  while (true) {
    Cronet_RunnablePtr runnable = nullptr;
    {
      // Wait for a task to run or stop signal.
      std::unique_lock<std::mutex> lock(lock_);
      while (task_queue_.empty() && !stop_thread_loop_)
        task_available_.wait(lock);

      if (stop_thread_loop_)
        break;

      if (task_queue_.empty())
        continue;

      runnable = task_queue_.front();
      task_queue_.pop();
    }
    Cronet_Runnable_Run(runnable);
    Cronet_Runnable_Destroy(runnable);
  }
  // Delete remaining tasks.
  std::queue<Cronet_RunnablePtr> tasks_to_destroy;
  {
    std::unique_lock<std::mutex> lock(lock_);
    tasks_to_destroy.swap(task_queue_);
  }
  while (!tasks_to_destroy.empty()) {
    Cronet_Runnable_Destroy(tasks_to_destroy.front());
    tasks_to_destroy.pop();
  }
}

/* static */
void Executor::ThreadLoop(Executor* executor) {
  executor->RunTasksInQueue();
}

void Executor::Execute(Cronet_RunnablePtr runnable) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!stop_thread_loop_) {
      task_queue_.push(runnable);
      runnable = nullptr;
    }
  }
  if (runnable) {
    Cronet_Runnable_Destroy(runnable);
  } else {
    task_available_.notify_one();
  }
}

/* static */
void Executor::Execute(Cronet_ExecutorPtr self,
                             Cronet_RunnablePtr runnable) {
  auto* executor =
      static_cast<Executor*>(Cronet_Executor_GetClientContext(self));
  executor->Execute(runnable);
}
