#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Get available COM Ports
    this->uart = new Uart(this);

    QList<QextPortInfo> ports = uart->getUSBPorts();
    for (int i = 0; i < ports.size(); i++) {
        ui->comboBox_port->addItem(ports.at(i).portName.toLocal8Bit().constData());
    }
    QObject::connect(uart, SIGNAL(debugReceived(QString)), this, SLOT(receive(QString)));
    QObject::connect(uart, SIGNAL(packetReceived(QByteArray)), this, SLOT(packet_received(QByteArray)));

    ui->table_patient->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->pushButton_nt->setEnabled(false);

    // Dummy Data

//    static linkaddr_t add_1;
//    add_1.u8[0] = 0xAA;
//    add_1.u8[1] = 0xBB;
//    add_1.u16 = 0xFFFF;

//    static linkaddr_t add_2;
//    add_2.u8[0] = 0xAB;
//    add_2.u8[1] = 0xBA;
//    add_2.u16 = 0xFFFF;

//    static linkaddr_t add_3;
//    add_3.u8[0] = 0xAC;
//    add_3.u8[1] = 0xBC;
//    add_3.u16 = 0xFFFF;

//    static linkaddr_t add_4;
//    add_4.u8[0] = 0xAD;
//    add_4.u8[1] = 0xBD;
//    add_4.u16 = 0xFFFF;

//    static struct patient_message patient_1;
//    patient_1.sensor_value = 30;
//    patient_1.Patient_Motion = true;
//    patient_1.history[0] = add_1;
//    patient_1.history[1] = add_2;
//    patient_1.depth = 1;

//    static struct patient_message patient_2;
//    patient_2.sensor_value = 90;
//    patient_2.Patient_Motion = true;
//    patient_2.history[0] = add_2;
//    patient_2.history[1] = add_1;
//    patient_2.depth = 1;

//    static struct patient_message patient_3;
//    patient_3.sensor_value = 59;
//    patient_3.Patient_Motion = true;
//    patient_3.history[0] = add_3;
//    patient_3.history[1] = add_1;
//    patient_3.depth = 1;

//    static struct patient_message patient_4;
//    patient_4.sensor_value = 80;
//    patient_4.Patient_Motion = false;
//    patient_4.history[0] = add_4;
//    patient_4.history[1] = add_1;
//    patient_4.depth = 1;


//    static struct routing_message received_message;
//    received_message.patient_messages[0] = patient_1;
//    received_message.patient_messages[1] = patient_2;
//    received_message.patient_messages[2] = patient_3;
//    received_message.patient_messages[3] = patient_4;
//    received_message.count = 4;

    // table_entry(received_message);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::table_entry(patient_message received_message) {

    QTableWidget* tp = ui->table_patient;
    QListWidget* an = ui->alert_notif;

    QString patient_id = QString("%1").arg(received_message.history[0].u8[1], 0, 16);
    uint16_t bpm = received_message.sensor_value;
    bool in_bed = received_message.Patient_Motion;

    // Check if there is an existing data in table
    int current_row;
    bool matched_table = false;
    QList<QTableWidgetItem*> table_matching_items =tp->findItems(patient_id, Qt::MatchExactly);
    if (!table_matching_items.isEmpty()) {
        foreach(QTableWidgetItem* item, table_matching_items) {
            if (item->column() == 0) {
                matched_table = true;
                current_row = item->row();
            }
        }
    }

    if (!matched_table) {
        current_row = tp->rowCount();
        tp->insertRow(current_row);
    }

    // Check if there is an existing data in list
    int current_list;
    bool matched_list = false;
    for (int i = 0; i < an->count(); ++i) {
        QListWidgetItem* currentItem = an->item(i);
        if (currentItem->text().contains("Patient " + patient_id, Qt::CaseInsensitive)) {
            matched_list = true;
            current_list = i;
        }
    }

    if (!matched_list) {
        current_list = an->count();
        an->addItem("");
    }

    tp->setItem(current_row,0,new QTableWidgetItem(patient_id));
    tp->setItem(current_row,1,new QTableWidgetItem(QString::number(bpm)));
    tp->setItem(current_row,2,new QTableWidgetItem(QString(in_bed ? "True" : "False")));

    QListWidgetItem *current_list_item = an->item(current_list);

    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestamp = " [" + currentDateTime.toString("yyyy-MM-dd HH:mm:ss") + "]";

    if (!in_bed) {
        tp->setItem(current_row,3,new QTableWidgetItem(QString("Not in Bed")));
        for (int j=0; j < tp->columnCount(); j++) tp->item(current_row,j)->setBackground(Qt::gray);
        if (!list_dict.contains(patient_id)) {
            current_list_item->setText(QString("Patient %1: Not in Bed").arg(patient_id) + timestamp);
            current_list_item->setBackground(Qt::gray);
            list_dict.insert(patient_id, false);
        }
    }
    else if (bpm >= 60 && bpm <= 100) {
        tp->setItem(current_row,3,new QTableWidgetItem(QString("Good")));
        tp->item(current_row,3)->setBackground(Qt::green);
        if (!matched_list) {
            QListWidgetItem *item = an->takeItem(current_list);
            delete item;
        }
    }
    else if ((bpm > 40 && bpm < 60) || (bpm > 100 && bpm <= 140)) {
        tp->setItem(current_row,3,new QTableWidgetItem(QString("Attention")));
        tp->item(current_row,3)->setBackground(Qt::yellow);
        if (!list_dict.contains(patient_id)) {
            current_list_item->setText(QString("Patient %1: Requires Attention").arg(patient_id) + timestamp);
            current_list_item->setBackground(Qt::yellow);
            list_dict.insert(patient_id, false);
        }
    }
    else if (bpm <= 40 || bpm > 140) {
        tp->setItem(current_row,3,new QTableWidgetItem(QString("Emergency")));
        tp->item(current_row,3)->setBackground(Qt::red);
        if (!list_dict.contains(patient_id)) {
            current_list_item->setText(QString("Patient %1: Emergency").arg(patient_id) + timestamp);
            current_list_item->setBackground(Qt::red);
            list_dict.insert(patient_id, false);
        }
    }
}

