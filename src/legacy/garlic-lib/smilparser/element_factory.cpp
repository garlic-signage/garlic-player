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
#include "element_factory.h"


#include "image.h"
#include "video.h"
#include "brush.h"
#include "web.h"
#include "widget.h"
#include "prefetch.h"
#include "unknown.h"
#include "seq.h"
#include "par.h"
#include "excl.h"
#include "ref_command.h"


using namespace MediaParser;


ElementFactory::ElementFactory(Files::MediaManager *mm, MainConfiguration *config, SmilHead::PlaceHolder *ph, Expr *expr, QObject *parent) : QObject(parent)
{
    MyMediaManager      = mm;
    MyMainConfiguration = config;
    MyPlaceHolder       = ph;
    MyExpr              = expr;
}

BaseTimings* ElementFactory::createBase(QDomElement dom_element, QObject *parent)
{
    QString tag_name = dom_element.tagName();
    QString type     = "";
    if(tag_name == "img" || tag_name == "video" || tag_name == "audio" || tag_name == "brush" || tag_name == "text" || tag_name == "prefetch" ||
            tag_name == "seq" || tag_name == "par" || tag_name == "body" || tag_name == "excl" )
    {
        type = tag_name;
    }
    else if(tag_name == "ref" && dom_element.hasAttribute("type"))
    {
        if(dom_element.attribute("type").contains("image"))
           type = "img";
        else if (dom_element.attribute("type").contains("video"))
           type = "video";
        else if (dom_element.attribute("type").contains("audio"))
           type = "audio";
        else if (dom_element.attribute("type").contains("text"))
           type = "text";
        else if (dom_element.attribute("type").contains("application/widget"))
           type = "widget";
    }
    else if(tag_name == "ref" && dom_element.hasAttribute("src") && dom_element.attribute("src").contains("adapi:"))
    {
        type = "ref_command";
    }

    if (type == "img")
    {
        return new TImage(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "brush")
    {
        return new TBrush(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "video")
    {
        return new TVideo(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "audio")
    {
        return new TAudio(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "text")
    {
        return new TWeb(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "widget")
    {
        return new TWidget(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "prefetch")
    {
        return new MediaParser::TPrefetch(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "ref_command")
    {
        return new TRefCommand(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
    }
    else if (type == "seq")
    {
        return new TSeq(MyExpr, parent);
    }
    else if (type == "par")
    {
        return new TPar(MyExpr, parent);
    }
    else if (type == "excl")
    {
        return new TExcl(MyExpr, parent);
    }

    return new TUnknown(MyMediaManager, MyMainConfiguration, MyPlaceHolder, MyExpr, parent);
}
