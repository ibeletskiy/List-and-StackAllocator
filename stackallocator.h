template<size_t N>
class StackStorage {
private:
    void* pointer;
    size_t current;
    alignas(std::max_align_t) char storage[N];
public:
    StackStorage() : pointer(storage), current(N), storage() {}
    StackStorage(const StackStorage& other) = delete;

    template<typename T>
    T* place(size_t count) {
        if (std::align(alignof(T), sizeof(T) * count, reinterpret_cast<void*&>(pointer), current)) {
            T* res = reinterpret_cast<T*>(pointer);
            pointer = reinterpret_cast<char*>(pointer) + sizeof(T) * count;
            current -= sizeof(T) * count;
            return res;
        }
        throw std::bad_alloc();
        return nullptr;
    }
};

template <typename T, size_t N>
class StackAllocator {
public:
    StackStorage<N>* data_;

    using value_type = T;

    StackAllocator(StackStorage<N>& now) : data_(&now) {}

    T* allocate (size_t count) {
        return data_->template place<T>(count);
    }

    void deallocate (T*, size_t) {}

    template<typename U, size_t M>
    StackAllocator(const StackAllocator<U, M>& alloc) : data_(alloc.data_) {}

    template<typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };
};

template<typename T, typename Alloc = std::allocator<T>>
class List {
private:
    struct BaseNode {
        BaseNode* prev;
        BaseNode* next;

        BaseNode() = default;
        BaseNode(BaseNode* prev, BaseNode* next) : prev(prev), next(next) {}
        ~BaseNode() = default;
    };
    struct Node : public BaseNode {
        T value;
        Node() = default;
        Node(const T& value) : BaseNode(), value(value) {}
        ~Node() = default;
    };

    using AllocTraits = typename std::allocator_traits<Alloc>::template rebind_traits<Node>;

    BaseNode fake_node_;
    size_t size_;
    [[no_unique_address]] AllocTraits::allocator_type alloc_;

    template<bool IsConst>
    class base_iterator {
    private:
        BaseNode* ptr;
        explicit base_iterator(BaseNode* ptr) : ptr(ptr) {}
    public:
        using pointer_type = std::conditional_t<IsConst, const T*, T*>;
        using reference_type = std::conditional_t<IsConst, const T&, T&>;
        using value_type = std::remove_reference_t<reference_type>;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;

        base_iterator(const base_iterator<false>& other) : ptr(other.ptr) {}
        base_iterator& operator=(const base_iterator<false>& other) {
            ptr = other.ptr;
            return *this;
        }

