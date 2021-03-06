#pragma once

// Engine implements backpropagation from output variables and their gradients
// to "root" variables (variables created by the user with requires_grad=True).

#include <Python.h>
#include <deque>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <functional>

#include "torch/csrc/autograd/function.h"
#include "torch/csrc/autograd/input_buffer.h"

namespace torch { namespace autograd {

struct ReadyQueue;
struct FunctionTask;
struct GraphTask;

// A single instance of this struct should be created through the whole process lifetime.
// The worker thread creation logic and Engine's destructor rely on this.
// 一个进程，只有一个 Engine

struct Engine {
  Engine();
  virtual ~Engine();
  // ready_queue_type 是个 double-ended-queue, 里面存放的是一个 pair <Function, InputBuffer>， 存放的是 前向还是方向？ InputBuffer 是对应于啥的？
  using ready_queue_type = std::deque<std::pair<std::shared_ptr<Function>, InputBuffer>>;

  // function_queue 是一个 存放 Function 的 vector，
  using function_queue = std::vector<Function*>;

  // 依赖类型， 无序 字典，  <Function*, int>, 这个 int 表示的是啥， 这个 Function* 是什么 Function。
  using dependencies_type = std::unordered_map<Function*, int>;

  using pre_callback_type = std::function<bool (Function*, variable_list&)>;
  using pre_callback_map = std::unordered_multimap<Function*, pre_callback_type>;
  using post_callback_type = std::function<bool (Function*, variable_list&, variable_list&)>;
  using post_callback_map = std::unordered_multimap<Function*, post_callback_type>;

  // Given a list of (Function, input number) pairs computes the value of the graph
  // by following next_function references.
  // 反向求导所调用的 方法
  // 里面会创建一个 GraphTask
  virtual void execute(
      const function_list& roots,
      const variable_list& inputs,
      bool keep_graph,
      const pre_callback_map& pre_callbacks = pre_callback_map(),
      const post_callback_map& post_callbacks = post_callback_map());

  void queue_callback(std::function<void()> callback);

protected:
  function_queue find_roots(
      const function_list& roots,
      variable_list& inputs,
      GraphTask& task);
  void find_stochastic_functions(function_queue& queue, Function* graph_root, GraphTask& task);
  void compute_dependencies(function_queue queue, GraphTask& task);
  void evaluate_function(FunctionTask& task);
  ReadyQueue& ready_queue(int device);
  void start_threads();
  virtual void thread_init(int device);
  virtual void thread_main(GraphTask *task);
  virtual void thread_on_exception(FunctionTask& task, std::exception& e);
  
  // 用来标记只执行一次的函数的标签
  std::once_flag start_threads_flag;
  // Engine 保存了一个 ready_queues !!!!!!!!!!!!
  // CPU 一个 ReadyQueue， GPUs 一个 ReadyQueue，存放可以被执行的 FunctionTask
  std::vector<std::shared_ptr<ReadyQueue>> ready_queues;
  std::vector<std::function<void()>> final_callbacks;
  std::mutex post_callbacks_lock;
};

}} // namespace torch::autograd
