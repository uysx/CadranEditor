#include "editdialog.h"
#include "ui_editdialog.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QSettings>



editDialog::editDialog(const QString &data, const QString& filePath, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::editDialog)
{
    ui->setupUi(this);
    ui->plainTextEdit->setPlainText(data);
    filePathName = filePath;


    setWindowTitle(filePath);
    resize(1000, 500);


    //  ui->pushButtonOuvrir->setVisible(true);
    // ui->pushButtonSauver->setVisible(true);
    // ui->plainTextEdit->setReadOnly(false);



}

editDialog::~editDialog()
{
    delete ui;
}


void editDialog::on_pushButtonQuit_clicked()
{
    close();
}



void editDialog::on_pushButtonSauver_clicked()
{

    int ret = QMessageBox::question(this, "Confirmation",
                                    "Sauver ?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;
    // -------- Sauvegarde du fichier font.json --------
    QString fullText = ui->plainTextEdit->toPlainText();
    // QString fontPathToSave = fontFilePath;

    //if (fontPathToSave.isEmpty()) {
    //QString filePathToSave = QFileDialog::getSaveFileName(this, "Sauver le fichier", "", "Fichiers (*.*)");
    if (filePathName.isEmpty()) return;
    //}

    QFile file1(filePathName);
    if (!file1.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Erreur", "Impossible d’écrire dans le fichier.");
        return;
    }

    QTextStream out1(&file1);
    out1 << fullText;
    file1.close();

    QMessageBox::information(this, "Attention", "Le fichier est modifier\nil faut recharger le cadran pour prendre en compte la modif...");
    close();

}

