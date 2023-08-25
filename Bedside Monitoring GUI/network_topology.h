#ifndef NETWORK_TOPOLOGY_H
#define NETWORK_TOPOLOGY_H

#include <QDialog>
#include <QMessageBox>
#include <QtGui>
#include <QtCore>
#include "qextserialport.h"
#include "qextserialenumerator.h"
#include "custom_struct.h"

namespace Ui {
class network_topology;
}

class network_topology : public QDialog
{
    Q_OBJECT

public:
    explicit network_topology(QWidget *parent = nullptr);
    ~network_topology();
    QMap<QString, int> node_dict;
    QMap<QString, QMap<QString, int>> edge_dict;
    QString port;
    bool repaint_window;

private:
    Ui::network_topology *ui;

protected:
    void paintEvent(QPaintEvent *e);        // For the QPainter

public slots:
    void receive_port(QString port);
    void uart_receive(QString str);
    void uart_packet_received(QByteArray str);

private slots:
    void on_pushButton_clicked();
};

#endif // NETWORK_TOPOLOGY_H
