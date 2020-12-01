/*
    enoki-thread/thread.h -- Simple thread pool with a task-based API

    Copyright (c) 2020 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#if defined(_MSC_VER)
#  if defined(ENOKI_THREAD_BUILD)
#    define ENOKI_THREAD_EXPORT    __declspec(dllexport)
#  else
#    define ENOKI_THREAD_EXPORT    __declspec(dllimport)
#  endif
#else
#  define ENOKI_THREAD_EXPORT    __attribute__ ((visibility("default")))
#endif

#if defined(__cplusplus)
#  define ENOKI_THREAD_DEF(x) = x
#else
#  define ENOKI_THREAD_DEF(x)
#endif

#define ENOKI_THREAD_AUTO ((uint32_t) -1)

typedef struct Pool Pool;
typedef struct Task Task;

#if defined(__cplusplus)
#define ENOKI_THREAD_THROW     noexcept(false)
extern "C" {
#else
#define ENOKI_THREAD_THROW
#endif

/**
 * \brief Create a new thread pool with the specified number of threads
 *
 * The default value \c ENOKI_THREAD_AUTO choses a thread count equal to the
 * number of available cores.
 */
extern ENOKI_THREAD_EXPORT Pool *
pool_create(uint32_t size ENOKI_THREAD_DEF(ENOKI_THREAD_AUTO));

/**
 * \brief Destroy the thread pool and discard remaining unfinished work.
 *
 * It is undefined behavior to destroy the thread pool while other threads
 * are waiting for the completion of scheduled work via \ref task_wait().
 *
 * \param pool
 *     The thread pool to destroy. \c nullptr refers to the default pool.
 */
extern ENOKI_THREAD_EXPORT void pool_destroy(Pool *pool ENOKI_THREAD_DEF(0));

/**
 * \brief Return the number of threads that are part of the pool
 *
 * \param pool
 *     The thread pool to query. \c nullptr refers to the default pool.
 */
extern ENOKI_THREAD_EXPORT uint32_t pool_size(Pool *pool ENOKI_THREAD_DEF(0));

/**
 * \brief Resize the thread pool to the given number of threads
 *
 * \param pool
 *     The thread pool to resize. \c nullptr refers to the default pool.
 */
extern ENOKI_THREAD_EXPORT void pool_set_size(Pool *pool, uint32_t size);

/**
 * \brief Return a unique number identifying the current worker thread
 *
 * When called from a thread pool worker (e.g. while executing a parallel
 * task), this function returns a unique identifying number between 1 and the
 * pool's total thread count.
 *
 * The IDs of separate thread pools overlap. When the current thread is not a
 * thread pool worker, the function returns zero.
 */
extern ENOKI_THREAD_EXPORT uint32_t pool_thread_id();

/*
 * \brief Submit a new task to a thread pool
 *
 * This function submits a new task consisting of \c size work units to the
 * thread pool \c pool.
 *
 * The \c parent and \c parent_count parameters can be used to specify parent
 * tasks that must be completed before execution of this task can commence. If
 * the task does not depend on any other tasks (e.g. <tt>parent_count == 0</tt>
 * and <tt>parent == nullptr</tt>), or when all of those other tasks have already
 * finished executing, then it will be immediately appended to the end of the
 * task queue. Otherwise, the task will be appended once all parent tasks
 * have finished executing.
 *
 * The task callback \c func will be invoked \c size times by the various
 * thread pool workers. Its first argument will range from 0 to \c size - 1,
 * and the second argument refers to a payload memory region specified via the
 * \c payload parameter.
 *
 * This payload is handled using one of two possible modes:
 *
 * <ol>
 *    <li>When <tt>size == 0</tt> or <tt>payload_deleter != nullptr</tt>, the
 *    value of \c payload is simply forwarded to \c func. In the latter case,
 *    <tt>payload_deleter(payload)</tt> is invoked following completion of the
 *    task, which can carry out additional cleanup operations if needed. In
 *    both cases, the memory region targeted by \c payload may be accessed
 *    asynchronously and must remain valid until the task is done.</li>
 *
 *    <li>Otherwise, the function will internally create a copy of the payload
 *    and free it following completion of the task. In this case, it is fine to
 *    delete the the memory region targeted by \c payload right after the
 *    function call.</li>
 * </ol>
 *
 * The function returns a task handle that can be used to schedule other
 * dependent tasks, and to wait for task completion if desired. This handle
 * must eventually be released using \ref task_release(). A failure to do
 * so will result in memory leaks.
 *
 * When the submitted task is "small" (\c size == 1), and when it does not have
 * parents, it will be executed right away without involving the thread pool,
 * in which case the function returns \c nullptr. A task of size zero si
 * handled equivalently  to unit-sized task, except that it enforces an
 * asynchronous execution.
 *
 * \remark
 *     Barriers and similar dependency relations can be encoded by via
 *     artificial tasks using <tt>size == 1</tt> and <tt>func == nullptr<tt>
 *     along with a set of parent tasks.
 *
 * \param pool
 *     The thread pool that should execute the specified task. \c nullptr
 *     refers to the default pool.
 *
 * \param parent
 *     List of parents of size \c parent_count. \c nullptr-valued elements
 *     are ignored
 *
 * \param parent_count
 *     Number of parent tasks
 *
 * \param size
 *     Total number of work units; the callback \c func will be called this
 *     many times if provided. Tasks of size 1 are considered tiny and will be
 *     executed on the current thread, in which case the function returns \c
 *     nullptr.
 *
 * \param func
 *     Callback function that will be invoked to perform the actual computation.
 *     If set to \c nullptr, the callback is ignored. This can be used to create
 *     artificial tasks that only encode dependencies.
 *
 * \param payload
 *     Optional payload that is passed to the function \c func
 *
 * \param payload_size
 *     When \c payload_deleter is equal to \c nullptr and when \c size is
 *     nonzero, a temporary copy of the payload will be made. This parameter is
 *     necessary to specify the payload size in that case.
 *
 * \param payload_deleter
 *     Optional callback that will be invoked to free the payload
 *
 * \return
 *     A task handle that must eventually be released via \ref task_release()
 */
