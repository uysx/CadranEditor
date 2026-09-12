#include "utils.h"

#include <QFile>
#include <QDir>
#include <QDebug>
#include <QImage>
#include <QFileInfo>
#include <QCoreApplication>
#include <QGraphicsItemGroup>
#include <QCheckBox>
#include <QLineEdit>


QPixmap loadPixmapWithFallback(const QString& baseDir, const QString& relativePath) {
    //qDebug() << "2 🔍 imageKey =" << baseDir + " - " + relativePath;

    QFileInfo fi(relativePath);
    QString nameNoExt = fi.completeBaseName();
    QString extension = fi.suffix().toLower();

    if (extension == "png" || extension == "bmp") {
        QString fullPath = QDir(baseDir).filePath(relativePath);
        QImage img(fullPath);
        if (!img.isNull()) {
            return QPixmap::fromImage(img);
        }

    } else {
        QString pngPath = QDir(baseDir).filePath(relativePath + ".png");
        if (QFile::exists(pngPath)) {
            QImage img(pngPath);
            if (!img.isNull()) {
                return QPixmap::fromImage(img);
            }
        }

        QString bmpPath = QDir(baseDir).filePath(relativePath + ".bmp");
        if (QFile::exists(bmpPath)) {
            QImage bmpImage(bmpPath);
            if (!bmpImage.isNull()) {
                return QPixmap::fromImage(bmpImage);
            }
        }
    }

    QString justFileName = fi.fileName();
    nameNoExt = fi.completeBaseName();
    QString rootPath = QDir(baseDir).filePath(justFileName);
    extension = fi.suffix().toLower();

    QImage img(rootPath);
    if (!img.isNull()) {
        return QPixmap::fromImage(img);
    }
    return QPixmap();
}

QGraphicsItemGroup* createDigitGroupAligned(const QVector<QPixmap>& digits, const QString& align, int x, int y, int w) {
    int totalWidth = 0;
    for (const QPixmap& pix : digits) {
        totalWidth += pix.width();
    }

    int xpos = x;
    if (align == "center") {
        xpos = x + (w - totalWidth) / 2;
    } else if (align == "right") {
        xpos = x + (w - totalWidth);
    }

    QGraphicsItemGroup* group = new QGraphicsItemGroup();
    for (const QPixmap& pix : digits) {
        auto* item = new QGraphicsPixmapItem(pix);
        item->setPos(xpos - x, 0);
        group->addToGroup(item);
        xpos += pix.width();
    }

    group->setPos(x, y);
    return group;
}

// void connectCheckBoxToLineEdit(QCheckBox *checkBox, QLineEdit *lineEdit) {
//     if (!checkBox || !lineEdit) return;

//     // Applique l'état initial
//     lineEdit->setEnabled(checkBox->isChecked());

//     // Connecte la checkbox au champ
//     QObject::connect(checkBox, &QCheckBox::toggled, lineEdit, &QLineEdit::setEnabled);
// }

void connectCheckBoxToLineEditAndLabel(QCheckBox* checkbox, QLineEdit* lineEdit, QLabel* label) {
    QObject::connect(checkbox, &QCheckBox::toggled, [lineEdit, label](bool checked) {
        lineEdit->setEnabled(checked);
        label->setEnabled(checked);
    });
    lineEdit->setEnabled(checkbox->isChecked());
    label->setEnabled(checkbox->isChecked());
}


void connectCheckBoxToField(QCheckBox* checkbox, QLineEdit* lineEdit, QLabel* label) {
    QObject::connect(checkbox, &QCheckBox::toggled, [lineEdit, label](bool checked) {
        lineEdit->setEnabled(checked);
        label->setEnabled(checked);
    });
    lineEdit->setEnabled(checkbox->isChecked());
    label->setEnabled(checkbox->isChecked());
}


