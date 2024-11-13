/*(Copyright 2005-2020 Kendall F. Morris

This file is part of the USF Neural Simulator suite.

    The Neural Simulator suite is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    The suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the suite.  If not, see <https://www.gnu.org/licenses/>.
*/


/* A utility app that is more or less a message box but is usable by non-gui
   programs such as simrun.
*/

#include "simmsg.h"
#include "ui_simmsg.h"
#include <QApplication>
#include <QMessageBox>
#include <QColor>
#include <QStyle>
#include <stdio.h>
#include <iostream>
#include <QStringList>
#include <QTextStream>
#include <QAbstractButton>

QString title="Notice";
QString msg="No Message\nTo Display";
using namespace std;

int main(int argc, char *argv[])
{
   QApplication a(argc, argv);
   if (argc > 2) // expect title and text to display
   {
      title = argv[1];
      msg = argv[2];
   }
   simMsg w;
   w.show();
   return a.exec();
}

simMsg::simMsg(QWidget *parent) : QDialog(parent), ui(new Ui::simMsg)
{
   ui->setupUi(this);
     // use same bg color as app, not default of white
     // so it looks more like a dialog box
   QColor col = QApplication::palette().color(QWidget::backgroundRole());
   QString style = "QPlainTextEdit{background-color: ";
   style += col.name();
   style += ";}";
   ui->msgText->setStyleSheet(style);
   ui->msgText->setEnabled(true);
   setWindowTitle(title);
   setWindowFlag(Qt::WindowStaysOnTopHint);

    // add icon
   mb = new QMessageBox;
   QStyle *i_style = QApplication::style();
   int iconSize = i_style->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, mb);
   QIcon tmpIcon;
   tmpIcon = i_style->standardIcon(QStyle::SP_MessageBoxWarning, 0, mb);
   ui->iconHolder->setFixedHeight(iconSize);
   ui->iconHolder->setFixedWidth(iconSize);
   ui->iconHolder->setPixmap(tmpIcon.pixmap(iconSize, iconSize));

    // larger button
    QList<QAbstractButton *> buttons = ui->buttonBox->buttons();
    for (auto iter  = buttons.begin(); iter != buttons.end(); ++iter)
    {
        (*iter)->setMinimumHeight(ui->buttonBox->minimumHeight());
        (*iter)->setMinimumWidth(120);
        (*iter)->setMaximumWidth(120);
    }

    // all this to turn "\n" into a real newline
   QStringList lines;
   QString msg2;
   QTextStream str(&msg2);
   lines = msg.split("\\n");
   for (auto iter = lines.begin(); iter != lines.end(); ++iter)
   {
      str << *iter;
      str << endl;
   }
   ui->msgText->setPlainText(msg2);
}

simMsg::~simMsg()
{
   delete ui;
   delete mb;
}
