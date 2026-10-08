// Headless (Xvfb) GUI smoke test for the CLAP build on Linux: embeds the
// plugin editor into an X11 window, drives JUCE's event loop through the
// CLAP posix-fd/timer host extensions for a while, then saves a screenshot.
//
//   xvfb-run -s "-screen 0 1024x768x24" clap_gui <plugin.clap> <out.png> [seconds]
#include <clap/clap.h>
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <poll.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

struct Fd { int fd; clap_posix_fd_flags_t flags; };
struct Timer { clap_id id; uint32_t periodMs; std::chrono::steady_clock::time_point next; };
static std::vector<Fd> g_fds;
static std::vector<Timer> g_timers;
static clap_id g_nextTimer = 1;
static bool g_callbackRequested = false;
static uint32_t g_reqW = 0, g_reqH = 0;

static bool fdRegister(const clap_host_t *, int fd, clap_posix_fd_flags_t flags) { g_fds.push_back({fd, flags}); return true; }
static bool fdModify(const clap_host_t *, int fd, clap_posix_fd_flags_t flags)
{
    for (auto &f : g_fds) if (f.fd == fd) { f.flags = flags; return true; }
    return false;
}
static bool fdUnregister(const clap_host_t *, int fd)
{
    for (size_t i = 0; i < g_fds.size(); ++i) if (g_fds[i].fd == fd) { g_fds.erase(g_fds.begin() + (long)i); return true; }
    return false;
}
static bool timerRegister(const clap_host_t *, uint32_t ms, clap_id *id)
{
    *id = g_nextTimer++;
    g_timers.push_back({*id, ms ? ms : 1, std::chrono::steady_clock::now()});
    return true;
}
static bool timerUnregister(const clap_host_t *, clap_id id)
{
    for (size_t i = 0; i < g_timers.size(); ++i) if (g_timers[i].id == id) { g_timers.erase(g_timers.begin() + (long)i); return true; }
    return false;
}
static void guiResizeHints(const clap_host_t *) {}
static bool guiRequestResize(const clap_host_t *, uint32_t w, uint32_t h) { g_reqW = w; g_reqH = h; return true; }
static bool guiRequestShow(const clap_host_t *) { return true; }
static bool guiRequestHide(const clap_host_t *) { return true; }
static void guiClosed(const clap_host_t *, bool) {}

static const clap_host_posix_fd_support_t s_fd{fdRegister, fdModify, fdUnregister};
static const clap_host_timer_support_t s_timer{timerRegister, timerUnregister};
static const clap_host_gui_t s_gui{guiResizeHints, guiRequestResize, guiRequestShow, guiRequestHide, guiClosed};

static const void *hostGetExtension(const clap_host_t *, const char *id)
{
    std::string s = id;
    if (s == CLAP_EXT_POSIX_FD_SUPPORT) return &s_fd;
    if (s == CLAP_EXT_TIMER_SUPPORT) return &s_timer;
    if (s == CLAP_EXT_GUI) return &s_gui;
    return nullptr;
}
static void hostNoop(const clap_host_t *) {}
static void hostRequestCallback(const clap_host_t *) { g_callbackRequested = true; }