void setupCheckboxLinks(Ui::MainWindow *ui) {
    //connectCheckBoxToField(ui->checkBoxWidget,  ui->lineEditWidget,      ui->labelWidget);
    //connectCheckBoxToField(ui->checkBoxType,  ui->lineEditType,      ui->labelType);
    connectCheckBoxToField(ui->checkBoxAlign,  ui->lineEditAlign,      ui->labelAlign);
    connectCheckBoxToField(ui->checkBoxFont,  ui->lineEditFont,       ui->labelFont);
    connectCheckBoxToField(ui->checkBoxFontnum, ui->lineEditFontnum,    ui->labelFontnum);
    connectCheckBoxToField(ui->checkBoxStyle, ui->lineEditStyle,      ui->labelStyle);
    connectCheckBoxToField(ui->checkBoxFgcolor, ui->lineEditFgcolor,    ui->labelFgcolor);
    connectCheckBoxToField(ui->checkBoxFgrender, ui->lineEditFgrender,   ui->labelFgrender);
    connectCheckBoxToField(ui->checkBoxBgcolor, ui->lineEditBgcolor,    ui->labelBgcolor);
    connectCheckBoxToField(ui->checkBoxBgrender, ui->lineEditBgrender,   ui->labelBgrender);
    connectCheckBoxToField(ui->checkBoxBg, ui->lineEditBg,         ui->labelBg);
    connectCheckBoxToField(ui->checkBoxAnimaicon, ui->lineEditAnimaicon,  ui->labelAnimaicon);
    connectCheckBoxToField(ui->checkBoxTurn, ui->lineEditTurn,       ui->labelTurn);
    connectCheckBoxToField(ui->checkBoxFrame, ui->lineEditFrame,      ui->labelFrame);
    connectCheckBoxToField(ui->checkBoxTime, ui->lineEditTime,       ui->labelTime);
    connectCheckBoxToField(ui->checkBoxAnimatype, ui->lineEditAnimatype,  ui->labelAnimatype);

    connectCheckBoxToField(ui->checkBoxAnimabpp, ui->lineEditAnimabpp,  ui->labelAnimabpp);
    connectCheckBoxToField(ui->checkBoxAnimaformat, ui->lineEditAnimaformat,  ui->labelAnimaformat);

    connectCheckBoxToField(ui->checkBoxProgress, ui->lineEditProgress,   ui->labelProgress);
    connectCheckBoxToField(ui->checkBoxStartangle, ui->lineEditStartangle, ui->labelStartangle);
    connectCheckBoxToField(ui->checkBoxEndangle, ui->lineEditEndangle,   ui->labelEndangle);
    connectCheckBoxToField(ui->checkBoxRingedge, ui->lineEditRingedge,   ui->labelRingedge);
    connectCheckBoxToField(ui->checkBoxApp, ui->lineEditApp,        ui->labelApp);
    connectCheckBoxToField(ui->checkBoxMetricinch, ui->lineEditMetricinch, ui->labelMetricinch);

    connectCheckBoxToField(ui->checkBoxHour, ui->lineEditHour, ui->labelHour);
    connectCheckBoxToField(ui->checkBoxHourcenterx, ui->lineEditHourcenterx, ui->labelHourcenterx);
    connectCheckBoxToField(ui->checkBoxHourcentery, ui->lineEditHourcentery, ui->labelHourcentery);
    connectCheckBoxToField(ui->checkBoxHouranchorx, ui->lineEditHouranchorx, ui->labelHouranchorx);
    connectCheckBoxToField(ui->checkBoxHouranchory, ui->lineEditHouranchory, ui->labelHouranchory);

    connectCheckBoxToField(ui->checkBoxMinute, ui->lineEditMinute, ui->labelMinute);
    connectCheckBoxToField(ui->checkBoxMincenterx, ui->lineEditMincenterx, ui->labelMincenterx);
    connectCheckBoxToField(ui->checkBoxMincentery, ui->lineEditMincentery, ui->labelMincentery);
    connectCheckBoxToField(ui->checkBoxMinanchorx, ui->lineEditMinanchorx, ui->labelMinanchorx);
    connectCheckBoxToField(ui->checkBoxMinanchory, ui->lineEditMinanchory, ui->labelMinanchory);

    connectCheckBoxToField(ui->checkBoxSecond, ui->lineEditSecond, ui->labelSecond);
    connectCheckBoxToField(ui->checkBoxSeccenterx, ui->lineEditSeccenterx, ui->labelSeccenterx);
    connectCheckBoxToField(ui->checkBoxSeccentery, ui->lineEditSeccentery, ui->labelSeccentery);
    connectCheckBoxToField(ui->checkBoxSecanchorx, ui->lineEditSecanchorx, ui->labelSecanchorx);
    connectCheckBoxToField(ui->checkBoxSecanchory, ui->lineEditSecanchory, ui->labelSecanchory);


    // QString Hour;
    // int lineEdit Hourcenterx = 0;
    // int lineEdit Hourcentery = 0;
    // int lineEdit Houranchorx = 0;
    // int lineEdit Houranchory = 0;

    // QString Minute;
    // int lineEdit Mincenterx = 0;
    // int lineEdit Mincentery = 0;
    // int lineEdit Minanchorx = 0;
    // int lineEdit Minanchory = 0;

    // QString Second;
    // int lineEdit Seccenterx = 0;
    // int lineEdit Seccentery = 0;
    // int lineEdit Secanchorx = 0;
    // int lineEdit Secanchory = 0;




}




