/*
 * concurrent_queue.h - thread safe queue
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */

#ifndef _CONCUR_QUEUE_
#define _CONCUR_QUEUE_

#include <condition_variable>
#include <mutex>
#include <memory>

template<typename T>
class ConcurrentQueue {
 public:
  ConcurrentQueue() : head_{new StockNode}, tail_{head_.get()} {}

  ConcurrentQueue(const ConcurrentQueue&) = delete;
  ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
  ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;

  ConcurrentQueue(ConcurrentQueue&& pq) {
    std::lock_guard<std::mutex> pq_head_lock(pq.head_mutex_);
    std::lock_guard<std::mutex> pq_tail_lock(pq.tail_mutex_);
    head_ = std::move(pq.head_);
    tail_ = pq.tail_;

    pq.head_.reset(new StockNode);
    pq.tail_ = pq.head_.get();
  }
  /*
   * empty - test if the stock is empty
   *
   * Return:
   *  true if empty, otherwise false.
   */
  bool empty() {
    std::lock_guard<std::mutex> head_lock(head_mutex_);
    return head_.get() == get_tail();
  }
  /*
   * push - add a new item to stock
   *
   * @t: new item
   */
  void push(std::unique_ptr<T>&& t) {
    std::unique_ptr<StockNode> new_dummy(new StockNode);
    {
      std::lock_guard<std::mutex> tail_lock(tail_mutex_);
      tail_->data = std::move(t);
      tail_->next = std::move(new_dummy);
      tail_ = tail_->next.get();
    }

    cond_.notify_one();
  }
  /*
   * get_next - get the head item
   *
   * Return:
   *  the first item or null if empty
   */
  std::unique_ptr<T> get_next(bool block_wait = true) {
    std::unique_ptr<StockNode> head = pop(block_wait);
    std::unique_ptr<T> ret {};
    if (head) {
      ret = std::move(head->data);
    }
    return ret;
  }

  /*
   * clear - clear the queue
   */
  void clear() {
    auto tmp{std::move(*this)};
  }

 private:
  /*
   * The presentation of the queue is ended with
   * a dummy end which is the end.
   *
   * --- Initial state:
   *  [dummy]
   *   |   |
   * Head End
   *
   * --- None empty:
   *  [Node0]->[Node1]->..._->_->[dummy]
   *    |                           |
   *   Head                        End
   */

  struct StockNode {
    std::unique_ptr<T> data;
    std::unique_ptr<StockNode> next;
  };

  std::mutex head_mutex_;
  std::mutex tail_mutex_;
  std::condition_variable cond_;
  std::unique_ptr<StockNode> head_;
  StockNode* tail_;

 private:
  /*
   * get_tail - get the dummy end
   *
   * Return:
   *  the last dummy item
   */
  StockNode* get_tail() {
    std::lock_guard<std::mutex> tail_lock(tail_mutex_);
    return tail_;
  }
  /*
   * pop - pop the head
   *
   * Return:
   *  the head node
   */
  std::unique_ptr<StockNode> pop(bool block_wait) {
    std::unique_lock<std::mutex> head_lock(head_mutex_);

    std::unique_ptr<StockNode> ret {};

    if (!block_wait) {
      if (head_.get() != get_tail()) {
        ret = std::move(head_);
        head_ = std::move(ret->next);
      }
    } else {
      cond_.wait(head_lock, [&] { return (head_.get() != get_tail()); });
      ret = std::move(head_);
      head_ = std::move(ret->next);
    }

    return ret;
  }

};

#endif // !_CONCUR_QUEUE_
