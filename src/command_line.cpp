#include "command_line.h"

command_line parse_command_line( const vector<wstring> &arguments ) {
    command_line result;
    auto fail = [&]( const wstring &message ) { if ( result.error.empty() ) result.error = message; };
    auto is_value = [&]( size_t i ) { return i < arguments.size() && ( arguments[i].empty() || arguments[i][0] != L'-' ); };

    for ( size_t i = 0; i < arguments.size(); ++i ) {
        wstring flag = lower( arguments[i] );
        if ( flag == L"--setup" ) {
            if ( !is_value( i + 1 ) ) { fail( L"--setup needs a setup name after it." ); continue; }
            result.setups.push_back( { arguments[++i] } );
        } else if ( flag == L"--pick" ) {
            if ( result.setups.empty() ) fail( L"--pick needs a --setup before it." );
            while ( is_value( i + 1 ) ) {
                ++i;
                if ( !result.setups.empty() ) result.setups.back().picks.push_back( arguments[i] );
            }
        } else if ( flag == L"--separate" ) {
            if ( result.setups.empty() ) fail( L"--separate needs a --setup before it." );
            else result.setups.back().separate = true;
        } else if ( flag == L"--list" ) {
            result.action = L"list";
            if ( is_value( i + 1 ) ) result.list_title = arguments[++i];
        } else if ( flag == L"--check" || flag == L"--edit" || flag == L"--background" || flag == L"--panel" ) {
            result.action = flag.substr( 2 );
        } else if ( flag == L"--show" ) {
            result.show = true;
        } else {
            fail( L"Unknown option: " + arguments[i] );
        }
    }
    return result;
}
