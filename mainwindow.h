#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

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

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;     // 串口物件
};
#endif // MAINWINDOW_H