void updateCheckboxesFromItem(Ui::MainWindow *ui, const JsonItem &item) {
    //ui->checkBoxWidget->setChecked(!item.widget.isEmpty());
    //ui->checkBoxType->setChecked(!item.type.isEmpty());
    ui->checkBoxAlign->setChecked(!item.align.isEmpty());
    ui->checkBoxFont->setChecked(!item.font.isEmpty());
    ui->checkBoxFontnum->setChecked(item.fontnum != -1);
    ui->checkBoxStyle->setChecked(item.style != -1);
    ui->checkBoxFgcolor->setChecked(!item.fgcolor.isEmpty());
    ui->checkBoxFgrender->setChecked(!item.fgrender.isEmpty());
    ui->checkBoxBgcolor->setChecked(!item.bgcolor.isEmpty());
    ui->checkBoxBgrender->setChecked(!item.bgrender.isEmpty());
    ui->checkBoxBg->setChecked(!item.bg.isEmpty());
    ui->checkBoxAnimaicon->setChecked(!item.animaicon.isEmpty());
    ui->checkBoxTurn->setChecked(item.turn != -1);
    ui->checkBoxFrame->setChecked(item.frame != -1);
    ui->checkBoxTime->setChecked(item.time != -1);
    ui->checkBoxAnimatype->setChecked(!item.animatype.isEmpty());

    ui->checkBoxAnimabpp->setChecked(item.animabpp != -1);
    ui->checkBoxAnimaformat->setChecked(!item.animaformat.isEmpty());

    ui->checkBoxProgress->setChecked(!item.progress.isEmpty());
    ui->checkBoxStartangle->setChecked(item.startangle != -1);
    ui->checkBoxEndangle->setChecked(item.endangle != -1);
    ui->checkBoxRingedge->setChecked(item.ringedge != -1);
    ui->checkBoxApp->setChecked(!item.app.isEmpty());
    ui->checkBoxMetricinch->setChecked(item.metricinch != -1);

    ui->checkBoxHour->setChecked(!item.hour.isEmpty());
    ui->checkBoxMinute->setChecked(!item.minute.isEmpty());
    ui->checkBoxSecond->setChecked(!item.second.isEmpty());

    ui->checkBoxHourcenterx->setChecked(item.hourcenterx != -1);
    ui->checkBoxHourcentery->setChecked(item.hourcentery != -1);
    ui->checkBoxHouranchorx->setChecked(item.houranchorx != -1);
    ui->checkBoxHouranchory->setChecked(item.houranchory != -1);

    ui->checkBoxMincenterx->setChecked(item.mincenterx != -1);
    ui->checkBoxMincentery->setChecked(item.mincentery != -1);
    ui->checkBoxMinanchorx->setChecked(item.minanchorx != -1);
    ui->checkBoxMinanchory->setChecked(item.minanchory != -1);

    ui->checkBoxSeccenterx->setChecked(item.seccenterx != -1);
    ui->checkBoxSeccentery->setChecked(item.seccentery != -1);
    ui->checkBoxSecanchorx->setChecked(item.secanchorx != -1);
    ui->checkBoxSecanchory->setChecked(item.secanchory != -1);


    // QString hour;
    // int hourcenterx = 0;
    // int hourcentery = 0;
    // int houranchorx = 0;
    // int houranchory = 0;

    // QString minute;
    // int mincenterx = 0;
    // int mincentery = 0;
    // int minanchorx = 0;
    // int minanchory = 0;

    // QString second;
    // int seccenterx = 0;
    // int seccentery = 0;
    // int secanchorx = 0;
    // int secanchory = 0;


    // connectCheckBoxToField(ui->checkBoxHour, ui->lineEditHour, ui->labelHour);
    // connectCheckBoxToField(ui->checkBoxHourcenterx, ui->lineEditHourcenterx, ui->labelHourcenterx);
    // connectCheckBoxToField(ui->checkBoxHourcentery, ui->lineEditHourcentery, ui->labelHourcentery);
    // connectCheckBoxToField(ui->checkBoxHouranchorx, ui->lineEditHouranchorx, ui->labelHouranchorx);
    // connectCheckBoxToField(ui->checkBoxHouranchory, ui->lineEditHouranchory, ui->labelHouranchory);

    // connectCheckBoxToField(ui->checkBoxMinute, ui->lineEditMinute, ui->labelMinute);
    // connectCheckBoxToField(ui->checkBoxMincenterx, ui->lineEditMincenterx, ui->labelMincenterx);
    // connectCheckBoxToField(ui->checkBoxMincentery, ui->lineEditMincentery, ui->labelMincentery);
    // connectCheckBoxToField(ui->checkBoxMinanchorx, ui->lineEditMinanchorx, ui->labelMinanchorx);
    // connectCheckBoxToField(ui->checkBoxMinanchory, ui->lineEditMinanchory, ui->labelMinanchory);

    // connectCheckBoxToField(ui->checkBoxSecond, ui->lineEditSecond, ui->labelSecond);
    // connectCheckBoxToField(ui->checkBoxSeccenterx, ui->lineEditSeccenterx, ui->labelSeccenterx);
    // connectCheckBoxToField(ui->checkBoxSeccentery, ui->lineEditSeccentery, ui->labelSeccentery);
    // connectCheckBoxToField(ui->checkBoxSecanchorx, ui->lineEditSecanchorx, ui->labelSecanchorx);
    // connectCheckBoxToField(ui->checkBoxSecanchory, ui->lineEditSecanchory, ui->labelSecanchory);
}

