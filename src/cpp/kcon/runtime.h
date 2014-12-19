#ifndef KCON_RUNTIME_H
#define KCON_RUNTIME_H

#include "container/kstack.h"
#include "stream.h"

namespace kcon {

struct Runtime;

template<bool AssertFlag=false>
struct Params
{
    template<bool flag> struct Assert : Params<flag> {};

    static void assert( bool cond, const std::string& msg )
    {
        if( AssertFlag && !cond )
            throw Error<Assertion>( msg );
    }
};

typedef Params<>::Assert<false> Debug;

template<
    class System,
    MetaScheme class SchemeT,
    MetaAllocator class AllocatorT
>
struct Env
{
    using Scheme = SchemeT<System>;
    using Allocator = AllocatorT<System, SchemeT>;

    template<class T> using kstack = container::kstack<System, SchemeT, Allocator, T>;
    using kostream = kcon::kostream<System, SchemeT>;
    using kistream = kcon::kistream<System, SchemeT, AllocatorT>;
};

}   //namespace

#endif
