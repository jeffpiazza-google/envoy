#pragma once

#include <functional>

#include "envoy/common/pure.h"
#include "envoy/event/dispatcher.h"

namespace Envoy {
namespace Server {

class ServerLifecycleNotifier {
public:
  virtual ~ServerLifecycleNotifier() = default;

  /**
   * Stages of the envoy server instance lifecycle.
   */
  enum class Stage {
    /**
     * The server instance main thread is about to enter the dispatcher loop.
     */
    Startup,

    /**
     * The server instance is being shutdown and the dispatcher is about to exit.
     * This provides listeners a last chance to run a callback on the main dispatcher.
     * Note: the server will wait for callbacks that registered to take a completion
     * before exiting the dispatcher loop.
     */
    ShutdownExit
  };

  /**
   * Callback invoked when the server reaches a certain lifecycle stage.
   *
   * Instances of the second type which take an Event::PostCb parameter must post
   * that callback to the main dispatcher when they have finished processing of
   * the new lifecycle state. This is useful when the main dispatcher needs to
   * wait for registered callbacks to finish their work before continuing, eg.
   * during server shutdown.
   */
  using StageCallback = std::function<void()>;
  using StageCallbackWithCompletion = std::function<void(Event::PostCb)>;

  /**
   * Register a callback function that will be invoked on the main thread when
   * the specified stage is reached.
   *
   * The second version which takes a completion back is currently only supported
   * for the ShutdownExit stage.
   */
  virtual void registerCallback(Stage stage, StageCallback callback) PURE;
  virtual void registerCallback(Stage stage, StageCallbackWithCompletion callback) PURE;
};

} // namespace Server
} // namespace Envoy
