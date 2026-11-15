#pragma once

#include "text.h"

struct setup_request {
    wstring title;              // the text after --setup
    vector<wstring> picks;      // the texts after --pick that follow this --setup
    bool separate = false;      // --separate after this --setup
};

struct command_line {
    wstring action;             // "", "list", "check", "edit", "background", "panel"
    wstring list_title;         // --list "title"
    vector<setup_request> setups;
    bool show = false;          // --show
    wstring error;              // what is wrong with the arguments, or ""
};

// arguments are the words after the program name
command_line parse_command_line( const vector<wstring> &arguments );
