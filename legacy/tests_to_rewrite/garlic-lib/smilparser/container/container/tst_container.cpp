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
#include <QString>
#include <QtTest>

#include "container.h"

class InhertitedTContainer : public TContainer
{
    Q_OBJECT
public:
    InhertitedTContainer(TBase * parent = 0){Q_UNUSED(parent);setObjectName("testBasePlaylist");}
    QString       getType(){return "test base playlist";}

    void          preloadParse(QDomElement element){Q_UNUSED(element);}                     // virtual
    void          childEnded(TBase *element){ Q_UNUSED(element);}                           // virtual
    void          play(){}                                                                  // virtual
    void          pause(){}                                                                 // virtual
    void          stop(){}                                                                  // virtual
    void          resume(){}                                                                // virtual
    BaseTimings  *getChildElementFromList(){ return this;}                                  // virtual
    bool          isChildPlayable(BaseTimings *element){Q_UNUSED(element); return true;}    // virtual
    void          next(BaseTimings *element){Q_UNUSED(element);}                            // virtual

    // for testing
    void      setActiveElement(QDomElement element){active_element = element;}
public slots:
    void      emitfinished(){}                                                              // virtual
protected:
    void      prepareDurationTimerBeforePlay(){}                                                // virtual
    void      traverseChilds(){}
};


class TestTContainer : public QObject
{
    Q_OBJECT

public:
    TestTContainer(){}

private Q_SLOTS:
//    void test_reactByTag();
//    void test_reactByTagForRef();
};

/**

void TestTContainer::test_reactByTag()
{
    InhertitedTContainer MyPlaylist;
    QDomDocument document;
    QDomElement  element = document.createElement(QString("img"));

    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("img"));

    element.setTagName("video");
    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("video"));

    element.setTagName("audio");
    MyPlaylist.setActiveElement(element);
    MyPlaylist.test_reactByTag();
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("audio"));

    element.setTagName("text");
    MyPlaylist.setActiveElement(element);
    MyPlaylist.test_reactByTag();
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("text"));

    element.setTagName("seq");
    MyPlaylist.setActiveElement(element);
    MyPlaylist.test_reactByTag();
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("seq"));

    element.setTagName("par");
    MyPlaylist.setActiveElement(element);
    MyPlaylist.test_reactByTag();
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("par"));

    element.setTagName("excl");
    MyPlaylist.setActiveElement(element);
    MyPlaylist.test_reactByTag();
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("excl"));
    return;
}

void TestTContainer::test_reactByTagForRef()
{
    InhertitedTContainer MyPlaylist;
    QDomDocument document;
    QDomElement element = document.createElement(QString("ref"));
    QCOMPARE(element.tagName(), QString("ref"));

    element.setAttribute("type", "image/png");
    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("img"));

    element.setAttribute("type", "video/mp4");
    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("video"));

    element.setAttribute("type", "audio/wav");
    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("audio"));

    element.setAttribute("type", "text/html");
    MyPlaylist.setActiveElement(element);
    QCOMPARE(MyPlaylist.test_reactByTag(), QString("text"));
    return;
}
*/

QTEST_APPLESS_MAIN(TestTContainer)

#include "tst_container.moc"
