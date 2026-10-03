#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "spatialgl/glfw_output.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef __APPLE__
#include <pthread.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace spatialgl::raster {
namespace {
struct Library {
#ifdef _WIN32
  HMODULE handle = nullptr;
#else
  void* handle = nullptr;
#endif
  template <typename T>
  T symbol(const char* name) {
#ifdef _WIN32
    auto p = reinterpret_cast<T>(GetProcAddress(handle, name));
#else
    auto p = reinterpret_cast<T>(dlsym(handle, name));
#endif
    if (!p) throw std::runtime_error(std::string("GLFW runtime is missing symbol ") + name);
    return p;
  }
  ~Library() {
#ifdef _WIN32
    if (handle) FreeLibrary(handle);
#else
    if (handle) dlclose(handle);
#endif
  }
};
std::shared_ptr<Library> load_library(const std::string& path) {
  auto lib = std::make_shared<Library>();
#ifdef _WIN32
  if (!path.empty())
    lib->handle = LoadLibraryA(path.c_str());
  else
    for (const char* n : {"glfw3.dll", "glfw.dll"})
      if ((lib->handle = LoadLibraryA(n))) break;
#else
  if (!path.empty())
    lib->handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  else
    for (const char* n : {"libglfw.3.dylib", "libglfw.dylib", "libglfw.so.3", "libglfw.so"})
      if ((lib->handle = dlopen(n, RTLD_NOW | RTLD_LOCAL))) break;
#endif
  if (!lib->handle) throw std::runtime_error("Could not load GLFW runtime library");
  return lib;
}
struct Api {
  std::shared_ptr<Library> lib;
  int (*init)() = nullptr;
  void (*terminate)() = nullptr;
  GLFWmonitor** (*get_monitors)(int*) = nullptr;
  const char* (*monitor_name)(GLFWmonitor*) = nullptr;
  const GLFWvidmode* (*video_mode)(GLFWmonitor*) = nullptr;
  void (*window_hint)(int, int) = nullptr;
  GLFWwindow* (*create_window)(int, int, const char*, GLFWmonitor*, GLFWwindow*) = nullptr;
  void (*destroy_window)(GLFWwindow*) = nullptr;
  void (*make_current)(GLFWwindow*) = nullptr;
  void (*swap_buffers)(GLFWwindow*) = nullptr;
  void (*poll_events)() = nullptr;
  int (*should_close)(GLFWwindow*) = nullptr;
  GLFWglproc (*get_proc)(const char*) = nullptr;
  void (*get_framebuffer_size)(GLFWwindow*, int*, int*) = nullptr;
  void (*set_window_pos)(GLFWwindow*, int, int) = nullptr;
  explicit Api(std::shared_ptr<Library> l) : lib(std::move(l)) {
#define LOAD(field, name) field = lib->symbol<decltype(field)>(name)
    LOAD(init, "glfwInit");
    LOAD(terminate, "glfwTerminate");
    LOAD(get_monitors, "glfwGetMonitors");
    monitor_name = lib->symbol<decltype(monitor_name)>("glfwGetMonitorName");
#undef LOAD
    video_mode = lib->symbol<decltype(video_mode)>("glfwGetVideoMode");
    window_hint = lib->symbol<decltype(window_hint)>("glfwWindowHint");
    create_window = lib->symbol<decltype(create_window)>("glfwCreateWindow");
    destroy_window = lib->symbol<decltype(destroy_window)>("glfwDestroyWindow");
    make_current = lib->symbol<decltype(make_current)>("glfwMakeContextCurrent");
    swap_buffers = lib->symbol<decltype(swap_buffers)>("glfwSwapBuffers");
    poll_events = lib->symbol<decltype(poll_events)>("glfwPollEvents");
    should_close = lib->symbol<decltype(should_close)>("glfwWindowShouldClose");
    get_proc = lib->symbol<decltype(get_proc)>("glfwGetProcAddress");
    get_framebuffer_size = lib->symbol<decltype(get_framebuffer_size)>("glfwGetFramebufferSize");
    set_window_pos = lib->symbol<decltype(set_window_pos)>("glfwSetWindowPos");
  }
};
struct SessionState {
  std::mutex mutex;
  std::size_t references = 0;
  std::thread::id owner;
  void* library_handle = nullptr;
  std::shared_ptr<Library> library;
};
SessionState& session_state() {
  static SessionState state;
  return state;
}
bool on_platform_main_thread() {
#ifdef __APPLE__
  return pthread_main_np() != 0;
#else
  return true;
#endif
}
void acquire_session(Api& api) {
  if (!on_platform_main_thread())
    throw std::logic_error("GLFW initialization and use must run on the macOS main thread");
  auto& s = session_state();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (s.references != 0) {
    if (s.owner != std::this_thread::get_id())
      throw std::logic_error("GLFW must be initialized and used on one owner thread");
    if (s.library_handle != api.lib->handle)
      throw std::logic_error("another GLFW runtime is already active");
  } else {
    if (!api.init())
      throw std::runtime_error("GLFW initialization failed (a display server may be unavailable)");
    s.owner = std::this_thread::get_id();
    s.library_handle = api.lib->handle;
    s.library = api.lib;
  }
  ++s.references;
}
void release_session(Api& api) {
  auto& s = session_state();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (s.references == 0) return;
  if (s.owner != std::this_thread::get_id())
    throw std::logic_error("GLFW session release must run on its owner thread");
  if (--s.references == 0) {
    api.terminate();
    s.library_handle = nullptr;
    s.library.reset();
    s.owner = {};
  }
}
struct GL {
  using Int = int;
  using Size = int;
  using Float = float;
  using Enum = unsigned int;
  using Bitfield = unsigned int;
  void (*viewport)(Int, Int, Size, Size);
  void (*clear_color)(Float, Float, Float, Float);
  void (*clear)(Bitfield);
  void (*pixel_store)(Enum, Int);
  void (*raster_pos)(Float, Float);
  void (*pixel_zoom)(Float, Float);
  void (*draw_pixels)(Size, Size, Enum, Enum, const void*);
  Enum (*get_error)();
  explicit GL(Api& a) {
#define GET(field, name)                                       \
  field = reinterpret_cast<decltype(field)>(a.get_proc(name)); \
  if (!field) throw std::runtime_error(std::string("OpenGL function unavailable: ") + name)
    GET(viewport, "glViewport");
    GET(clear_color, "glClearColor");
    GET(clear, "glClear");
    GET(pixel_store, "glPixelStorei");
    GET(raster_pos, "glRasterPos2f");
    GET(pixel_zoom, "glPixelZoom");
    GET(draw_pixels, "glDrawPixels");
    GET(get_error, "glGetError");
#undef GET
  }
};
constexpr unsigned int kColorBufferBit = 0x00004000, kUnpackAlignment = 0x0CF5, kRgb = 0x1907,
                       kUnsignedByte = 0x1401;
}  // namespace
struct GlfwOutput::Impl {
  std::shared_ptr<Library> library;
  Api api;
  GLFWwindow* window = nullptr;
  std::unique_ptr<GL> gl;
  std::thread::id owner = std::this_thread::get_id();
  int width = 0, height = 0;
  bool session_acquired = false;
  double expiry = -std::numeric_limits<double>::infinity();
  double last_now = -std::numeric_limits<double>::infinity();
  double last_frame_time = -std::numeric_limits<double>::infinity();
  bool closed = false;
  Impl(int w, int h, const std::string& title, int display, const std::string& path)
      : library(load_library(path)), api(library), width(w), height(h) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384)
      throw std::invalid_argument("invalid GLFW window dimensions");
    if (display < -1)
      throw std::invalid_argument("display index must be -1 or a nonnegative monitor index");
    acquire_session(api);
    session_acquired = true;
    try {
      int count = 0;
      auto monitors = api.get_monitors(&count);
      GLFWmonitor* monitor = nullptr;
      if (display >= 0) {
        if (display >= count) throw std::out_of_range("display index is unavailable");
        monitor = monitors[display];
      }
      api.window_hint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      api.window_hint(GLFW_CONTEXT_VERSION_MINOR, 1);
      window = api.create_window(w, h, title.c_str(), monitor, nullptr);
      if (!window) throw std::runtime_error("GLFW window creation failed");
      api.make_current(window);
      gl = std::make_unique<GL>(api);
      clear_now();
    } catch (...) {
      if (window) api.destroy_window(window);
      window = nullptr;
      if (session_acquired) {
        release_session(api);
        session_acquired = false;
      }
      throw;
    }
  }
  void check_thread() const {
    if (owner != std::this_thread::get_id())
      throw std::logic_error("GLFW output must be used on its owner thread");
  }
  void check_gl() {
    const auto error = gl->get_error();
    if (error != 0)
      throw std::runtime_error("OpenGL presentation failed with error " + std::to_string(error));
  }
  void clear_now() {
    if (!window || !gl) return;
    api.make_current(window);
    int fw = 0, fh = 0;
    api.get_framebuffer_size(window, &fw, &fh);
    gl->viewport(0, 0, fw, fh);
    gl->clear_color(0, 0, 0, 1);
    gl->clear(kColorBufferBit);
    check_gl();
    api.swap_buffers(window);
  }
  [[noreturn]] void fail_closed(std::exception_ptr failure) {
    try {
      clear_now();
      expiry = -std::numeric_limits<double>::infinity();
    } catch (...) {
      try {
        close();
      } catch (...) {
      }
    }
    std::rethrow_exception(failure);
  }
  void close() {
    if (closed) return;
    check_thread();
    std::exception_ptr failure;
    try {
      clear_now();
    } catch (...) {
      failure = std::current_exception();
    }
    if (window) {
      api.destroy_window(window);
      window = nullptr;
    }
    if (session_acquired) {
      release_session(api);
      session_acquired = false;
    }
    closed = true;
    if (failure) std::rethrow_exception(failure);
  }
  ~Impl() {
    if (!closed && std::this_thread::get_id() == owner) {
      try {
        close();
      } catch (...) {
        if (window) {
          api.destroy_window(window);
          window = nullptr;
        }
        if (session_acquired) {
          release_session(api);
          session_acquired = false;
        }
        closed = true;
      }
    }
  }
};
GlfwOutput::GlfwOutput(int w, int h, const std::string& title, int display, const std::string& path)
    : impl_(std::make_unique<Impl>(w, h, title, display, path)) {}
