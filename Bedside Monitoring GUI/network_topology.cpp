#include "network_topology.h"
#include "ui_network_topology.h"

network_topology::network_topology(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::network_topology)
{
    ui->setupUi(this);
}

void network_topology::receive_port(QString p) {
    port = p;
    ui->label_port->setText(port);
}

void network_topology::uart_receive(QString str) {
    ui->textEdit_Status->append(str);
    ui->textEdit_Status->ensureCursorVisible();
}

void network_topology::uart_packet_received(QByteArray str) {

    repaint_window = false;

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

    // Node and Edge List Maintenance
    foreach(QString node, node_dict.keys()) {
        node_dict[node]++;
    }

    for (int i=0; i <= patient.depth; i++) {
        QString u8_0 = patient.history[i].u8[0] >= 0x10 ? QString("%1").arg(patient.history[i].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[0], 0, 16);
        QString u8_1 = patient.history[i].u8[1] >= 0x10 ? QString("%1").arg(patient.history[i].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[1], 0, 16);
        QString node_id = "0x" + u8_0 + u8_1;
        if (node_dict.contains(node_id)) {
            node_dict[node_id] = 0;
        } else {
            node_dict.insert(node_id, 1);
            repaint_window = true;
        }
    }

    foreach(QString a, edge_dict.keys()) {
        foreach(QString b, edge_dict[a].keys()) {
            edge_dict[a][b]++;
        }
    }

    for (int i=0; i <= patient.depth-1; i++) {
        QString u8_0_a = patient.history[i].u8[0] >= 0x10 ? QString("%1").arg(patient.history[i].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[0], 0, 16);
        QString u8_1_a = patient.history[i].u8[1] >= 0x10 ? QString("%1").arg(patient.history[i].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i].u8[1], 0, 16);
        QString u8_0_b = patient.history[i+1].u8[0] >= 0x10 ? QString("%1").arg(patient.history[i+1].u8[0], 0, 16)
                : "0" + QString("%1").arg(patient.history[i+1].u8[0], 0, 16);
        QString u8_1_b = patient.history[i+1].u8[1] >= 0x10 ? QString("%1").arg(patient.history[i+1].u8[1], 0, 16)
                : "0" + QString("%1").arg(patient.history[i+1].u8[1], 0, 16);
        QString a = "0x" + u8_0_a + u8_1_a;
        QString b = "0x" + u8_0_b + u8_1_b;
        \
        if ((edge_dict.contains(a) && edge_dict[a].contains(b))) edge_dict[a][b] = 0;
        else if ((edge_dict.contains(b) && edge_dict[b].contains(a))) edge_dict[b][a] = 0;
        else {
            edge_dict[a].insert(b, 1);
            repaint_window = true;
        }
    }

    // Remove expired nodes and edges

    foreach(QString search_node, node_dict.keys()) {
        if (node_dict[search_node] > MAX_NEIGHBORS*2) {
            node_dict.remove(search_node);
            edge_dict.remove(search_node);

            foreach(QString search_edge, edge_dict.keys()) {
                edge_dict[search_edge].remove(search_node);
            }

            repaint_window = true;
        }
    }

    foreach(QString a, edge_dict.keys()) {
        foreach(QString b, edge_dict[a].keys()) {
            if (edge_dict[a][b] > MAX_NEIGHBORS*2) {
                edge_dict[a].remove(b);
                repaint_window = true;
            }
        }
    }

    ui->textEdit_Status->append("List of Nodes: ");
    foreach(QString node, node_dict.keys()) {
        ui->textEdit_Status->append(node);
    }

    ui->textEdit_Status->append("List of Edges: ");
    foreach(QString a, edge_dict.keys()) {
        foreach(QString b, edge_dict[a].keys()) {
            ui->textEdit_Status->append(a + " " + b + " "  + QString::number(edge_dict[a][b]));
        }
    }

    // Start doing things

    if (repaint_window) this->repaint();

}

network_topology::~network_topology()
{
    delete ui;
}

