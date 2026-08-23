#pragma once

#include <cstdint>
#include <functional>

#include "activities/Activity.h"
#include "activities/ScreenTransitionRefresh.h"

/**
 * Activity for testing KOReader credentials, or — in sign-up mode — creating a
 * new account on the sync server with the entered username/password.
 * Connects to WiFi, then authenticates or registers.
 */
class KOReaderAuthActivity final : public Activity {
 public:
  enum class Mode { AUTHENTICATE, SIGN_UP };

  explicit KOReaderAuthActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode = Mode::AUTHENTICATE)
      : Activity("KOReaderAuth", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == CONNECTING || state == AUTHENTICATING; }

 private:
  enum State { WIFI_SELECTION, CONNECTING, AUTHENTICATING, SUCCESS, FAILED };

  Mode mode = Mode::AUTHENTICATE;
  State state = WIFI_SELECTION;
  ScreenTransitionRefresh screenTransitionRefresh;
  std::string statusMessage;
  std::string errorMessage;
  // This screen releases the SD font twice on the full path: once in onEnter
  // and again to drop the registry after connecting. The second can be skipped,
  // so count them and repay exactly that many restoreAfterRelease() calls on
  // exit. Repaying too few leaves the CJK UI fallback unloaded; too many would
  // steal an enclosing screen's restore.
  uint8_t fontReleases = 0;

  void onWifiSelectionComplete(bool success);
  void performAuthentication();
};
