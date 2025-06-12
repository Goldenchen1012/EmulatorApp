#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QDebug>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QString>
#include <QByteArray>
#include <QTextStream>
#include <cstdint>
#include <cmath>
#include "LibCrc15Crc10TableCalc.h"

#define EMULATOR_APP_NAME_STR         QString("EmulatorApp")
#define EMULATOR_APP_VERSION_STR      QString("V1.0")

#define APP_CMD_MASK                               (0x8000)
#define APP_CMD_AFE_NUM                            (0x8001)
#define APP_CMD_AFE_V_INC                          (0x8010)

#define APP_AFECASE_NUM_MAX                        (30)

#define APP_EMU_UART_HAED1                         (0x55)
#define APP_EMU_UART_HAED2                         (0xAA)

#define APP_EMU_A_HEAD1                             (0)
#define APP_EMU_A_HEAD2                             (1)
#define APP_EMU_A_CMD1                              (2)
#define APP_EMU_A_CMD2                              (3)
#define APP_EMU_A_CMD3                              (4)
#define APP_EMU_A_CMD4                              (5)
#define APP_EMU_A_AFEINDEX                          (6)
#define APP_EMU_A_DATA                              (7)
#define APP_EMU_A_CHECKSUM                          (15)

#define APP_EMU_UART_DATA_LEN                       (8)

#define APP_EMU_UART_PACKET_LEN                     (16)

#define APP_EMU_REMAIN_DATA_DELAY                   (100)   //ms

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(EMULATOR_APP_NAME_STR + " " + EMULATOR_APP_VERSION_STR);

    // 初始化通訊用 serialPort
    serial = new QSerialPort(this);

    // 預設CMD1-CMD4選項
    ui->comboBoxCmd->addItem("RDCVA", QVariant::fromValue(QByteArray::fromHex("00000004")));
    ui->comboBoxCmd->addItem("RDCVB", QVariant::fromValue(QByteArray::fromHex("00000006")));
    ui->comboBoxCmd->addItem("RDCVC", QVariant::fromValue(QByteArray::fromHex("00000008")));
    ui->comboBoxCmd->addItem("RDCVD", QVariant::fromValue(QByteArray::fromHex("0000000A")));
    ui->comboBoxCmd->addItem("RDCVE", QVariant::fromValue(QByteArray::fromHex("00000009")));
    ui->comboBoxCmd->addItem("RDCVF", QVariant::fromValue(QByteArray::fromHex("0000000B")));

    for (int i = 1; i <= APP_AFECASE_NUM_MAX; ++i)
    {
        ui->comboBoxAfeIndex->addItem(QString("AFE%1").arg(i), i - 1);
        ui->comboBoxTotalAFE->addItem(QString::number(i), i); // 新增 AFE TOTAL 組數
    }

    ui->comboBoxPEC->addItem("Correct PEC", true);
    ui->comboBoxPEC->addItem("Incorrect PEC", false);

    // 初始化 comboBoxCmdType
    ui->comboBoxCmdType->addItem("RDCVA", 0x01);
    ui->comboBoxCmdType->addItem("RDCVB", 0x02);
    ui->comboBoxCmdType->addItem("RDCVC", 0x03);
    ui->comboBoxCmdType->addItem("RDCVD", 0x04);
    ui->comboBoxCmdType->addItem("RDCVE", 0x05);
    ui->comboBoxCmdType->addItem("RDCVF", 0x06);

    // 初始化 comboBoxStartIndex 和 comboBoxEndIndex
    for (int i = 1; i <= APP_AFECASE_NUM_MAX; ++i)
    {
        ui->comboBoxStartIndex->addItem(QString("AFE%1").arg(i), i - 1);
        ui->comboBoxEndIndex->addItem(QString("AFE%1").arg(i), i - 1);
    }

    connect(ui->btnScan, &QPushButton::clicked, this, &MainWindow::onScanPorts);
    connect(ui->btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenPort);
    connect(ui->btnSend, &QPushButton::clicked, this, &MainWindow::onSendPacket);
    connect(ui->btnSendTotalAFE, &QPushButton::clicked, this, &MainWindow::onSendTotalAFE);
    connect(ui->btnSendRangeVoltage, &QPushButton::clicked, this, &MainWindow::onSendRangeVoltage);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onSerialReceived);

    // 清除TX按鈕
    connect(ui->btnClearTx, &QPushButton::clicked, this, [=]() {
        ui->textEditTx->clear();
    });

    // 清除RX按鈕
    connect(ui->btnClearRx, &QPushButton::clicked, this, [=]() {
        ui->textEditRx->clear();
    });

    crc10Edits[0] = ui->lineEditCrc10_0;
    crc10Edits[1] = ui->lineEditCrc10_1;
    crc10Edits[2] = ui->lineEditCrc10_2;
    crc10Edits[3] = ui->lineEditCrc10_3;
    crc10Edits[4] = ui->lineEditCrc10_4;
    crc10Edits[5] = ui->lineEditCrc10_5;
    crc10Edits[6] = ui->lineEditCrc10_6;

    connect(ui->btnCalcCrc15, &QPushButton::clicked, this, &MainWindow::onCalcCrc15);
    connect(ui->btnCalcCrc10, &QPushButton::clicked, this, &MainWindow::onCalcCrc10);

    connect(ui->btnClearCrcResult, &QPushButton::clicked, this, [=]() {
        ui->textEditCrcResult->clear();
    });


    // 加入這段到 MainWindow 建構子中
    //---------------------------------------------
    serialBuffer.clear();
    rxDelayTimer = new QTimer(this);
    rxDelayTimer->setSingleShot(true);
    rxDelayTimer->setInterval(APP_EMU_REMAIN_DATA_DELAY); //ms 延遲顯示

    connect(rxDelayTimer, &QTimer::timeout, this, [=]() {
        if (!serialBuffer.isEmpty()) {
            QString hexStr;
            for (int i = 0; i < serialBuffer.size(); ++i)
                hexStr += QString("%1 ").arg(static_cast<uint8_t>(serialBuffer[i]), 2, 16, QChar('0')).toUpper();

            ui->textEditRx->append("RX (Rem): " + hexStr.trimmed());
            qDebug() << "Received (Rem): " << serialBuffer.toHex(' ').toUpper();
            serialBuffer.clear();
        }
    });
    //---------------------------------------------
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    delete ui;
}

