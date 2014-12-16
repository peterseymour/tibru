#ifndef HEADER_KCON_ALLOCATOR
#define HEADER_KCON_ALLOCATOR

#include "types.h"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <set>

namespace kcon {

struct OutOfMemory {};

template<class Scheme>
class SimpleAllocator
{
    class FreeCell
    {
        value_t _salt;
        FreeCell* _next;

        static FreeCell* _hash( value_t salt, FreeCell* p )
        {
            return reinterpret_cast<FreeCell*>( reinterpret_cast<uintptr_t>( p ) ^ salt );
        }
    public:
        FreeCell( FreeCell* next=0 )
            : _salt( rand() & ADDR_MASK ), _next( _hash( _salt, next ) ) {}

        FreeCell* next() const { return _hash( _salt, _next ); }
    };

    ASSERT( sizeof(FreeCell) == sizeof(Cell) );

    const size_t _ncells;
    FreeCell* _page;
    FreeCell* _free_list;
    size_t _gc_count;

    static void _mark( std::set<pcell_t>& live, pcell_t pcell );
public:
    typedef std::initializer_list<pcell_t*> Roots;

    SimpleAllocator( size_t ncells )
        : _ncells( ncells ), _page( new FreeCell[ncells] ), _free_list( 0 ), _gc_count( 0 )
    {
        assert( reinterpret_cast<uintptr_t>(_page) % sizeof(FreeCell) == 0, "Page not cell aligned" );
        gc({});
        _gc_count = 0;
    }

	~SimpleAllocator()
	{
		delete[] _page;
	}

    void gc( const Roots& roots );

    size_t gc_count() const { return _gc_count; }

    void* allocate( const Roots& roots )
    {
        if( _free_list == 0 )
            gc( roots );

        void* p = _free_list;
        _free_list = _free_list->next();
        return p;
    }

    size_t num_allocated() const
    {
        size_t n = _ncells;
        for( const FreeCell* p = _free_list; p != 0; p = p->next() )
            --n;

        return n;
    }

    const Cell* new_Cell( const elem_t& head, const elem_t& tail, const Roots& roots={} )
    {
        return new ( allocate( roots ) ) Cell( head, tail );
    }
};

typedef SimpleAllocator<SimpleScheme> Allocator;

}	//namespace

#endif