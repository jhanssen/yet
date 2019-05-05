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
    enum class Result {
        Run,
        DontRun,
        Stop
    };

    typedef unsigned long int TimerType;

    void addAnalog(int pin, int threshold, Analog mode, std::function<Pin(TimerType, int, int)>&& func);
    void addDigital(int pin, Digital mode, std::function<Pin(TimerType, int, Digital)>&& func);
    void addTimer(TimerType when, std::function<Timer(TimerType)>&& func);
    void add(std::function<Result(TimerType)>&& cond, std::function<void(TimerType)>&& func);

    void step();

    static void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

private:
    struct AnalogData
    {
        int pin;
        int threshold;
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
        bool edge, stopped;
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
        std::function<Result(TimerType)> cond;
        std::function<void(TimerType)> func;
        bool stopped;
    };
    std::vector<CustomData> customs;

    unsigned long steps { 0 };
    enum { CleanupEvery = 1000 };
};

inline void Yet::addAnalog(int pin, int threshold, Analog mode, std::function<Pin(TimerType, int, int)>&& func)
{
    analogs.push_back({ pin, threshold, mode, std::move(func), false, false });
}

inline void Yet::addDigital(int pin, Digital mode, std::function<Pin(TimerType, int, Digital)>&& func)
{
    digitals.push_back({ pin, mode, std::move(func), false, false });
}

inline void Yet::addTimer(TimerType when, std::function<Timer(TimerType)>&& func)
{
    timers.push_back({ millis() + when, when, std::move(func), false });
}

inline void Yet::add(std::function<Result(TimerType)>&& cond, std::function<void(TimerType)>&& func)
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
            if ((a.mode == Analog::Greater) && r <= a.threshold)
                a.edge = false;
            else if ((a.mode == Analog::GreaterEquals) && r < a.threshold)
                a.edge = false;
            else if ((a.mode == Analog::Less) && r >= a.threshold)
                a.edge = false;
            else if ((a.mode == Analog::LessEquals) && r > a.threshold)
                a.edge = false;
            else if ((a.mode == Analog::Equals) && r != a.threshold)
                a.edge = false;
        } else {
            if ((a.mode == Analog::Greater) && r > a.threshold)
                run = true;
            else if ((a.mode == Analog::GreaterEquals) && r >= a.threshold)
                run = true;
            else if ((a.mode == Analog::Less) && r < a.threshold)
                run = true;
            else if ((a.mode == Analog::LessEquals) && r <= a.threshold)
                run = true;
            else if ((a.mode == Analog::Equals) && r == a.threshold)
                run = true;
        }
        if (run) {
            const Pin res = a.func(ms, a.pin, r);
            if (res == Pin::Edge)
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
            if (r != d.wanted)
                d.edge = false;
        } else if (r == d.wanted) {
            const Pin res = d.func(ms, d.pin, d.wanted);
            if (res == Pin::Edge)
                d.edge = true;
            else if (res == Pin::Stop)
                d.stopped = true;
        }
    }
    for (auto& c : customs) {
        if (c.stopped) {
            cleanups |= CleanupCustom;
            continue;
        }
        const Result res = c.cond(ms);
        if (res == Result::Stop)
            c.stopped = true;
        else if (res == Result::Run)
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
