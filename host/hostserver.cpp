// Real-time host server: WebSocket stream (default port 81) + the web pages (http) for browser tests.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <string>
#define PROGMEM
#include "../Heltec-AppleII/apple2.h"
#include "../Heltec-AppleII/stream.h"
#include "../Heltec-AppleII/player.h"
#include "../Heltec-AppleII/webui.h"

struct SockConn : NetConn {
  int fd; bool ok = true;
  explicit SockConn(int f) : fd(f) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one); }
  int read(uint8_t* b, int n) override { int r = ::recv(fd, b, n, 0); if (r == 0) { ok = false; return 0; } if (r < 0) { if (errno != EAGAIN) ok = false; return 0; } return r; }
  bool write(const uint8_t* b, int n) override { while (n > 0) { int w = ::send(fd, b, n, MSG_NOSIGNAL); if (w < 0) { if (errno == EAGAIN) { usleep(500); continue; } ok = false; return false; } b += w; n -= w; } return true; }
  bool connected() override { return ok; }
  void close() override { ::close(fd); }
};

static int listenOn(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0); int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
  if (bind(s, (sockaddr*)&a, sizeof a) < 0) { perror("bind"); exit(1); }
  listen(s, 8); fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
  return s;
}

int runServer(Apple2& m, int wsPort, int webPort) {
  static InputQueue q;
  static StreamServer stream(m, q);
  stream.onPaste = [&](const std::string& t) { m.typeText(t.c_str()); };
  static volatile bool resetReq = false;
  std::atomic<bool> run{true};
  std::thread emu([&] {
    auto next = std::chrono::steady_clock::now();
    while (run) {
      q.apply(m, resetReq);
      if (resetReq) { resetReq = false; m.reset(); }
      m.runFrame();
      next += std::chrono::microseconds((int)A2_FRAME_US);
      std::this_thread::sleep_until(next);
    }
  });
  int ws = listenOn(wsPort), web = webPort ? listenOn(webPort) : -1;
  fprintf(stderr, "ws on %d, web on %d\n", wsPort, webPort);
  for (;;) {
    int c = accept(ws, nullptr, nullptr);
    if (c >= 0) stream.attach(new SockConn(c));
    if (web >= 0) {
      int h = accept(web, nullptr, nullptr);
      if (h >= 0) {
        char buf[4096]; usleep(20000); int n = recv(h, buf, sizeof buf - 1, 0); buf[n > 0 ? n : 0] = 0;
        std::string req(buf), body, type = "text/html";
        if (req.rfind("GET / ", 0) == 0) body = PLAYER_HTML;
        else if (req.rfind("GET /settings", 0) == 0) body = SETTINGS_HTML;
        else if (req.find("/api/status") != std::string::npos) { type = "application/json";
          body = "{\"ip\":\"localhost\",\"mode\":\"sta\",\"ssid\":\"host\",\"rssi\":-40,\"viewer\":true,\"sound\":false,\"speed\":100,\"warp\":false,\"romName\":\"Apple IIe (16 KB)\",\"romCustom\":true,\"romCrc\":\"00000000\",\"drives\":[{\"name\":\"DOS33.DSK\",\"wp\":false,\"track\":17,\"busy\":true},{\"name\":\"\",\"wp\":false,\"track\":0,\"busy\":false}],\"disks\":[{\"n\":\"DOS33.DSK\",\"s\":143360},{\"n\":\"GAME.PO\",\"s\":143360}],\"fsfree\":4800000}"; }
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        send(h, resp.data(), resp.size(), MSG_NOSIGNAL); close(h);
      }
    }
    stream.poll();
    usleep(1000);
  }
}
