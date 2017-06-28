#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <cstdint>

template<class T>
class ring_buffer {
    T *buffer;
    uint64_t head;
    uint64_t tail;
    size_t buffer_size;

public:
    ring_buffer(size_t buffer_size_) : buffer_size(buffer_size_) {
        buffer = new T[buffer_size];
        for (size_t i = 0; i < buffer_size; i++)
            buffer[i] = 0;
        head = 0;
        tail = 0;
    }
    ~ring_buffer() {
        delete [] buffer;
    }

    void virtual_write(size_t lenght) {
        tail += lenght;
    }

    bool available() {
        return (tail > head);
    }

    size_t size() const {
        return tail - head;
    }

    void write(const T *buf, size_t lenght) {
        uint64_t tail_ = tail;
        for (auto i = 0; i < lenght; i++) {
            buffer[tail_ % buffer_size] = buf[i];
            ++tail_;
        }
        tail = tail_;
    }

    size_t read(T *buf, size_t lenght) {
        uint64_t head_ = head;
        if (lenght > size())
            lenght = size();

        for (auto i = 0; i < lenght; i++) {
            buf[i] = buffer[head_ % buffer_size];
            ++head_;
        }
        head = head_;
        return lenght;
    }
};

#endif // RINGBUFFER_H