extern ENOKI_THREAD_EXPORT
Task *task_submit_dep(Pool *pool,
                      const Task * const *parent,
                      uint32_t parent_count,
                      uint32_t size ENOKI_THREAD_DEF(1),
                      void (*func)(uint32_t, void *) ENOKI_THREAD_DEF(0),
                      void *payload ENOKI_THREAD_DEF(0),
                      uint32_t payload_size ENOKI_THREAD_DEF(0),
                      void (*payload_deleter)(void *) ENOKI_THREAD_DEF(0));

/*
 * \brief Release a task handle so that it can eventually be reused
 *
 * Releasing a task handle does not impact the tasks's execution, which could
 * be in one of three states: waiting, running, or complete. This operation is
 * important because it frees internal resources that would otherwise leak.
 *
 * Following a call to \ref task_release(), the associated task can no
 * longer be used as a direct parent of other tasks, and it is no longer
 * possible to wait for its completion using an operation like \ref
 * task_wait().
 *
 * \param pool
 *     The thread pool containing the task. \c nullptr refers to the default pool.
 *
 * \param task
 *     The task in question. When equal to \c nullptr, the operation is a no-op.
 */
extern ENOKI_THREAD_EXPORT void task_release(Task *task);

/*
 * \brief Wait for the completion of the specified task
 *
 * This function causes the calling thread to sleep until all work units of
 * 'task' have been completed.
 *
 * If an exception was caught during parallel excecution of 'task', the
 * function \ref task_wait() will re-raise this exception in the context of the
 * caller. Note that if a parallel task raises many exceptions, only a single
 * one of them will be be captured in this way.
 *
 * \param task
 *     The task in question. When equal to \c nullptr, the operation is a no-op.
 */
extern ENOKI_THREAD_EXPORT void task_wait(Task *task) ENOKI_THREAD_THROW;

/*
 * \brief Wait for the completion of the specified task and release its handle
 *
 * This function is equivalent to calling \ref task_wait() followed by \ref
 * task_release().
 *
 * If an exception was caught during parallel excecution of 'task', the
 * function \ref task_wait_and_release() will perform the release step and then
 * re-raise this exception in the context of the caller. Note that if a
 * parallel task raises many exceptions, only a single one of them will be be
 * captured in this way.
 *
 * \param task
 *     The task in question. When equal to \c nullptr, the operation is a no-op.
 */
extern ENOKI_THREAD_EXPORT void task_wait_and_release(Task *task) ENOKI_THREAD_THROW;

/// Convenience wrapper around task_submit_dep(), but without dependencies
static inline
Task *task_submit(Pool *pool,
                  uint32_t size ENOKI_THREAD_DEF(1),
                  void (*func)(uint32_t, void *) ENOKI_THREAD_DEF(0),
                  void *payload ENOKI_THREAD_DEF(0),
                  uint32_t payload_size ENOKI_THREAD_DEF(0),
                  void (*payload_deleter)(void *) ENOKI_THREAD_DEF(0)) {

    return task_submit_dep(pool, 0, 0, size, func, payload, payload_size,
                           payload_deleter);
}

