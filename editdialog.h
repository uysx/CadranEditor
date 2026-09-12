#ifndef EDITDIALOG_H
#define EDITDIALOG_H

#include <QDialog>


// namespace {
// constexpr char DB_NAME[]    = "ksixcadran2025";
// constexpr char TABLE_TEXT[] = "ksix_table_cadran_compass"; //ksix_table_cadran_compass
// constexpr char TABLE_PNG[]  = "ksix_table_image_cadran_compass"; //ksix_table_image_cadran_compass
// }

namespace Ui {
class editDialog;
}

class editDialog : public QDialog
{
    Q_OBJECT

public:
    explicit editDialog(const QString& data, const QString& filePath, QWidget *parent = nullptr);
    ~editDialog();

private:
    Ui::editDialog *ui;
    QString filePathName;



   // QString sqlEscape(const QString &in);
   // void CreerWatchfacesCompassSql(const QStringList &fichiersTxt, int perFile);

    //void CreerWatchfacesCompassSql(const QString &fichiersSql);
    // void exportCompassTextSql(const QString &sqlPath);
    // void exportCompassPngSql(const QString &sqlPath, const QString &pngFiles);
    //QString baseNameNoExtLimit64(const QString &path);
private slots:

    //void on_buttonBox_accepted();
    // void on_pushButtonIwl_clicked();
    // void on_pushButtonPng_clicked();
    void on_pushButtonQuit_clicked();
    // void on_pushButtonOuvrir_clicked();
    void on_pushButtonSauver_clicked();
};

#endif // EDITDIALOG_H