void MainWindow::on_pushButton_nt_clicked()
{
    nt = new network_topology(this);
    QObject::connect(this,  SIGNAL(transmit_port(QString)), nt, SLOT(receive_port(QString)));
    QObject::connect(this,  SIGNAL(transmit_uart(QString)), nt, SLOT(uart_receive(QString)));
    QObject::connect(this, SIGNAL(transmit_uart_packet(QByteArray)), nt, SLOT(uart_packet_received(QByteArray)));
    emit transmit_port(portname);
    nt->show();
    ui->pushButton_nt->setEnabled(false);
}


void MainWindow::on_ack_clicked()
{
    QListWidgetItem* item = ui->alert_notif->currentItem();
    QString text = item->text();
    int colonIndex = text.indexOf(":");
    int spaceIndex = text.indexOf(" ");
    QString patient_id = text.mid(spaceIndex + 1, colonIndex - spaceIndex - 1);
    if (!list_dict[patient_id]) {
        item->setText(text + " [ON CHECK]");
        item->setBackground(Qt::white);
        list_dict[patient_id] = true;
    }
}

void MainWindow::on_ack_done_clicked()
{
    QListWidgetItem* item = ui->alert_notif->currentItem();
    QString text = item->text();
    int colonIndex = text.indexOf(":");
    int spaceIndex = text.indexOf(" ");
    QString patient_id = text.mid(spaceIndex + 1, colonIndex - spaceIndex - 1);
    if (list_dict[patient_id]) {
        delete ui->alert_notif->currentItem();
        list_dict.remove(patient_id);
    }
}


void MainWindow::on_checkBox_stateChanged(int arg1)
{
    QTableWidget* tp = ui->table_patient;

    if (arg1) {
        show_urgent = true;
        QList<QTableWidgetItem*> table_matching_items
                = tp->findItems("Good", Qt::MatchExactly);
        if (!table_matching_items.isEmpty()) {
            foreach(QTableWidgetItem* item, table_matching_items) {
                tp->hideRow(item->row());
            }
        }
    }
    else {
        show_urgent = false;
        int row_count = tp->rowCount();
        for(int i=0; i<row_count; i++) {
            tp->showRow(i);
        }
    }
}

