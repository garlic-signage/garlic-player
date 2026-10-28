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
#include "smil/expr.hpp"

#include "common/time_format.hpp"

#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <format>
#include <memory>

namespace garlic::core::smil
{
    namespace
    {
        using namespace std::chrono;

        struct ContextDeleter
        {
            void operator()(xmlXPathContext *context) const { xmlXPathFreeContext(context); }
        };

        struct ObjectDeleter
        {
            void operator()(xmlXPathObject *object) const { xmlXPathFreeObject(object); }
        };

        const ExprContext &exprContext(xmlXPathParserContext *ctxt)
        {
            return *static_cast<const ExprContext *>(ctxt->context->userData);
        }

        void returnString(xmlXPathParserContext *ctxt, const std::string &value)
        {
            xmlXPathReturnString(ctxt, xmlStrdup(reinterpret_cast<const xmlChar *>(value.c_str())));
        }

        std::string isoDate(local_seconds time)
        {
            return std::format("{:%Y-%m-%dT%H:%M:%S}", time);
        }

        /// adapi-date(): local time "2026-10-28T08:00:00"
        void adapiDate(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            returnString(ctxt, isoDate(common::toLocalTime(exprContext(ctxt).now)));
        }

        /// adapi-gmdate(): UTC "2026-10-28T07:00:00Z"
        void adapiGmDate(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            auto utc = floor<seconds>(exprContext(ctxt).now);
            returnString(ctxt, isoDate(local_seconds{utc.time_since_epoch()}) + "Z");
        }

        /// adapi-weekday(): local weekday, 0 = Sunday … 6 = Saturday
        void adapiWeekday(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            weekday day{floor<days>(common::toLocalTime(exprContext(ctxt).now))};
            xmlXPathReturnNumber(ctxt, day.c_encoding());
        }

        /// adapi-gmweekday(): weekday in UTC; garlic-lib replaced adapi-weekday() here by mistake
        void adapiGmWeekday(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            weekday day{floor<days>(exprContext(ctxt).now)};
            xmlXPathReturnNumber(ctxt, day.c_encoding());
        }

        /// compare(a, b), adapi-compare(a, b): -1, 0, 1 like XPath 2.0 fn:compare (UTF-8 bytes = code points)
        void compare(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(2);
            xmlChar *second = xmlXPathPopString(ctxt);
            xmlChar *first  = xmlXPathPopString(ctxt);
            int      result = xmlStrcmp(first, second);
            xmlFree(first);
            xmlFree(second);
            xmlXPathReturnNumber(ctxt, result < 0 ? -1 : (result > 0 ? 1 : 0));
        }

        /// smil-playerId(); garlic-lib replaced it by an empty string, so expressions with it were always false
        void playerId(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            returnString(ctxt, exprContext(ctxt).player_id);
        }

        void playerName(xmlXPathParserContext *ctxt, int nargs)
        {
            CHECK_ARITY(0);
            returnString(ctxt, exprContext(ctxt).player_name);
        }

        void ignoreError(void *, const xmlError *)
        {
        }
    }

    bool evaluateExpr(std::string_view expr, const ExprContext &context)
    {
        if (expr.find_first_not_of(" \t\r\n") == std::string_view::npos)
            return true; // play the element if there is no condition

        std::unique_ptr<xmlXPathContext, ContextDeleter> xpath(xmlXPathNewContext(nullptr));
        if (!xpath)
            return false;
        xmlXPathSetErrorHandler(xpath.get(), ignoreError, nullptr); // errors are just false, not printed
        xpath->userData = const_cast<ExprContext *>(&context);

        struct Function
        {
            const char        *name;
            xmlXPathFunction   function;
        };
        static const Function functions[] = {
            {"adapi-date", adapiDate},         {"adapi-gmdate", adapiGmDate}, {"adapi-weekday", adapiWeekday},
            {"adapi-gmweekday", adapiGmWeekday}, {"adapi-compare", compare},  {"compare", compare},
            {"smil-playerId", playerId},       {"smil-playerName", playerName},
        };
        for (const Function &f : functions)
            xmlXPathRegisterFunc(xpath.get(), reinterpret_cast<const xmlChar *>(f.name), f.function);

        std::string text(expr); // libxml2 needs a terminated string
        std::unique_ptr<xmlXPathObject, ObjectDeleter> result(
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar *>(text.c_str()), xpath.get()));
        return result && result->type == XPATH_BOOLEAN && result->boolval != 0;
    }
}