/// Convenience wrapper around task_submit(), but fully synchronous
static inline
void task_submit_and_wait(Pool *pool,
                          uint32_t size ENOKI_THREAD_DEF(1),
                          void (*func)(uint32_t, void *) ENOKI_THREAD_DEF(0),
                          void *payload ENOKI_THREAD_DEF(0)) {

    Task *task = task_submit(pool, size, func, payload, 0, 0);
    task_wait_and_release(task);
}

#if defined(__cplusplus)
}

#include <utility>

namespace enoki {
    template <typename Int> struct blocked_range {
    public:
        blocked_range(Int begin, Int end, Int block_size = 1)
            : m_begin(begin), m_end(end), m_block_size(block_size) { }

        struct iterator {
            Int value;

            iterator(Int value) : value(value) { }

            Int operator*() const { return value; }
            operator Int() const { return value;}

            void operator++() { value++; }
            bool operator==(const iterator &it) { return value == it.value; }
            bool operator!=(const iterator &it) { return value != it.value; }
        };

        uint32_t blocks() const {
            return (uint32_t) ((m_end - m_begin + m_block_size - 1) / m_block_size);
        }

        iterator begin() const { return iterator(m_begin); }
        iterator end() const { return iterator(m_end); }
        Int block_size() const { return m_block_size; }

    private:
        Int m_begin;
        Int m_end;
        Int m_block_size;
    };

    template <typename Int, typename Func>
    void parallel_for(const blocked_range<Int> &range, Func &&func,
                      Pool *pool = nullptr) {

        struct Payload {
            Func *f;
            Int begin, end, block_size;
        };

        Payload payload{ &func, range.begin(), range.end(),
                         range.block_size() };

        auto callback = [](uint32_t index, void *payload) {
            Payload *p = (Payload *) payload;
            Int begin = p->begin + p->block_size * (Int) index,
                end = begin + p->block_size;

            if (end > p->end)
                end = p->end;

            (*p->f)(blocked_range<Int>(begin, end));
        };

        task_submit_and_wait(pool, range.blocks(), callback, &payload);
    }

    template <typename Int, typename Func>
    Task *parallel_for_async(const blocked_range<Int> &range, Func &&func,
                             std::initializer_list<Task *> parents = { },
                             Pool *pool = nullptr) {

        struct Payload {
            Func f;
            Int begin, end, block_size;
        };

        auto callback = [](uint32_t index, void *payload) {
            Payload *p = (Payload *) payload;
            Int begin = p->begin + p->block_size * (Int) index,
                end = begin + p->block_size;

            if (end > p->end)
                end = p->end;

            p->f(blocked_range<Int>(begin, end));
        };

        if (std::is_trivially_copyable<Func>::value &&
            std::is_trivially_destructible<Func>::value) {
            Payload payload{ std::forward<Func>(func), range.begin(),
                             range.end(), range.block_size() };

            return task_submit_dep(pool, parents.begin(),
                                   (uint32_t) parents.size(), range.blocks(),
                                   callback, &payload, sizeof(Payload));
        } else {
            Payload *payload = new Payload{ std::forward<Func>(func), range.begin(),
                                            range.end(), range.block_size() };

            auto deleter = [](void *payload) {
                delete (Payload *) payload;
            };

            return task_submit_dep(pool, parents.begin(),
                                   (uint32_t) parents.size(), range.blocks(),
                                   callback, payload, 0, deleter);
        }
    }

    template <typename Func>
    Task *parallel_do_async(Func &&func,
                            std::initializer_list<Task *> parents = { },
                            Pool *pool = nullptr) {

        struct Payload { Func f; };

        auto callback = [](uint32_t /* unused */, void *payload) {
            ((Payload *) payload)->f();
        };

        if (std::is_trivially_copyable<Func>::value &&
            std::is_trivially_destructible<Func>::value) {
            Payload payload{ std::forward<Func>(func) };

            return task_submit_dep(pool, parents.begin(),
                                   (uint32_t) parents.size(), 0, callback,
                                   &payload, sizeof(Payload));
        } else {
            Payload *payload = new Payload{ std::forward<Func>(func) };

            auto deleter = [](void *payload) { delete (Payload *) payload; };

            return task_submit_dep(pool, parents.begin(),
                                   (uint32_t) parents.size(), 0, callback,
                                   payload, 0, deleter);
        }
    }
}
#endif
