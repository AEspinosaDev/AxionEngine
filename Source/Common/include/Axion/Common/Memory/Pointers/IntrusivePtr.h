
// // Template to add reference counting to any base
// template <class T>
// class RefCounter : public T
// {
// public:
//     RefCounter()
//         : _refCount( 1 ) {
//         // std::cout << "[RefCounter] Created: " << this << " RefCount=1\n";
//     }

//     virtual ~RefCounter() {
//         // std::cout << "[RefCounter] Destroyed: " << this << "\n";
//     }

//     ulong addRef() noexcept override {
//         ulong val = ++_refCount;
//         // std::cout << "[RefCounter] addRef: " << this << " RefCount=" << val << "\n";
//         return val;
//     }

//     ulong release() noexcept override {
//         ulong val = --_refCount;
//         // std::cout << "[RefCounter] release: " << this << " RefCount=" << val << "\n";
//         if ( val == 0 )
//         {
//             // std::cout << "[RefCounter] deleting: " << this << "\n";
//             delete this;
//         }
//         return val;
//     }

//     ulong getRefCount() const noexcept override {
//         return _refCount.load();
//     }

// private:
//     std::atomic<ulong> _refCount;
// };

// // COM-style smart pointer
// template <class T>
// class Ptr
// {
// public:
//     Ptr()
//         : _ptr( nullptr ) {}
//     Ptr( std::nullptr_t )
//         : _ptr( nullptr ) {}

//     Ptr( T* raw )
//         : _ptr( raw ) {
//         internalAddRef();
//     }

//     Ptr( const Ptr& other )
//         : _ptr( other._ptr )
//         , _ownsReference( true ) {
//         internalAddRef();
//     }
//     Ptr( Ptr&& other ) noexcept
//         : _ptr( other._ptr )
//         , _ownsReference( other._ownsReference ) {
//         other._ptr           = nullptr;
//         other._ownsReference = false;
//     }

//     template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
//     Ptr( const Ptr<U>& other )
//         : _ptr( other._ptr ) {
//         internalAddRef();
//     }

//     template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
//     Ptr( Ptr<U>&& other ) noexcept
//         : _ptr( other._ptr ) {
//         other._ptr = nullptr;
//     }
//     Ptr( T* raw, bool takeOwnership )
//         : _ptr( raw )
//         , _ownsReference( takeOwnership ) // Nuevo flag
//     {
//         if ( _ownsReference )
//         {
//             internalAddRef();
//         }
//     }

//     ~Ptr() {
//         if ( _ownsReference )
//         {
//             internalRelease();
//         }
//     }

//     Ptr& operator=( const Ptr& other ) {
//         if ( this != &other )
//         {
//             internalRelease();
//             _ptr = other._ptr;
//             internalAddRef();
//         }
//         return *this;
//     }

//     // operators
//     T* operator->() const { return _ptr; }
//     T& operator*() const { return *_ptr; }
//        operator bool() const { return _ptr != nullptr; }
//        operator T*() const { return _ptr; }

//     T* get() const { return _ptr; }

//     // Returns a pointer to the internal pointer (like COM & operator)
//     T** operator&() {
//         internalRelease();
//         _ptr = nullptr;
//         return &_ptr;
//     }

//     // Detach the pointer (caller takes ownership, RefPtr forgets it)
//     T* detach() {
//         T* tmp = _ptr;
//         _ptr   = nullptr;
//         return tmp;
//     }

//     // Attach a raw pointer (takes ownership)
//     void attach( T* raw ) {
//         internalRelease();
//         _ptr = raw;
//     }

//     // Factory method, returns Ptr that owns new object
//     template <class... Args>
//     static Ptr<T> create( Args&&... args ) {
//         T* obj = new T( std::forward<Args>( args )... );
//         return Ptr<T>( obj );
//     }

// private:
//     void internalAddRef() {
//         if ( _ptr )
//             _ptr->addRef();
//     }

//     void internalRelease() {
//         if ( _ptr )
//             _ptr->release();
//         _ptr = nullptr;
//     }

// private:
//     T*   _ptr;
//     bool _ownsReference = true;

//     template <typename>
//     friend class Ptr;
// };

// #define DEFINE_COM_PTR_FOR_TYPE( type, clean ) \
//     class type;                                \
//     typedef Ptr<type> clean##Ptr;
