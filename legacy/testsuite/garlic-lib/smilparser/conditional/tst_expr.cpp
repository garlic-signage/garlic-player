/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#include "tst_expr.hpp"
#include "smilparser/conditional/expr.hpp"

void TestExpr::testEasy()
{
    Expr *MyExpr = new Expr;

    QString expr = "compare('2021-01-26T00:00:00', '2021-01-25T00:00:00')>0";
    QVERIFY(MyExpr->executeQueryWithoutAdapi(expr));

    expr = "adapi-compare('2021-01-26T00:00:00', '2021-01-25T00:00:00')>0";
    QVERIFY(MyExpr->executeQuery(expr));

    expr = "adapi-compare('2021-01-26T00:00:00', '2021-01-25T00:00:00')=0";
    QVERIFY(!MyExpr->executeQuery(expr));

    expr = "";
    QVERIFY(MyExpr->executeQuery(expr));
    QVERIFY(MyExpr->executeQueryWithoutAdapi(expr));

}

void TestExpr::testCombination()
{
    Expr *MyExpr = new Expr;

    QString expr = "compare('2021-01-26T00:00:00', '2021-01-25T00:00:00')>0 and compare('2021-01-26T00:00:00', '2021-01-29T00:00:00')<=0";
    QVERIFY(MyExpr->executeQueryWithoutAdapi(expr));

    expr = "adapi-compare('2021-01-26T00:00:00', '2021-01-25T00:00:00')>0 and adapi-compare('2021-01-26T00:00:00', '2021-01-29T00:00:00')<=0";
    QVERIFY(MyExpr->executeQuery(expr));

}


void TestExpr::testComplex()
{
    Expr *MyExpr = new Expr;
    // Play between 10th and 25th of May 2022 every day between 8:00 and 12:00 o clock and additional on Mondays, Wednesdays, and Fridays between 15:00 and 20:00 o'clock.
    // when 2022-05-14 at 11:00:00
    QString expr = "compare(substring-before('2022-05-14T11:00:00', 'T'), '2022-05-10')>=0 and \
                   compare(substring-before('2022-05-14T11:00:00', 'T'), '2022-05-25')<=0 and \
                   compare(substring-after('2022-05-14T11:00:00', 'T'), '08:00:00')>=0 and \
                   compare(substring-after('2022-05-14T11:00:00', 'T'), '12:00:00')<=0 or \
                   ((1=5 and compare(substring-after('2022-05-14T11:00:00', 'T'), '15:00:00')>=0 and  \
                         compare(substring-after('2022-05-14T11:00:00', 'T'), '20:00:00')<=0) or (3=5 and  \
                            compare(substring-after('2022-05-14T11:00:00', 'T'), '15:00:00')>=0 and  \
                            compare(substring-after('2022-05-14T11:00:00', 'T'), '20:00:00')<=0) or (5=5 and  \
                            compare(substring-after('2022-05-14T11:00:00', 'T'), '15:00:00')>=0 and  \
                            compare(substring-after('2022-05-14T11:00:00', 'T'), '20:00:00')<=0))";

    QVERIFY(MyExpr->executeQueryWithoutAdapi(expr));

    // Play between 10th and 25th of May 2022 every day between 8:00 and 12:00 o clock and additional on Mondays, Wednesdays, and Fridays between 15:00 and 20:00 o'clock.
    // when 2022-05-16 at 16:00:00
    expr = "compare(substring-before('2022-05-16T16:00:00', 'T'), '2022-05-10')>=0 and \
                           compare(substring-before('2022-05-16T16:00:00', 'T'), '2022-05-25')<=0 and \
                               compare(substring-after('2022-05-16T16:00:00', 'T'), '08:00:00')>=0 and \
                               compare(substring-after('2022-05-16T16:00:00', 'T'), '12:00:00')<=0 or \
                   ((1=5 and compare(substring-after('2022-05-16T16:00:00', 'T'), '15:00:00')>=0 and  \
                                       compare(substring-after('2022-05-16T16:00:00', 'T'), '15:00:00')>=0 and  \
                                     compare(substring-after('2022-05-16T16:00:00', 'T'), '20:00:00')<=0) or (5=5 and  \
                                       compare(substring-after('2022-05-16T16:00:00', 'T'), '15:00:00')>=0 and  \
                                     compare(substring-after('2022-05-16T16:00:00', 'T'), '20:00:00')<=0))";

    QVERIFY(MyExpr->executeQueryWithoutAdapi(expr));

    // should be false because of the first part every day is not in time
    expr = "compare(substring-before('2022-05-16T16:00:00', 'T'), '2022-05-10')>=0 and \
                   compare(substring-before('2022-05-16T16:00:00', 'T'), '2022-05-25')<=0 and \
                       compare(substring-after('2022-05-16T16:00:00', 'T'), '08:00:00')>=0 and \
                       compare(substring-after('2022-05-16T16:00:00', 'T'), '12:00:00')<=0";

    QVERIFY(!MyExpr->executeQueryWithoutAdapi(expr));

}


static TestExpr TEST_EXPR;