// void setCheckboxesVisibleFalse(Ui::MainWindow *ui) {

//     ui->checkBoxAlign->setVisible(false);
//     ui->checkBoxFont->setVisible(false);
//     ui->checkBoxFontnum->setVisible(false);
//     ui->checkBoxStyle->setVisible(false);
//     ui->checkBoxFgcolor->setVisible(false);
//     ui->checkBoxFgrender->setVisible(false);
//     ui->checkBoxBgcolor->setVisible(false);
//     ui->checkBoxBgrender->setVisible(false);
//     ui->checkBoxBg->setVisible(false);
//     ui->checkBoxAnimaicon->setVisible(false);
//     ui->checkBoxTurn->setVisible(false);
//     ui->checkBoxFrame->setVisible(false);
//     ui->checkBoxTime->setVisible(false);
//     ui->checkBoxAnimatype->setVisible(false);

//     ui->checkBoxAnimabpp->setVisible(false);
//     ui->checkBoxAnimaformat->setVisible(false);

//     ui->checkBoxProgress->setVisible(false);
//     ui->checkBoxStartangle->setVisible(false);
//     ui->checkBoxEndangle->setVisible(false);
//     ui->checkBoxRingedge->setVisible(false);
//     ui->checkBoxApp->setVisible(false);
//     ui->checkBoxMetricinch->setVisible(false);

//     ui->checkBoxHour->setVisible(false);
//     ui->checkBoxMinute->setVisible(false);
//     ui->checkBoxSecond->setVisible(false);

//     ui->checkBoxHourcenterx->setVisible(false);
//     ui->checkBoxHourcentery->setVisible(false);
//     ui->checkBoxHouranchorx->setVisible(false);
//     ui->checkBoxHouranchory->setVisible(false);

//     ui->checkBoxMincenterx->setVisible(false);
//     ui->checkBoxMincentery->setVisible(false);
//     ui->checkBoxMinanchorx->setVisible(false);
//     ui->checkBoxMinanchory->setVisible(false);

//     ui->checkBoxSeccenterx->setVisible(false);
//     ui->checkBoxSeccentery->setVisible(false);
//     ui->checkBoxSecanchorx->setVisible(false);
//     ui->checkBoxSecanchory->setVisible(false);

// }

// void setLinesEditVisibleFalse(Ui::MainWindow *ui) {
//     ui->lineEditAlign->setVisible(false);
//     ui->lineEditFont->setVisible(false);
//     ui->lineEditFgcolor->setVisible(false);
//     ui->lineEditFgrender->setVisible(false);
//     ui->lineEditBgcolor->setVisible(false);
//     ui->lineEditBgrender->setVisible(false);
//     ui->lineEditBg->setVisible(false);
//     ui->lineEditAnimaicon->setVisible(false);
//     ui->lineEditAnimatype->setVisible(false);
//     ui->lineEditAnimaformat->setVisible(false);
//     ui->lineEditProgress->setVisible(false);
//     ui->lineEditApp->setVisible(false);

//     ui->lineEditHour->setVisible(false);
//     ui->lineEditMinute->setVisible(false);
//     ui->lineEditSecond->setVisible(false);


//     ui->lineEditFontnum->setVisible(false);
//     ui->lineEditStyle->setVisible(false);
//     ui->lineEditTurn->setVisible(false);
//     ui->lineEditFrame->setVisible(false);
//     ui->lineEditTime->setVisible(false);

//     ui->lineEditAnimabpp->setVisible(false);

//     ui->lineEditStartangle->setVisible(false);
//     ui->lineEditEndangle->setVisible(false);
//     ui->lineEditRingedge->setVisible(false);
//     ui->lineEditMetricinch->setVisible(false);

//     ui->lineEditHourcenterx->setVisible(false);
//     ui->lineEditHourcentery->setVisible(false);
//     ui->lineEditHouranchorx->setVisible(false);
//     ui->lineEditHouranchory->setVisible(false);

//     ui->lineEditMincenterx->setVisible(false);
//     ui->lineEditMincentery->setVisible(false);
//     ui->lineEditMinanchorx->setVisible(false);
//     ui->lineEditMinanchory->setVisible(false);

//     ui->lineEditSeccenterx->setVisible(false);
//     ui->lineEditSeccentery->setVisible(false);
//     ui->lineEditSecanchorx->setVisible(false);
//     ui->lineEditSecanchory->setVisible(false);
// }