void MainWindow::packet_received(QByteArray str) {

    if (str.length() == 0) return;

    emit transmit_uart_packet(str);

    qDebug() << "Bytes received: " << QString::number(str.count());

    struct patient_message patient;
    patient.sensor_value = 0x80 + (128 + (int) str.at(0));
    patient.Patient_Motion = str.at(1);

    patient.depth = (int)(str.at(str.count()-2))+0;

    for (int i=0; i <= patient.depth; i++) {
        int k = 2 + i*2;
        for (int j=0; j < 2; j++) {
            patient.history[i].u8[j] = 0x80 + (128 + (int) str.at(k+j));
        }
    }

    QString patient_id = QString("%1").arg(patient.history[0].u8[1], 0, 16);

    // Table maintenance, remove stalling data
    foreach(QString search_patient, table_dict.keys()) {
        table_dict[search_patient]++;
    }

    if (table_dict.contains(patient_id)) {
        table_dict[patient_id] = 0;
    } else {
        table_dict.insert(patient_id, 1);
    }

    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestamp = " [" + currentDateTime.toString("yyyy-MM-dd HH:mm:ss") + "]";

    foreach(QString search_patient, table_dict.keys()) {
        QTableWidget* tp = ui->table_patient;
        QListWidget* an = ui->alert_notif;
        if (table_dict[search_patient] > MAX_NEIGHBORS*2) {
            table_dict.remove(search_patient);
            QList<QTableWidgetItem*> table_matching_items
                    = tp->findItems(search_patient, Qt::MatchExactly);
            if (!table_matching_items.isEmpty()) {
                foreach(QTableWidgetItem* item, table_matching_items) {
                    if (item->column() == 0) {
                        tp->removeRow(item->row());
                        list_dict[search_patient] = false;
                        int current_list = an->count();
                        for (int i = 0; i < an->count(); ++i) {
                            QListWidgetItem* currentItem = an->item(i);
                            if (currentItem->text().contains("Patient " + search_patient, Qt::CaseInsensitive)) {
                                current_list = i;
                            }
                        }
                        an->item(current_list)->setText(QString("Patient %1: OFFLINE").arg(search_patient) + timestamp);
                        an->item(current_list)->setBackground(Qt::white);
                    }
                }
            }
        }
    }

    qDebug() << "BPM: " << QString::number(patient.sensor_value);
    qDebug() << "In Bed: " << QString::number(patient.Patient_Motion);
    qDebug() << "History: ";
    for (int i=0; i <= patient.depth; i++) {
        QString u8_0 = patient.history[i].u8[0] >= 10 ? QString("%1").arg(patient.history[i].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[0], 0, 16);
        QString u8_1 = patient.history[i].u8[1] >= 10 ? QString("%1").arg(patient.history[i].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[1], 0, 16);
        qDebug() << "0x" + u8_0 + u8_1;
    }
    qDebug() << "Depth: " << QString::number(patient.depth);

    table_entry(patient);

    // Node and Edge List Maintenance
    foreach(QString node, node_dict.keys()) {
        node_dict[node]++;
    }

    for (int i=0; i <= patient.depth; i++) {
        QString u8_0 = patient.history[i].u8[0] >= 10 ? QString("%1").arg(patient.history[i].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[0], 0, 16);
        QString u8_1 = patient.history[i].u8[1] >= 10 ? QString("%1").arg(patient.history[i].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[1], 0, 16);
        QString node_id = "0x" + u8_0 + u8_1;
        if (node_dict.contains(node_id)) {
            node_dict[node_id] = 0;
        } else {
            node_dict.insert(node_id, 1);
        }
    }

    for (int i=0; i <= patient.depth-1; i++) {
        QString u8_0_a = patient.history[i].u8[0] >= 10 ? QString("%1").arg(patient.history[i].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[0], 0, 16);
        QString u8_1_a = patient.history[i].u8[1] >= 10 ? QString("%1").arg(patient.history[i].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[1], 0, 16);
        QString u8_0_b = patient.history[i].u8[0] >= 10 ? QString("%1").arg(patient.history[i+1].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i+1].u8[0], 0, 16);
        QString u8_1_b = patient.history[i].u8[1] >= 10 ? QString("%1").arg(patient.history[i+1].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i+1].u8[1], 0, 16);
        QString a = "0x" + u8_0_a + u8_1_a;
        QString b = "0x" + u8_0_b + u8_1_b;
        \
        if ((edge_dict.contains(a) && edge_dict[a].contains(b))) edge_dict[a][b]++;
        else if ((edge_dict.contains(b) && edge_dict[b].contains(a))) edge_dict[b][a]++;
        else {
            edge_dict[a].insert(b, 1);
        }
    }

    foreach(QString search_node, node_dict.keys()) {
        if (node_dict[search_node] > MAX_NEIGHBORS*2) {
            node_dict.remove(search_node);
            edge_dict.remove(search_node);

            foreach(QString search_edge, edge_dict.keys()) {
                edge_dict[search_edge].remove(search_node);
            }
        }
    }

    int patient_count = 0;
    int routing_count = 0;
    int gateway_count = 0;

    foreach(QString node, node_dict.keys()) {
        if (node == "0xFFFF" || node == "0xffff") gateway_count++;
        else if (node < "0x8000") patient_count++;
        else if (node >= "0x8000") routing_count++;
    }

    ui->label_patient_count->setText(QString::number(patient_count));
    ui->label_routing_count->setText(QString::number(routing_count));
    ui->label_gateway_count->setText(QString::number(gateway_count));

    // Urgent Patients Tick Behaviour
    QTableWidget* tp = ui->table_patient;
    int row_count = tp->rowCount();
    for(int i=0; i<row_count; i++) {
        tp->showRow(i);
    }

    if (show_urgent) {
        QList<QTableWidgetItem*> table_matching_items
                = tp->findItems("Good", Qt::MatchExactly);
        if (!table_matching_items.isEmpty()) {
            foreach(QTableWidgetItem* item, table_matching_items) {
                tp->hideRow(item->row());
            }
        }
    }

}

void MainWindow::receive(QString str) {

    emit transmit_uart(str);
}

void MainWindow::on_pushButton_open_clicked()
{
    portname = "/dev/";
    portname.append(ui->comboBox_port->currentText());
    uart->open(portname);
    if (!uart->isOpen())
    {
        error.setText("Unable to open port!");
        error.show();
        return;
    }

    ui->pushButton_close->setEnabled(true);
    ui->pushButton_open->setEnabled(false);
    ui->comboBox_port->setEnabled(false);
    ui->pushButton_nt->setEnabled(true);

    if (nt) {
        emit transmit_port(portname);
    }

}

void MainWindow::on_pushButton_close_clicked()
{
    if (uart->isOpen()) uart->close();
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->comboBox_port->setEnabled(true);

    if (nt) {
        emit transmit_port("");
    }
}


