#pragma once

namespace edm {

class Dispatcher {
public:
    Dispatcher();
    ~Dispatcher();

    void linkMainWindowSignals();
};

} // namespace edm