void network_topology::paintEvent(QPaintEvent *e){
    if (repaint_window) {
        QPainter painter(this);
        int coordinateX = 370; int coordinateY = 110;
        int rectwidth = 400; int rectheight = 380;
        int coordinateX_center = coordinateX + rectwidth / 2; int coordinateY_center = coordinateY + rectheight/2;
        painter.eraseRect(coordinateX, coordinateY, rectwidth, rectheight);   // Clean the designated painting area

        QRect rec(coordinateX, coordinateY, rectwidth, rectheight);
        QPen framepen(Qt::red);
        framepen.setWidth(4);
        painter.drawRect(rec);                 // Draw the designated painting area

        int node_radius_gateway = 4;
        int node_radius_router = 3;
        int node_radius_patient = 2;

        // Set the font size
        QFont font;
        font.setPointSize(5); // Set the desired font size

        // Set the font in the painter
        painter.setFont(font);

        // Legends
        QPoint info1(coordinateX + node_radius_gateway + 10, coordinateY + rectheight + node_radius_gateway + 10);
        painter.setBrush(Qt::magenta);
        painter.drawEllipse(info1, node_radius_gateway*2, node_radius_gateway*2);
        painter.drawText(info1 + QPoint(10, 3), "Gateway");

        QPoint info2(info1 + QPoint(50, 0));
        painter.setBrush(Qt::blue);
        painter.drawEllipse(info2, node_radius_router*2, node_radius_router*2);
        painter.drawText(info2 + QPoint(8, 3), "Router");

        QPoint info3(info2 + QPoint(40, 0));
        painter.setBrush(Qt::cyan);
        painter.drawEllipse(info3, node_radius_patient*2, node_radius_patient*2);
        painter.drawText(info3 + QPoint(6, 3), "Patient");
        
        QMap<QString, QPoint> node_map;

        // Gateway
        int coordinateX_gateway = coordinateX + 20;
        int coordinateY_gateway = coordinateY_center;

        QPoint gateway(coordinateX_gateway, coordinateY_gateway);
        node_map["0xffff"] = gateway;
        font.setPointSize(7);
        painter.setFont(font);
        painter.setBrush(Qt::magenta);
        painter.drawEllipse(gateway, node_radius_gateway*2, node_radius_gateway*2);
        painter.drawText(gateway + QPoint(0, -10), "0xffff");

        // Routers and Patients
        int router_count = 0;
        int router_gap;
        int coordinateY_router;
        
        int patient_count = 0;
        int patient_gap;
        int coordinateY_patient;
        
        foreach(QString node, node_dict.keys()) {
            if (node >= "0x8000" && node < "0xffff") router_count++;
            else if (node < "0x8000") patient_count++;
        }
       
        if (router_count == 1) {
            router_gap = 0;
            coordinateY_router = coordinateY_center;
        } else {
            router_gap = 340 / (router_count - 1);
            coordinateY_router = coordinateY + 20;
        }
        int coordinateX_router = coordinateX_center;
        int router_offset_x = -10;
        int router_offset_y = 10;
        
        if (patient_count == 1) {
            patient_gap = 0;
            coordinateY_patient = coordinateY_center;
        } else {
            patient_gap = 340 / (patient_count - 1);
            coordinateY_patient = coordinateY + 20;
        }
        int coordinateX_patient = coordinateX + rectwidth - 20;
        int patient_offset_x = 10;
        int patient_offset_y = -10;

        int i = 0; int j = 0;
        foreach(QString node, node_dict.keys()) {
            if (node >= "0x8000" && node < "0xffff") {
                router_offset_x *= -1;
                router_offset_y *= -1;
                QPoint coordinates(coordinateX_router + router_offset_x, coordinateY_router + router_offset_y + i*router_gap);
                node_map[node] = coordinates;
                painter.setBrush(Qt::blue);
                painter.drawEllipse(coordinates, node_radius_router*2, node_radius_router*2);
                painter.drawText(coordinates + QPoint(0, -10), node);
                i++;
            }
            else if (node < "0x8000") {
                patient_offset_x *= -1;
                patient_offset_y *= -1;
                QPoint coordinates(coordinateX_patient + patient_offset_x, coordinateY_patient + patient_offset_y + j*patient_gap);
                node_map[node] = coordinates;
                painter.setBrush(Qt::cyan);
                painter.drawEllipse(coordinates, node_radius_patient*2, node_radius_patient*2);
                painter.drawText(coordinates + QPoint(0, -10), node);
                j++;
            }
        }
        
        // Draw edges
        foreach(QString a, edge_dict.keys()) {
            foreach(QString b, edge_dict[a].keys()) {
                QPoint coordinate_a = node_map[a];
                QPoint coordinate_b = node_map[b];
                painter.drawLine(coordinate_a, coordinate_b);
            }
        }

        repaint_window = false;
    }

}


void network_topology::on_pushButton_clicked()
{
    ui->textEdit_Status->clear();
    repaint_window = true;
}

