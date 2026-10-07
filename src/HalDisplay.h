#pragma once
#include <Arduino.h>
#include <EInkDisplay.h>

#include <cstdint>

class HalDisplay {
public:
  // CrossDink mirrors the SDK's grayscale capability types on HalDisplay. The
  // simulator composes grayscale previews from overlay masks only, so it
  // reports Absolute/Direct as unsupported and callers keep the overlay path.
  enum class GrayscaleMode : uint8_t { Overlay, Absolute, Direct };
  struct GrayscaleCapabilities {
    bool overlay = false;
    constexpr bool supported() const { return overlay; }
  };
  GrayscaleCapabilities
  grayscaleCapabilities(GrayscaleMode mode = GrayscaleMode::Overlay) const;

  // Constructor with pin configuration
  HalDisplay();

  // Destructor
  ~HalDisplay();

  // Refresh modes
  enum RefreshMode {
    FULL_REFRESH, // Full refresh with complete waveform
    HALF_REFRESH, // Half refresh (1720ms) - balanced quality and speed
    FAST_REFRESH  // Fast refresh using custom LUT
  };

  // Initialize the display hardware and driver
  void begin();
  void begin(bool seamless);
  // The simulator always presents the framebuffer as-is, so there is no
  // previous panel frame for the driver to diff against.
  bool seedDisplayedFrame(const uint8_t *frame);
  bool grayOnPanel() const;

  // Display dimensions
  static constexpr uint16_t DISPLAY_WIDTH = EInkDisplay::DISPLAY_WIDTH;
  static constexpr uint16_t DISPLAY_HEIGHT = EInkDisplay::DISPLAY_HEIGHT;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  // Frame buffer operations
  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t *imageData, uint16_t x, uint16_t y, uint16_t w,
                 uint16_t h, bool fromProgmem = false) const;
  void drawImageTransparent(const uint8_t *imageData, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h,
                            bool fromProgmem = false) const;

  // Persistent black/white polarity used by the X4 Pro frontlight panel.
  void setInverted(bool inverted);
  bool toggleInverted();
  bool isInverted() const;

  void displayBuffer(RefreshMode mode = RefreshMode::FAST_REFRESH,
                     bool turnOffScreen = false);
  void displayBufferAsync(RefreshMode mode = RefreshMode::FAST_REFRESH);
  void waitRefreshComplete();
  void displayBufferDeferred(RefreshMode mode = RefreshMode::FAST_REFRESH);
  bool isRefreshPending() const;
  bool isRefreshBusy();
  bool supportsAsyncRefresh() const;
  bool supportsAsyncGrayscaleBase() const;
  void displayWindow(int x, int y, int w, int h);
  void refreshDisplay(RefreshMode mode = RefreshMode::FAST_REFRESH,
                      bool turnOffScreen = false);

  // Power management
  void deepSleep();
  bool powerOffIdle();
  bool powerOnIdle();
  void setRefreshLightSleep(bool allowed);

  // Access to frame buffer
  uint8_t *getFrameBuffer() const;
  uint8_t *lendFrameBufferStorage(uint32_t *sizeOut);
  void returnFrameBufferStorage();

  // Runtime geometry passthrough
  uint16_t getDisplayWidth() const;
  uint16_t getDisplayHeight() const;
  uint16_t getDisplayWidthBytes() const;
  uint32_t getBufferSize() const;

  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH,
                            bool turnOffScreen = false);
  bool displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback,
                            bool turnOffScreen = false);
  bool displayGrayscaleBaseAsync(RefreshMode fallback = FAST_REFRESH);
  bool supportsDeferredGrayscaleBase() const;
  void preconditionGrayscale();
  void preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

  void copyGrayscaleBuffers(const uint8_t *lsbBuffer, const uint8_t *msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t *lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t *msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t *bwBuffer);

  void displayGrayBuffer(bool turnOffScreen = false,
                         const unsigned char *lut = nullptr,
                         bool factoryMode = false);
  void displayFactoryGrayBuffer(bool turnOffScreen = false);

  // The simulator intentionally advertises strip grayscale support so host
  // builds exercise the same low-memory path as the device firmware, and so
  // streamed plane data can feed the same grayscale preview compositor as the
  // legacy full-frame API.
  void writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t *rows,
                                uint16_t yStart, uint16_t numRows);
  bool supportsStripGrayscale() const;

  // Refresh flash timing for the frontlight duck. The simulator has no panel
  // waveform, so nothing ever flashes.
  enum class FlashKind : uint8_t { Gray, Full, Paint, GrayDark };
  uint32_t flashStartedMs() const;
  uint32_t flashMarkedMs() const;
  uint32_t flashEndsMs() const;
  FlashKind flashKind() const;
  uint32_t flashPlannedMs() const;
  FlashKind flashPlannedKind() const;
  enum { GRAY_PASSES = 3, FLASHING = 4 };
  struct RefreshCounts {
    uint32_t magic;
    uint32_t n[5];
  };
  // Zero outside the firmware's RTC-backed builds, as on device.
  static RefreshCounts &refreshCounts();

  bool grayShotReady() const;
  uint8_t grayShotLevel(uint32_t x, uint32_t y) const;
  bool shouldSkipImageBlanking() const;
  void setSmoothGray(bool smooth);
  void setInvertedTextGray(bool enabled);
  bool fastTracksPanel() const;

  // Simulator only: call from main thread to push rendered pixels to SDL.
  void presentIfNeeded();
  // Simulator only: returns true once a hard shutdown has been requested.
  bool shouldQuit() const;

private:
  EInkDisplay einkDisplay;
  bool inverted = false;
};

extern HalDisplay display;
