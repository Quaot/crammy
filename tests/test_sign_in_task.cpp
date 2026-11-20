// Written from tests\specs\sign_in_task.md.

#include <windows.h>
#include <objbase.h>

#include "check.h"
#include "../src/background.h"
#include "../src/text.h"

namespace {

wstring test_program_folder() {
    wchar_t path[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 4 );
    return parent_folder( wstring( path, length ) );
}

bool has( const wstring &text, const wstring &part ) {
    return text.find( part ) != wstring::npos;
}

int times_in( const wstring &text, const wstring &part ) {
    int count = 0;
    for ( size_t at = text.find( part ); at != wstring::npos; at = text.find( part, at + part.size() ) ) count++;
    return count;
}

void check_task_xml() {
    wstring xml = sign_in_task_xml( L"C:\\Tools\\crammy.exe", L"PC\\Ann" );
    wstring heading = L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>";
    check( xml.compare( 0, heading.size(), heading ) == 0, "the task's XML starts with the UTF-16 xml declaration" );
    check( has( xml, L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">" ),
           "the XML opens a version 1.2 Task in the Task Scheduler namespace" );
    check( has( xml, L"</Task>" ), "the XML closes the Task" );

    size_t trigger_start = xml.find( L"<LogonTrigger>" );
    size_t trigger_end = xml.find( L"</LogonTrigger>" );
    check( trigger_start != wstring::npos && trigger_end != wstring::npos && trigger_start < trigger_end,
           "the XML has a LogonTrigger" );
    wstring trigger = trigger_start != wstring::npos && trigger_end != wstring::npos && trigger_start < trigger_end
                          ? xml.substr( trigger_start, trigger_end - trigger_start )
                          : L"";
    check( has( trigger, L"<Enabled>true</Enabled>" ), "the LogonTrigger is enabled" );
    check( has( trigger, L"<UserId>PC\\Ann</UserId>" ), "the LogonTrigger is for the user PC\\Ann" );
    check( !has( xml, L"<Delay>" ), "the XML has no Delay anywhere" );

    check( has( xml, L"<LogonType>InteractiveToken</LogonType>" ), "the task runs with an interactive token" );
    check( has( xml, L"<RunLevel>LeastPrivilege</RunLevel>" ), "the task runs with least privilege" );
    check( times_in( xml, L"<UserId>PC\\Ann</UserId>" ) == 2, "the user's UserId appears twice, in the trigger and the principal" );

    check( has( xml, L"<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>" ),
           "a second start while Crammy runs is ignored" );
    check( has( xml, L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>" ),
           "the task may start on battery" );
    check( has( xml, L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>" ),
           "the task does not stop when the laptop goes on battery" );
    check( has( xml, L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>" ), "the task has no time limit" );
    check( has( xml, L"<Priority>5</Priority>" ), "the task runs at priority 5" );

    check( has( xml, L"<Command>C:\\Tools\\crammy.exe</Command>" ), "the command is the exe given" );
    check( has( xml, L"<Arguments>--background</Arguments>" ), "the argument is --background" );
    check( has( xml, L"<WorkingDirectory>C:\\Tools</WorkingDirectory>" ), "the working folder is the exe's folder" );
    check( times_in( xml, L"<Enabled>true</Enabled>" ) >= 2, "Enabled true appears for the trigger and the settings" );

    wstring odd = sign_in_task_xml( L"C:\\A&B <x>\\crammy.exe", L"PC\\Ann" );
    check( has( odd, L"<Command>C:\\A&amp;B &lt;x&gt;\\crammy.exe</Command>" ),
           "an ampersand and angle brackets in the command are escaped" );
    check( has( odd, L"<WorkingDirectory>C:\\A&amp;B &lt;x&gt;</WorkingDirectory>" ),
           "an ampersand and angle brackets in the working folder are escaped" );
    check( !has( odd, L"A&B" ), "the bare ampersand from the path appears nowhere" );
}

void check_name_and_account() {
    SetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", nullptr );
    check( sign_in_task_name() == L"Crammy", "with CRAMMY_SIGN_IN_TASK unset the task is named Crammy" );
    SetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", L"Crammy check" );
    check( sign_in_task_name() == L"Crammy check", "with CRAMMY_SIGN_IN_TASK set the task takes that name" );
    SetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", nullptr );

    wstring account = current_account();
    check( !account.empty(), "the current account is not empty" );
    size_t slash = account.find( L'\\' );
    check( times_in( account, L"\\" ) == 1 && slash != 0 && slash != account.size() - 1,
           "the current account has one backslash, neither first nor last" );
    wchar_t name[256 + 1];
    DWORD length = 256 + 1;
    wstring user = GetUserNameW( name, &length ) ? wstring( name ) : L"";
    wstring after = slash == wstring::npos ? L"" : account.substr( slash + 1 );
    check( !user.empty() && lower( after ) == lower( user ), "the part after the backslash is the signed-in user's name" );
}

void check_on_and_off() {
    wstring folder = join_path( expand_variables( L"%TEMP%" ), L"crammy sign-in check" );
    CreateDirectoryW( folder.c_str(), nullptr );
    wstring shortcut = join_path( folder, L"Crammy.lnk" );
    SetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", L"Crammy check" );
    SetEnvironmentVariableW( L"CRAMMY_STARTUP_FOLDER", folder.c_str() );

    check( set_start_at_sign_in( false ), "turning start at sign-in off first returns true" );
    check( !sign_in_task_exists(), "after turning it off there is no check task" );
    check( registered_sign_in_task_xml().empty(), "with no task the registered XML is empty" );
    check( !start_at_sign_in_is_on(), "with no task and no shortcut start at sign-in is off" );

    write_text_file( shortcut, L"old shortcut\r\n" );
    check( start_at_sign_in_is_on(), "an old Crammy.lnk in the Startup folder counts as on" );

    check( set_start_at_sign_in( true ), "turning start at sign-in on returns true" );
    check( sign_in_task_exists(), "turning it on registers the check task" );
    check( start_at_sign_in_is_on(), "with the task registered start at sign-in is on" );
    check( !is_file( shortcut ), "turning it on removes the old Crammy.lnk" );

    wstring xml = registered_sign_in_task_xml();
    check( !xml.empty(), "the registered task's XML is not empty" );
    check( contains_ci( xml, join_path( test_program_folder(), L"crammy.exe" ) ),
           "the registered task runs crammy.exe from the test program's folder" );
    check( has( xml, L"--background" ), "the registered task passes --background" );
    check( has( xml, L"LogonTrigger" ), "the registered task has a sign-in trigger" );
    check( has( xml, L"IgnoreNew" ), "the registered task ignores a second start" );
    check( contains_ci( xml, current_account() ), "the registered task names the current account" );

    check( set_start_at_sign_in( true ), "turning it on a second time returns true" );
    check( sign_in_task_exists(), "the task still exists after turning it on twice" );

    check( set_start_at_sign_in( false ), "turning start at sign-in off returns true" );
    check( !sign_in_task_exists(), "turning it off removes the task" );
    check( !start_at_sign_in_is_on(), "after turning it off start at sign-in is off" );
    check( registered_sign_in_task_xml().empty(), "after turning it off the registered XML is empty" );
    check( set_start_at_sign_in( false ), "turning it off again with no task also returns true" );

    SetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", nullptr );
    SetEnvironmentVariableW( L"CRAMMY_STARTUP_FOLDER", nullptr );
    DeleteFileW( shortcut.c_str() );
    RemoveDirectoryW( folder.c_str() );
    check( sign_in_task_name() == L"Crammy", "once the variable is unset the task is named Crammy again" );
}

}  // namespace

void test_sign_in_task() {
    HRESULT started = CoInitialize( nullptr );
    check_task_xml();
    check_name_and_account();
    check_on_and_off();
    if ( SUCCEEDED( started ) ) CoUninitialize();
}
