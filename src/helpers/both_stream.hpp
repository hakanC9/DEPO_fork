#pragma once

#include <iostream>
#include <fstream>

class BothStream {
public:
    BothStream() = delete;
    BothStream(BothStream&) = delete;
    BothStream(const BothStream&) = delete;
    virtual ~BothStream() = default;
    BothStream (std::ofstream& f) :
        fstr_(std::move(f)) {}

    template <class T>
    BothStream& operator<<(const T& obj) {
        std::cout << obj;
        fstr_ << obj;
        return *this;
    }
    BothStream& flush() {
        std::cout << std::flush;
        fstr_ << std::flush;
        return *this;
    }
private:
    std::ofstream fstr_;
};