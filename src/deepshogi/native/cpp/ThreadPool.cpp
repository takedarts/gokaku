#include "ThreadPool.h"

namespace deepshogi {

/**
 * スレッド管理オブジェクトを作成する。
 * @param threads スレッド数
 */
ThreadPool::ThreadPool(int32_t threads)
    : _mutex(),
      _condition(),
      _threads(),
      _tasks(),
      _terminated(false) {
  for (int32_t i = 0; i < threads; i++) {
    _threads.emplace_back([this]() { _run(); });
  }
}

/**
 * スレッド管理オブジェクトを破棄する。
 */
ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _terminated = true;
  }

  _condition.notify_all();

  for (auto& thread : _threads) {
    thread.join();
  }
}

/**
 * 実行対象のタスクを登録する。
 * @param task タスク
 */
void ThreadPool::submit(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _tasks.push(task);
  }

  _condition.notify_all();
}

/**
 * 探索を実行する。
 */
void ThreadPool::_run() {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(_mutex);
      _condition.wait(lock, [this]() { return _terminated || !_tasks.empty(); });

      if (_terminated) {
        break;
      }

      task = _tasks.front();
      _tasks.pop();
    }

    task();
  }
}

}  // namespace deepshogi
