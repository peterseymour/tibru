#ifndef HEADER_ELPA_ALLOCATOR
#define HEADER_ELPA_ALLOCATOR

#include "types.h"
#include <cstdint>
#include <initializer_list>
#include <set>

namespace elpa {

struct OutOfMemory {};

#define MetaAllocator template<class System, template<class> class SchemeT>

/** Auto register roots with allocator instance **/

template<class System, MetaScheme class SchemeT, MetaAllocator class AllocatorT>
class AllocatorBase
{
protected:
	typedef SchemeT<System> Scheme;
	typedef AllocatorT<System, SchemeT> Allocator;
	typedef typename Scheme::elem_t elem_t;

	const size_t _ncells;
	size_t _gc_count;
	std::set<elem_t*> _elem_roots;

	AllocatorBase( size_t ncells )
		: _ncells( ncells ), _gc_count( 0 )
    {
        System::assert( ncells > 0, "Allocator must allocate at least one cell" );
    }
public:
    auto gc_count() const -> size_t { return _gc_count; }

    void add_root( elem_t* root ) { _elem_roots.insert( root ); }
	void del_root( elem_t* root ) { _elem_roots.erase( root ); }

    template<class T>
    struct auto_root_ref : T
    {
        Allocator& alloc;

        auto_root_ref( Allocator& a, const T& root ) : T( root ), alloc( a ) {}
    };

    template<class T>
    class auto_root : public auto_root_ref<T>
    {
        explicit auto_root( const auto_root& );
        auto_root& operator=( const auto_root& );
    public:
        typedef auto_root_ref<T> ref;

        auto_root( Allocator& alloc, const T& root=T() ) : ref( alloc, root ) { this->alloc.add_root( this ); }
        auto_root( const auto_root_ref<T>& r ) : ref( r ) { this->alloc.add_root( this ); }
        ~auto_root() { this->alloc.del_root( this ); }
        auto_root& operator=( const T& t ) { (T&) *this = t; return *this; }
    };
};

/**
    TestAllocator
    Simple but inefficient allocator for testing.
    It shifts all the cells on each memory allocation to test that the roots are correctly defined
**/

template<class System, MetaScheme class SchemeT>
struct TestAllocator : AllocatorBase< System, SchemeT, TestAllocator >
{
    typedef SchemeT<System> Scheme;
    typedef typename Scheme::value_t value_t;
    typedef typename Scheme::pcell_t pcell_t;
    typedef typename Scheme::elem_t elem_t;
    typedef typename Scheme::Cell Cell;
private:
    std::set<pcell_t> _allocated;

    static void _mark( std::set<pcell_t>& live, pcell_t pcell );
    void _shift();
public:
    TestAllocator( size_t ncells )
        : AllocatorBase<System, SchemeT, TestAllocator>( ncells ), _allocated() {}

	~TestAllocator()
	{
		for( auto p : _allocated )
            delete p;
	}

	void gc();

    auto new_Cell( const elem_t& head, const elem_t& tail ) -> const Cell*
    {
		elem_t e = System::check_address( new Cell( head, tail ) );	//switch pcell_t
		_allocated.insert( e.pcell() );
        this->add_root( &e );
        _shift();
        if( _allocated.size() == this->_ncells )
            gc();
        this->del_root( &e );
        return e.pcell();
    }

    auto num_allocated() const -> size_t
    {
        return _allocated.size();
    }
};

/**
    SimpleAllocator
    Simple semi-efficient allocator that uses a fixed page of cells
    Cells are never moved in memory
**/

template<class System, MetaScheme class SchemeT>
struct SimpleAllocator : AllocatorBase<System, SchemeT, SimpleAllocator>
{
    typedef SchemeT<System> Scheme;
    typedef typename Scheme::value_t value_t;
    typedef typename Scheme::pcell_t pcell_t;
    typedef typename Scheme::elem_t elem_t;
    typedef typename Scheme::Cell Cell;
private:
    struct FreeCell
    {
        FreeCell* next;
        elem_t _;
    };

    ASSERT( sizeof(FreeCell) == sizeof(Cell) );

    FreeCell* _page;
    FreeCell* _free_list;

    static void _mark( std::set<pcell_t>& live, pcell_t pcell );
public:
    SimpleAllocator( size_t ncells )
        : AllocatorBase<System, SchemeT, SimpleAllocator>( ncells ), _page( new FreeCell[ncells] ), _free_list( 0 )
    {
    	System::check_address( _page );
    	System::check_address( _page + ncells - 1 );
        System::assert( reinterpret_cast<uintptr_t>(_page) % sizeof(FreeCell) == 0, "Page not cell aligned" );
        gc();
        this->_gc_count = 0;
    }

	~SimpleAllocator()
	{
		delete[] _page;
	}

    void gc();

    auto num_allocated() const -> size_t
    {
        size_t n = this->_ncells;
        for( const FreeCell* p = _free_list; p != 0; p = p->next )
            --n;

        return n;
    }

    auto _next() -> void*
    {
        System::assert( _free_list != 0, "SimpleAllocator failed to reserve cell" );

        void* p = _free_list;
        _free_list = _free_list->next;
        return p;
    }

    auto new_Cell( const elem_t& head, const elem_t& tail ) -> const Cell*
    {
        pcell_t p = new (_next()) Cell( head, tail );

        if( _free_list == 0 )
        {
            elem_t e = p;
            this->add_root( &e);
            gc();
            this->del_root( &e );
            p = e.pcell();
        }

        return p;
    }
};

}	//namespace

#endif