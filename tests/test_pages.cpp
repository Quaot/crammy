// Written from tests\specs\pages.md.

#include <filesystem>
#include <string>

#include "check.h"
#include "../src/command_line.h"
#include "../src/launch.h"
#include "../src/profiles.h"
#include "../src/text.h"
#include "../src/tiling.h"

namespace {

namespace fs = std::filesystem;

wstring page_temp;      // %TEMP%\crammy page checks
wstring page_root;      // %TEMP%\crammy page checks\Math 117

void make_page_folder( const wstring &path ) {
    std::error_code ignored;
    fs::create_directories( fs::path( path ), ignored );
}

void make_page_file( const wstring &path ) {
    make_page_folder( parent_folder( path ) );
    write_text_file( path, L"x\r\n" );
}

vector<wstring> page_lines_of( const wstring &text ) {
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

bool has_page_line( const wstring &text, const wstring &wanted ) {
    for ( const wstring &line : page_lines_of( text ) )
        if ( line == wanted ) return true;
    return false;
}

// Every item in the book, from the subjects and from their setups
vector<written_item> every_item( const profile_book &book ) {
    vector<written_item> items;
    for ( const profile_block &profile : book.profiles ) {
        items.insert( items.end(), profile.items.begin(), profile.items.end() );
        for ( const setup_block &each : profile.setups ) items.insert( items.end(), each.items.begin(), each.items.end() );
    }
    return items;
}

// Reads a book of one subject [X] with this window line on line 3
profile_book one_line_book( const wstring &line ) {
    return read_book( L"[X]\nroot = C:\\X\n" + line + L"\n" );
}

bool same_item( const written_item &first, const written_item &second ) {
    return first.side == second.side && first.kind == second.kind && first.paths == second.paths && first.app == second.app &&
           first.page == second.page;
}

// ---- 1. page N on a window line ----

const wchar_t *notes_book_text =
    L"[Math 117]\n"
    L"root = %TEMP%\\crammy page checks\\Math 117\n"
    L"\n"
    L"[Math 117: Notes]\n"
    L"left = pdf \"notes.pdf\" page 121\n"
    L"right = newest \"1 Lectures\\*.pdf\"\n";

void check_reading_pages() {
    profile_book book = read_book( notes_book_text );
    check( book.errors.empty(), "the Math 117 notes book reads with no errors" );
    bool shaped = book.profiles.size() == 1 && book.profiles[0].setups.size() == 1 && book.profiles[0].setups[0].items.size() == 2;
    check( shaped, "the Math 117 notes book has one subject with one setup of two items" );
    if ( shaped ) {
        const vector<written_item> &items = book.profiles[0].setups[0].items;
        check( items[0].page == 121, "the first item of the notes setup has page 121" );
        check( items[0].kind == item_kind::pdf && items[0].paths == vector<wstring>{ L"notes.pdf" },
               "the first item is a pdf item of notes.pdf" );
        check( items[1].page == 0, "the second item, with no page given, has page 0" );
    }

    vector<setup> setups = all_setups( book );
    check( setups.size() == 1 && setups[0].items.size() == 2, "all_setups gives one setup of two items for the notes book" );
    if ( setups.size() == 1 && setups[0].items.size() == 2 ) {
        check( setups[0].items[0].page == 121, "all_setups gives the first layout item page 121" );
        check( setups[0].items[1].page == 0, "all_setups gives the second layout item page 0" );
    }

    const struct {
        const wchar_t *line;
        item_kind kind;
        int page;
        const char *what;
    } kinds[] = {
        { L"left = pdf \"a.pdf\" page 2", item_kind::pdf, 2, "a pdf line ending in page 2 reads with no error and page 2" },
        { L"left = newest \"L*.pdf\" page 3", item_kind::newest, 3, "a newest line ending in page 3 reads with no error and page 3" },
        { L"left = tabs \"a.pdf\" \"b.pdf\" page 4", item_kind::tabs, 4, "a tabs line ending in page 4 reads with no error and page 4" },
        { L"left = pick \"Exams\" page 5", item_kind::pick, 5, "a pick line ending in page 5 reads with no error and page 5" },
        { L"left = sequence \"Slides\" page 6", item_kind::sequence, 6,
          "a sequence line ending in page 6 reads with no error and page 6" },
    };
    for ( const auto &each : kinds ) {
        profile_book one = one_line_book( each.line );
        vector<written_item> items = every_item( one );
        check( one.errors.empty() && items.size() == 1 && items[0].kind == each.kind && items[0].page == each.page, each.what );
    }
    {
        vector<written_item> items = every_item( one_line_book( L"left = tabs \"a.pdf\" \"b.pdf\" page 4" ) );
        check( items.size() == 1 && items[0].paths == vector<wstring>{ L"a.pdf", L"b.pdf" },
               "a tabs line with a page keeps both paths and no more" );
    }

    {
        profile_book upper = one_line_book( L"left = pdf \"a.pdf\" PAGE 5" );
        vector<written_item> items = every_item( upper );
        check( upper.errors.empty() && items.size() == 1 && items[0].page == 5, "the word PAGE in capitals gives page 5" );
        profile_book mixed = one_line_book( L"left = pdf \"a.pdf\" Page 7" );
        items = every_item( mixed );
        check( mixed.errors.empty() && items.size() == 1 && items[0].page == 7, "the word Page with one capital gives page 7" );
    }

    {
        profile_book page_first = one_line_book( L"left = pdf \"a.pdf\" page 3 with acrobat" );
        vector<written_item> items = every_item( page_first );
        check( page_first.errors.empty() && items.size() == 1 && items[0].app == viewer::acrobat && items[0].page == 3 &&
                   items[0].paths == vector<wstring>{ L"a.pdf" },
               "page 3 before with acrobat gives Acrobat, page 3 and the one path a.pdf" );
        profile_book acrobat_first = one_line_book( L"left = pdf \"a.pdf\" with acrobat page 3" );
        items = every_item( acrobat_first );
        check( acrobat_first.errors.empty() && items.size() == 1 && items[0].app == viewer::acrobat && items[0].page == 3 &&
                   items[0].paths == vector<wstring>{ L"a.pdf" },
               "with acrobat before page 3 gives Acrobat, page 3 and the one path a.pdf" );
    }

    {
        profile_book quoted_page = one_line_book( L"left = pdf \"my page 3.pdf\"" );
        vector<written_item> items = every_item( quoted_page );
        check( quoted_page.errors.empty() && items.size() == 1 && items[0].paths == vector<wstring>{ L"my page 3.pdf" } &&
                   items[0].page == 0,
               "page 3 inside the quotes is part of the path my page 3.pdf and gives page 0" );
        profile_book quoted_acrobat = one_line_book( L"left = pdf \"a with acrobat page 2.pdf\"" );
        items = every_item( quoted_acrobat );
        check( quoted_acrobat.errors.empty() && items.size() == 1 &&
                   items[0].paths == vector<wstring>{ L"a with acrobat page 2.pdf" } && items[0].page == 0 &&
                   items[0].app == viewer::pdfcraft,
               "with acrobat page 2 inside the quotes is part of the path and leaves PdfCraft and page 0" );
    }

    const struct {
        const wchar_t *line;
        const char *what;
    } bad_numbers[] = {
        { L"left = pdf \"a.pdf\" page 0", "page 0 gives one line 3 error about the page and adds no item" },
        { L"left = pdf \"a.pdf\" page x", "page x gives one line 3 error about the page and adds no item" },
        { L"left = pdf \"a.pdf\" page", "page with no number gives one line 3 error about the page and adds no item" },
        { L"left = pdf \"a.pdf\" page -2", "page -2 gives one line 3 error about the page and adds no item" },
        { L"left = pdf \"a.pdf\" page 1.5", "page 1.5 gives one line 3 error about the page and adds no item" },
    };
    for ( const auto &each : bad_numbers ) {
        profile_book bad = one_line_book( each.line );
        check( bad.errors.size() == 1 && starts_with( bad.errors[0], L"line 3: " ) && contains_ci( bad.errors[0], L"page" ) &&
                   every_item( bad ).empty(),
               each.what );
    }

    {
        profile_book later = read_book( L"[X]\nroot = C:\\X\n\nright = pdf \"b.pdf\"\nleft = pdf \"a.pdf\" page x\n" );
        check( later.errors.size() == 1 && starts_with( later.errors[0], L"line 5: " ) && every_item( later ).size() == 1,
               "a bad page on line 5 gives one line 5 error and keeps the good line before it" );
    }

    const struct {
        const wchar_t *line;
        const char *what;
    } not_pdf_lines[] = {
        { L"left = notes page 2", "a notes line with a page gives one line 3 error about the page and adds no item" },
        { L"left = folder \"x\" page 2", "a folder line with a page gives one line 3 error about the page and adds no item" },
        { L"left = web \"https://a.com\" page 2", "a web line with a page gives one line 3 error about the page and adds no item" },
    };
    for ( const auto &each : not_pdf_lines ) {
        profile_book bad = one_line_book( each.line );
        check( bad.errors.size() == 1 && starts_with( bad.errors[0], L"line 3: " ) && contains_ci( bad.errors[0], L"page" ) &&
                   every_item( bad ).empty(),
               each.what );
    }
}

void check_writing_pages() {
    {
        wstring written = write_book( one_line_book( L"left = pdf \"a.pdf\" page 121" ) );
        check( has_page_line( written, L"left = pdf \"a.pdf\" page 121" ), "an item with page 121 is written as left = pdf \"a.pdf\" page 121" );
    }
    {
        wstring written = write_book( one_line_book( L"left = pdf \"a.pdf\" with acrobat page 3" ) );
        check( has_page_line( written, L"left = pdf \"a.pdf\" page 3 with acrobat" ),
               "an Acrobat item with page 3 is written with the page before with acrobat" );
    }
    {
        wstring written = write_book( one_line_book( L"left = tabs \"a.pdf\" \"b.pdf\" page 4" ) );
        check( has_page_line( written, L"left = tabs \"a.pdf\" \"b.pdf\" page 4" ), "a tabs item with page 4 is written with the page after both paths" );
    }
    {
        wstring written = write_book( one_line_book( L"left = pdf \"a.pdf\"" ) );
        check( has_page_line( written, L"left = pdf \"a.pdf\"" ), "an item with page 0 is written as left = pdf \"a.pdf\"" );
        bool page_word = false;
        for ( const wstring &line : page_lines_of( written ) )
            if ( starts_with( line, L"left =" ) && contains_ci( line, L"page" ) ) page_word = true;
        check( !page_word, "an item with page 0 is written with no page word" );
    }
    {
        wstring written = write_book( one_line_book( L"left = pdf \"a.pdf\" with acrobat" ) );
        check( has_page_line( written, L"left = pdf \"a.pdf\" with acrobat" ), "an Acrobat item with page 0 is written with no page word" );
    }

    const wchar_t *round_text =
        L"[Math 117]\n"
        L"root = C:\\Courses\\Math 117\n"
        L"left = pdf \"notes.pdf\" page 121\n"
        L"\n"
        L"[Math 117: Reading]\n"
        L"left = pdf \"a.pdf\" with acrobat page 3\n"
        L"right = tabs \"b.pdf\" \"c.pdf\" page 4\n"
        L"\n"
        L"[Math 117: Practice]\n"
        L"left = pick \"Exams\" page 2\n"
        L"right = newest \"1 Lectures\\*.pdf\"\n"
        L"full = sequence \"Slides\" page 8\n";
    profile_book first = read_book( round_text );
    check( first.errors.empty(), "the round trip book reads with no errors" );
    profile_book again = read_book( write_book( first ) );
    check( again.errors.empty(), "the written round trip book reads again with no errors" );
    vector<written_item> before = every_item( first ), after = every_item( again );
    bool same = before.size() == 6 && before.size() == after.size();
    for ( size_t at = 0; same && at < before.size(); ++at ) same = same_item( before[at], after[at] );
    check( same, "reading what was written gives the same six items again, pages included" );
    check( write_book( again ) == write_book( first ), "writing the book read back gives the same text" );
}

// ---- 2. --page on the command line ----

void check_command_line_pages() {
    {
        command_line line = parse_command_line( { L"--setup", L"Math 117: Reading", L"--pick", L"MATH117-Notes-v3.pdf", L"--page", L"121" } );
        check( line.error.empty(), "--setup with --pick and --page 121 gives no error" );
        check( line.setups.size() == 1 && line.setups[0].title == L"Math 117: Reading" &&
                   line.setups[0].picks == vector<wstring>{ L"MATH117-Notes-v3.pdf" } && line.setups[0].page == 121,
               "--setup Math 117: Reading with --pick and --page 121 gives one setup with that pick and page 121" );
    }
    {
        command_line line = parse_command_line( { L"--setup", L"A", L"--page", L"4", L"--pick", L"x" } );
        check( line.error.empty() && line.setups.size() == 1 && line.setups[0].picks == vector<wstring>{ L"x" } &&
                   line.setups[0].page == 4,
               "--page before --pick still gives the pick x and page 4" );
    }
    {
        command_line line = parse_command_line( { L"--setup", L"A", L"--page", L"5", L"--setup", L"B" } );
        check( line.error.empty() && line.setups.size() == 2 && line.setups[0].title == L"A" && line.setups[0].page == 5 &&
                   line.setups[1].title == L"B" && line.setups[1].page == 0,
               "a page belongs to the --setup before it: A gets page 5 and B page 0" );
    }
    {
        command_line line = parse_command_line( { L"--setup", L"A" } );
        check( line.error.empty() && line.setups.size() == 1 && line.setups[0].page == 0, "a --setup with no --page has page 0" );
    }
    {
        command_line line = parse_command_line( { L"--page", L"7" } );
        check( line.error == L"--page needs a --setup before it.", "--page 7 with no --setup gives the error --page needs a --setup before it." );
    }

    const struct {
        vector<wstring> words;
        const char *what;
    } bad_pages[] = {
        { { L"--setup", L"A", L"--page" }, "--page with no number gives an error about the --page page number" },
        { { L"--setup", L"A", L"--page", L"x" }, "--page x gives an error about the --page page number" },
        { { L"--setup", L"A", L"--page", L"0" }, "--page 0 gives an error about the --page page number" },
        { { L"--setup", L"A", L"--page", L"-3" }, "--page -3 gives an error about the --page page number" },
    };
    for ( const auto &each : bad_pages ) {
        command_line line = parse_command_line( each.words );
        check( contains_ci( line.error, L"--page" ) && contains_ci( line.error, L"page number" ), each.what );
    }
}

// ---- 3. Planning the windows ----

const setup *setup_titled( const vector<setup> &setups, const wstring &title ) {
    for ( const setup &each : setups )
        if ( each.title == title ) return &each;
    return nullptr;
}

vector<planned_window> plan_of( const setup &chosen, vector<vector<wstring>> picks, bool separate, int page ) {
    vector<wstring> problems;
    picks.resize( chosen.items.size() );
    return plan_setup( chosen, picks, separate, problems, page );
}

vector<planned_window> plan_without_page( const setup &chosen, vector<vector<wstring>> picks, bool separate ) {
    vector<wstring> problems;
    picks.resize( chosen.items.size() );
    return plan_setup( chosen, picks, separate, problems );
}

bool same_window( const planned_window &first, const planned_window &second ) {
    return first.side == second.side && first.app == second.app && first.paths == second.paths &&
           first.control_file == second.control_file && first.desktop == second.desktop && first.split == second.split &&
           first.has_frame == second.has_frame && first.page == second.page;
}

bool same_plan( const vector<planned_window> &first, const vector<planned_window> &second ) {
    if ( first.size() != second.size() ) return false;
    for ( size_t at = 0; at < first.size(); ++at )
        if ( !same_window( first[at], second[at] ) ) return false;
    return true;
}

// The pages of the windows that open in this program, in plan order
vector<int> pages_in( const vector<planned_window> &windows, program app ) {
    vector<int> pages;
    for ( const planned_window &window : windows )
        if ( window.app == app ) pages.push_back( window.page );
    return pages;
}

bool pages_all( const vector<planned_window> &windows, int page ) {
    for ( const planned_window &window : windows )
        if ( window.page != page ) return false;
    return true;
}

const wchar_t *plan_book_text =
    L"[Math 117]\n"
    L"root = %TEMP%\\crammy page checks\\Math 117\n"
    L"\n"
    L"[Math 117: Notes]\n"
    L"left = pdf \"notes.pdf\" page 121\n"
    L"right = newest \"1 Lectures\\*.pdf\"\n"
    L"\n"
    L"[Math 117: Acrobat]\n"
    L"left = pdf \"notes.pdf\" with acrobat page 3\n"
    L"right = tabs \"a.pdf\" \"b.pdf\" with acrobat page 3\n"
    L"\n"
    L"[Math 117: Others]\n"
    L"left = notes\n"
    L"left = folder \"1 Lectures\"\n"
    L"right = web \"https://a.com\"\n"
    L"\n"
    L"[Math 117: Pick]\n"
    L"full = pick \"1 Lectures\" page 6\n";

void check_planning_pages() {
    wstring notes_pdf = page_root + L"\\notes.pdf", a_pdf = page_root + L"\\a.pdf", b_pdf = page_root + L"\\b.pdf";
    wstring lecture_one = page_root + L"\\1 Lectures\\L1.pdf", lecture_two = page_root + L"\\1 Lectures\\L2.pdf";
    for ( const wstring &path : { notes_pdf, a_pdf, b_pdf, lecture_one, lecture_two } ) make_page_file( path );

    profile_book book = read_book( plan_book_text );
    check( book.errors.empty(), "the planning book reads with no errors" );
    vector<setup> setups = all_setups( book );

    const setup *notes_setup = setup_titled( setups, L"Math 117: Notes" );
    check( notes_setup != nullptr, "the planning book has the setup Math 117: Notes" );
    if ( notes_setup ) {
        vector<planned_window> windows = plan_of( *notes_setup, {}, false, 0 );
        check( windows.size() == 2, "Math 117: Notes plans two windows" );
        if ( windows.size() == 2 ) {
            check( windows[0].app == program::pdfcraft && windows[0].paths == vector<wstring>{ notes_pdf } && windows[0].page == 121,
                   "with page 0 the notes.pdf line with page 121 gives a PdfCraft window with page 121" );
            check( windows[1].app == program::pdfcraft && windows[1].page == 0,
                   "with page 0 the newest line with no page gives a PdfCraft window with page 0" );
        }
        windows = plan_of( *notes_setup, {}, false, 9 );
        check( windows.size() == 2 && pages_all( windows, 9 ), "with page 9 both PdfCraft windows of Math 117: Notes get page 9" );
        check( same_plan( plan_without_page( *notes_setup, {}, false ), plan_of( *notes_setup, {}, false, 0 ) ),
               "leaving the page out plans Math 117: Notes the same as passing 0" );
    }

    const setup *acrobat_setup = setup_titled( setups, L"Math 117: Acrobat" );
    check( acrobat_setup != nullptr, "the planning book has the setup Math 117: Acrobat" );
    if ( acrobat_setup ) {
        vector<planned_window> windows = plan_of( *acrobat_setup, {}, false, 0 );
        check( windows.size() == 3 && pages_in( windows, program::acrobat ) == vector<int>{ 3, 3, 3 },
               "with page 0 the Acrobat lines with page 3 give three Acrobat windows, each with page 3" );
        if ( windows.size() == 3 )
            check( windows[1].paths == vector<wstring>{ a_pdf } && windows[2].paths == vector<wstring>{ b_pdf },
                   "the Acrobat tabs line gives a.pdf and b.pdf a window each" );
        windows = plan_of( *acrobat_setup, {}, false, 9 );
        check( windows.size() == 3 && pages_in( windows, program::acrobat ) == vector<int>{ 9, 9, 9 },
               "with page 9 every Acrobat window gets page 9 in place of 3" );
        check( same_plan( plan_without_page( *acrobat_setup, {}, false ), plan_of( *acrobat_setup, {}, false, 0 ) ),
               "leaving the page out plans Math 117: Acrobat the same as passing 0" );
    }

    const setup *others_setup = setup_titled( setups, L"Math 117: Others" );
    check( others_setup != nullptr, "the planning book has the setup Math 117: Others" );
    if ( others_setup ) {
        vector<planned_window> plain = plan_without_page( *others_setup, {}, false );
        vector<planned_window> zero = plan_of( *others_setup, {}, false, 0 );
        vector<planned_window> nine = plan_of( *others_setup, {}, false, 9 );
        bool kinds = plain.size() == 3 && plain[0].app == program::notepad_pp && plain[1].app == program::explorer &&
                     plain[2].app == program::chrome;
        check( kinds, "Math 117: Others plans a Notepad++, an Explorer and a Chrome window" );
        check( pages_all( plain, 0 ), "notes, folder and web windows have page 0 with the page left out" );
        check( pages_all( zero, 0 ), "notes, folder and web windows have page 0 with page 0 passed" );
        check( nine.size() == 3 && pages_all( nine, 0 ), "notes, folder and web windows still have page 0 with page 9 passed" );
        check( same_plan( plain, zero ), "leaving the page out plans Math 117: Others the same as passing 0" );
    }

    const setup *pick_setup = setup_titled( setups, L"Math 117: Pick" );
    check( pick_setup != nullptr, "the planning book has the setup Math 117: Pick" );
    if ( pick_setup ) {
        vector<vector<wstring>> picks = { { lecture_one, lecture_two } };
        vector<planned_window> windows = plan_of( *pick_setup, picks, true, 0 );
        check( windows.size() == 2 && pages_in( windows, program::pdfcraft ) == vector<int>{ 6, 6 },
               "a pick line with page 6, two ticked PDFs and separate set gives two PdfCraft windows with page 6" );
        windows = plan_of( *pick_setup, picks, true, 9 );
        check( windows.size() == 2 && pages_in( windows, program::pdfcraft ) == vector<int>{ 9, 9 },
               "with page 9 both separate pick windows get page 9" );
        windows = plan_of( *pick_setup, picks, false, 0 );
        check( windows.size() == 1 && windows[0].page == 6, "the pick line not separate gives one PdfCraft window with page 6" );
        check( same_plan( plan_without_page( *pick_setup, picks, true ), plan_of( *pick_setup, picks, true, 0 ) ),
               "leaving the page out plans Math 117: Pick the same as passing 0" );
    }

    {
        setup made;
        made.title = L"Made";
        made.root = page_root;
        made.notes_folder = page_root;
        made.items.push_back( { L"left", item_kind::pdf, { notes_pdf }, viewer::pdfcraft, 0 } );
        made.items.push_back( { L"right", item_kind::pdf, { a_pdf }, viewer::acrobat, 0 } );
        vector<planned_window> windows = plan_of( made, {}, false, 9 );
        check( windows.size() == 2 && windows[0].app == program::pdfcraft && windows[0].page == 9 &&
                   windows[1].app == program::acrobat && windows[1].page == 9,
               "with page 9 a PdfCraft and an Acrobat window with no page of their own both get page 9" );
    }
}

// ---- 4. The commands ----

void check_commands() {
    wstring pdfcraft = quoted( program_file( program::pdfcraft ) );
    wstring acrobat = quoted( program_file( program::acrobat ) );

    check( pdfcraft_command( { L"a.pdf" }, L"", 121 ) == pdfcraft + L" --new-window --mode read --page 121 \"a.pdf\"",
           "pdfcraft_command with page 121 puts --page 121 after --mode read and before the PDF" );
    check( pdfcraft_command( { L"a.pdf" }, L"C:\\t\\ctl.txt", 2 ) ==
               pdfcraft + L" --new-window --mode read --control \"C:\\t\\ctl.txt\" --page 2 \"a.pdf\"",
           "pdfcraft_command with a control file and page 2 puts --control before --page 2" );

    wstring pdfcraft_plain = pdfcraft_command( { L"a.pdf" }, L"" );
    wstring pdfcraft_zero = pdfcraft_command( { L"a.pdf" }, L"", 0 );
    check( ends_with( pdfcraft_plain, L"--mode read \"a.pdf\"" ), "pdfcraft_command with the page left out ends --mode read \"a.pdf\"" );
    check( pdfcraft_zero == pdfcraft_plain, "pdfcraft_command with page 0 is the same as with the page left out" );
    check( pdfcraft_plain == pdfcraft + L" --new-window --mode read \"a.pdf\"", "pdfcraft_command with no page is what it was before" );
    check( !contains_ci( pdfcraft_zero, L"--page" ) && !contains_ci( pdfcraft_zero, L"page=" ),
           "pdfcraft_command with page 0 has neither --page nor page=" );

    wstring acrobat_plain = acrobat_command( L"a b.pdf" );
    wstring acrobat_zero = acrobat_command( L"a b.pdf", 0 );
    check( acrobat_plain == acrobat + L" /n /A \"view=FitH\" \"a b.pdf\"", "acrobat_command with the page left out is what it was before" );
    check( acrobat_zero == acrobat_plain, "acrobat_command with page 0 is the same as with the page left out" );
    check( !contains_ci( acrobat_zero, L"--page" ) && !contains_ci( acrobat_zero, L"page=" ),
           "acrobat_command with page 0 has neither --page nor page=" );
    check( acrobat_command( L"a b.pdf", 121 ) == acrobat + L" /n /A \"page=121&view=FitH\" \"a b.pdf\"",
           "acrobat_command with page 121 opens with page=121&view=FitH" );
}

}  // namespace

void test_pages() {
    page_temp = join_path( expand_variables( L"%TEMP%" ), L"crammy page checks" );
    page_root = join_path( page_temp, L"Math 117" );
    std::error_code ignored;
    fs::remove_all( fs::path( page_temp ), ignored );
    make_page_folder( page_root );

    check_reading_pages();
    check_writing_pages();
    check_command_line_pages();
    check_planning_pages();
    check_commands();

    fs::remove_all( fs::path( page_temp ), ignored );
}