// void setQlabelVisibleFalse(Ui::MainWindow *ui) {

//     ui->labelAlign->setVisible(false);
//     ui->labelFont->setVisible(false);
//     ui->labelFontnum->setVisible(false);
//     ui->labelStyle->setVisible(false);
//     ui->labelFgcolor->setVisible(false);
//     ui->labelFgrender->setVisible(false);
//     ui->labelBgcolor->setVisible(false);
//     ui->labelBgrender->setVisible(false);
//     ui->labelBg->setVisible(false);
//     ui->labelAnimaicon->setVisible(false);
//     ui->labelTurn->setVisible(false);
//     ui->labelFrame->setVisible(false);
//     ui->labelTime->setVisible(false);
//     ui->labelAnimatype->setVisible(false);

//     ui->labelAnimabpp->setVisible(false);
//     ui->labelAnimaformat->setVisible(false);

//     ui->labelProgress->setVisible(false);
//     ui->labelStartangle->setVisible(false);
//     ui->labelEndangle->setVisible(false);
//     ui->labelRingedge->setVisible(false);
//     ui->labelApp->setVisible(false);
//     ui->labelMetricinch->setVisible(false);

//     ui->labelHour->setVisible(false);
//     ui->labelMinute->setVisible(false);
//     ui->labelSecond->setVisible(false);

//     ui->labelHourcenterx->setVisible(false);
//     ui->labelHourcentery->setVisible(false);
//     ui->labelHouranchorx->setVisible(false);
//     ui->labelHouranchory->setVisible(false);

//     ui->labelMincenterx->setVisible(false);
//     ui->labelMincentery->setVisible(false);
//     ui->labelMinanchorx->setVisible(false);
//     ui->labelMinanchory->setVisible(false);

//     ui->labelSeccenterx->setVisible(false);
//     ui->labelSeccentery->setVisible(false);
//     ui->labelSecanchorx->setVisible(false);
//     ui->labelSecanchory->setVisible(false);
// }


