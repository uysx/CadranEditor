#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QScreen>
#include <QLabel>

#include "jsonwatchface.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void onXChanged(int value);
    void onYChanged(int value);

    void onWChanged(int w);
    void onHChanged(int h);
    void on_pushButtonAppliquer_clicked();
    void on_pushButtonCharger_clicked();
    void on_pushButtonSauver_clicked();
    void on_pushButtonAjouterWidget_clicked();
    void on_pushButtonQuitter_clicked();
    void on_pushButtonSupprimerWidget_clicked();

    void on_pushButtonEnvoyer_clicked();
    void on_pushButtonChargeIwfLz_clicked();

    void on_pushButtonCreateIwfLz_clicked();

    void on_pushButtonCreerPreview_clicked();

    void on_pushButtonCreatelaunchPicture_clicked();

    void on_pushButtonNew_clicked();

    void on_checkBoxHourSystem_toggled(bool checked);

    void on_timeEdit_timeChanged(const QTime &time);

    void on_dateEdit_dateChanged(const QDate &date);

    void on_pushButtonCreateSqlFromBin_clicked();

    void on_pushButtonCreateFontJson_clicked();
    void on_pushButtonDeleteFontJson_clicked();
    void on_pushButtonEditPicture_clicked();
private:


    QLabel* statusLabel;


    void openJson();
    void populateTypeCombo();

    //void updateHighlightForType(const QString& widget, const QString& type);
    void updateHighlightForType(const int& index);
    void refreshView();
    bool readJsonFile(const QString &filePath);

    Ui::MainWindow *ui;
    QGraphicsScene* scene = nullptr;

    QString jsonFilePath;
    QString fontFilePath;
    QString dossierCourant;
    QString dossierAZipper;
    //QString dossierTempo = QCoreApplication::applicationDirPath() + "/dossierziptempo";
    QString dossierTempo = QCoreApplication::applicationDirPath() + "/dossiertempo";

    bool jsonCharge = false;

    JsonWatchface jwf;
    bool isLoadingJson = false;
    bool jsonOuvertDepuisArgument = false;

    QGraphicsRectItem* highlightRect = nullptr;
    int currentItemIndex = -1;

    QMap<int, QGraphicsItem*> pixmapItemsMap;
    QGraphicsItem* currentPixmapItem = nullptr;

    void updateHighlightRect();
    void onTypeComboChanged(int index);
    void oncurrentWidgetComboChanged(int index);
    void onWidgetComboAddChanged(int index);
    void updateJsonSelectionField(const QString &key, int newValue);
    void displayHexFromBin(const QByteArray &data);


    //*********************************************************************************************************

    void clearBeforeLoad();
    void restartApp();
    void populateTypeComboAdd();
    void ouvrirJson(QString fileName);
    void envoyerCadranVersTelVeryfit();
    bool zipFolder(const QString &folderPath, const QString &zipPath);
    QString detectAdbPath();
    void ajouterLogStatus(const QString &message);
    void populatecurrentWidgetCombo();
    void populateWidgetComboAdd();


    void ouvrirEditDialog(QString data, QString sqlName);
    void ouvrirUnTxt();

    void displayAllWidget();
    bool createIwfLz(const QString &inPath);

    bool copyDirectoryRecursively(const QString &srcPath, const QString &destPath);
    void copyDigitsFolder();
    void copyWeekFolder();
    void displayEmptyScene();


};

#endif // MAINWINDOW_H

