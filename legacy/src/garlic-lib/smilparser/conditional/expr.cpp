#include "expr.hpp"

Expr::Expr()
{
}

bool Expr::executeQuery(QString expr)
{
    if (expr.isEmpty()) // play media if expr is empty
        return true;

    return execute(MyAdapiWrapper.replaceAdapiFunctions(expr));
}

bool Expr::executeQueryWithoutAdapi(QString expr)
{
    if (expr.isEmpty()) // play media if expr is empty
        return true;

    return execute(expr);

}

bool Expr::execute(QString converted_expr)
{
    QXmlQuery query;
    query.setQuery(converted_expr);
    if (!query.isValid())
        return false;

    query.evaluateTo(&result);

    if (result != "true\n" && result != "true")
        return false;

    return true;
}
