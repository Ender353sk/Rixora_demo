#ifndef MEMORYLANG_H
#define MEMORYLANG_H

#include <QString>

struct MemoryLang {
    const char *userLabel;
    const char *assistantLabel;
    const char *name;
};

QString userRoleLabel(const QString &langCode);
QString assistantRoleLabel(const QString &langCode);
QString memoryLanguageName(const QString &langCode);

#endif // MEMORYLANG_H