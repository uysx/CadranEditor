
#ifndef UTILS_H
#define UTILS_H

#include <QPixmap>
#include <QColor>
#include <QString>
#include <QImage>
#include <QGraphicsItemGroup>
#include <QCheckBox>
#include <QLineEdit>
#include "ui_mainwindow.h"  // pour accéder à `ui->lineEdit...`
#include "jsonwatchface.h"

QPixmap loadPixmapWithFallback(const QString& baseDir, const QString& relativePath);
QGraphicsItemGroup* createDigitGroupAligned(const QVector<QPixmap>& digits, const QString& align, int x, int y, int w);


//void connectCheckBoxToLineEdit(QCheckBox *checkBox, QLineEdit *lineEdit);
void setupCheckboxLinks(Ui::MainWindow *ui);  // nouvelle fonction
void updateCheckboxesFromItem(Ui::MainWindow *ui, const JsonItem &item);

void setAllAttributVisibleFalse(Ui::MainWindow *ui);
void setOnlyCurrentWidgetAttributVisibleTrue(Ui::MainWindow *ui, const JsonItem &item);

void connectCheckBoxToField(QCheckBox* checkbox, QLineEdit* lineEdit, QLabel* label);

bool copyFileForce(const QString &srcFile, const QString &dstFile);
bool copyDirRecursive(const QString &srcDirPath, const QString &dstDirPath);


#endif // UTILS_H


