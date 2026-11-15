// Written from tests\specs\setup_options.md.

#include <windows.h>
#include <ole2.h>
#include <shlobj.h>

#include <filesystem>

#include "check.h"
#include "../src/canvas.h"
#include "../src/files.h"
#include "../src/launch.h"
#include "../src/profiles.h"
#include "../src/session.h"
#include "../src/side_choices.h"
#include "../src/text.h"
#include "../src/tiling.h"

namespace {

namespace fs = std::filesystem;

wstring option_temp;    // %TEMP%\crammy option checks
wstring option_root;    // %TEMP%\crammy option checks\ENGL 192

void make_option_folder( const wstring &path ) {
    std::error_code ignored;
    fs::create_directories( fs::path( path ), ignored );
}

void make_option_file( const wstring &path, const wstring &text ) {
    make_option_folder( parent_folder( path ) );
    write_text_file( path, text );
}

vector<wstring> lines_of( const wstring &text ) {
    vector<wstring> lines;
    size_t start = 0;
    while ( start <= text.size() ) {
        size_t end = text.find( L'\n', start );
        if ( end == wstring::npos ) end = text.size();
        wstring line = text.substr( start, end - start );
        if ( !line.empty() && line.back() == L'\r' ) line.pop_back();
        lines.push_back( line );
        start = end + 1;
    }
    return lines;
}

bool has_line( const wstring &text, const wstring &wanted ) {
    for ( const wstring &line : lines_of( text ) )
        if ( line == wanted ) return true;
    return false;
}

bool items_match( const written_item &first, const written_item &second ) {
    return first.side == second.side && first.kind == second.kind && first.paths == second.paths && first.app == second.app;
}

// ---- 1. The web line ----

const wchar_t *web_book_text =
    L"[ENGL 192]\n"
    L"root = %TEMP%\\crammy option checks\\ENGL 192\n"
    L"\n"
    L"[ENGL 192: Reading]\n"
    L"left = pdf \"a.pdf\"\n"
    L"right = web \"https://learn.uwaterloo.ca/d2l/home\" \"https://www.example.com/a b\"\n";

const vector<wstring> web_addresses = { L"https://learn.uwaterloo.ca/d2l/home", L"https://www.example.com/a b" };

void check_web_line() {
    profile_book book = read_book( web_book_text );
    check( book.errors.empty(), "the web line reads without errors" );
    bool shaped = book.profiles.size() == 1 && book.profiles[0].setups.size() == 1 &&
                  book.profiles[0].setups[0].items.size() == 2;
    check( shaped, "the ENGL 192 book has one subject with one setup of two items" );
    if ( !shaped ) return;
    const written_item &web = book.profiles[0].setups[0].items[1];
    check( web.side == L"right" && web.kind == item_kind::web, "the second item is a right web item" );
    check( web.paths == web_addresses, "the web item keeps both addresses as written, in order" );

    vector<setup> setups = all_setups( book );
    check( setups.size() == 1 && setups[0].items.size() == 2 && setups[0].items[1].paths == web_addresses,
           "all_setups leaves the web addresses exactly as written, not joined to the root" );

    vector<setup> variable = all_setups( read_book( L"[X]\nroot = C:\\X\nfull = web \"https://a.ca/%TEMP%\"\n" ) );
    check( variable.size() == 1 && variable[0].items.size() == 1 && variable[0].items[0].paths.size() == 1 &&
               variable[0].items[0].paths[0] == L"https://a.ca/%TEMP%",
           "all_setups does not expand variables in a web address" );

    wstring written = write_book( book );
    profile_book again = read_book( written );
    check( again.errors.empty() && again.profiles.size() == 1 && again.profiles[0].setups.size() == 1 &&
               again.profiles[0].setups[0].items.size() == 2 && items_match( again.profiles[0].setups[0].items[1], web ),
           "writing the book and reading it again gives the same web item" );
    check( has_line( written, L"right = web \"https://learn.uwaterloo.ca/d2l/home\" \"https://www.example.com/a b\"" ),
           "the written text has the web line with both addresses quoted" );

    profile_book empty = read_book( L"[X]\n\nleft = web\n" );
    check( empty.errors.size() == 1 && starts_with( empty.errors[0], L"line 3: " ), "a web line with no address gives one line 3 error" );

    profile_book bare = read_book( L"[X]\n\nleft = web \"learn.uwaterloo.ca\"\n" );
    check( bare.errors.size() == 1 && starts_with( bare.errors[0], L"line 3: " ) && contains_ci( bare.errors[0], L"http" ),
           "a web address without http:// or https:// gives one line 3 error that mentions http" );

    profile_book upper = read_book( L"[X]\n\nleft = web \"HTTPS://a.ca\"\nright = web \"Http://b.ca\"\n" );
    check( upper.errors.empty(), "http:// and https:// in capitals are accepted in the file" );

    profile_book unknown = read_book( L"[X]\n\nleft = magic \"x\"\n" );
    check( unknown.errors.size() == 1 && starts_with( unknown.errors[0], L"line 3: " ), "an unknown kind still gives its line 3 error" );
    check( unknown.errors.size() == 1 && contains_ci( unknown.errors[0], L"web" ), "the unknown kind error lists web among the kinds" );
}

// ---- 2. Opening web pages ----

void check_web_plan() {
    check( wstring( program_file( program::chrome ) ) == L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
           "Chrome's program file is in Program Files\\Google\\Chrome\\Application" );
    check( chrome_command( { L"https://a.ca/x", L"https://b.ca/y z" } ) ==
               L"\"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" --new-window \"https://a.ca/x\" \"https://b.ca/y z\"",
           "chrome_command quotes Chrome and each address after --new-window" );

    make_option_file( option_root + L"\\a.pdf", L"x\r\n" );
    vector<setup> setups = all_setups( read_book( web_book_text ) );
    check( setups.size() == 1, "the ENGL 192 book gives one setup" );
    if ( setups.size() == 1 ) {
        const setup &chosen = setups[0];
        vector<vector<wstring>> picks( chosen.items.size() );
        vector<wstring> problems;
        vector<planned_window> windows = plan_setup( chosen, picks, false, problems );
        check( windows.size() == 2 && problems.empty(), "the reading setup plans two windows with no problems" );
        if ( windows.size() == 2 ) {
            check( windows[0].side == L"left" && windows[0].app == program::pdfcraft && windows[0].paths.size() == 1 &&
                       windows[0].paths[0] == option_root + L"\\a.pdf",
                   "the first window is PdfCraft on the left with a.pdf in the root" );
            check( windows[1].side == L"right" && windows[1].app == program::chrome && windows[1].paths == web_addresses,
                   "the second window is one Chrome window on the right with both addresses in order" );
            check( windows[0].split == chosen.split && windows[1].split == chosen.split, "both windows have the setup's split" );
        }
    }

    vector<setup> only_web =
        all_setups( read_book( L"[X]\nroot = %TEMP%\\crammy option checks\\ENGL 192\nfull = web \"https://a.ca/no such file\"\n" ) );
    check( only_web.size() == 1, "a subject with one web line gives one setup" );
    if ( only_web.size() == 1 ) {
        vector<vector<wstring>> picks( only_web[0].items.size() );
        vector<wstring> problems;
        vector<planned_window> windows = plan_setup( only_web[0], picks, false, problems );
        check( windows.size() == 1 && windows[0].app == program::chrome && problems.empty(),
               "a setup of one web line plans one Chrome window and no problems" );
    }
}

// ---- 3. Choices for an empty side ----

side_answer answer_of( side_choice choice, vector<wstring> paths, bool accepted = true ) {
    side_answer answer;
    answer.accepted = accepted;
    answer.choice = choice;
    answer.paths = std::move( paths );
    return answer;
}

// Calls item_for_answer and checks the parts every accepted case shares
bool answer_gives( const side_answer &answer, const wstring &side, written_item &item ) {
    wstring problem;
    item = written_item{};
    item.side = L"wrong";
    item.app = viewer::acrobat;
    bool made = item_for_answer( answer, side, option_root, item, problem );
    return made && item.side == side && item.app == viewer::pdfcraft;
}

bool answer_refused( const side_answer &answer ) {
    written_item item{};
    wstring problem;
    bool made = item_for_answer( answer, L"left", option_root, item, problem );
    return !made && !problem.empty();
}

void check_answers() {
    written_item item{};

    check( answer_gives( answer_of( side_choice::pdf, { option_root + L"\\3 Assignments\\A2.pdf" } ), L"left", item ) &&
               item.kind == item_kind::pdf && item.paths == vector<wstring>{ L"3 Assignments\\A2.pdf" },
           "a pdf answer with one path inside the root gives a left pdf item with the path relative" );

    check( answer_gives( answer_of( side_choice::pdf, { option_root + L"\\a.pdf", option_root + L"\\3 Assignments\\b.pdf" } ),
                         L"right", item ) &&
               item.kind == item_kind::tabs && item.paths == vector<wstring>{ L"a.pdf", L"3 Assignments\\b.pdf" },
           "a pdf answer with two paths gives a tabs item with both relative, in order, on the side passed in" );

    check( answer_gives( answer_of( side_choice::pdf, { L"D:\\Other\\x.pdf" } ), L"left", item ) &&
               item.kind == item_kind::pdf && item.paths == vector<wstring>{ L"D:\\Other\\x.pdf" },
           "a pdf answer outside the root keeps the whole path" );

    check( answer_gives( answer_of( side_choice::newest, { option_root + L"\\1 Lectures" } ), L"full", item ) &&
               item.kind == item_kind::newest && item.paths == vector<wstring>{ L"1 Lectures\\*.pdf" },
           "a newest answer with a folder gives a newest item of that folder's *.pdf" );

    check( answer_gives( answer_of( side_choice::newest, { option_root } ), L"left", item ) && item.kind == item_kind::newest &&
               item.paths == vector<wstring>{ L"*.pdf" },
           "a newest answer with the root itself gives *.pdf" );

    const struct {
        side_choice choice;
        item_kind kind;
        const char *in_folder;
        const char *at_root;
    } folder_kinds[] = {
        { side_choice::sequence, item_kind::sequence, "a sequence answer with a folder gives a sequence item of that folder",
          "a sequence answer with the root gives the path ." },
        { side_choice::pick, item_kind::pick, "a pick answer with a folder gives a pick item of that folder",
          "a pick answer with the root gives the path ." },
        { side_choice::folder, item_kind::folder, "a folder answer with a folder gives a folder item of that folder",
          "a folder answer with the root gives the path ." },
    };
    for ( const auto &each : folder_kinds ) {
        check( answer_gives( answer_of( each.choice, { option_root + L"\\1 Lectures" } ), L"left", item ) && item.kind == each.kind &&
                   item.paths == vector<wstring>{ L"1 Lectures" },
               each.in_folder );
        check( answer_gives( answer_of( each.choice, { option_root } ), L"left", item ) && item.kind == each.kind &&
                   item.paths == vector<wstring>{ L"." },
               each.at_root );
    }

    check( answer_gives( answer_of( side_choice::web, { L"learn.uwaterloo.ca/d2l" } ), L"left", item ) &&
               item.kind == item_kind::web && item.paths == vector<wstring>{ L"https://learn.uwaterloo.ca/d2l" },
           "a web answer gives a web item with https:// added" );

    check( answer_gives( answer_of( side_choice::notes, {} ), L"left", item ) && item.kind == item_kind::notes && item.paths.empty(),
           "a notes answer gives a notes item with no paths" );

    check( answer_refused( answer_of( side_choice::pdf, {} ) ), "a pdf answer with no paths is refused with a problem" );
    check( answer_refused( answer_of( side_choice::newest, {} ) ), "a newest answer with no paths is refused with a problem" );
    check( answer_refused( answer_of( side_choice::web, { L"two words" } ) ), "a web answer with a space inside is refused with a problem" );
    check( answer_refused( answer_of( side_choice::web, { L"" } ) ), "a web answer with an empty text is refused with a problem" );
}

// ---- 4. Small helpers ----

void check_helpers() {
    const struct {
        const wchar_t *typed;
        const wchar_t *gives;
        const char *what;
    } addresses[] = {
        { L"https://a.ca", L"https://a.ca", "web_address keeps an address that starts with https://" },
        { L"HTTP://a.ca/x", L"HTTP://a.ca/x", "web_address keeps an address that starts with HTTP:// in capitals" },
        { L"  a.ca/x  ", L"https://a.ca/x", "web_address trims spaces and adds https://" },
        { L"www.youtube.com/watch?v=1", L"https://www.youtube.com/watch?v=1", "web_address adds https:// in front of www" },
        { L"", L"", "web_address of an empty text is empty" },
        { L"   ", L"", "web_address of only spaces is empty" },
        { L"a b.ca", L"", "web_address of a text with a space inside is empty" },
    };
    for ( const auto &each : addresses ) check( web_address( each.typed ) == each.gives, each.what );

    check( web_label( L"https://www.example.com/a/b" ) == L"example.com", "web_label drops www. and the path" );
    check( web_label( L"https://learn.uwaterloo.ca/d2l/home" ) == L"learn.uwaterloo.ca", "web_label gives the host learn.uwaterloo.ca" );
    check( web_label( L"http://a.ca" ) == L"a.ca", "web_label of http://a.ca is a.ca" );
    check( web_label( L"https://a.ca:8080/x?y" ) == L"a.ca", "web_label drops the port, path and query" );

    check( address_in_shortcut( L"[InternetShortcut]\r\nURL=https://a.ca/x\r\nIDList=\r\n" ) == L"https://a.ca/x",
           "address_in_shortcut reads the URL= line" );
    check( address_in_shortcut( L"[InternetShortcut]\r\nIDList=\r\n" ).empty(), "a shortcut text with no URL= line gives an empty address" );
    check( address_in_shortcut( L"[InternetShortcut]\r\nurl = https://b.ca\r\n" ) == L"https://b.ca",
           "address_in_shortcut accepts url in other case with spaces around =" );

    wstring downloads = expand_variables( L"%USERPROFILE%\\Downloads" );
    check( start_folder_for( option_root ) == option_root, "start_folder_for an existing folder gives that folder" );
    check( start_folder_for( option_temp + L"\\No Such Course" ) == downloads,
           "start_folder_for a folder that does not exist gives Downloads" );
    check( start_folder_for( L"" ) == downloads, "start_folder_for an empty root gives Downloads" );

    vector<written_item> none;
    check( empty_side_at( none, 0.1, 50 ) == L"left", "with no items 0.1 across is the left side" );
    check( empty_side_at( none, 0.5, 50 ) == L"full", "with no items 0.5 across is the whole screen" );
    check( empty_side_at( none, 0.9, 50 ) == L"right", "with no items 0.9 across is the right side" );

    vector<written_item> left_only = { { L"left", item_kind::pdf, { L"a.pdf" } } };
    check( empty_side_at( left_only, 0.7, 50 ) == L"right", "with a left item and split 50, 0.7 across is the empty right side" );
    check( empty_side_at( left_only, 0.3, 50 ).empty(), "with a left item and split 50, 0.3 across is already taken" );
    check( empty_side_at( left_only, 0.7, 75 ).empty(), "with a left item and split 75, 0.7 across is still the left tile" );
    check( empty_side_at( left_only, 0.8, 75 ) == L"right", "with a left item and split 75, 0.8 across is the empty right side" );

    vector<written_item> right_only = { { L"right", item_kind::pdf, { L"a.pdf" } } };
    check( empty_side_at( right_only, 0.2, 25 ) == L"left", "with a right item and split 25, 0.2 across is the empty left side" );
    check( empty_side_at( right_only, 0.3, 25 ).empty(), "with a right item and split 25, 0.3 across is the right tile" );

    vector<written_item> full_only = { { L"full", item_kind::pdf, { L"a.pdf" } } };
    vector<written_item> both = { { L"left", item_kind::pdf, { L"a.pdf" } }, { L"right", item_kind::notes, {} } };
    bool all_taken = true;
    for ( double across : { 0.05, 0.2, 0.4, 0.5, 0.6, 0.8, 0.95 } )
        for ( int split : { 25, 50, 75 } )
            all_taken = all_taken && empty_side_at( full_only, across, split ).empty() && empty_side_at( both, across, split ).empty();
    check( all_taken, "with a full item, or with both a left and a right item, no place is empty" );
}

// ---- 5. Splits ----

bool choices_are( const vector<split_choice> &choices, const vector<int> &splits, const vector<wstring> &labels ) {
    if ( choices.size() != splits.size() ) return false;
    for ( size_t at = 0; at < choices.size(); ++at )
        if ( choices[at].split != splits[at] || choices[at].label != labels[at] ) return false;
    return true;
}

const wchar_t *split_book_text =
    L"[Math 115]\n"
    L"root = C:\\Courses\\Math 115\n"
    L"\n"
    L"[Math 115: Exam]\n"
    L"split = 60\n"
    L"\n"
    L"[Math 115: Lecture]\n"
    L"\n"
    L"[ECE 198]\n"
    L"root = C:\\Courses\\ECE 198\n"
    L"split = 60\n"
    L"full = pick \".\"\n"
    L"\n"
    L"[ECE 190]\n"
    L"root = C:\\Courses\\ECE 190\n"
    L"split = 75\n"
    L"\n"
    L"[ECE 190: Lecture]\n";

// The split all_setups gives the setup with this title, or -1 when there is none
int shown_split( const profile_book &book, const wstring &title ) {
    for ( const setup &each : all_setups( book ) )
        if ( each.title == title ) return each.split;
    return -1;
}

// Writes and reads the book again, and checks that nothing about the splits changed
bool splits_survive( const profile_book &book ) {
    profile_book again = read_book( write_book( book ) );
    if ( !again.errors.empty() || again.profiles.size() != book.profiles.size() ) return false;
    for ( size_t at = 0; at < book.profiles.size(); ++at ) {
        const profile_block &one = book.profiles[at], &two = again.profiles[at];
        if ( one.split != two.split || one.setups.size() != two.setups.size() ) return false;
        for ( size_t each = 0; each < one.setups.size(); ++each )
            if ( one.setups[each].split != two.setups[each].split ) return false;
    }
    vector<setup> first = all_setups( book ), second = all_setups( again );
    if ( first.size() != second.size() ) return false;
    for ( size_t at = 0; at < first.size(); ++at )
        if ( first[at].title != second[at].title || first[at].split != second[at].split ) return false;
    return true;
}

void check_splits() {
    vector<int> five = { 50, 60, 75, 40, 25 };
    vector<wstring> five_labels = { L"50 / 50", L"60 / 40", L"75 / 25", L"40 / 60", L"25 / 75" };
    check( choices_are( split_choices( 50 ), five, five_labels ), "split_choices( 50 ) gives the five usual splits in order" );
    check( choices_are( split_choices( 60 ), five, five_labels ), "split_choices( 60 ) gives the same five" );
    check( choices_are( split_choices( 70 ), { 50, 60, 75, 40, 25, 70 },
                        { L"50 / 50", L"60 / 40", L"75 / 25", L"40 / 60", L"25 / 75", L"70 / 30" } ),
           "split_choices( 70 ) gives the five and then 70 labelled 70 / 30" );

    profile_book start = read_book( split_book_text );
    check( start.errors.empty() && start.profiles.size() == 3 && start.profiles[0].setups.size() == 2 &&
               start.profiles[1].setups.empty() && start.profiles[2].setups.size() == 1,
           "the split book reads without errors as three subjects" );
    if ( start.profiles.size() != 3 || start.profiles[0].setups.size() != 2 || start.profiles[2].setups.size() != 1 ) return;

    {
        profile_book book = start;
        set_split( book, 0, 1, 75 );
        check( book.profiles[0].setups[1].split == 75, "set_split gives Math 115: Lecture its own split 75" );
        check( shown_split( book, L"Math 115: Lecture" ) == 75 && shown_split( book, L"Math 115: Exam" ) == 60,
               "all_setups then shows 75 for Lecture and still 60 for Exam" );
        check( splits_survive( book ), "after setting Lecture to 75 the book writes and reads back with the same splits" );
    }
    {
        profile_book book = start;
        set_split( book, 0, 0, 50 );
        check( book.profiles[0].setups[0].split == 0, "setting Exam to 50 leaves it with no split of its own" );
        check( shown_split( book, L"Math 115: Exam" ) == 50, "all_setups then shows 50 for Exam" );
        check( splits_survive( book ), "after setting Exam to 50 the book writes and reads back with the same splits" );
    }
    {
        profile_book book = start;
        set_split( book, 2, 0, 75 );
        check( book.profiles[2].setups[0].split == 0, "setting ECE 190: Lecture to its subject's 75 leaves it with no split of its own" );
        check( shown_split( book, L"ECE 190: Lecture" ) == 75, "all_setups still shows 75 for ECE 190: Lecture" );
        check( splits_survive( book ), "after setting ECE 190: Lecture to 75 the book writes and reads back with the same splits" );
        set_split( book, 2, 0, 50 );
        check( book.profiles[2].setups[0].split == 50, "setting ECE 190: Lecture to 50 gives it its own 50" );
        check( shown_split( book, L"ECE 190: Lecture" ) == 50, "all_setups then shows 50 for ECE 190: Lecture" );
        check( splits_survive( book ), "after setting ECE 190: Lecture to 50 the book writes and reads back with the same splits" );
    }
    {
        profile_book book = start;
        set_split( book, 1, -1, 25 );
        check( book.profiles[1].split == 25, "set_split with index -1 sets the subject ECE 198's split to 25" );
        check( shown_split( book, L"ECE 198" ) == 25, "all_setups then shows 25 for ECE 198" );
        check( splits_survive( book ), "after setting ECE 198 to 25 the book writes and reads back with the same splits" );
        set_split( book, 1, -1, 50 );
        check( book.profiles[1].split == 0, "setting the subject ECE 198 to 50 leaves its split at 0" );
        check( shown_split( book, L"ECE 198" ) == 50, "all_setups then shows 50 for ECE 198" );
        check( splits_survive( book ), "after setting ECE 198 to 50 the book writes and reads back with the same splits" );
    }
}

// ---- 6. The screen picture ----

// What the test asker was called with, and what it gives back
int asker_calls = 0;
wstring asker_side;
wstring asker_start;
side_answer asker_answer;

side_answer test_asker( HWND, const wstring &side, const wstring &start_folder ) {
    ++asker_calls;
    asker_side = side;
    asker_start = start_folder;
    return asker_answer;
}

void reset_asker( side_answer answer ) {
    asker_calls = 0;
    asker_side.clear();
    asker_start.clear();
    asker_answer = std::move( answer );
}

// The canvas's parent is a hidden STATIC window; this counts canvas_changed_message on it
WNDPROC parent_old_proc = nullptr;
int changed_count = 0;

LRESULT CALLBACK counting_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == canvas_changed_message ) ++changed_count;
    return CallWindowProcW( parent_old_proc, window, message, wparam, lparam );
}

