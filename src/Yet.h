#include <Arduino.h>
#include <stdlib.h>
#include <functional>
#include <vector>

class Yet
{
public:
    enum class Analog {
        Greater,
        GreaterEquals,
        Less,
        LessEquals,
        Equals,
        LowHigh,
    };
    enum class Digital {
        Off,
        On
    };
    enum class Pin {
        Level,
        Edge,
        Stop
    };
    enum class Timer {
        Continue,
        Stop
    };
    enum class Custom {
        Run,
        DontRun,
        Stop
    };

    typedef unsigned long int TimerType;

    void analog(int pin, int low, int high, std::function<Pin(TimerType, int, int)>&& func);
    void analog(int pin, int threshold, Analog mode, std::function<Pin(TimerType, int, int)>&& func);
    void digital(int pin, std::function<Pin(TimerType, int, Digital)>&& func);
    void digital(int pin, Digital mode, std::function<Pin(TimerType, int, Digital)>&& func);
    void timer(TimerType when, std::function<Timer(TimerType)>&& func);
    void custom(std::function<Custom(TimerType)>&& cond, std::function<void(TimerType)>&& func);

    void step();

    static void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

private:
    struct AnalogData
    {
        int pin;
        int low, high;
        Analog mode;
        std::function<Pin(TimerType, int, int)> func;
        bool edge, stopped;
    };
    std::vector<AnalogData> analogs;

    struct DigitalData
    {
        int pin;
        Digital wanted;
        std::function<Pin(TimerType, int, Digital)> func;
        bool edge, stopped, wantsWanted;
    };
    std::vector<DigitalData> digitals;

    struct TimerData
    {
        TimerType when, interval;
        std::function<Timer(TimerType)> func;
        bool stopped;
    };
    std::vector<TimerData> timers;

    struct CustomData
    {
        std::function<Custom(TimerType)> cond;
        std::function<void(TimerType)> func;
        bool stopped;
    };
    std::vector<CustomData> customs;

    unsigned long steps { 0 };
    enum { CleanupEvery = 1000 };
};

inline void Yet::analog(int pin, int threshold, Analog mode, std::function<Pin(TimerType, int, int)>&& func)
{
    analogs.push_back({ pin, threshold, threshold, mode, std::move(func), false, false });
}

inline void Yet::analog(int pin, int low, int high, std::function<Pin(TimerType, int, int)>&& func)
{
    if (high > low)
        analogs.push_back({ pin, low, high, Analog::LowHigh, std::move(func), false, false });
    else
        analogs.push_back({ pin, high, low, Analog::LowHigh, std::move(func), true, false });
}

inline void Yet::digital(int pin, Digital mode, std::function<Pin(TimerType, int, Digital)>&& func)
{
    digitals.push_back({ pin, mode, std::move(func), false, false, true });
}

inline void Yet::digital(int pin, std::function<Pin(TimerType, int, Digital)>&& func)
{
    digitals.push_back({ pin, Digital::On, std::move(func), false, false, false });
}

inline void Yet::timer(TimerType when, std::function<Timer(TimerType)>&& func)
{
    timers.push_back({ millis() + when, when, std::move(func), false });
}

inline void Yet::custom(std::function<Custom(TimerType)>&& cond, std::function<void(TimerType)>&& func)
{
    customs.push_back({ std::move(cond), std::move(func), false });
}

inline void Yet::step()
{
    enum {
        CleanupAnalog  = 0x1,
        CleanupDigital = 0x2,
        CleanupCustom  = 0x4,
        CleanupTimer   = 0x8
    };
    int cleanups = 0;
    const auto ms = millis();
    for (auto& a : analogs) {
        if (a.stopped) {
            cleanups |= CleanupAnalog;
            continue;
        }
        const int r = analogRead(a.pin);
        bool run = false;
        if (a.edge) {
            if (a.mode == Analog::Greater && r <= a.low)
                a.edge = false;
            else if (a.mode == Analog::GreaterEquals && r < a.low)
                a.edge = false;
            else if (a.mode == Analog::Less && r >= a.high)
                a.edge = false;
            else if (a.mode == Analog::LessEquals && r > a.high)
                a.edge = false;
            else if (a.mode == Analog::Equals && r != a.high)
                a.edge = false;
            else if (a.mode == Analog::LowHigh && r <= a.low) {
                run = true;
                a.edge = false;
            }
        } else {
            if (a.mode == Analog::Greater && r > a.high)
                run = true;
            else if (a.mode == Analog::GreaterEquals && r >= a.high)
                run = true;
            else if (a.mode == Analog::Less && r < a.low)
                run = true;
            else if (a.mode == Analog::LessEquals && r <= a.low)
                run = true;
            else if (a.mode == Analog::Equals && r == a.high)
                run = true;
            else if (a.mode == Analog::LowHigh && r >= a.high) {
                run = true;
                a.edge = true;
            }
        }
        if (run) {
            const Pin res = a.func(ms, a.pin, r);
            if (res == Pin::Edge && a.mode != Analog::LowHigh)
                a.edge = true;
            else if (res == Pin::Stop)
                a.stopped = true;
        }
    }
    for (auto& d : digitals) {
        if (d.stopped) {
            cleanups |= CleanupDigital;
            continue;
        }
        const Digital r = (digitalRead(d.pin) == HIGH) ? Digital::On : Digital::Off;
        if (d.edge) {
            if (r != d.wanted) {
                d.edge = false;
                if (!d.wantsWanted) {
                    if (d.func(ms, d.pin, r) == Pin::Stop)
                        d.stopped = true;
                }
            }
        } else if (r == d.wanted) {
            const Pin res = d.func(ms, d.pin, d.wanted);
            if (res == Pin::Stop)
                d.stopped = true;
            else if (res == Pin::Edge || !d.wantsWanted)
                d.edge = true;
        }
    }
    for (auto& c : customs) {
        if (c.stopped) {
            cleanups |= CleanupCustom;
            continue;
        }
        const Custom res = c.cond(ms);
        if (res == Custom::Stop)
            c.stopped = true;
        else if (res == Custom::Run)
            c.func(ms);
    }
    for (auto& t : timers) {
        if (t.stopped) {
            cleanups |= CleanupTimer;
            continue;
        }
        if (t.when > ms)
            continue;
        const long int delta = t.when - ms;
        t.when = ms + delta + t.interval;
        const Timer res = t.func(ms);
        if (res == Timer::Stop)
            t.stopped = true;
    }

    if (!(++steps & CleanupEvery)) {
        if (cleanups & CleanupAnalog) {
            auto it = analogs.begin();
            while (it != analogs.end()) {
                if (it->stopped)
                    it = analogs.erase(it);
                else
                    ++it;
            }
        }
        if (cleanups & CleanupDigital) {
            auto it = digitals.begin();
            while (it != digitals.end()) {
                if (it->stopped)
                    it = digitals.erase(it);
                else
                    ++it;
            }
        }
        if (cleanups & CleanupCustom) {
            auto it = customs.begin();
            while (it != customs.end()) {
                if (it->stopped)
                    it = customs.erase(it);
                else
                    ++it;
            }
        }
        if (cleanups & CleanupTimer) {
            auto it = timers.begin();
            while (it != timers.end()) {
                if (it->stopped)
                    it = timers.erase(it);
                else
                    ++it;
            }
        }
    }
}
