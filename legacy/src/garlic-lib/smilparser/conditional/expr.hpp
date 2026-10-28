#ifndef EXPR_H
#define EXPR_H
#include <QString>
#include <QtGlobal>
#include <QXmlQuery>


#include <QDebug>
#include "adapi_wrapper.hpp"

class Expr
{
    public:
        Expr();
        bool executeQuery(QString expr);
        bool executeQueryWithoutAdapi(QString expr);
    protected:
    private:
        AdapiWrapper MyAdapiWrapper;
        QString result = "";

        bool execute(QString converted_expr);
};

#endif // EXPR_H
