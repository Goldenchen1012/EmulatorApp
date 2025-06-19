#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QMouseEvent>
#include <QLineEdit>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onScanPorts();        // 掃描可用的COM埠
    void onOpenPort();         // 開啟選擇的COM埠
    void onSendPacket();       // 傳送16Bytes資料封包
    void onSendTotalAFE();     // 傳送AFE總數設定封包
    void onSendRangeVoltage(); // 傳送設定範圍電壓封包
    void onSerialReceived();   // 接收資料事件處理
    void onCalcCrc15();
    void onCalcCrc10();

    void on_comboBoxCmd_currentIndexChanged(int index);
    void on_comboBoxCmdType_currentIndexChanged(int index);

    void onSendSpiMode();
    void onLineEditSetHexStringHead();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;       // 串口物件
    QLineEdit* crc10Edits[7];  // 對應 lineEditCrc10_0 ~ _6
    QByteArray serialBuffer;   // Buffer 用來暫存串口接收資料
    QTimer *rxDelayTimer;      // 延遲顯示用的 Timer
};

/*
class HexLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            QString text = this->text();
            if (!text.startsWith("0x", Qt::CaseInsensitive)) {
                this->setText("0x" + text);
            }
        }
        QLineEdit::mousePressEvent(event);
    }
};
*/

#endif // MAINWINDOW_H
