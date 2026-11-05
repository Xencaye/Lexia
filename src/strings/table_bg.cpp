// Bulgarian (Interface language "bg"). A text left out falls back to English. The wording of
// every text is for the owner to check (Bulgarian is untested by a native reader).
#include "strings/string_ids.h"
#include "strings/strings.h"

namespace strings {

const Table& BulgarianTable() {
    static const Table table = {
        {id::kKeyNone, "(без клавиш)"},

        {id::kAnnounceNoOtherLanguage, "Няма инсталиран друг език."},
        {id::kAnnouncePickNoText, "Не е намерен текст."},
        {id::kAnnounceCaptureFailed, "Снимането на екрана не успя."},
        {id::kAnnounceVoiceMissingFor, "Липсва гласът за {language}."},

        {id::kControlBarPause, "Пауза"},
        {id::kControlBarPauseTip, "Пауза ({key})"},
        {id::kControlBarResume, "Продължи"},
        {id::kControlBarResumeTip, "Продължи ({key})"},
        {id::kControlBarSkip, "Пропусни"},
        {id::kControlBarSkipTip, "Пропусни ({key})"},
        {id::kControlBarStop, "Спри"},
        {id::kControlBarStopTip, "Спри ({key})"},

        {id::kTrayOn, "Включен"},
        {id::kTrayOff, "Изключен"},
        {id::kTrayOpen, "Отвори Lexia"},
        {id::kTrayQuit, "Изход"},

        {id::kLanguageEng, "английски"},
        {id::kLanguageBul, "български"},
    };
    return table;
}

}  // namespace strings
