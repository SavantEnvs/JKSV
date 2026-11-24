// keyboard::Dictionary::add_word_to_list — UTF-8 → UTF-16 conversion into the fixed 0x19-unit
// software-keyboard dictionary words (successor of swkbdDictWordCreate, fixed upstream in 6442209).
#include "keyboard/Dictionary.hpp"

#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string word(reinterpret_cast<const char *>(data), size);
    keyboard::Dictionary dictionary{};
    dictionary.add_word_to_list(word.c_str());
    volatile char16_t sink = 0;
    const keyboard::Dictionary::Word *words = dictionary.get_words();
    for (size_t i = 0; i < dictionary.get_count(); i++) { sink = sink + words[i].word[0] + words[i].predict[0]; }
    return 0;
}
