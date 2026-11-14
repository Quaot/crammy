// Checks for choosing the file that replaces an open slide deck. Uses only data built here.

#include <string>
#include <vector>

#include "check.h"
#include "../src/slide_watch.h"

namespace {

const wstring folder = L"C:\M\1 Lectures\PDF\\";

watched_slide math_watch() {
    return { folder + L"MATH117-L*.pdf", folder + L"MATH117-L23.pdf", 100 };
}

seen_file seen( const wstring &path, unsigned long long modified, unsigned long long settled_ms = 5000 ) {
    return { path, modified, 1000, settled_ms };
}

}  // namespace

void test_slide_watch() {
    watched_slide watch = math_watch();
    wstring l24 = folder + L"MATH117-L24.pdf";
    wstring l25 = folder + L"MATH117-L25.pdf";

    // One newer lecture that has stopped growing
    check( replacement_for( watch, { seen( l24, 200 ) } ) == l24, "a newer settled lecture replaces the open one" );

    // The latest modified time wins, whatever the names are
    check( replacement_for( watch, { seen( l24, 200 ), seen( l25, 300 ) } ) == l25,
           "the later of two newer files wins" );
    check( replacement_for( watch, { seen( l24, 300 ), seen( l25, 200 ) } ) == l24,
           "modified time decides, not the name" );

    // Files that must be ignored
    check( replacement_for( watch, { seen( folder + L"MATH115-W10-Shell.pdf", 200 ) } ).empty(),
           "a file that does not match the pattern is ignored" );
    check( replacement_for( watch, { seen( L"C:\M\Other\MATH117-L24.pdf", 200 ) } ).empty(),
           "a match in another folder is ignored" );
    check( replacement_for( watch, { seen( L"c:\m\1 lectures\pdf\math117-l23.pdf", 200 ) } ).empty(),
           "the open file itself is ignored, whatever its case" );
    check( replacement_for( watch, { seen( folder + L"MATH117-L22.pdf", 50 ) } ).empty(),
           "a file older than the open one is ignored" );

    // A download still being written is passed over
    check( replacement_for( watch, { seen( l24, 200, 1500 ) } ).empty(), "a file that changed 1500 ms ago is passed over" );
    check( replacement_for( watch, { seen( l24, 200, 2500 ) } ) == l24, "a file that changed 2500 ms ago is taken" );
    check( replacement_for( watch, { seen( l24, 200, 2000 ) } ) == l24, "a file that changed exactly 2000 ms ago is taken" );

    // An unsettled newer file does not block a settled one
    check( replacement_for( watch, { seen( l24, 200 ), seen( l25, 300, 500 ) } ) == l24,
           "a settled file wins when the newest one is still being written" );

    // Nothing to look at
    check( replacement_for( watch, {} ).empty(), "an empty list gives no replacement" );

    // ECE 190 slides and summaries sit side by side
    wstring ece = L"C:\E\1 Lectures\PDF\\";
    vector<seen_file> ece_files = { seen( ece + L"ECE190-W11-Slides.pdf", 200 ), seen( ece + L"ECE190-W11-Sum.pdf", 200 ) };
    watched_slide slides_watch = { ece + L"ECE190-W*-Slides.pdf", ece + L"ECE190-W10-Slides.pdf", 100 };
    watched_slide sum_watch = { ece + L"ECE190-W*-Sum.pdf", ece + L"ECE190-W10-Sum.pdf", 100 };
    check( replacement_for( slides_watch, ece_files ) == ece + L"ECE190-W11-Slides.pdf", "the slides watch picks the W11 slides" );
    check( replacement_for( sum_watch, ece_files ) == ece + L"ECE190-W11-Sum.pdf", "the summary watch picks the W11 summary" );

    // matches_pattern
    wstring lecture_pattern = L"C:\A\MATH117-L*.pdf";
    check( matches_pattern( L"C:\A\MATH117-L24.pdf", lecture_pattern ), "a lecture file matches its pattern" );
    check( matches_pattern( L"c:\a\math117-l24.PDF", lecture_pattern ), "matching ignores case" );
    check( !matches_pattern( L"C:\A\MATH117-L24.pdf", L"C:\B\MATH117-L*.pdf" ), "a different folder does not match" );
    check( !matches_pattern( L"C:\A\ECE190-W11-Sum.pdf", L"C:\A\ECE190-W*-Slides.pdf" ),
           "a summary does not match the slides pattern" );
    check( matches_pattern( L"C:\A\X1.pdf", L"C:\A\X?.pdf" ), "? matches one character" );
    check( !matches_pattern( L"C:\A\X12.pdf", L"C:\A\X?.pdf" ), "? does not match two characters" );
    check( matches_pattern( L"C:\A\L.pdf", L"C:\A\L*.pdf" ), "* may match nothing" );
}
