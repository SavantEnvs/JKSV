// Host stand-in for strings::get_by_name (source/strings/strings.cpp loads the localized UI text
// from romfs:/Text/*.json.z): GoogleDrive::get_sign_in_data only formats its English template.
#include "strings/strings.hpp"

const char *strings::get_by_name(std::string_view name, int) noexcept
{
    if (name == strings::names::GOOGLE_DRIVE) { return "Visit %s and enter the code %s."; }
    return "";
}
