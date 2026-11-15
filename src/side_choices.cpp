#include "side_choices.h"

#include <cwctype>

namespace {

bool has_web_scheme( const wstring &text ) {
    wstring start = lower( text.substr( 0, 8 ) );
    return starts_with( start, L"http://" ) || starts_with( start, L"https://" );
}

bool has_side( const vector<written_item> &items, const wchar_t *side ) {
    for ( const written_item &item : items ) if ( item.side == side ) return true;
    return false;
}

wstring downloads_folder() {
    return join_path( user_folder(), L"Downloads" );
}

}  // namespace

wstring web_address( const wstring &text ) {
    wstring address = trim( text );
    if ( address.empty() ) return L"";
    for ( wchar_t c : address ) if ( std::iswspace( c ) ) return L"";
    return has_web_scheme( address ) ? address : L"https://" + address;
}

wstring web_label( const wstring &address ) {
    size_t start = address.find( L"://" );
    start = start == wstring::npos ? 0 : start + 3;
    size_t end = address.find_first_of( L"/:?#", start );
    wstring host = address.substr( start, end == wstring::npos ? wstring::npos : end - start );
    if ( starts_with( lower( host ), L"www." ) ) host = host.substr( 4 );
    return host;
}

wstring address_in_shortcut( const wstring &text ) {
    size_t start = 0;
    while ( start <= text.size() ) {
        size_t end = text.find( L'\n', start );
        if ( end == wstring::npos ) end = text.size();
        wstring line = trim( text.substr( start, end - start ) );
        start = end + 1;
        size_t equals = line.find( L'=' );
        if ( equals != wstring::npos && lower( trim( line.substr( 0, equals ) ) ) == L"url" )
            return trim( line.substr( equals + 1 ) );
    }
    return L"";
}

wstring start_folder_for( const wstring &root ) {
    return !root.empty() && is_folder( root ) ? root : downloads_folder();
}

wstring empty_side_at( const vector<written_item> &items, double across, int split ) {
    bool left = has_side( items, L"left" ), right = has_side( items, L"right" );
    if ( has_side( items, L"full" ) || ( left && right ) ) return L"";
    if ( !left && !right ) return across < 1.0 / 3.0 ? L"left" : across > 2.0 / 3.0 ? L"right" : L"full";
    bool on_left = across * 100 < split;
    if ( left ) return on_left ? L"" : L"right";
    return on_left ? L"left" : L"";
}

bool item_for_answer( const side_answer &answer, const wstring &side, const wstring &root, written_item &item,
                      wstring &problem ) {
    item = { side, item_kind::pdf, {} };
    vector<wstring> stored;
    for ( const wstring &path : answer.paths ) stored.push_back( relative_to( root, path ) );

    switch ( answer.choice ) {
    case side_choice::notes:
        item.kind = item_kind::notes;
        return true;
    case side_choice::web: {
        wstring address = answer.paths.empty() ? L"" : web_address( answer.paths[0] );
        if ( address.empty() ) {
            problem = L"That is not a web address.";
            return false;
        }
        item.kind = item_kind::web;
        item.paths = { address };
        return true;
    }
    case side_choice::pdf:
        if ( stored.empty() ) {
            problem = L"No PDF was chosen.";
            return false;
        }
        item.kind = stored.size() == 1 ? item_kind::pdf : item_kind::tabs;
        item.paths = stored;
        return true;
    default:
        break;
    }

    if ( stored.empty() ) {
        problem = L"No folder was chosen.";
        return false;
    }
    if ( answer.choice == side_choice::newest ) {
        item.kind = item_kind::newest;
        item.paths = { stored[0] == L"." ? L"*.pdf" : join_path( stored[0], L"*.pdf" ) };
        return true;
    }
    item.kind = answer.choice == side_choice::sequence ? item_kind::sequence
              : answer.choice == side_choice::pick   ? item_kind::pick
                                                      : item_kind::folder;
    item.paths = { stored[0] };
    return true;
}

vector<split_choice> split_choices( int current ) {
    vector<split_choice> choices;
    for ( int split : { 50, 60, 75, 40, 25 } )
        choices.push_back( { split, std::to_wstring( split ) + L" / " + std::to_wstring( 100 - split ) } );
    bool known = false;
    for ( const split_choice &choice : choices ) if ( choice.split == current ) known = true;
    if ( !known && current >= 10 && current <= 90 )
        choices.push_back( { current, std::to_wstring( current ) + L" / " + std::to_wstring( 100 - current ) } );
    return choices;
}

void set_split( profile_book &book, size_t profile_index, int setup_index, int split ) {
    profile_block &profile = book.profiles[profile_index];
    if ( setup_index < 0 ) {
        profile.split = split == 50 ? 0 : split;
        return;
    }
    int subject_split = profile.split ? profile.split : 50;
    profile.setups[setup_index].split = split == subject_split ? 0 : split;
}
