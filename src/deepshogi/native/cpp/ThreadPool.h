#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace deepshogi {

/**
 * Thread pool class.
 */
class ThreadPool {
 public:
  /**
   * Constructs a thread pool object.
   * @param threads Number of threads
   */
  ThreadPool(int32_t threads);

  /**
   * Destroys the thread pool object.
   */
  virtual ~ThreadPool();

  /**
   * Submits a task for execution.
   * @param task Task to execute
   */
  void submit(std::function<void()> task);

  /**
   * Returns the number of threads.
   * @return Number of threads
   */
  inline int32_t getSize() const {
    return static_cast<int32_t>(_threads.size());
  }

 private:
  /**
   * Mutex for synchronization.
   */
  std::mutex _mutex;

  /**
   * Condition variable for synchronization.
   */
  std::condition_variable _condition;

  /**
   * List of thread objects.
   */
  std::vector<std::thread> _threads;

  /**
   * Queue of pending tasks.
   */
  std::queue<std::function<void()>> _tasks;

  /**
   * True if the pool should stop.
   */
  bool _terminated;

  /**
   * Worker function executed by each thread.
   */
  void _run();
};

}  // namespace deepshogi