void MainWindow::onScanPorts()
{
    ui->comboBoxPort->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
        ui->comboBoxPort->addItem(info.portName());
}

void MainWindow::onOpenPort()
{
    if (serial->isOpen())
        serial->close();

    QString portName = ui->comboBoxPort->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a COM port");
        return;
    }

    serial->setPortName(portName);
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, "Error", "Failed to open COM port");
    } else {
        QMessageBox::information(this, "Success", "COM port opened successfully");
    }
}

static bool parseHexIfNeeded(const QString& input, uint16_t& out)
{
    if (input.startsWith("0x", Qt::CaseInsensitive)) {
        bool ok = false;
        out = input.toUShort(&ok, 16);
        return ok;
    }
    return false;
}

static void voltage_to_bytes(double voltage_uV, uint8_t* out_bytes) {
    uint16_t raw = static_cast<uint16_t>((voltage_uV - 1500000.0) / 150.0);
    out_bytes[0] = static_cast<uint8_t>(raw & 0xFF);
    out_bytes[1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
}

void MainWindow::onSendPacket()
{
    if (ui->lineEditV1->text().isEmpty() || ui->lineEditV2->text().isEmpty() || ui->lineEditV3->text().isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter all voltage values (V1, V2, V3).");
        return;
    }

    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Error", "COM port not open");
        return;
    }

    QByteArray packet(16, 0);
    packet[0] = APP_EMU_UART_HAED1;
    packet[1] = APP_EMU_UART_HAED2;

    QByteArray cmdBytes = ui->comboBoxCmd->currentData().toByteArray();
    packet.replace(2, 4, cmdBytes);

    uint8_t afe_index = static_cast<uint8_t>(ui->comboBoxAfeIndex->currentData().toUInt());
    packet[6] = afe_index;

    uint8_t data[7];    
    uint16_t value = 0;

    for (int i = 0; i < 7; ++i)
    {
        data[i] = 0;
    }

    // V1
    if (parseHexIfNeeded(ui->lineEditV1->text(), value)) {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
    } else {
        double v = ui->lineEditV1->text().toDouble() * 1000000;
        voltage_to_bytes(v, &data[0]);
    }

    // V2
    if (parseHexIfNeeded(ui->lineEditV2->text(), value)) {
        data[2] = value & 0xFF;
        data[3] = (value >> 8) & 0xFF;
    } else {
        double v = ui->lineEditV2->text().toDouble() * 1000000;
        voltage_to_bytes(v, &data[2]);
    }

    // V3
    if (parseHexIfNeeded(ui->lineEditV3->text(), value)) {
        data[4] = value & 0xFF;
        data[5] = (value >> 8) & 0xFF;
    } else {
        double v = ui->lineEditV3->text().toDouble() * 1000000;
        voltage_to_bytes(v, &data[4]);
    }

    for (int i = 0; i < 6; ++i)
        packet[7 + i] = data[i];

    //CRC 10(DPEC) Calc
    bool correctPEC = ui->comboBoxPEC->currentData().toBool();
    uint16_t crc = pec10_calc(true, 6, data);
    uint8_t dpec[2] = { static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc & 0xFF) };
    if (!correctPEC)
        dpec[1] ^= 0xFF;

    packet[13] = dpec[0];    //Data 7
    packet[14] = dpec[1];    //Data 8

    uint8_t checksum = 0;
    for (int i = 0; i < 15; ++i)
        checksum += static_cast<uint8_t>(packet[i]);
    packet[15] = checksum;

    serial->write(packet);

    QString hexStr;
    for (int i = 0; i < 16; ++i)
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(packet[i]), 2, 16, QChar('0')).toUpper();

    ui->textEditTx->append("TX: " + hexStr.trimmed());
    qDebug() << "Sent Packet: " << packet.toHex(' ').toUpper();
}

