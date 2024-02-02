
#ifndef EXECUTOR_H_
#define EXECUTOR_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "cronet_c.h"


class Executor {
 public:
  Executor();
  ~Executor();

  // Gets Cronet_ExecutorPtr implemented by |this|.
  Cronet_ExecutorPtr GetExecutor();

  // Shuts down the executor, so all pending tasks are destroyed without
  // getting executed.
  void ShutdownExecutor();

 private:
  // Runs tasks in |task_queue_| until |stop_thread_loop_| is set to true.
  void RunTasksInQueue();
  static void ThreadLoop(Executor* executor);

  // Adds |runnable| to |task_queue_| to execute on |executor_thread_|.
  void Execute(Cronet_RunnablePtr runnable);
  // Implementation of Cronet_Executor methods.
  static void Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr runnable);

  // Synchronise access to |task_queue_| and |stop_thread_loop_|;
  std::mutex lock_;
  // Tasks to run.
  std::queue<Cronet_RunnablePtr> task_queue_;
  // Notified if task is added to |task_queue_| or |stop_thread_loop_| is set.
  std::condition_variable task_available_;
  // Set to true to stop running tasks.
  bool stop_thread_loop_ = false;

  // Thread on which tasks are executed.
  std::thread executor_thread_;

  Cronet_ExecutorPtr const executor_;
};

#endif  // EXECUTOR_H_
