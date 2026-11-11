#include "memorylang.h"

namespace {
const MemoryLang &memoryLangImpl(const QString &code) {
    static const MemoryLang ru = {"Пользователь", "Риксора", "русский"};
    static const MemoryLang en = {"User", "Rixora", "English"};
    static const MemoryLang es = {"Usuario", "Rixora", "espanol"};
    static const MemoryLang ko = {"사용자", "릭소라", "한국어"};
    static const MemoryLang de = {"Benutzer", "Rixora", "Deutsch"};
    static const MemoryLang uk = {"Користувач", "Ріксора", "українська"};
    static const MemoryLang fr = {"Utilisateur", "Rixora", "francais"};
    static const MemoryLang ja = {"ユーザー", "リクソラ", "日本語"};

    if (code == "en") return en;
    if (code == "es") return es;
    if (code == "ko") return ko;
    if (code == "de") return de;
    if (code == "uk") return uk;
    if (code == "fr") return fr;
    if (code == "ja") return ja;
    return ru;
}
}

QString userRoleLabel(const QString &langCode) {
    return QString::fromUtf8(memoryLangImpl(langCode).userLabel);
}

QString assistantRoleLabel(const QString &langCode) {
    return QString::fromUtf8(memoryLangImpl(langCode).assistantLabel);
}

QString memoryLanguageName(const QString &langCode) {
    return QString::fromUtf8(memoryLangImpl(langCode).name);
}