#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "network_topology.h"
#include <QMessageBox>
#include <QtGui>
#include <QtCore>
#include "qextserialport.h"
#include "qextserialenumerator.h"
#include "custom_struct.h"
#include "uart.h"
#include "stdlib.h"
#include <string>



namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void table_entry(patient_message received_message);
    QMap<QString, int> table_dict;
    QMap<QString, bool> list_dict;
    QMap<QString, int> node_dict;
    QMap<QString, QMap<QString, int>> edge_dict;
    bool show_urgent = false;
    QString portname;

protected:
//    void changeEvent(QEvent *e);

private slots:

    void on_ack_clicked();

    void on_checkBox_stateChanged(int arg1);

    void on_pushButton_nt_clicked();

    void packet_received(QByteArray str);

    void receive(QString str);

    void on_pushButton_open_clicked();

    void on_pushButton_close_clicked();

    void on_ack_done_clicked();

private:
    Ui::MainWindow *ui;
    network_topology *nt;
    QextSerialPort port;
    QMessageBox error;
    Uart *uart;

signals:
    void transmit_port(QString port);
    void transmit_uart(QString str);
    void transmit_uart_packet(QByteArray str);

};
#endif // MAINWINDOW_H