void setAllAttributVisibleFalse(Ui::MainWindow *ui) {
    ui->checkBoxAlign->setVisible(false);
    ui->labelAlign->setVisible(false);
    ui->lineEditAlign->setVisible(false);

    ui->checkBoxFont->setVisible(false);
    ui->labelFont->setVisible(false);
    ui->lineEditFont->setVisible(false);

    ui->checkBoxFontnum->setVisible(false);
    ui->labelFontnum->setVisible(false);
    ui->lineEditFontnum->setVisible(false);

    ui->checkBoxStyle->setVisible(false);
    ui->labelStyle->setVisible(false);
    ui->lineEditStyle->setVisible(false);

    ui->checkBoxFgcolor->setVisible(false);
    ui->labelFgcolor->setVisible(false);
    ui->lineEditFgcolor->setVisible(false);

    ui->checkBoxFgrender->setVisible(false);
    ui->labelFgrender->setVisible(false);
    ui->lineEditFgrender->setVisible(false);

    ui->checkBoxBgcolor->setVisible(false);
    ui->labelBgcolor->setVisible(false);
    ui->lineEditBgcolor->setVisible(false);

    ui->checkBoxBgrender->setVisible(false);
    ui->labelBgrender->setVisible(false);
    ui->lineEditBgrender->setVisible(false);

    ui->checkBoxBg->setVisible(false);
    ui->labelBg->setVisible(false);
    ui->lineEditBg->setVisible(false);

    ui->checkBoxAnimaicon->setVisible(false);
    ui->labelAnimaicon->setVisible(false);
    ui->lineEditAnimaicon->setVisible(false);

    ui->checkBoxTurn->setVisible(false);
    ui->labelTurn->setVisible(false);
    ui->lineEditTurn->setVisible(false);

    ui->checkBoxFrame->setVisible(false);
    ui->labelFrame->setVisible(false);
    ui->lineEditFrame->setVisible(false);

    ui->checkBoxTime->setVisible(false);
    ui->labelTime->setVisible(false);
    ui->lineEditTime->setVisible(false);

    ui->checkBoxAnimatype->setVisible(false);
    ui->labelAnimatype->setVisible(false);
    ui->lineEditAnimatype->setVisible(false);

    ui->checkBoxAnimabpp->setVisible(false);
    ui->labelAnimabpp->setVisible(false);
    ui->lineEditAnimabpp->setVisible(false);

    ui->checkBoxAnimaformat->setVisible(false);
    ui->labelAnimaformat->setVisible(false);
    ui->lineEditAnimaformat->setVisible(false);

    ui->checkBoxProgress->setVisible(false);
    ui->labelProgress->setVisible(false);
    ui->lineEditProgress->setVisible(false);

    ui->checkBoxStartangle->setVisible(false);
    ui->labelStartangle->setVisible(false);
    ui->lineEditStartangle->setVisible(false);

    ui->checkBoxEndangle->setVisible(false);
    ui->labelEndangle->setVisible(false);
    ui->lineEditEndangle->setVisible(false);

    ui->checkBoxRingedge->setVisible(false);
    ui->labelRingedge->setVisible(false);
    ui->lineEditRingedge->setVisible(false);

    ui->checkBoxApp->setVisible(false);
    ui->labelApp->setVisible(false);
    ui->lineEditApp->setVisible(false);

    ui->checkBoxMetricinch->setVisible(false);
    ui->labelMetricinch->setVisible(false);
    ui->lineEditMetricinch->setVisible(false);

    ui->checkBoxHour->setVisible(false);
    ui->labelHour->setVisible(false);
    ui->lineEditHour->setVisible(false);

    ui->checkBoxMinute->setVisible(false);
    ui->labelMinute->setVisible(false);
    ui->lineEditMinute->setVisible(false);

    ui->checkBoxSecond->setVisible(false);
    ui->labelSecond->setVisible(false);
    ui->lineEditSecond->setVisible(false);

    ui->checkBoxHourcenterx->setVisible(false);
    ui->labelHourcenterx->setVisible(false);
    ui->lineEditHourcenterx->setVisible(false);

    ui->checkBoxHourcentery->setVisible(false);
    ui->labelHourcentery->setVisible(false);
    ui->lineEditHourcentery->setVisible(false);

    ui->checkBoxHouranchorx->setVisible(false);
    ui->labelHouranchorx->setVisible(false);
    ui->lineEditHouranchorx->setVisible(false);

    ui->checkBoxHouranchory->setVisible(false);
    ui->labelHouranchory->setVisible(false);
    ui->lineEditHouranchory->setVisible(false);

    ui->checkBoxMincenterx->setVisible(false);
    ui->labelMincenterx->setVisible(false);
    ui->lineEditMincenterx->setVisible(false);

    ui->checkBoxMincentery->setVisible(false);
    ui->labelMincentery->setVisible(false);
    ui->lineEditMincentery->setVisible(false);

    ui->checkBoxMinanchorx->setVisible(false);
    ui->labelMinanchorx->setVisible(false);
    ui->lineEditMinanchorx->setVisible(false);

    ui->checkBoxMinanchory->setVisible(false);
    ui->labelMinanchory->setVisible(false);
    ui->lineEditMinanchory->setVisible(false);

    ui->checkBoxSeccenterx->setVisible(false);
    ui->labelSeccenterx->setVisible(false);
    ui->lineEditSeccenterx->setVisible(false);

    ui->checkBoxSeccentery->setVisible(false);
    ui->labelSeccentery->setVisible(false);
    ui->lineEditSeccentery->setVisible(false);

    ui->checkBoxSecanchorx->setVisible(false);
    ui->labelSecanchorx->setVisible(false);
    ui->lineEditSecanchorx->setVisible(false);

    ui->checkBoxSecanchory->setVisible(false);
    ui->labelSecanchory->setVisible(false);
    ui->lineEditSecanchory->setVisible(false);

}

