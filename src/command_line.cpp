#include "command_line.h"

// A whole number from 1; 0 when the text is anything else
static int read_page_number( const wstring &text ) {
    if ( text.empty() || text.size() > 6 ) return 0;
    for ( wchar_t c : text ) if ( c < L'0' || c > L'9' ) return 0;
    return std::stoi( text );
}

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
        } else if ( flag == L"--page" ) {
            if ( result.setups.empty() ) { fail( L"--page needs a --setup before it." ); continue; }
            int page = is_value( i + 1 ) ? read_page_number( arguments[i + 1] ) : 0;
            if ( !page ) { fail( L"--page needs a page number after it, such as --page 121." ); continue; }
            result.setups.back().page = page;
            ++i;
        } else if ( flag == L"--list" ) {
            result.action = L"list";
            if ( is_value( i + 1 ) ) result.list_title = arguments[++i];
        } else if ( flag == L"--check" || flag == L"--edit" || flag == L"--background" || flag == L"--panel" ||
                    flag == L"--save-and-close" || flag == L"--reopen" ) {
            result.action = flag.substr( 2 );
        } else if ( flag == L"--show" ) {
            result.show = true;
        } else {
            fail( L"Unknown option: " + arguments[i] );
        }
    }
    return result;
}