GlfwOutput::~GlfwOutput() = default;
GlfwOutput::GlfwOutput(GlfwOutput&&) noexcept = default;
GlfwOutput& GlfwOutput::operator=(GlfwOutput&&) noexcept = default;
std::vector<DisplayInfo> GlfwOutput::displays(const std::string& path) {
  auto lib = load_library(path);
  Api a(lib);
  acquire_session(a);
  try {
    int n = 0;
    auto monitors = a.get_monitors(&n);
    std::vector<DisplayInfo> out;
    for (int i = 0; i < n; i++) {
      const auto* m = a.video_mode(monitors[i]);
      const char* name = a.monitor_name(monitors[i]);
      out.push_back({i, name ? name : "", m ? m->width : 0, m ? m->height : 0});
    }
    release_session(a);
    return out;
  } catch (...) {
    try {
      release_session(a);
    } catch (...) {
    }
    throw;
  }
}

bool GlfwOutput::present(const RasterFrame& f, double now) {
  if (!impl_ || impl_->closed) return false;
  auto& p = *impl_;
  p.check_thread();
  auto reject = [&](const char* message) {
    p.fail_closed(std::make_exception_ptr(std::invalid_argument(message)));
  };
  if (!std::isfinite(now) || now < p.last_now)
    reject("presentation time must be finite and monotonic");
  p.last_now = now;
  p.api.poll_events();
  if (p.api.should_close(p.window)) {
    close();
    return false;
  }
  if (f.width <= 0 || f.height <= 0 || f.width > 16384 || f.height > 16384 ||
      static_cast<std::uint64_t>(f.width) * f.height > 64000000 ||
      f.rgb.size() != static_cast<std::size_t>(f.width) * f.height * 3 ||
      !std::isfinite(f.expires_at) || !std::isfinite(f.time))
    reject("malformed raster frame");
  if (f.time > now) reject("cannot present a future raster frame");
  if (f.time < p.last_frame_time) reject("cannot replay an older raster frame");
  p.last_frame_time = f.time;
  try {
    p.api.make_current(p.window);
    int w = 0, h = 0;
    p.api.get_framebuffer_size(p.window, &w, &h);
    p.gl->viewport(0, 0, w, h);
    p.gl->clear_color(0, 0, 0, 1);
    p.gl->clear(kColorBufferBit);
    p.check_gl();
    if (now < f.expires_at) {
      std::vector<std::uint8_t> bottom_up(f.rgb.size());
      const std::size_t row = static_cast<std::size_t>(f.width) * 3;
      for (int y = 0; y < f.height; y++)
        std::memcpy(bottom_up.data() + static_cast<std::size_t>(f.height - 1 - y) * row,
                    f.rgb.data() + static_cast<std::size_t>(y) * row, row);
      p.gl->pixel_store(kUnpackAlignment, 1);
      p.gl->raster_pos(-1, -1);
      p.gl->pixel_zoom(static_cast<float>(w) / f.width, static_cast<float>(h) / f.height);
      p.gl->draw_pixels(f.width, f.height, kRgb, kUnsignedByte, bottom_up.data());
      p.gl->pixel_zoom(1, 1);
      p.check_gl();
      p.expiry = f.expires_at;
    } else
      p.expiry = -std::numeric_limits<double>::infinity();
    p.api.swap_buffers(p.window);
  } catch (...) {
    p.fail_closed(std::current_exception());
  }
  return true;
}

bool GlfwOutput::poll(double now) {
  if (!impl_ || impl_->closed) return false;
  auto& p = *impl_;
  p.check_thread();
  if (!std::isfinite(now) || now < p.last_now)
    p.fail_closed(std::make_exception_ptr(
        std::invalid_argument("presentation time must be finite and monotonic")));
  p.last_now = now;
  p.api.poll_events();
  if (p.api.should_close(p.window)) {
    close();
    return false;
  }
  if (now >= p.expiry && p.expiry != -std::numeric_limits<double>::infinity()) {
    try {
      p.clear_now();
      p.expiry = -std::numeric_limits<double>::infinity();
    } catch (...) {
      p.fail_closed(std::current_exception());
    }
  }
  return true;
}
void GlfwOutput::blackout() {
  if (impl_ && !impl_->closed) {
    impl_->check_thread();
    try {
      impl_->clear_now();
      impl_->expiry = -std::numeric_limits<double>::infinity();
    } catch (...) {
      impl_->fail_closed(std::current_exception());
    }
  }
}
void GlfwOutput::close() {
  if (impl_) impl_->close();
}
}  // namespace spatialgl::raster
