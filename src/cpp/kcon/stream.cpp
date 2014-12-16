#include "stream.h"
#include "kstack.h"
#include <stack>

using namespace kcon;

kostream& kostream::operator<<( pcell_t pcell )
{
	_os << '[';
	_format( pcell );
	_os << ']';
	return *this;
}

kostream& kostream::operator<<( byte_t value )
{
	_format( value );
	return *this;
}

kostream& kostream::operator<<( elem_t elem )
{
    if( elem.is_pcell() )
        return kostream::operator<<( elem.pcell() );
    else
        return kostream::operator<<( elem.byte() );
}

struct Tail
{
    elem_t elem;
    size_t len;
};

//complicated but avoids recursion on c-stack
void kostream::_format( pcell_t pcell )
{
    std::stack<Tail> tails;
    Tail tail{ pcell, 0 };

    while( true )
    {
        if( tail.elem.is_byte() || (tail.elem == null<elem_t>()) )
        {
            if( tail.elem == null<elem_t>() )
                _os << "<null>";
            else
                _format( tail.elem.byte() );

            if( !_flatten )
                for( size_t l = tail.len; l != 0; --l )
                    _os << ']';

            if( tails.empty() )
                return;

            _os << "] ";
            tail = tails.top();
            tails.pop();

            if( !_flatten && tail.elem.is_pcell()) _os << '[';
        }
        else
        {
            auto p = tail.elem.pcell();

            if( p->head().is_pcell() && p->tail().is_pcell() )
            {
                tails.push( Tail{ p->tail(), tail.len + 1 } );

                _os << '[';
                tail = Tail{ p->head(), 0 };
            }
            else if( p->head().is_pcell() && p->tail().is_byte() )
            {
                tails.push( Tail{ p->tail(), tail.len } );

                _os << '[';
                tail = Tail{ p->head(), 0 };
            }
            else if( p->head().is_byte() && p->tail().is_pcell() )
            {
                _os << (short) p->head().byte() << ' ';
                tail = Tail{ p->tail(), tail.len + 1 };
                if( !_flatten ) _os << '[';
            }
            else
            {
                _os << (short) p->head().byte() << ' ';
                tail = Tail{ p->tail(), tail.len };
            }
        }
    }
}

void kostream::_format( byte_t value )
{
    _os << short(value);
}

byte_t kistream::_parse_byte()
{
    value_t value;
    if( !(_is >> value) || (value >= 256) )
        throw Error<Syntax>( "Malformed byte" );

    return static_cast<byte_t>( value );
}

pcell_t kistream::_parse_elems()
{
	pcell_t tail = null<pcell_t>();
	kstack<pcell_t> tails( _alloc );

	char c;
	while( _is >> c )
	{
		if( c == ']' )
		{
		    if( tail == null<pcell_t>() )
                throw Error<Syntax>( "Unexpected empty cell" );

		    if( is_singleton( tail ) )
                throw Error<Syntax>( "Unexpected singleton" );

			if( tails.empty() )
				return tail;

			pcell_t elems = tail;
			tail = tails.top();
			tails.pop();
			tail = _alloc.new_Cell( elems, tail, {&tails.items()} );
		}
		else if( c == '[' )
		{
			tails.push( tail, {&tails.items(), &tail} );
			tail = null<pcell_t>();
		}
		else if( isdigit( c ) )
		{
			_is.putback( c );
			tail = _alloc.new_Cell( _parse_byte(), tail, {&tails.items()} );
		}
		else
			throw Error<Syntax>( "Unexpected '"s + c + "'" );
	}

	throw Error<Syntax,EOS>( "Unexpected end of input" );
}

pcell_t kistream::_reverse_and_reduce( pcell_t pcell )
{
    pcell_t p = pcell;
    elem_t tail = null<pcell_t>();
	kstack<elem_t> tails( _alloc );
	kstack<pcell_t> pcells( _alloc );

    while( !((p == null<pcell_t>()) && pcells.empty()) )
    {
        if( p == null<pcell_t>() )
        {
        	assert( tail.is_pcell(), "Expected recursive cell tail" );

            pcell_t head = tail.pcell();

            p = pcells.top(); pcells.pop();
            tail = tails.top(); tails.pop();

			if( tail == null<elem_t>() )
				tail = head;
			else if( tail.is_byte() )
				tail = _alloc.new_Cell( head, tail.byte() );
			else
            	tail = _alloc.new_Cell( head, tail.pcell() );
        }
        else
        {
            assert( p->tail().is_pcell(), "Expected tail to be cell in reverse and reduce" );

            if( p->head().is_pcell() )
            {
                pcells.push( p->tail().pcell(), {&p, &pcells.items()} );
                tails.push( tail, {&p, &pcells.items()} );

                p = p->head().pcell();
                tail = null<pcell_t>();
            }
            else
            {
                assert( p->head().is_byte(), "" );
                const byte_t head = p->head().byte();

                if( tail== null<elem_t>() )
                    tail = head;
                else if( tail.is_byte() )
                    tail = _alloc.new_Cell( head, tail.byte() );
                else
                    tail = _alloc.new_Cell( head, tail.pcell() );

                p = p->tail().pcell();
            }
        }
    }

    assert( tails.empty(), "Cell and tail stack mismatch" );

	return tail.pcell();
}

elem_t kistream::_parse()
{
	char c;
	if( !(_is >> c) )
        throw Error<Syntax>( "Unexpected end of input" );

    if( c == '[' )
    {
        return _reverse_and_reduce( _parse_elems() );
    }
    else if( isdigit( c ) )
    {
        _is.putback( c );
        return _parse_byte();
    }
    else
        throw Error<Syntax>( "Unexpected '"s + c + "'" );
}

kistream& kistream::operator>>( elem_t& elem )
{
    elem = _parse();
    return *this;
}
