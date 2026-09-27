#pragma once

namespace ftl {

// Interface for platform-specific thread implementation.
class IThread {
 public:
  virtual ~IThread() = default;

  // Blocks until thread completes execution.
  virtual void join() = 0;

  // Detaches the thread from current context.
  virtual void detach() = 0;

  // Returns true if joinable.
  virtual bool joinable() const = 0;
};

}  // namespace ftl