        bool operator==(base_iterator other) const {
            return ptr == other.ptr;
        }
        bool operator!=(base_iterator other) const {
            return ptr != other.ptr;
        }
        reference_type operator*() const {
            Node* tmp = static_cast<Node*>(ptr);
            return tmp->value;
        }
        pointer_type operator->() const {
            Node* tmp = static_cast<Node*>(ptr);
            return &tmp->value;
        }
        base_iterator& operator++() {
            ptr = ptr->next;
            return *this;
        }
        base_iterator operator++(int) {
            base_iterator copy = *this;
            ptr = ptr->next;
            return copy;
        }
        base_iterator& operator--() {
            ptr = ptr->prev;
            return *this;
        }
        base_iterator operator--(int) {
            base_iterator copy = *this;
            ptr = ptr->prev;
            return copy;
        }
        friend List;
    };

public:
    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
        return iterator(fake_node_.next);
    }
    iterator end() {
        return iterator(&fake_node_);
    }
    const_iterator begin() const {
        return const_iterator(fake_node_.next);
    }
    const_iterator end() const {
        return const_iterator(const_cast<BaseNode*>(&fake_node_));
    }
    const_iterator cbegin() const {
        return const_iterator(const_cast<BaseNode*>(fake_node_.next));
    }
    const_iterator cend() const {
        return const_iterator(const_cast<BaseNode*>(&fake_node_));
    }
    reverse_iterator rbegin() {
        return reverse_iterator(iterator(&fake_node_));
    }
    reverse_iterator rend() {
        return reverse_iterator(iterator(&fake_node_));
    }
    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(const_iterator(const_cast<BaseNode*>(&fake_node_)));
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(const_iterator(const_cast<BaseNode*>(&fake_node_)));
    }
    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(const_iterator(const_cast<BaseNode*>(&fake_node_)));
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(const_iterator(const_cast<BaseNode*>(&fake_node_)));
    }

    List() : fake_node_(&fake_node_, &fake_node_), size_(0) {}
    List(size_t size) : fake_node_(&fake_node_, &fake_node_), size_(0) {
        try {
            for (size_t i = 0; i < size; ++i) {
                insert_default(end());
            }
        } catch(...) {
            while (size_ > 0) {
                pop_back();
            }
            throw;
        }
    }
    List(size_t size, const T& value) : fake_node_(&fake_node_, &fake_node_), size_(0) {
        try {
            for (size_t i = 0; i < size; ++i) {
                push_back(value);
            }
        } catch(...) {
            while (size_ > 0) {
                pop_back();
            }
            throw;
        }
    }
    List(const Alloc& alloc) : fake_node_(&fake_node_, &fake_node_), size_(0), alloc_(alloc) {}
    List(size_t size, const Alloc& alloc) : fake_node_(&fake_node_, &fake_node_), size_(0), alloc_(alloc) {
        try {
            for (size_t i = 0; i < size; ++i) {
                insert_default(end());
            }
        } catch(...) {
            while (size_ > 0) {
                pop_back();
            }
            throw;
        }
    }
    List(size_t size, const T& value, const Alloc& alloc) : fake_node_(&fake_node_, &fake_node_), size_(0), alloc_(alloc) {
        try {
            for (size_t i = 0; i < size; ++i) {
                push_back(value);
            }
        } catch(...) {
            while (size_ > 0) {
                pop_back();
            }
            throw;
        }
    }

    ~List() {
        while (size_ > 0) {
            pop_back();
        }
    }

    List(const List& other) : fake_node_(&fake_node_, &fake_node_), size_(0), alloc_(AllocTraits::select_on_container_copy_construction(other.alloc_)) {
        size_t count = 0;
        try {
            const_iterator now_it = other.begin();
            for (; count < other.size_; ++count, ++now_it) {
                push_back(*now_it);
            }
        } catch(...) {
            for (size_t i = 0; i < count; ++i) {
                pop_back();
            }
            throw;
        }
    }

    List& operator=(const List& other) {
        size_t previous_size = size_;
        typename AllocTraits::allocator_type previous_alloc = alloc_;
        if (AllocTraits::propagate_on_container_copy_assignment::value) {
            alloc_ = other.alloc_;
        }
        size_t count = 0;
        try {
            const_iterator now_it = other.begin();
            for (; count < other.size_; ++count, ++now_it) {
                push_back(*now_it);
            }
        } catch(...) {
            for (size_t i = 0; i < count; ++i) {
                pop_back();
            }
            alloc_ = previous_alloc;
            throw;
        }
        for (size_t i = 0; i < previous_size; ++i) {
            pop_front();
        }
        size_ = other.size_;
        return *this;
    }

    size_t size() const {
        return size_;
    }
    Alloc get_allocator() const {
        return alloc_;
    }
    void push_back(const T& value) {
        insert(end(), value);
    }
    void push_front(const T& value) {
        insert(begin(), value);
    }
    void pop_back() {
        iterator tmp = end();
        --tmp;
        erase(tmp);
    }
    void pop_front() {
        erase(begin());
    }

    void insert_default(const_iterator it) {
        Node* tmp = AllocTraits::allocate(alloc_, 1);
        try {
            AllocTraits::construct(alloc_, tmp);
        } catch(...) {
            AllocTraits::deallocate(alloc_, tmp, 1);
            throw;
        }
        BaseNode* prev = it.ptr->prev;
        tmp->next = it.ptr;
        tmp->prev = prev;
        it.ptr->prev = tmp;
        prev->next = tmp;
        ++size_;
    }

    void insert(const_iterator it, const T& value) {
        Node* tmp = AllocTraits::allocate(alloc_, 1);
        try {
            AllocTraits::construct(alloc_, tmp, value);
        } catch(...) {
            AllocTraits::deallocate(alloc_, tmp, 1);
            throw;
        }
        BaseNode* prev = it.ptr->prev;
        tmp->next = it.ptr;
        tmp->prev = prev;
        it.ptr->prev = tmp;
        prev->next = tmp;
        ++size_;
    }
    void erase(const_iterator it) {
        BaseNode* prev = it.ptr->prev;
        BaseNode* next = it.ptr->next;
        prev->next = next;
        next->prev = prev;
        try {
            AllocTraits::destroy(alloc_, static_cast<Node*>(it.ptr));
            AllocTraits::deallocate(alloc_, static_cast<Node*>(it.ptr), 1);
        } catch(...) {
            prev->next = it.ptr;
            next->prev = it.ptr;
            throw;
        }
        --size_;
    }
};