void click_at( HWND canvas, double across, double down ) {
    RECT client;
    GetClientRect( canvas, &client );
    int x = int( client.right * across ), y = int( client.bottom * down );
    SendMessageW( canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM( x, y ) );
    SendMessageW( canvas, WM_LBUTTONUP, 0, MAKELPARAM( x, y ) );
}

IDataObject *option_files_object( const vector<wstring> &paths ) {
    wstring list;
    for ( const wstring &path : paths ) list += path + L'\0';
    list += L'\0';
    size_t bytes = sizeof( DROPFILES ) + list.size() * sizeof( wchar_t );
    HGLOBAL memory = GlobalAlloc( GHND, bytes );
    auto *drop = static_cast<DROPFILES *>( GlobalLock( memory ) );
    drop->pFiles = sizeof( DROPFILES );
    drop->fWide = TRUE;
    memcpy( reinterpret_cast<BYTE *>( drop ) + sizeof( DROPFILES ), list.data(), list.size() * sizeof( wchar_t ) );
    GlobalUnlock( memory );

    IDataObject *data = nullptr;
    SHCreateDataObject( nullptr, 0, nullptr, nullptr, IID_PPV_ARGS( &data ) );
    FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium{};
    medium.tymed = TYMED_HGLOBAL;
    medium.hGlobal = memory;
    data->SetData( &format, &medium, TRUE );
    return data;
}