void MainWindow::onSendTotalAFE()
{
    uint16_t u16Cmd;

    u16Cmd = APP_CMD_AFE_NUM;

    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Error", "COM port not open");
        return;
    }

    QByteArray packet(16, 0);
    packet[0] = APP_EMU_UART_HAED1;
    packet[1] = APP_EMU_UART_HAED2;

    packet[2] = 0x00;
    packet[3] = 0x00;
    packet[4] = (u16Cmd >> 8);
    packet[5] = (u16Cmd & 0xFF);

    packet[6] = 0x00; // AFE index 可設為0

    uint8_t afe_total = static_cast<uint8_t>(ui->comboBoxTotalAFE->currentData().toUInt());
    packet[7] = afe_total;

    // Data2 = 是否初始化（0x01 表示需要初始化）
    if(ui->checkBoxInitDevice->isChecked() == true)
    {
        packet[8] = 0x01;
    }else
    {
        packet[8] = 0x00;
    }

    for (int i = 9; i < 15; ++i)
        packet[i] = 0x00;

    uint8_t checksum = 0;
    for (int i = 0; i < 15; ++i)
        checksum += static_cast<uint8_t>(packet[i]);

    packet[15] = checksum;

    serial->write(packet);

    QString hexStr;
    for (int i = 0; i < 16; ++i)
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(packet[i]), 2, 16, QChar('0')).toUpper();

    ui->textEditTx->append("TX: " + hexStr.trimmed());
    qDebug() << "Sent AFE Total Packet: " << packet.toHex(' ').toUpper();
}

void MainWindow::onSendRangeVoltage()
{
    uint16_t u16Cmd;
    u16Cmd = APP_CMD_AFE_V_INC;

    if (ui->lineEditStartVolt->text().isEmpty() || ui->lineEditStepVolt->text().isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter both Start Voltage and Step Voltage.");
        return;
    }

    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Error", "COM port not open");
        return;
    }

    QByteArray packet(16, 0);
    packet[0] = APP_EMU_UART_HAED1;
    packet[1] = APP_EMU_UART_HAED2;
    packet[2] = 0x00;
    packet[3] = 0x00;
    packet[4] = (u16Cmd>>8);
    packet[5] = (u16Cmd & 0xFF);
    packet[6] = 0x00; // AFE index 無使用

    // Data1 = CMD選項
    packet[7] = static_cast<uint8_t>(ui->comboBoxCmdType->currentIndex() + 1); // RDCVA = 1

    // Data2 = 起始 AFE Index
    packet[8] = static_cast<uint8_t>(ui->comboBoxStartIndex->currentData().toUInt());

    // Data3 = 結束 AFE Index
    packet[9] = static_cast<uint8_t>(ui->comboBoxEndIndex->currentData().toUInt());

    // Data4~5 = 開始電壓
    double startV = ui->lineEditStartVolt->text().toDouble() * 1000000.0;
    uint8_t bytesV[2];
    voltage_to_bytes(startV, bytesV);
    packet[10] = bytesV[1]; // Big Endian
    packet[11] = bytesV[0];

    // Data6~7 = 遞增電壓 (mV)，直接轉成 uint16_t，Big Endian
    uint16_t stepMv = static_cast<uint16_t>(ui->lineEditStepVolt->text().toUInt());
    packet[12] = (stepMv >> 8) & 0xFF;
    packet[13] = stepMv & 0xFF;

    packet[14] = 0x00; // 保留位

    // Checksum B0~B14
    uint8_t checksum = 0;
    for (int i = 0; i < 15; ++i)
        checksum += static_cast<uint8_t>(packet[i]);

    packet[15] = checksum;

    serial->write(packet);

    QString hexStr;
    for (int i = 0; i < 16; ++i)
        hexStr += QString("%1 ").arg(static_cast<uint8_t>(packet[i]), 2, 16, QChar('0')).toUpper();

    ui->textEditTx->append("TX: " + hexStr.trimmed());
    qDebug() << "SendRangeVoltage: " << packet.toHex(' ').toUpper();
}