// Like most hosts, don't die on X errors (Xvfb has no window manager atoms).
static int g_xErrors = 0;
static int xErrorHandler(Display *, XErrorEvent *) { ++g_xErrors; return 0; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s plugin.clap out.png [seconds]\n", argv[0]); return 2; }
    double seconds = argc > 3 ? atof(argv[3]) : 3.0;

    XSetErrorHandler(xErrorHandler);
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) { fprintf(stderr, "no X display\n"); return 1; }

    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto entry = (const clap_plugin_entry_t *)dlsym(lib, "clap_entry");
    entry->init(argv[1]);
    auto factory = (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    auto desc = factory->get_plugin_descriptor(factory, 0);

    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "clap_gui"; host.vendor = "NULL JSFX"; host.version = "1";
    host.get_extension = hostGetExtension;
    host.request_restart = hostNoop; host.request_process = hostNoop;
    host.request_callback = hostRequestCallback;

    auto plugin = factory->create_plugin(factory, &host, desc->id);
    if (!plugin || !plugin->init(plugin)) { fprintf(stderr, "create/init failed\n"); return 1; }
    auto gui = (const clap_plugin_gui_t *)plugin->get_extension(plugin, CLAP_EXT_GUI);
    if (!gui || !gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false)) { fprintf(stderr, "no x11 gui\n"); return 1; }
    if (!gui->create(plugin, CLAP_WINDOW_API_X11, false)) { fprintf(stderr, "gui create failed\n"); return 1; }

    uint32_t w = 0, h = 0;
    gui->get_size(plugin, &w, &h);
    printf("initial editor size: %ux%u\n", w, h);
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, w ? w : 400, h ? h : 300, 0, 0, 0);
    XMapWindow(dpy, win);
    XFlush(dpy);
    clap_window_t cw{};
    cw.api = CLAP_WINDOW_API_X11;
    cw.x11 = win;
    XSetErrorHandler(xErrorHandler);
    if (!gui->set_parent(plugin, &cw)) { fprintf(stderr, "set_parent failed\n"); return 1; }
    gui->show(plugin);

    auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < end) {
        std::vector<pollfd> pfds;
        for (auto &f : g_fds) pfds.push_back({f.fd, (short)((f.flags & CLAP_POSIX_FD_READ ? POLLIN : 0) | (f.flags & CLAP_POSIX_FD_WRITE ? POLLOUT : 0)), 0});
        poll(pfds.data(), pfds.size(), 5);
        for (auto &p : pfds)
            if (p.revents) {
                clap_posix_fd_flags_t fl = 0;
                if (p.revents & POLLIN) fl |= CLAP_POSIX_FD_READ;
                if (p.revents & POLLOUT) fl |= CLAP_POSIX_FD_WRITE;
                if (p.revents & (POLLERR | POLLHUP)) fl |= CLAP_POSIX_FD_ERROR;
                auto fdExt = (const clap_plugin_posix_fd_support_t *)plugin->get_extension(plugin, CLAP_EXT_POSIX_FD_SUPPORT);
                if (fdExt) fdExt->on_fd(plugin, p.fd, fl);
            }
        auto now = std::chrono::steady_clock::now();
        auto timerExt = (const clap_plugin_timer_support_t *)plugin->get_extension(plugin, CLAP_EXT_TIMER_SUPPORT);
        for (size_t i = 0; i < g_timers.size(); ++i)
            if (now >= g_timers[i].next) {
                g_timers[i].next = now + std::chrono::milliseconds(g_timers[i].periodMs);
                if (timerExt) timerExt->on_timer(plugin, g_timers[i].id);
            }
        if (g_callbackRequested) { g_callbackRequested = false; plugin->on_main_thread(plugin); }
        if (g_reqW && g_reqH) {
            printf("plugin requested resize: %ux%u\n", g_reqW, g_reqH);
            XResizeWindow(dpy, win, g_reqW, g_reqH);
            XFlush(dpy);
            gui->set_size(plugin, g_reqW, g_reqH);
            g_reqW = g_reqH = 0;
        }
        while (XPending(dpy)) { XEvent e; XNextEvent(dpy, &e); }
    }
    gui->get_size(plugin, &w, &h);
    printf("final editor size: %ux%u (ignored X errors: %d)\n", w, h, g_xErrors);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "import -window 0x%lx '%s'", (unsigned long)win, argv[2]);
    int rc = system(cmd);
    printf("screenshot: %s (rc=%d)\n", argv[2], rc);

    gui->hide(plugin);
    gui->destroy(plugin);
    plugin->destroy(plugin);
    entry->deinit();
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return rc == 0 ? 0 : 1;
}
