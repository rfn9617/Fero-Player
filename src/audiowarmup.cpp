#include "audiowarmup.h"

#ifdef _WIN32
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
constexpr auto KeepAwake = std::chrono::minutes(20);

std::mutex mutex;
std::thread worker;
std::atomic_bool running{false}, quit{false};
std::atomic<long long> deadline{0};

long long now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

template <typename T> void release(T *&p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// One silent stream on the current default device; returns when the device
// changes (so the new one is warmed), on timeout or on quit.
void runStream(IMMDeviceEnumerator *enumerator) {
    IMMDevice *device = nullptr;
    IAudioClient *client = nullptr;
    IAudioRenderClient *render = nullptr;
    WAVEFORMATEX *format = nullptr;
    LPWSTR id = nullptr;
    UINT32 frames = 0;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) || FAILED(device->GetId(&id)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client))) ||
        FAILED(client->GetMixFormat(&format)) ||
        FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 2000000 /* 200 ms */, 0, format, nullptr)) ||
        FAILED(client->GetBufferSize(&frames)) ||
        FAILED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void **>(&render)))) {
        // No device or it is held exclusively: retry later.
        for (int i = 0; i < 50 && !quit && now() < deadline; ++i)
            Sleep(100);
    } else {
        BYTE *data = nullptr;
        if (SUCCEEDED(render->GetBuffer(frames, &data)))
            render->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
        client->Start();
        int checks = 0;
        while (!quit && now() < deadline) {
            Sleep(50);
            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding)))
                break;   // device removed or changed format
            const UINT32 avail = frames - padding;
            if (avail && SUCCEEDED(render->GetBuffer(avail, &data)))
                render->ReleaseBuffer(avail, AUDCLNT_BUFFERFLAGS_SILENT);
            if (++checks % 40 == 0) {   // every 2 s: follow default-device changes
                IMMDevice *current = nullptr;
                LPWSTR currentId = nullptr;
                bool changed = false;
                if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &current)) &&
                    SUCCEEDED(current->GetId(&currentId)))
                    changed = wcscmp(currentId, id) != 0;
                if (currentId)
                    CoTaskMemFree(currentId);
                release(current);
                if (changed)
                    break;
            }
        }
        client->Stop();
    }
    if (format)
        CoTaskMemFree(format);
    if (id)
        CoTaskMemFree(id);
    release(render);
    release(client);
    release(device);
}

void threadMain() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        IMMDeviceEnumerator *enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator)))) {
            while (!quit && now() < deadline)
                runStream(enumerator);
            release(enumerator);
        }
        CoUninitialize();
    }
    running = false;
}
} // namespace

namespace AudioWarmup {
void touch() {
    deadline = now() + std::chrono::duration_cast<std::chrono::milliseconds>(KeepAwake).count();
    if (running)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    if (running)
        return;
    if (worker.joinable())
        worker.join();
    quit = false;
    running = true;
    worker = std::thread(threadMain);
}

void stop() {
    std::lock_guard<std::mutex> lock(mutex);
    quit = true;
    if (worker.joinable())
        worker.join();
    running = false;
}
} // namespace AudioWarmup

#else
namespace AudioWarmup {
void touch() {}
void stop() {}
} // namespace AudioWarmup
#endif