void MainWindow::onSerialReceived()
{
#if 0
    QByteArray data = serial->readAll();
    if (data.size() >= 0) {
        QString hexStr;
        for (int i = 0; i < data.size(); ++i)
            hexStr += QString("%1 ").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();

        ui->textEditRx->append("RX: " + hexStr.trimmed());
        qDebug() << "Received: " << data.toHex(' ').toUpper();
    }
#endif

    serialBuffer += serial->readAll();

    // 顯示完整的 16 Bytes 封包
    while (serialBuffer.size() >= APP_EMU_UART_PACKET_LEN) {
        QByteArray onePacket = serialBuffer.left(APP_EMU_UART_PACKET_LEN);
        serialBuffer.remove(0, APP_EMU_UART_PACKET_LEN);

        QString hexStr;
        for (int i = 0; i < onePacket.size(); ++i)
            hexStr += QString("%1 ").arg(static_cast<uint8_t>(onePacket[i]), 2, 16, QChar('0')).toUpper();

        ui->textEditRx->append("RX: " + hexStr.trimmed());
        qDebug() << "Received (16B): " << onePacket.toHex(' ').toUpper();
    }

    // 若還有殘留不滿16 bytes，啟動延遲顯示定時器
    if (!serialBuffer.isEmpty()) {
        rxDelayTimer->start();  // 每次接收到資料就重新啟動倒數
    }

}

void MainWindow::onCalcCrc15()
{
    uint8_t u8Data[4] = {0};
    bool ok1, ok2;
    uint16_t val1 = ui->lineEditCrc15Data1->text().toUShort(&ok1, 16);
    uint16_t val2 = ui->lineEditCrc15Data2->text().toUShort(&ok2, 16);

    if (!ok1 || !ok2) {
        QMessageBox::warning(this, "Input Error", "Invalid HEX input for CRC15.");
        return;
    }

    u8Data[2] = (val1 & 0xFF);
    u8Data[3] = (val2 & 0xFF);

    uint16_t u16Result = Pec15_Calc(2, &u8Data[2]);

    QString resultStr = QString("CRC15 = 0x%1 (DATA: 0x%2, 0x%3)")
                            .arg(u16Result, 4, 16, QChar('0')).toUpper()
                            .arg(QString("%1").arg(u8Data[2], 2, 16, QChar('0')).toUpper())
                            .arg(QString("%1").arg(u8Data[3], 2, 16, QChar('0')).toUpper());

    resultStr.replace('X','x');

    ui->textEditCrcResult->append(resultStr);
}

void MainWindow::onCalcCrc10()
{
    uint8_t u8Data2[7] = {0};
    bool ok = true;
    QString dataStr;
    for (int i = 0; i < 7; ++i) {
        QString text = crc10Edits[i]->text();
        bool thisOk = false;
        u8Data2[i] = static_cast<uint8_t>(text.toUInt(&thisOk, 16));
        ok &= thisOk;
        dataStr += QString("0x%1, ").arg(u8Data2[i], 2, 16, QChar('0')).toUpper();
    }
    if (!ok) {
        QMessageBox::warning(this, "Input Error", "Invalid HEX input for CRC10.");
        return;
    }

    dataStr = dataStr.trimmed();
    dataStr = dataStr.left(dataStr.length()-1);

    uint16_t u16Result = pec10_calc(true, 6, &u8Data2[0]);
    QString resultStr = QString("CRC10 = 0x%1 (DATA: %2)")
                            .arg(u16Result, 4, 16, QChar('0')).toUpper()
                            .arg(dataStr.trimmed());

    resultStr.replace('X','x');

    ui->textEditCrcResult->append(resultStr);
}
