#pragma once

#include <new>
#include <memory>
#include <cassert>
#include <utility>
#include <cstdlib>

template<typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(const size_t count) 
    : buffer_(Allocate(count)) 
    , capacity_(count) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept 
    : buffer_(std::exchange(other.buffer_, nullptr)) 
    , capacity_(std::exchange(other.capacity_, 0)) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    [[nodiscard]] size_t Capacity() const noexcept {
        return capacity_;
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }
    
    T* GetAddress() noexcept {
        return buffer_;
    }

    const T& operator[](const size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](const size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    const T* operator+(const size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset; 
    }

    T* operator+(const size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory& operator=(RawMemory&& other) noexcept {
        if(this != &other) {
            this->buffer_ = std::exchange(other.buffer_, nullptr);
            this->capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }
 
    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    static T* Allocate(const size_t count) {
        return count != 0 ? static_cast<T*>(operator new(count * sizeof(T))) : nullptr; 
    }

    static void Deallocate(T* buff) noexcept {
        operator delete(buff);
    } 
};

template<typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(const size_t count) 
    : data_(count)
    , size_(count) {
        std::uninitialized_value_construct_n(data_.GetAddress(), count);
    }

    Vector(const Vector& other) 
    : data_(other.size_)
    , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept 
    : data_(std::move(other.data_))
    , size_(std::exchange(other.size_, 0)) {
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    const_iterator end() const noexcept {
        return cend();
    }


    [[nodiscard]] size_t Capacity() const noexcept {
        return data_.Capacity();
    }
    
    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    void Reserve(const size_t new_capacity) {
        if(new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        CopyOrMove(data_.GetAddress(), new_data.GetAddress(), size_);

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(const size_t new_size) {
        if(data_.Capacity() > new_size) {
            if(new_size < size_) {
                std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            } else if(new_size > size_) {
                std::uninitialized_default_construct_n(data_.GetAddress() + size_, new_size - size_);
            }
        } else if (data_.Capacity() < new_size) {
            Reserve(new_size);
            std::uninitialized_default_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template<typename Type>
    void PushBack(Type&& elem) {
        Emplace(cend(), std::forward<Type>(elem));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + (size_ - 1));
        --size_;
    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t position = pos - begin();
        if(size_ == data_.Capacity()) {
            EmplaceWithAllocate(position, std::forward<Args>(args)...);
        } else {
            EmplaceWithoutAllocate(position, std::forward<Args>(args)...);
        }
        ++size_;
        return begin() + position;
    }

    template<typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

    iterator Insert(const_iterator pos, const T& elem) {
        return Emplace(pos, elem);
    }

    iterator Insert(const_iterator pos, T&& elem) {
        return Emplace(pos, std::move(elem));
    }

    iterator Erase(const_iterator pos) {
        assert(pos < end() && pos >= begin());
        size_t position = pos - begin();
        if(position == size_) {
            std::destroy_at(data_ + size_ - 1);
            --size_;
        } else {
            std::move(data_ + position + 1, data_ + size_, data_ + position);
            std::destroy_at(data_ + size_ - 1);
            --size_;
        }
        return begin() + position;
    }
    
    void Swap(Vector& other) noexcept {
        this->data_.Swap(other.data_);
        this->size_ = std::exchange(other.size_, this->size_);
    }

    const T& operator[](const size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](const size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& other) {
        if(this != &other) {
            if(this->data_.Capacity() < other.size_) {
                Vector temp(other);
                Swap(temp);
            } else {
                if(this->size_ < other.size_) {
                    std::copy(other.data_.GetAddress(), other.data_.GetAddress() + this->size_, this->data_.GetAddress());
                    std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, this->data_.GetAddress() + size_);
                } else {
                    std::copy(other.data_.GetAddress(), other.data_.GetAddress() + other.size_, this->data_.GetAddress());
                    std::destroy_n(this->data_.GetAddress() + other.size_, size_ - other.size_);
                }
                this->size_ = other.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if(this != &other) {
            this->data_ = std::move(other.data_);
            this->size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    static void DestroyN(T* buf, const size_t count) noexcept {
        for(size_t i = 0; i != count; ++i) {
            Destroy(buf + i);
        }
    }

    static void CopyOrMove(T* from, T* to, const size_t count) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, count, to);
        } else {
            std::uninitialized_copy_n(from, count, to);
        }
    }

    template<typename... Args>
    void EmplaceWithAllocate(const size_t position, Args&&... args) {
        if(size_ == 0) {
            RawMemory<T> temp(size_ + 1);
            new (temp.GetAddress()) T(std::forward<Args>(args)...);
            data_.Swap(temp);
        } else {
            RawMemory<T> temp(data_.Capacity() * 2);
            new (temp + position) T(std::forward<Args>(args)...);
            CopyOrMove(data_.GetAddress(), temp.GetAddress(), position);
            CopyOrMove(data_ + position, temp + position + 1, size_ - position);
            data_.Swap(temp);
            std::destroy_n(temp.GetAddress(), size_);
        }
    }

    template<typename... Args>
    void EmplaceWithoutAllocate(const size_t position, Args&&... args) {
        if(position < size_) {
            try {
                T new_elem = T(std::forward<Args>(args)...);
                new (data_ + size_) T(std::forward<T>(data_[size_ - 1]));
                std::move_backward(data_ + position, end() - 1, end());
                *(begin() + position) = std::forward<T>(new_elem);
            } catch (...) {
                operator delete (end());
                throw;
            }
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
    }
};