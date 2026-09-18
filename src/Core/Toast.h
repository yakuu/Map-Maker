#pragma once
#include <string>
#include <vector>
#include <chrono>

enum class ToastLevel { Info, Warning, Error };

struct ToastItem {
    std::string message;
    ToastLevel level;
    double expiresAt;
};

class ToastQueue {
public:
    void push(const std::string& msg, ToastLevel lvl = ToastLevel::Info,
              double seconds = 4.0) {
        double now = nowSeconds();
        toasts.push_back({ msg, lvl, now + seconds });
    }

    void update() {
        double now = nowSeconds();
        for (auto it = toasts.begin(); it != toasts.end();) {
            if (it->expiresAt <= now) it = toasts.erase(it);
            else ++it;
        }
    }

    const std::vector<ToastItem>& items() const { return toasts; }
    void clear() { toasts.clear(); }

private:
    static double nowSeconds() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
    std::vector<ToastItem> toasts;
};