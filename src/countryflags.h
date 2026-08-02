#pragma once

#include <QString>

namespace CountryFlags {

/*!
 * Maps a NordVPN country name ("Bosnia_And_Herzegovina") to a flag emoji.
 *
 * The bulk of the mapping comes from QLocale's territory table rather than a
 * hand-maintained list; only names where NordVPN and CLDR disagree are spelled
 * out explicitly. Returns an empty string when there is no confident match, so
 * callers should treat the flag as optional decoration.
 */
QString flagFor(const QString &countryName);

/*! "Bosnia_And_Herzegovina" -> "Bosnia And Herzegovina" */
QString prettify(const QString &cliName);

} // namespace CountryFlags
