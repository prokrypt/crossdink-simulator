#pragma once
#if defined(CROSSPOINT_SIM_USE_SDK_HTTP)
#include "Client.h"
#include <memory>
// Socket transport for running the real SDK HTTP parser in the simulator.
class WiFiClient : public Client {
public:
  WiFiClient() = default;
  WiFiClient(const WiFiClient &) = delete;
  WiFiClient &operator=(const WiFiClient &) = delete;
  int socketFd() const { return fd_; }
  ~WiFiClient() override { stop(); }
  int connect(IPAddress ip, uint16_t port) override {
    return connect(ip.toString().c_str(), port);
  }
  int connect(const char *, uint16_t) override;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *, size_t) override;
  int available() override;
  int read() override {
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
  }
  int read(uint8_t *, size_t) override;
  int peek() override;
  void flush() override {}
  void stop() override;
  uint8_t connected() override;
  operator bool() override { return connected(); }
  void setConnectionTimeout(uint32_t ms) { connectTimeout_ = ms; }

private:
  int fd_ = -1;
  uint32_t connectTimeout_ = 10000;
};

#else
#include "WiFi.h"
#endif