void option_drop_at( HWND canvas, const vector<wstring> &paths, double across, double down ) {
    RECT client;
    GetClientRect( canvas, &client );
    POINT point = { LONG( client.right * across ), LONG( client.bottom * down ) };
    ClientToScreen( canvas, &point );
    POINTL at = { point.x, point.y };
    IDataObject *data = option_files_object( paths );
    DWORD effect = DROPEFFECT_COPY;
    IDropTarget *target = canvas_drop_target();
    target->DragEnter( data, MK_LBUTTON, at, &effect );
    target->DragOver( MK_LBUTTON, at, &effect );
    target->Drop( data, MK_LBUTTON, at, &effect );
    data->Release();
}

void check_canvas( HWND canvas ) {
    HWND parent = GetParent( canvas );
    parent_old_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW( parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( counting_proc ) ) );

    wstring lectures = option_root + L"\\1 Lectures";
    wstring pdf = option_root + L"\\b.pdf";
    wstring shortcut = option_root + L"\\Learn.url";
    make_option_folder( lectures );
    make_option_file( option_root + L"\\a.pdf", L"x\r\n" );
    make_option_file( pdf, L"x\r\n" );
    make_option_file( shortcut, L"[InternetShortcut]\r\nURL=https://a.ca/x\r\n" );

    const written_item left_item = { L"left", item_kind::pdf, { L"a.pdf" } };
    set_side_asker( test_asker );
    set_canvas_split( 50 );

    {
        vector<written_item> items;
        show_items( &items, option_root );
        reset_asker( answer_of( side_choice::newest, { lectures } ) );
        int changes = changed_count;
        click_at( canvas, 0.5, 0.5 );
        check( asker_calls == 1 && asker_side == L"full" && asker_start == option_root,
               "a click in the middle of an empty setup asks once for the full side, starting in the root" );
        check( items.size() == 1 && items[0].side == L"full" && items[0].kind == item_kind::newest &&
                   items[0].paths == vector<wstring>{ L"1 Lectures\\*.pdf" },
               "a newest answer for 1 Lectures makes one full newest item of 1 Lectures\\*.pdf" );
        check( changed_count > changes, "adding the newest item tells the parent the canvas changed" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        reset_asker( answer_of( side_choice::web, { L"a.ca" } ) );
        int changes = changed_count;
        click_at( canvas, 0.8, 0.5 );
        check( asker_calls == 1 && asker_side == L"right", "with a left item, a click at 0.8 asks for the right side" );
        check( items.size() == 2 && items[1].side == L"right" && items[1].kind == item_kind::web &&
                   items[1].paths == vector<wstring>{ L"https://a.ca" },
               "a web answer of a.ca adds a right web item of https://a.ca" );
        check( items.size() == 2 && items_match( items[0], left_item ), "the left item is unchanged after the web item is added" );
        check( changed_count > changes, "adding the web item tells the parent the canvas changed" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        reset_asker( answer_of( side_choice::web, { L"a.ca" } ) );
        click_at( canvas, 0.3, 0.5 );
        check( asker_calls == 0, "a click on the left tile does not ask anything" );
        check( items.size() == 1 && items_match( items[0], left_item ), "a click on the left tile adds nothing" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        reset_asker( answer_of( side_choice::web, { L"a.ca" }, false ) );
        click_at( canvas, 0.8, 0.5 );
        check( asker_calls == 1 && items.size() == 1, "an answer that was not accepted adds nothing" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        reset_asker( answer_of( side_choice::web, { L"two words" } ) );
        click_at( canvas, 0.8, 0.5 );
        check( asker_calls == 1 && items.size() == 1, "an answer that item_for_answer refuses adds nothing" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        set_canvas_split( 75 );
        reset_asker( answer_of( side_choice::web, { L"a.ca" }, false ) );
        click_at( canvas, 0.7, 0.5 );
        check( asker_calls == 0, "with split 75 and a left item, a click at 0.7 is on the left tile and asks nothing" );
        click_at( canvas, 0.85, 0.5 );
        check( asker_calls == 1 && asker_side == L"right", "with split 75 and a left item, a click at 0.85 asks for the right side" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        set_canvas_split( 75 );
        int changes = changed_count;
        option_drop_at( canvas, { pdf }, 0.7, 0.5 );
        check( items.size() == 2 && items[1].side == L"left", "with split 75 and a left item, a PDF dropped at 0.7 goes to the left side" );
        check( changed_count > changes, "the drop at 0.7 tells the parent the canvas changed" );
    }
    {
        vector<written_item> items = { left_item };
        show_items( &items, option_root );
        set_canvas_split( 75 );
        option_drop_at( canvas, { pdf }, 0.85, 0.5 );
        check( items.size() == 2 && items[1].side == L"right",
               "with split 75 and a left item, a PDF dropped at 0.85 goes to the right side" );
    }
    {
        vector<written_item> items;
        set_canvas_split( 50 );
        show_items( &items, option_root );
        int changes = changed_count;
        option_drop_at( canvas, { shortcut }, 0.9, 0.5 );
        check( items.size() == 1 && items[0].side == L"right" && items[0].kind == item_kind::web &&
                   items[0].paths == vector<wstring>{ L"https://a.ca/x" },
               "an Internet shortcut dropped on the right adds a right web item with its address" );
        check( changed_count > changes, "the shortcut drop tells the parent the canvas changed" );
    }

    set_side_asker( nullptr );
    set_canvas_split( 50 );
    show_items( nullptr, L"" );
    SetWindowLongPtrW( parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( parent_old_proc ) );
}

// ---- 7. Saving and closing ----

void check_chrome_not_study() {
    check( !is_study_window( L"chrome.exe", L"Chrome_WidgetWin_1" ), "a Chrome window is not a study window" );
    check( app_of( L"chrome.exe" ).empty(), "chrome.exe has no app name in the session file" );
}

}  // namespace

void test_setup_options( HWND canvas ) {
    option_temp = join_path( expand_variables( L"%TEMP%" ), L"crammy option checks" );
    option_root = join_path( option_temp, L"ENGL 192" );
    std::error_code ignored;
    fs::remove_all( fs::path( option_temp ), ignored );
    make_option_folder( option_root );

    check_web_line();
    check_web_plan();
    check_answers();
    check_helpers();
    check_splits();
    check_canvas( canvas );
    check_chrome_not_study();

    fs::remove_all( fs::path( option_temp ), ignored );
}
