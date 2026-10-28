/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GARLIC_CORE_TESTS_TIME_ZONE_HPP
#define GARLIC_CORE_TESTS_TIME_ZONE_HPP

#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>

namespace garlic::core::tests
{
    /// Sets the local time zone (POSIX TZ string, e.g. "UTC", "CET-1") for a test and restores it afterwards.
    class TimeZone
    {
        public:
            explicit TimeZone(const char *tz)
            {
                if (const char *old = std::getenv("TZ"))
                    previous = old;
                set(tz);
            }

            ~TimeZone() { previous ? set(previous->c_str()) : unset(); }

            TimeZone(const TimeZone &)            = delete;
            TimeZone &operator=(const TimeZone &) = delete;

        private:
            static void set(const char *tz)
            {
#ifdef _WIN32
                _putenv_s("TZ", tz);
                _tzset();
#else
                setenv("TZ", tz, 1);
                tzset();
#endif
            }

            static void unset()
            {
#ifdef _WIN32
                _putenv_s("TZ", "");
                _tzset();
#else
                unsetenv("TZ");
                tzset();
#endif
            }

            std::optional<std::string> previous;
    };
}

#endif // GARLIC_CORE_TESTS_TIME_ZONE_HPP
