#include "countryflags.h"

#include <QHash>
#include <QLocale>
#include <QMetaEnum>

namespace CountryFlags {

namespace {

QString normalize(const QString &name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            out.append(c.toLower());
        }
    }
    return out;
}

QString emojiFromCode(const QString &iso2)
{
    if (iso2.size() != 2) {
        return {};
    }
    QString flag;
    for (const QChar c : iso2) {
        if (!c.isLetter()) {
            return {};
        }
        // Regional indicator symbols live at U+1F1E6 ('A') onwards.
        const char32_t codepoint = 0x1F1E6 + (c.toUpper().unicode() - u'A');
        flag.append(QString::fromUcs4(&codepoint, 1));
    }
    return flag;
}

/*! Names where NordVPN's spelling does not match CLDR's display name. */
const QHash<QString, QString> &overrides()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("czechrepublic"), QStringLiteral("cz")},
        {QStringLiteral("unitedkingdom"), QStringLiteral("gb")},
        {QStringLiteral("southkorea"), QStringLiteral("kr")},
        {QStringLiteral("northmacedonia"), QStringLiteral("mk")},
        {QStringLiteral("macedonia"), QStringLiteral("mk")},
        {QStringLiteral("moldova"), QStringLiteral("md")},
        {QStringLiteral("vietnam"), QStringLiteral("vn")},
        {QStringLiteral("laos"), QStringLiteral("la")},
        {QStringLiteral("brunei"), QStringLiteral("bn")},
        {QStringLiteral("bruneidarussalam"), QStringLiteral("bn")},
        {QStringLiteral("isleofman"), QStringLiteral("im")},
        {QStringLiteral("bosniaandherzegovina"), QStringLiteral("ba")},
        {QStringLiteral("trinidadandtobago"), QStringLiteral("tt")},
        {QStringLiteral("turkey"), QStringLiteral("tr")},
        {QStringLiteral("turkiye"), QStringLiteral("tr")},
        {QStringLiteral("myanmar"), QStringLiteral("mm")},
        {QStringLiteral("ivorycoast"), QStringLiteral("ci")},
        {QStringLiteral("capeverde"), QStringLiteral("cv")},
        {QStringLiteral("swaziland"), QStringLiteral("sz")},
        {QStringLiteral("eswatini"), QStringLiteral("sz")},
        {QStringLiteral("easttimor"), QStringLiteral("tl")},
        {QStringLiteral("timorleste"), QStringLiteral("tl")},
        {QStringLiteral("russia"), QStringLiteral("ru")},
        {QStringLiteral("bolivia"), QStringLiteral("bo")},
        {QStringLiteral("venezuela"), QStringLiteral("ve")},
        {QStringLiteral("tanzania"), QStringLiteral("tz")},
        {QStringLiteral("syria"), QStringLiteral("sy")},
        {QStringLiteral("iran"), QStringLiteral("ir")},
        {QStringLiteral("unitedstates"), QStringLiteral("us")},
        {QStringLiteral("unitedarabemirates"), QStringLiteral("ae")},
        {QStringLiteral("hongkong"), QStringLiteral("hk")},
        {QStringLiteral("macau"), QStringLiteral("mo")},
        {QStringLiteral("palestine"), QStringLiteral("ps")},
        {QStringLiteral("democraticrepublicofthecongo"), QStringLiteral("cd")},
        {QStringLiteral("republicofthecongo"), QStringLiteral("cg")},
        {QStringLiteral("congo"), QStringLiteral("cg")},
    };
    return map;
}

/*! Normalized CLDR territory name -> ISO 3166-1 alpha-2, built once. */
const QHash<QString, QString> &territoryIndex()
{
    static const QHash<QString, QString> index = [] {
        QHash<QString, QString> map;
        const QMetaEnum meta = QMetaEnum::fromType<QLocale::Territory>();
        for (int i = 0; i < meta.keyCount(); ++i) {
            const auto territory = static_cast<QLocale::Territory>(meta.value(i));
            const QString code = QLocale::territoryToCode(territory);
            if (code.size() != 2) {
                continue; // world regions and the like
            }
            const QString name = normalize(QLocale::territoryToString(territory));
            if (!name.isEmpty()) {
                map.insert(name, code.toLower());
            }
        }
        return map;
    }();
    return index;
}

} // namespace

QString prettify(const QString &cliName)
{
    QString pretty = cliName;
    pretty.replace(u'_', u' ');
    return pretty;
}

QString flagFor(const QString &countryName)
{
    const QString key = normalize(countryName);
    if (key.isEmpty()) {
        return {};
    }

    if (const auto it = overrides().constFind(key); it != overrides().cend()) {
        return emojiFromCode(*it);
    }
    if (const auto it = territoryIndex().constFind(key); it != territoryIndex().cend()) {
        return emojiFromCode(*it);
    }
    return {};
}

} // namespace CountryFlags