void setOnlyCurrentWidgetAttributVisibleTrue(Ui::MainWindow *ui, const JsonItem &item) {
    //qDebug() << "🔵 setOnlyCurrentWidgetAttributVisibleTrue - widget:" << item.widget << "type:" << item.type;
    if (item.widget == "custom") {
        if (item.type == "date"
            || item.type == "time"
            || item.type == "hour"
            || item.type == "min"
            || item.type == "second"
            || item.type == "week"
            || item.type == "day"
            || item.type == "month"
            || item.type == "calorie"
            || item.type == "distance"
            || item.type == "heartrate"
            || item.type == "battery"
            || item.type == "step"
            || item.type == "sleep"
            || item.type == "apm") {

            ////qDebug << "   ✅✅ TYPE MATCHED! Setting visible...";

            ui->checkBoxAlign->setVisible(true);
            ui->labelAlign->setVisible(true);
            ui->lineEditAlign->setVisible(true);

            ui->checkBoxFont->setVisible(true);
            ui->labelFont->setVisible(true);
            ui->lineEditFont->setVisible(true);

            ui->checkBoxFontnum->setVisible(true);
            ui->labelFontnum->setVisible(true);
            ui->lineEditFontnum->setVisible(true);
            //qDebug << "   ✅✅✅ FONT setVisible(true) appelé!";

            ui->checkBoxStyle->setVisible(true);
            ui->labelStyle->setVisible(true);
            ui->lineEditStyle->setVisible(true);

            ui->checkBoxFgcolor->setVisible(true);
            ui->lineEditFgcolor->setVisible(true);
            ui->labelFgcolor->setVisible(true);

            ui->checkBoxFgrender->setVisible(true);
            ui->lineEditFgrender->setVisible(true);
            ui->labelFgrender->setVisible(true);

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);
        }
        else if (item.type == "shortcut"){

            ui->checkBoxApp->setVisible(true);
            ui->labelApp->setVisible(true);
            ui->lineEditApp->setVisible(true);

            ui->checkBoxFgcolor->setVisible(true);
            ui->lineEditFgcolor->setVisible(true);
            ui->labelFgcolor->setVisible(true);

            ui->checkBoxFgrender->setVisible(true);
            ui->lineEditFgrender->setVisible(true);
            ui->labelFgrender->setVisible(true);

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);
        }

        else if (item.type == "redpoint"){

            ui->checkBoxFont->setVisible(true);
            ui->checkBoxFontnum->setVisible(true);

            ui->lineEditFont->setVisible(true);
            ui->lineEditFontnum->setVisible(true);

            ui->labelFont->setVisible(true);
            ui->labelFontnum->setVisible(true);

        }

        else if (item.type == "icon" || item.type == "sleep"){

            ui->checkBoxFgcolor->setVisible(true);
            ui->lineEditFgcolor->setVisible(true);
            ui->labelFgcolor->setVisible(true);

            ui->checkBoxFgrender->setVisible(true);
            ui->lineEditFgrender->setVisible(true);
            ui->labelFgrender->setVisible(true);

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);

            ui->checkBoxBg->setVisible(true);
            ui->lineEditBg->setVisible(true);
            ui->labelBg->setVisible(true);


        }

        else if (item.type == "anima"){

            ui->checkBoxAnimaicon->setVisible(true);
            ui->labelAnimaicon->setVisible(true);
            ui->lineEditAnimaicon->setVisible(true);

            ui->checkBoxTurn->setVisible(true);
            ui->labelTurn->setVisible(true);
            ui->lineEditTurn->setVisible(true);

            ui->checkBoxFrame->setVisible(true);
            ui->labelFrame->setVisible(true);
            ui->lineEditFrame->setVisible(true);

            ui->checkBoxTime->setVisible(true);
            ui->labelTime->setVisible(true);
            ui->lineEditTime->setVisible(true);

            ui->checkBoxAnimatype->setVisible(true);
            ui->labelAnimatype->setVisible(true);
            ui->lineEditAnimatype->setVisible(true);

            ui->checkBoxAnimabpp->setVisible(true);
            ui->labelAnimabpp->setVisible(true);
            ui->lineEditAnimabpp->setVisible(true);

            ui->checkBoxAnimaformat->setVisible(true);
            ui->labelAnimaformat->setVisible(true);
            ui->lineEditAnimaformat->setVisible(true);

        }

        else if (item.type == "bluetooth"){

            ui->checkBoxAnimaicon->setVisible(true);
            ui->labelAnimaicon->setVisible(true);
            ui->lineEditAnimaicon->setVisible(true);

            ui->checkBoxTurn->setVisible(true);
            ui->labelTurn->setVisible(true);
            ui->lineEditTurn->setVisible(true);

            ui->checkBoxFrame->setVisible(true);
            ui->labelFrame->setVisible(true);
            ui->lineEditFrame->setVisible(true);

            ui->checkBoxTime->setVisible(true);
            ui->labelTime->setVisible(true);
            ui->lineEditTime->setVisible(true);

            ui->checkBoxAnimatype->setVisible(true);
            ui->labelAnimatype->setVisible(true);
            ui->lineEditAnimatype->setVisible(true);

        }



    } else if (item.widget == "progressbar") {
        if (item.type == "battery"){

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);

            ui->checkBoxBg->setVisible(true);
            ui->lineEditBg->setVisible(true);
            ui->labelBg->setVisible(true);

            ui->checkBoxProgress->setVisible(true);
            ui->labelProgress->setVisible(true);
            ui->lineEditProgress->setVisible(true);
        }

    } else if (item.widget == "ring") {
        if (item.type == "calorie" || item.type == "battery" || item.type == "step"){

            ui->checkBoxFgcolor->setVisible(true);
            ui->lineEditFgcolor->setVisible(true);
            ui->labelFgcolor->setVisible(true);

            ui->checkBoxFgrender->setVisible(true);
            ui->lineEditFgrender->setVisible(true);
            ui->labelFgrender->setVisible(true);

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);

            ui->checkBoxBg->setVisible(true);
            ui->lineEditBg->setVisible(true);
            ui->labelBg->setVisible(true);

            ui->checkBoxStartangle->setVisible(true);
            ui->labelStartangle->setVisible(true);
            ui->lineEditStartangle->setVisible(true);

            ui->checkBoxEndangle->setVisible(true);
            ui->labelEndangle->setVisible(true);
            ui->lineEditEndangle->setVisible(true);

            ui->checkBoxRingedge->setVisible(true);
            ui->labelRingedge->setVisible(true);
            ui->lineEditRingedge->setVisible(true);
        }

    } else if (item.widget == "watch") {
        if (item.type == "time"){

            ui->checkBoxFgcolor->setVisible(true);
            ui->lineEditFgcolor->setVisible(true);
            ui->labelFgcolor->setVisible(true);

            ui->checkBoxFgrender->setVisible(true);
            ui->lineEditFgrender->setVisible(true);
            ui->labelFgrender->setVisible(true);

            ui->checkBoxBgcolor->setVisible(true);
            ui->lineEditBgcolor->setVisible(true);
            ui->labelBgcolor->setVisible(true);

            ui->checkBoxBgrender->setVisible(true);
            ui->lineEditBgrender->setVisible(true);
            ui->labelBgrender->setVisible(true);



            ui->checkBoxHour->setVisible(true);
            ui->labelHour->setVisible(true);
            ui->lineEditHour->setVisible(true);

            ui->checkBoxMinute->setVisible(true);
            ui->labelMinute->setVisible(true);
            ui->lineEditMinute->setVisible(true);

            ui->checkBoxSecond->setVisible(true);
            ui->labelSecond->setVisible(true);
            ui->lineEditSecond->setVisible(true);

            ui->checkBoxHourcenterx->setVisible(true);
            ui->labelHourcenterx->setVisible(true);
            ui->lineEditHourcenterx->setVisible(true);

            ui->checkBoxHourcentery->setVisible(true);
            ui->labelHourcentery->setVisible(true);
            ui->lineEditHourcentery->setVisible(true);

            ui->checkBoxHouranchorx->setVisible(true);
            ui->labelHouranchorx->setVisible(true);
            ui->lineEditHouranchorx->setVisible(true);

            ui->checkBoxHouranchory->setVisible(true);
            ui->labelHouranchory->setVisible(true);
            ui->lineEditHouranchory->setVisible(true);

            ui->checkBoxMincenterx->setVisible(true);
            ui->labelMincenterx->setVisible(true);
            ui->lineEditMincenterx->setVisible(true);

            ui->checkBoxMincentery->setVisible(true);
            ui->labelMincentery->setVisible(true);
            ui->lineEditMincentery->setVisible(true);

            ui->checkBoxMinanchorx->setVisible(true);
            ui->labelMinanchorx->setVisible(true);
            ui->lineEditMinanchorx->setVisible(true);

            ui->checkBoxMinanchory->setVisible(true);
            ui->labelMinanchory->setVisible(true);
            ui->lineEditMinanchory->setVisible(true);

            ui->checkBoxSeccenterx->setVisible(true);
            ui->labelSeccenterx->setVisible(true);
            ui->lineEditSeccenterx->setVisible(true);

            ui->checkBoxSeccentery->setVisible(true);
            ui->labelSeccentery->setVisible(true);
            ui->lineEditSeccentery->setVisible(true);

            ui->checkBoxSecanchorx->setVisible(true);
            ui->labelSecanchorx->setVisible(true);
            ui->lineEditSecanchorx->setVisible(true);

            ui->checkBoxSecanchory->setVisible(true);
            ui->labelSecanchory->setVisible(true);
            ui->lineEditSecanchory->setVisible(true);
        }

    }

}
bool copyFileForce(const QString &srcFile, const QString &dstFile)
{
    if (QFile::exists(dstFile)) {
        QFile::remove(dstFile); // on écrase
    }
    QDir dstDir(QFileInfo(dstFile).absolutePath());
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) {
            qWarning() << "Impossible de créer le dossier" << dstDir.absolutePath();
            return false;
        }
    }
    if (!QFile::copy(srcFile, dstFile)) {
        qWarning() << "Échec copie" << srcFile << "->" << dstFile;
        return false;
    }
    return true;
}

bool copyDirRecursive(const QString &srcDirPath, const QString &dstDirPath)
{
    QDir srcDir(srcDirPath);
    if (!srcDir.exists()) {
        qWarning() << "Source inexistante:" << srcDirPath;
        return false;
    }

    QDir dstDir(dstDirPath);
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) {
            qWarning() << "Impossible de créer:" << dstDirPath;
            return false;
        }
    }

    // fichiers à la racine
    for (const QFileInfo &fi : srcDir.entryInfoList(QDir::Files)) {
        const QString srcFile = fi.absoluteFilePath();
        const QString dstFile = dstDir.filePath(fi.fileName());
        if (!copyFileForce(srcFile, dstFile)) {
            return false;
        }
    }

    // sous-dossiers
    for (const QFileInfo &fi : srcDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString subSrc = fi.absoluteFilePath();
        const QString subDst = dstDir.filePath(fi.fileName());
        if (!copyDirRecursive(subSrc, subDst)) {
            return false;
        }
    }

    return true;
}


